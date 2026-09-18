#include "badblocks.h"
#include "device.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

int rufux_badblocks(const char *path, int allow_file, RufuxScanProgress cb, void *user,
                    unsigned long long *bad_out, char *err, unsigned long cap) {
  // safety: blocks must be unmounted/fixed-guarded; files need --allow-file
  if (rufux_check_target(path, 1 /*scan allows fixed*/, allow_file, err, cap) != 0) return -1;
  struct stat st;
  unsigned long long total = 0;
  int is_reg = (stat(path, &st) == 0 && S_ISREG(st.st_mode));
  if (is_reg) total = (unsigned long long)st.st_size;
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) { snprintf(err, cap, "cannot open '%s': %s", path, strerror(errno)); return -1; }
  if (!is_reg) {
    unsigned long long b = 0;
    if (ioctl(fd, BLKGETSIZE64, &b) == 0) total = b;
  }
  char *buf = malloc(1 << 20);
  if (!buf) { close(fd); snprintf(err, cap, "out of memory"); return -1; }
  unsigned long long done = 0, bad = 0;
  ssize_t r;
  while ((r = read(fd, buf, 1 << 20)) > 0) {
    done += (unsigned long long)r;
    if (cb) cb(done, total, user);
  }
  if (r < 0) bad++; // short read error counts as bad region
  free(buf);
  close(fd);
  if (cb) cb(done, total, user);
  if (bad_out) *bad_out = bad;
  return 0;
}

int rufux_badblocks_write(const char *path, int allow_file, int passes, int first,
                          RufuxScanProgress cb, void *user,
                          unsigned long long *bad_out, char *err, unsigned long cap) {
  static const unsigned char patterns[] = {0xAA, 0x55, 0xFF, 0x00};
  if (passes < 1) passes = 1;
  if (passes > 4) passes = 4;
  // safety: same guards as the read scan (unmounted, fixed, files)
  if (rufux_check_target(path, 1, allow_file, err, cap) != 0) return -1;
  struct stat st;
  unsigned long long total = 0;
  int is_reg = (stat(path, &st) == 0 && S_ISREG(st.st_mode));
  if (is_reg) total = (unsigned long long)st.st_size;
  int fd = open(path, O_RDWR | O_CLOEXEC);
  if (fd < 0) { snprintf(err, cap, "cannot open '%s': %s", path, strerror(errno)); return -1; }
  if (!is_reg) {
    unsigned long long b = 0;
    if (ioctl(fd, BLKGETSIZE64, &b) != 0 || b == 0) {
      snprintf(err, cap, "cannot size '%s'", path);
      close(fd);
      return -1;
    }
    total = b;
  }
  if (total == 0) { snprintf(err, cap, "target '%s' is empty", path); close(fd); return -1; }
  char *wbuf = malloc(1 << 20);
  char *rbuf = malloc(1 << 20);
  if (!wbuf || !rbuf) {
    snprintf(err, cap, "out of memory");
    free(wbuf); free(rbuf); close(fd);
    return -1;
  }
  unsigned long long grand = 2ULL * (unsigned long long)passes * total;
  unsigned long long done = 0, bad = 0;
  int rc = 0;
  for (int p = 0; p < passes && !rc; p++) {
    unsigned char pat = patterns[(first + p) % 4];
    memset(wbuf, pat, 1 << 20);
    // write phase
    if (lseek(fd, 0, SEEK_SET) != 0) { snprintf(err, cap, "seek failed"); rc = -1; break; }
    unsigned long long left = total;
    while (left > 0) {
      size_t want = left > (1u << 20) ? (1u << 20) : (size_t)left;
      ssize_t w = write(fd, wbuf, want);
      if (w <= 0) {
        if (w < 0 && errno == EINTR) continue;
        snprintf(err, cap, "write failed at pass %d: %s", p + 1, strerror(errno));
        rc = -1;
        break;
      }
      left -= (unsigned long long)w;
      done += (unsigned long long)w;
      if (cb) cb(done, grand, user);
    }
    if (rc) break;
    if (fsync(fd) != 0) { snprintf(err, cap, "fsync failed"); rc = -1; break; }
    // verify phase
    if (lseek(fd, 0, SEEK_SET) != 0) { snprintf(err, cap, "seek failed"); rc = -1; break; }
    left = total;
    while (left > 0) {
      size_t want = left > (1u << 20) ? (1u << 20) : (size_t)left;
      ssize_t r = read(fd, rbuf, want);
      if (r <= 0) {
        if (r < 0 && errno == EINTR) continue;
        bad++; // unreadable region
        break;
      }
      for (ssize_t i = 0; i < r; i++) {
        if ((unsigned char)rbuf[i] != pat) { bad++; break; }
      }
      left -= (unsigned long long)r;
      done += (unsigned long long)r;
      if (cb) cb(done, grand, user);
    }
  }
  free(wbuf);
  free(rbuf);
  close(fd);
  if (!rc && cb) cb(grand, grand, user);
  if (bad_out) *bad_out = bad;
  return rc;
}

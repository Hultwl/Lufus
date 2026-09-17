#define _GNU_SOURCE
#include "writer.h"
#include "device.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#define CHUNK (1u << 20)

int lufus_write_image(const char *src, const char *dst,
                      const LufusWriteOpts *opts,
                      LufusWriteProgress cb, void *user,
                      char *err, unsigned long errcap) {
  struct stat sst;
  if (stat(src, &sst) != 0 || !S_ISREG(sst.st_mode)) {
    snprintf(err, errcap, "source '%s' is not a readable file", src);
    return -1;
  }
  unsigned long long total = (unsigned long long)sst.st_size;
  if (total == 0) {
    snprintf(err, errcap, "source '%s' is empty", src);
    return -1;
  }
  if (lufus_check_target(dst, opts->allow_fixed, opts->allow_file,
                         err, errcap) != 0)
    return -1;

  FILE *fin = fopen(src, "rb");
  if (!fin) {
    snprintf(err, errcap, "cannot open source '%s': %s", src, strerror(errno));
    return -1;
  }

  if (opts->dry_run) {
    // simulate: read source fully, report progress, write nothing
    char *buf = malloc(CHUNK);
    unsigned long long done = 0;
    size_t n;
    while ((n = fread(buf, 1, CHUNK, fin)) > 0) {
      done += n;
      if (cb) cb(done, total, user);
    }
    int bad = ferror(fin);
    free(buf);
    fclose(fin);
    if (bad) { snprintf(err, errcap, "read error on '%s'", src); return -1; }
    if (cb) cb(total, total, user);
    return 0;
  }

  if (!opts->yes) {
    fclose(fin);
    snprintf(err, errcap, "refusing real write without --yes (use --dry-run to simulate)");
    return -1;
  }

  struct stat dstst;
  int dst_is_reg = (stat(dst, &dstst) == 0 && S_ISREG(dstst.st_mode));
  int fd = -1;
  if (dst_is_reg) {
    fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  } else {
    fd = open(dst, O_WRONLY | O_CLOEXEC);
    if (fd >= 0) {
      // best-effort exclusive lock so two writers can't race
      if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        snprintf(err, errcap, "target '%s' is busy/locked", dst);
        close(fd); fclose(fin);
        return -1;
      }
    }
  }
  if (fd < 0) {
    snprintf(err, errcap, "cannot open target '%s': %s (need sudo?)", dst, strerror(errno));
    fclose(fin);
    return -1;
  }

  // If block device, check capacity fits
  if (!dst_is_reg) {
    unsigned long long cap = 0;
    if (ioctl(fd, BLKGETSIZE64, &cap) == 0 && cap < total) {
      snprintf(err, errcap, "image (%llu B) larger than target (%llu B)", total, cap);
      close(fd); fclose(fin);
      return -1;
    }
  }

  char *buf = malloc(CHUNK);
  if (!buf) {
    snprintf(err, errcap, "out of memory");
    close(fd); fclose(fin);
    return -1;
  }
  unsigned long long done = 0;
  size_t n;
  int rc = 0;
  while ((n = fread(buf, 1, CHUNK, fin)) > 0) {
    size_t off = 0;
    while (off < n) {
      ssize_t w = write(fd, buf + off, n - off);
      if (w < 0) {
        if (errno == EINTR) continue;
        snprintf(err, errcap, "write failed at %llu: %s", done + off, strerror(errno));
        rc = -1;
        break;
      }
      off += (size_t)w;
    }
    if (rc != 0) break;
    done += n;
    if (cb) cb(done, total, user);
  }
  if (!rc && ferror(fin)) {
    snprintf(err, errcap, "read error on '%s'", src);
    rc = -1;
  }
  free(buf);
  fclose(fin);
  if (fsync(fd) != 0) {
    snprintf(err, errcap, "fsync failed: %s", strerror(errno));
    rc = -1;
  }
  // revalidate + rescan partitions for block devices
  if (!rc && !dst_is_reg) {
    ioctl(fd, BLKRRPART);
  }
  if (cb) cb(done, total, user);

  if (!rc && opts->verify) {
    FILE *fa = fopen(src, "rb");
    int fb = open(dst, O_RDONLY | O_CLOEXEC);
    if (!fa || fb < 0) {
      snprintf(err, errcap, "verify: cannot reopen src/dst");
      if (fa) fclose(fa);
      if (fb >= 0) close(fb);
      close(fd);
      return -1;
    }
    char *ba = malloc(CHUNK), *bb = malloc(CHUNK);
    unsigned long long vdone = 0;
    size_t na;
    // dst offset 0; for block, reads from start
    lseek(fb, 0, SEEK_SET);
    while ((na = fread(ba, 1, CHUNK, fa)) > 0) {
      size_t got = 0;
      while (got < na) {
        ssize_t r = read(fb, bb + got, na - got);
        if (r <= 0) break;
        got += (size_t)r;
      }
      if (got != na || memcmp(ba, bb, na) != 0) {
        snprintf(err, errcap, "verify mismatch at offset %llu", vdone);
        rc = -1;
        break;
      }
      vdone += na;
      if (cb) cb(vdone, total, user);
    }
    free(ba); free(bb);
    fclose(fa); close(fb);
  }
  close(fd);
  return rc;
}

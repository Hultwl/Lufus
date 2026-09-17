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

int lufus_badblocks(const char *path, int allow_file, LufusScanProgress cb, void *user,
                    unsigned long long *bad_out, char *err, unsigned long cap) {
  // safety: blocks must be unmounted/fixed-guarded; files need --allow-file
  if (lufus_check_target(path, 1 /*scan allows fixed*/, allow_file, err, cap) != 0) return -1;
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

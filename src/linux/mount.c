#include "mount.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void lufus_part1(const char *disk, char *out, unsigned long cap) {
  size_t L = strlen(disk);
  // nvme/mmcblk/loop need 'p' separator: /dev/nvme0n1p1, /dev/mmcblk0p1
  int need_p = 0;
  if (L && isdigit((unsigned char)disk[L - 1])) need_p = 1;
  snprintf(out, cap, "%s%s1", disk, need_p ? "p" : "");
}

static int run_capture(const char *cmd, char *out, unsigned long cap) {
  FILE *p = popen(cmd, "r");
  if (!p) return -1;
  size_t n = 0;
  int c;
  while ((c = fgetc(p)) != EOF && n + 1 < cap) out[n++] = (char)c;
  out[n] = 0;
  int rc = pclose(p);
  return rc == 0 ? 0 : -1;
}

int lufus_mount(const char *dev, int dry_run, char *mnt_out, unsigned long cap,
                char *err, unsigned long errcap) {
  if (!lufus_have("/usr/bin/udisksctl")) {
    snprintf(err, errcap, "udisksctl not found (install udisks2)");
    return -1;
  }
  char cmd[512];
  snprintf(cmd, sizeof cmd, "/usr/bin/udisksctl mount -b %s --no-user-interaction 2>&1", dev);
  if (dry_run) { fprintf(stderr, "+ %s\n", cmd); snprintf(mnt_out, cap, "(dry-run)"); return 0; }
  char out[1024] = {0};
  if (run_capture(cmd, out, sizeof out) != 0) {
    snprintf(err, errcap, "udisksctl mount failed: %.400s", out);
    return -1;
  }
  // "Mounted /dev/sda1 at /run/media/user/LUFUS."
  const char *at = strstr(out, " at ");
  if (!at) { snprintf(err, errcap, "cannot parse mount output: %.400s", out); return -1; }
  at += 4;
  size_t i = 0;
  while (at[i] && at[i] != '\n' && at[i] != '.' && i + 1 < cap) { mnt_out[i] = at[i]; i++; }
  mnt_out[i] = 0;
  // trim trailing spaces/dots
  while (i && (mnt_out[i-1] == ' ' || mnt_out[i-1] == '.')) mnt_out[--i] = 0;
  if (!mnt_out[0]) { snprintf(err, errcap, "empty mountpoint"); return -1; }
  return 0;
}

int lufus_unmount(const char *dev, int dry_run, char *err, unsigned long errcap) {
  if (!lufus_have("/usr/bin/udisksctl")) {
    snprintf(err, errcap, "udisksctl not found (install udisks2)");
    return -1;
  }
  char cmd[512];
  snprintf(cmd, sizeof cmd, "/usr/bin/udisksctl unmount -b %s --no-user-interaction 2>&1", dev);
  if (dry_run) { fprintf(stderr, "+ %s\n", cmd); return 0; }
  char out[1024] = {0};
  if (run_capture(cmd, out, sizeof out) != 0) {
    snprintf(err, errcap, "udisksctl unmount failed: %.400s", out);
    return -1;
  }
  return 0;
}

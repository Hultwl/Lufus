#include "mount.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void rufux_part1(const char *disk, char *out, unsigned long cap) {
  size_t L = strlen(disk);
  // nvme/mmcblk/loop need 'p' separator: /dev/nvme0n1p1, /dev/mmcblk0p1
  int need_p = 0;
  if (L && isdigit((unsigned char)disk[L - 1])) need_p = 1;
  snprintf(out, cap, "%s%s1", disk, need_p ? "p" : "");
}

static void dry_log(const char *prog, const char *op, const char *dev) {
  fprintf(stderr, "+ %s %s -b %s --no-user-interaction\n", prog, op, dev);
}

int rufux_mount(const char *dev, int dry_run, char *mnt_out, unsigned long cap,
                char *err, unsigned long errcap) {
  if (!rufux_have("udisksctl")) {
    snprintf(err, errcap, "udisksctl not found (install udisks2)");
    return -1;
  }
  if (dry_run) {
    dry_log("udisksctl", "mount", dev);
    snprintf(mnt_out, cap, "(dry-run)");
    return 0;
  }
  const char *av[] = {"udisksctl", "mount", "-b", dev, "--no-user-interaction", NULL};
  char out[1024] = {0};
  if (rufux_capture(av, out, sizeof out) != 0) {
    snprintf(err, errcap, "udisksctl mount failed: %.400s", out);
    return -1;
  }
  // "Mounted /dev/sda1 at /run/media/user/RUFUX."
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

int rufux_unmount(const char *dev, int dry_run, char *err, unsigned long errcap) {
  if (!rufux_have("udisksctl")) {
    snprintf(err, errcap, "udisksctl not found (install udisks2)");
    return -1;
  }
  if (dry_run) { dry_log("udisksctl", "unmount", dev); return 0; }
  const char *av[] = {"udisksctl", "unmount", "-b", dev, "--no-user-interaction", NULL};
  char out[1024] = {0};
  if (rufux_capture(av, out, sizeof out) != 0) {
    snprintf(err, errcap, "udisksctl unmount failed: %.400s", out);
    return -1;
  }
  return 0;
}

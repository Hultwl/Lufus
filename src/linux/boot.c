#include "boot.h"
#include "device.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static const char *mbr_path(const char *kind) {
  // Arch nests BIOS modules under bios/, Debian/Ubuntu flatten them.
  static const char *gpt_c[] = {
    "/usr/lib/syslinux/bios/gptmbr.bin",
    "/usr/lib/syslinux/gptmbr.bin",
    "/usr/share/syslinux/gptmbr.bin", NULL};
  static const char *bios_c[] = {
    "/usr/lib/syslinux/bios/mbr.bin",
    "/usr/lib/syslinux/mbr.bin",
    "/usr/share/syslinux/bios/mbr.bin",
    "/usr/share/syslinux/mbr.bin", NULL};
  const char **c = (kind && !strcmp(kind, "gpt")) ? gpt_c : bios_c;
  for (int i = 0; c[i]; i++)
    if (!access(c[i], R_OK)) return c[i];
  return NULL;
}

int rufux_install_mbr(const char *dst, const RufuxBootOpts *o,
                      char *err, unsigned long cap) {
  if (!o->dry_run && !o->yes) { snprintf(err, cap, "refusing real MBR write without --yes"); return -1; }
  if (rufux_check_target(dst, o->allow_fixed, o->allow_file, err, cap) != 0) return -1;
  const char *mbr = mbr_path(o->kind);
  if (!mbr) { snprintf(err, cap, "syslinux MBR binary not found"); return -1; }
  if (o->dry_run) {
    fprintf(stderr, "+ dd MBR %s -> %s (first 440 bytes, table preserved)\n", mbr, dst);
    return 0;
  }
  int fm = open(mbr, O_RDONLY | O_CLOEXEC);
  if (fm < 0) { snprintf(err, cap, "cannot open %s", mbr); return -1; }
  unsigned char code[440];
  ssize_t n = read(fm, code, sizeof code);
  close(fm);
  if (n != (ssize_t)sizeof code) { snprintf(err, cap, "bad MBR binary size"); return -1; }
  int fd = open(dst, O_WRONLY | O_CLOEXEC);
  if (fd < 0) { snprintf(err, cap, "cannot open '%s': %s", dst, strerror(errno)); return -1; }
  ssize_t w = write(fd, code, sizeof code);
  fsync(fd);
  close(fd);
  if (w != (ssize_t)sizeof code) { snprintf(err, cap, "MBR write failed"); return -1; }
  return 0;
}

int rufux_install_syslinux(const char *part_dev, int dry_run,
                           char *err, unsigned long cap) {
  if (!rufux_have("syslinux")) { snprintf(err, cap, "syslinux missing"); return -1; }
  const char *av[] = {"syslinux", "--install", part_dev, NULL};
  if (rufux_run(av, dry_run) != 0) {
    if (!dry_run) snprintf(err, cap, "syslinux --install failed on '%s'", part_dev);
    return dry_run ? 0 : -1;
  }
  return 0;
}

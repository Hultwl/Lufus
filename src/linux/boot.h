#ifndef LUFUS_BOOT_H
#define LUFUS_BOOT_H
// MBR install preserving partition table + syslinux/grub helpers.
typedef struct {
  const char *kind; // "bios" (mbr.bin) | "gpt" (gptmbr.bin)
  int dry_run;
  int allow_fixed;
  int allow_file;
  int yes;
} LufusBootOpts;
int lufus_install_mbr(const char *dst, const LufusBootOpts *o, char *err, unsigned long cap);
// Run syslinux --install on an already-mounted vfat partition dir marker:
// writes ldlinux.sys via syslinux binary onto block partition dev.
int lufus_install_syslinux(const char *part_dev, int dry_run, char *err, unsigned long cap);
#endif

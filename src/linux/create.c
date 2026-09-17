#define _GNU_SOURCE
#include "create.h"
#include "device.h"
#include "writer.h"
#include "partition.h"
#include "mkfs_task.h"
#include "extract.h"
#include "boot.h"
#include "persist.h"
#include "badblocks.h"
#include "mount.h"
#include "priv.h"
#include "secureboot.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

void rufux_create_defaults(RufuxCreateOpts *o) {
  memset(o, 0, sizeof *o);
  o->mode = "dd";
  o->scheme = "gpt";
  o->fs = "vfat";
  o->label = "RUFUX";
  o->quick_format = 1;
  o->extended_label = 1;
  o->dry_run = 1;
}

static int is_dir(const char *p) {
  struct stat st;
  return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_block(const char *p) {
  struct stat st;
  return stat(p, &st) == 0 && S_ISBLK(st.st_mode);
}

// Zero the first 16MB (Rufus "full format" equivalent for the boot area).
static int zero_head(const char *dst, RufuxCreateLog log, void *luser,
                     char *err, unsigned long cap) {
  if (log) log("Clearing first 16MB (full format)...", luser);
  int fd = open(dst, O_WRONLY | O_CLOEXEC);
  if (fd < 0) { snprintf(err, cap, "cannot open '%s': %s", dst, strerror(errno)); return -1; }
  static char z[1 << 20];
  memset(z, 0, sizeof z);
  for (int i = 0; i < 16; i++) {
    size_t off = 0;
    while (off < sizeof z) {
      ssize_t w = write(fd, z + off, sizeof z - off);
      if (w < 0) {
        if (errno == EINTR) continue;
        snprintf(err, cap, "zero failed: %s", strerror(errno));
        close(fd);
        return -1;
      }
      off += (size_t)w;
    }
  }
  if (fsync(fd) != 0) { snprintf(err, cap, "fsync failed"); close(fd); return -1; }
  close(fd);
  return 0;
}

static int run_badblocks(const char *dst, int passes, int allow_file,
                         RufuxCreateProgress prog, void *puser,
                         RufuxCreateLog log, void *luser,
                         char *err, unsigned long cap) {
  for (int i = 1; i <= passes; i++) {
    char m[128];
    snprintf(m, sizeof m, "Checking device for bad blocks: pass %d/%d...", i, passes);
    if (log) log(m, luser);
    unsigned long long bad = 0;
    if (rufux_badblocks(dst, allow_file, (RufuxScanProgress)prog, puser,
                        &bad, err, cap) != 0)
      return -1;
    snprintf(m, sizeof m, "Bad blocks pass %d/%d: %llu bad regions", i, passes, bad);
    if (log) log(m, luser);
    if (bad) { snprintf(err, cap, "%llu bad regions found", bad); return -1; }
  }
  return 0;
}

static int flow_dd(const char *src, const char *dst, const RufuxCreateOpts *o,
                   RufuxCreateProgress prog, void *puser,
                   char *err, unsigned long cap) {
  RufuxWriteOpts w = {.dry_run = o->dry_run, .verify = o->verify,
                      .allow_fixed = o->allow_fixed, .allow_file = o->allow_file,
                      .yes = o->yes};
  return rufux_write_image(src, dst, &w, (RufuxWriteProgress)prog, puser, err, cap);
}

static int flow_extract_dir(const char *src, const char *dir, const RufuxCreateOpts *o,
                            RufuxCreateLog log, void *luser,
                            char *err, unsigned long cap) {
  if (rufux_extract_iso(src, dir, o->dry_run, err, cap) != 0) return -1;
  if (o->extended_label) {
    if (log) log("Creating extended label and icon files (autorun.inf)...", luser);
    if (rufux_write_autorun(dir, o->label, o->dry_run, err, cap) != 0) return -1;
  }
  if (o->persist_mb && !o->dry_run) {
    if (rufux_create_persist(dir, "casper-rw", o->persist_mb, 0, err, cap) != 0) return -1;
  } else if (o->persist_mb) {
    char m[256];
    snprintf(m, sizeof m, "+ persist casper-rw %luMB in %s [dry-run]", o->persist_mb, dir);
    if (log) log(m, luser);
  }
  return 0;
}

// Whole-disk UEFI file flow with udisks2 auto-mount.
static int flow_extract_disk(const char *src, const char *dst, const RufuxCreateOpts *o,
                             RufuxCreateProgress prog, void *puser,
                             RufuxCreateLog log, void *luser,
                             char *err, unsigned long cap) {
  char p1[160], m[512];
  rufux_part1(dst, p1, sizeof p1);
  if (o->dry_run && log) {
    // keep the classic step listing (also asserted by tests)
    snprintf(m, sizeof m, "steps:\n  1. partition %s %s/esp+main (sfdisk)\n  2. format %s %s [%s]\n"
             "  3. mount %s (udisks2) + extract %s",
             dst, o->scheme, p1, o->fs, o->label ? o->label : "", p1, src);
    log(m, luser);
    if (o->persist_mb) {
      snprintf(m, sizeof m, "  4. persist casper-rw %luMB", o->persist_mb);
      log(m, luser);
    }
    snprintf(m, sizeof m, "  5. install-boot %s (syslinux MBR)", dst);
    log(m, luser);
    return 0;
  }

  RufuxPartOpts po = {.scheme = o->scheme, .layout = "esp+main", .dry_run = 0,
                      .allow_fixed = o->allow_fixed, .allow_file = 0, .yes = 1};
  if (rufux_partition(dst, &po, err, cap) != 0) return -1;
  const char *pp[] = {"/usr/bin/partprobe", dst, NULL};
  if (rufux_have("/usr/bin/partprobe")) rufux_run(pp, 0);
  const char *us[] = {"/usr/bin/udevadm", "settle", NULL};
  if (rufux_have("/usr/bin/udevadm")) rufux_run(us, 0);
  int waited = 0;
  while (access(p1, F_OK) != 0 && waited < 100) { usleep(100000); waited++; }
  if (access(p1, F_OK) != 0) { snprintf(err, cap, "partition '%s' did not appear", p1); return -1; }
  if (!o->quick_format) {
    if (zero_head(p1, log, luser, err, cap) != 0) return -1;
  }
  RufuxMkfsOpts mo = {.fs = o->fs, .label = o->label, .cluster_sectors = o->cluster_sectors,
                      .dry_run = 0, .allow_fixed = o->allow_fixed, .allow_file = 0, .yes = 1};
  snprintf(m, sizeof m, "Creating file system (%s)...", o->fs);
  if (log) log(m, luser);
  if (rufux_format(p1, &mo, err, cap) != 0) return -1;
  char mnt[512] = {0};
  if (rufux_mount(p1, 0, mnt, sizeof mnt, err, cap) != 0) return -1;
  int rc = 0;
  if (rufux_extract_iso(src, mnt, 0, err, cap) != 0) rc = -1;
  if (!rc && o->extended_label) {
    if (log) log("Creating extended label and icon files (autorun.inf)...", luser);
    if (rufux_write_autorun(mnt, o->label, 0, err, cap) != 0) rc = -1;
  }
  if (!rc && o->persist_mb) {
    if (rufux_create_persist(mnt, "casper-rw", o->persist_mb, 0, err, cap) != 0) rc = -1;
  }
  if (!rc && o->uefi_validate) {
    char efi[768];
    snprintf(efi, sizeof efi, "%s/EFI/BOOT/bootx64.efi", mnt);
    if (access(efi, R_OK) == 0) {
      unsigned sub = 0;
      char verr[256] = {0};
      if (rufux_validate_efi(efi, &sub, verr, sizeof verr) == 0) {
        if (log) log("UEFI media validation: bootloader OK (Secure Boot compatible signature check passed).", luser);
      } else {
        snprintf(m, sizeof m, "UEFI media validation warning: %s", verr);
        if (log) log(m, luser);
      }
    } else if (log) {
      log("UEFI media validation: no EFI/BOOT/bootx64.efi found, skipping.", luser);
    }
  }
  char uerr[512] = {0};
  if (rufux_unmount(p1, 0, uerr, sizeof uerr) != 0 && !rc) {
    snprintf(err, cap, "extracted but unmount failed: %s", uerr);
    rc = -1;
  }
  if (!rc) {
    RufuxBootOpts bo = {.kind = !strcmp(o->scheme, "gpt") ? "gpt" : "bios",
                        .dry_run = 0, .allow_fixed = o->allow_fixed, .yes = 1};
    if (rufux_install_mbr(dst, &bo, err, cap) != 0) rc = -1;
  }
  return rc;
}

// Non bootable: partition + format + MBR, no image.
static int flow_format(const char *dst, const RufuxCreateOpts *o,
                       RufuxCreateLog log, void *luser,
                       char *err, unsigned long cap) {
  char m[512];
  if (is_block(dst)) {
    snprintf(m, sizeof m, "Plan: partition %s %s/single, format %s, MBR (non bootable)",
             dst, o->scheme, o->fs);
    if (log) log(m, luser);
    if (o->dry_run) return 0;
    RufuxPartOpts po = {.scheme = o->scheme, .layout = "single", .dry_run = 0,
                        .allow_fixed = o->allow_fixed, .allow_file = 0, .yes = 1};
    if (rufux_partition(dst, &po, err, cap) != 0) return -1;
    const char *pp[] = {"/usr/bin/partprobe", dst, NULL};
    if (rufux_have("/usr/bin/partprobe")) rufux_run(pp, 0);
    const char *us[] = {"/usr/bin/udevadm", "settle", NULL};
    if (rufux_have("/usr/bin/udevadm")) rufux_run(us, 0);
    char p1[160];
    rufux_part1(dst, p1, sizeof p1);
    int waited = 0;
    while (access(p1, F_OK) != 0 && waited < 100) { usleep(100000); waited++; }
    if (access(p1, F_OK) != 0) { snprintf(err, cap, "partition '%s' did not appear", p1); return -1; }
    if (!o->quick_format) {
      if (zero_head(p1, log, luser, err, cap) != 0) return -1;
    }
    RufuxMkfsOpts mo = {.fs = o->fs, .label = o->label, .cluster_sectors = o->cluster_sectors,
                        .dry_run = 0, .allow_fixed = o->allow_fixed, .allow_file = 0, .yes = 1};
    if (rufux_format(p1, &mo, err, cap) != 0) return -1;
    RufuxBootOpts bo = {.kind = !strcmp(o->scheme, "gpt") ? "gpt" : "bios",
                        .dry_run = 0, .allow_fixed = o->allow_fixed, .yes = 1};
    return rufux_install_mbr(dst, &bo, err, cap);
  }
  // regular file: whole-file format (rootless)
  snprintf(m, sizeof m, "Plan: format file %s as %s (non bootable)", dst, o->fs);
  if (log) log(m, luser);
  if (o->dry_run) return 0;
  RufuxMkfsOpts mo = {.fs = o->fs, .label = o->label, .cluster_sectors = o->cluster_sectors,
                      .dry_run = 0, .allow_fixed = o->allow_fixed, .allow_file = 1, .yes = 1};
  return rufux_format(dst, &mo, err, cap);
}

int rufux_create(const char *src, const char *dst, const RufuxCreateOpts *o,
                 RufuxCreateProgress prog, void *puser,
                 RufuxCreateLog log, void *luser,
                 char *err, unsigned long errcap) {
  char m[1024];
  snprintf(m, sizeof m, "Rufux: %s -> %s [mode=%s scheme=%s fs=%s label=%s]%s",
           src ? src : "(none)", dst, o->mode, o->scheme, o->fs,
           o->label ? o->label : "", o->dry_run ? " [dry-run]" : "");
  if (log) log(m, luser);

  if (!o->dry_run && !o->yes) {
    snprintf(err, errcap, "refusing real run without --yes (use --dry-run to plan)");
    return -1;
  }
  if (!o->dry_run && is_block(dst)) {
    if (rufux_need_root_for_block(dst, err, errcap) != 0) return -1;
  }
  if (o->badblock_passes > 0 && !o->dry_run) {
    if (run_badblocks(dst, o->badblock_passes, o->allow_file, prog, puser,
                      log, luser, err, errcap) != 0)
      return -1;
  } else if (o->badblock_passes > 0 && log) {
    snprintf(m, sizeof m, "+ bad-blocks check %d pass(es) on %s [dry-run]", o->badblock_passes, dst);
    log(m, luser);
  }
  if (!o->dry_run && !o->quick_format && is_block(dst) && strcmp(o->mode, "format")) {
    // dd/extract on blocks: zero head of whole disk first (format flow zeroes its partition)
    if (!strcmp(o->mode, "dd")) {
      if (zero_head(dst, log, luser, err, errcap) != 0) return -1;
    }
  }

  int rc;
  if (!strcmp(o->mode, "dd")) {
    rc = flow_dd(src, dst, o, prog, puser, err, errcap);
  } else if (!strcmp(o->mode, "extract")) {
    if (is_dir(dst)) rc = flow_extract_dir(src, dst, o, log, luser, err, errcap);
    else rc = flow_extract_disk(src, dst, o, prog, puser, log, luser, err, errcap);
  } else if (!strcmp(o->mode, "format")) {
    rc = flow_format(dst, o, log, luser, err, errcap);
  } else {
    snprintf(err, errcap, "unknown mode '%s' (dd|extract|format)", o->mode);
    return -1;
  }
  if (rc == 0 && log) {
    snprintf(m, sizeof m, "%s", o->dry_run ? "Plan OK (dry-run, nothing written)." : "Done.");
    log(m, luser);
  }
  return rc;
}

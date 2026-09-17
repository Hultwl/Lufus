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

// ---- Overall progress: every flow reports percent as prog(pct, 100).
// Sub-operations reporting byte progress are mapped into their stage
// window via ProgMap, so the bar moves continuously end to end.
typedef struct {
  RufuxCreateProgress prog;
  void *user;
  unsigned base; // stage start, percent
  unsigned span; // stage width, percent
} ProgMap;

static void mapped(unsigned long long done, unsigned long long total, void *u) {
  ProgMap *m = (ProgMap *)u;
  if (!m->prog) return;
  unsigned v = m->base;
  if (total) {
    unsigned long long add = m->span * done / total;
    if (add > m->span) add = m->span;
    v += (unsigned)add;
  }
  if (v > 100) v = 100;
  m->prog(v, 100, m->user);
}

static void stage(RufuxCreateProgress prog, void *user, unsigned pct) {
  if (prog && pct <= 100) prog(pct, 100, user);
}

static int is_dir(const char *p) {
  struct stat st;
  return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_block(const char *p) {
  struct stat st;
  return stat(p, &st) == 0 && S_ISBLK(st.st_mode);
}

// Rufus dismounts the target's volumes before touching them instead of
// refusing: unmount every mounted partition belonging to this disk.
// Returns -1 if anything is still mounted afterwards.
static int unmount_disk(const char *dst, int dry_run,
                        RufuxCreateLog log, void *luser,
                        char *err, unsigned long cap) {
  const char *base = strrchr(dst, '/');
  base = base ? base + 1 : dst;
  FILE *f = fopen("/proc/mounts", "r");
  if (!f) return 0;
  char line[1024];
  char parts[32][128];
  int nparts = 0;
  size_t blen = strlen(base);
  while (fgets(line, sizeof line, f) && nparts < 32) {
    char dev[512] = {0};
    if (sscanf(line, "%511s", dev) != 1) continue;
    if (strncmp(dev, "/dev/", 5)) continue;
    const char *d = dev + 5;
    if (strncmp(d, base, blen)) continue;
    // dst itself, or dst + partition suffix (digits / p+digits)
    const char *rest = d + blen;
    if (rest[0] && rest[0] != 'p' && (rest[0] < '0' || rest[0] > '9')) continue;
    int dup = 0;
    for (int i = 0; i < nparts; i++)
      if (!strcmp(parts[i], dev)) { dup = 1; break; }
    if (!dup) snprintf(parts[nparts++], sizeof parts[0], "%s", dev);
  }
  fclose(f);
  for (int i = 0; i < nparts; i++) {
    char m[256];
    snprintf(m, sizeof m, "Unmounting %s...", parts[i]);
    if (log) log(m, luser);
    if (rufux_unmount(parts[i], dry_run, err, cap) != 0) {
      if (!dry_run) return -1;
    }
  }
  return 0;
}

static unsigned long long file_size(const char *p) {
  struct stat st;
  if (stat(p, &st) != 0 || !S_ISREG(st.st_mode)) return 0;
  return (unsigned long long)st.st_size;
}

// FAT32 cannot hold files >= 4GiB: refuse early with a useful message
// (Rufus solves this with UEFI:NTFS; we point at NTFS/exFAT instead).
static int vfat_size_guard(const char *src, const char *fs, char *err, unsigned long cap) {
  if (strcmp(fs, "vfat") && strcmp(fs, "fat32")) return 0;
  unsigned long long sz = file_size(src);
  if (sz > 0xFFFFFFFFULL) {
    snprintf(err, cap, "image is %.1f GiB: FAT32 cannot hold files >= 4 GiB; "
                       "use --fs ntfs or --fs exfat",
             sz / 1073741824.0);
    return -1;
  }
  return 0;
}

// Zero the first 16MB (Rufus "full format" equivalent for the boot area).
static int zero_head(const char *dst, RufuxCreateLog log, void *luser,
                     RufuxCreateProgress prog, void *puser,
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
    if (prog) prog((unsigned long long)(i + 1), 16, puser);
  }
  if (fsync(fd) != 0) { snprintf(err, cap, "fsync failed"); close(fd); return -1; }
  close(fd);
  return 0;
}

// dd layout: badblocks 0-10, zero 10-15, write 15-(verify?85:100), verify 85-100.
static int run_badblocks(const char *dst, int passes, int allow_file,
                         unsigned base, unsigned span,
                         RufuxCreateProgress prog, void *puser,
                         RufuxCreateLog log, void *luser,
                         char *err, unsigned long cap) {
  for (int i = 1; i <= passes; i++) {
    char m[128];
    snprintf(m, sizeof m, "Checking device for bad blocks: pass %d/%d...", i, passes);
    if (log) log(m, luser);
    ProgMap pm = {prog, puser, base + span * (unsigned)(i - 1) / (unsigned)passes,
                  span / (unsigned)passes};
    unsigned long long bad = 0;
    if (rufux_badblocks(dst, allow_file, prog ? (RufuxScanProgress)mapped : NULL, &pm,
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
  ProgMap wm = {prog, puser, 15, o->verify ? 70 : 85};
  ProgMap vm = {prog, puser, 85, 15};
  RufuxWriteOpts w = {.dry_run = o->dry_run, .verify = o->verify,
                      .allow_fixed = o->allow_fixed, .allow_file = o->allow_file,
                      .yes = o->yes,
                      .vprog = prog ? (RufuxWriteProgress)mapped : NULL, .vuser = &vm};
  int rc = rufux_write_image(src, dst, &w, prog ? (RufuxWriteProgress)mapped : NULL, &wm,
                             err, cap);
  if (rc == 0) stage(prog, puser, 100);
  return rc;
}

// dir layout: extract 0-90, autorun/persist 90-100.
static int flow_extract_dir(const char *src, const char *dir, const RufuxCreateOpts *o,
                            RufuxCreateProgress prog, void *puser,
                            RufuxCreateLog log, void *luser,
                            char *err, unsigned long cap) {
  if (vfat_size_guard(src, o->fs, err, cap) != 0) return -1;
  ProgMap em = {prog, puser, 0, 90};
  if (rufux_extract_iso_progress(src, dir, o->dry_run,
                                 prog ? (RufuxExtractProgress)mapped : NULL, &em,
                                 err, cap) != 0)
    return -1;
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
  stage(prog, puser, 100);
  return 0;
}

// Whole-disk UEFI file flow with udisks2 auto-mount.
// Layout: partition 0-4, format 4-8, extract 8-82, persist 82-88,
// validate 88-90, unmount+MBR 90-100.
static int flow_extract_disk(const char *src, const char *dst, const RufuxCreateOpts *o,
                             RufuxCreateProgress prog, void *puser,
                             RufuxCreateLog log, void *luser,
                             char *err, unsigned long cap) {
  char p1[160], m[512];
  rufux_part1(dst, p1, sizeof p1);
  if (vfat_size_guard(src, o->fs, err, cap) != 0) return -1;
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
  stage(prog, puser, 4);
  const char *pp[] = {"partprobe", dst, NULL};
  if (rufux_have("partprobe")) rufux_run(pp, 0);
  const char *us[] = {"udevadm", "settle", NULL};
  if (rufux_have("udevadm")) rufux_run(us, 0);
  int waited = 0;
  while (access(p1, F_OK) != 0 && waited < 100) { usleep(100000); waited++; }
  if (access(p1, F_OK) != 0) { snprintf(err, cap, "partition '%s' did not appear", p1); return -1; }
  if (!o->quick_format) {
    ProgMap zm = {prog, puser, 4, 2};
    if (zero_head(p1, log, luser, prog ? (RufuxCreateProgress)mapped : NULL, &zm,
                  err, cap) != 0)
      return -1;
  }
  RufuxMkfsOpts mo = {.fs = o->fs, .label = o->label, .cluster_sectors = o->cluster_sectors,
                      .dry_run = 0, .allow_fixed = o->allow_fixed, .allow_file = 0, .yes = 1};
  snprintf(m, sizeof m, "Creating file system (%s)...", o->fs);
  if (log) log(m, luser);
  if (rufux_format(p1, &mo, err, cap) != 0) return -1;
  stage(prog, puser, 8);
  char mnt[512] = {0};
  if (rufux_mount(p1, 0, mnt, sizeof mnt, err, cap) != 0) return -1;
  int rc = 0;
  ProgMap em = {prog, puser, 8, 74};
  if (rufux_extract_iso_progress(src, mnt, 0,
                                 prog ? (RufuxExtractProgress)mapped : NULL, &em,
                                 err, cap) != 0)
    rc = -1;
  if (!rc && o->extended_label) {
    if (log) log("Creating extended label and icon files (autorun.inf)...", luser);
    if (rufux_write_autorun(mnt, o->label, 0, err, cap) != 0) rc = -1;
  }
  if (!rc && o->persist_mb) {
    if (rufux_create_persist(mnt, "casper-rw", o->persist_mb, 0, err, cap) != 0) rc = -1;
  }
  stage(prog, puser, 88);
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
  stage(prog, puser, 90);
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
  if (!rc) stage(prog, puser, 100);
  return rc;
}

// Non bootable: partition + format + MBR, no image.
// Layout: partition 0-30, format 30-85, MBR 85-100.
static int flow_format(const char *dst, const RufuxCreateOpts *o,
                       RufuxCreateProgress prog, void *puser,
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
    stage(prog, puser, 30);
    const char *pp[] = {"partprobe", dst, NULL};
    if (rufux_have("partprobe")) rufux_run(pp, 0);
    const char *us[] = {"udevadm", "settle", NULL};
    if (rufux_have("udevadm")) rufux_run(us, 0);
    char p1[160];
    rufux_part1(dst, p1, sizeof p1);
    int waited = 0;
    while (access(p1, F_OK) != 0 && waited < 100) { usleep(100000); waited++; }
    if (access(p1, F_OK) != 0) { snprintf(err, cap, "partition '%s' did not appear", p1); return -1; }
    if (!o->quick_format) {
      ProgMap zm = {prog, puser, 30, 10};
      if (zero_head(p1, log, luser, prog ? (RufuxCreateProgress)mapped : NULL, &zm,
                    err, cap) != 0)
        return -1;
    }
    RufuxMkfsOpts mo = {.fs = o->fs, .label = o->label, .cluster_sectors = o->cluster_sectors,
                        .dry_run = 0, .allow_fixed = o->allow_fixed, .allow_file = 0, .yes = 1};
    snprintf(m, sizeof m, "Creating file system (%s)...", o->fs);
    if (log) log(m, luser);
    if (rufux_format(p1, &mo, err, cap) != 0) return -1;
    stage(prog, puser, 85);
    RufuxBootOpts bo = {.kind = !strcmp(o->scheme, "gpt") ? "gpt" : "bios",
                        .dry_run = 0, .allow_fixed = o->allow_fixed, .yes = 1};
    if (rufux_install_mbr(dst, &bo, err, cap) != 0) return -1;
    stage(prog, puser, 100);
    return 0;
  }
  // regular file: whole-file format (rootless)
  snprintf(m, sizeof m, "Plan: format file %s as %s (non bootable)", dst, o->fs);
  if (log) log(m, luser);
  if (o->dry_run) return 0;
  RufuxMkfsOpts mo = {.fs = o->fs, .label = o->label, .cluster_sectors = o->cluster_sectors,
                      .dry_run = 0, .allow_fixed = o->allow_fixed, .allow_file = 1, .yes = 1};
  if (rufux_format(dst, &mo, err, cap) != 0) return -1;
  stage(prog, puser, 100);
  return 0;
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
    // Dismount our own target first (consent was the START warning);
    // the per-operation checks below re-verify it afterwards.
    if (unmount_disk(dst, 0, log, luser, err, errcap) != 0) {
      if (!err[0]) snprintf(err, errcap, "cannot unmount %s (close files and retry)", dst);
      return -1;
    }
  }
  // dd layout reserves 0-15 for checks; extract/format start at 0.
  if (o->badblock_passes > 0 && !o->dry_run) {
    unsigned span = !strcmp(o->mode, "dd") ? 10 : 8;
    if (run_badblocks(dst, o->badblock_passes, o->allow_file, 0, span,
                      prog, puser, log, luser, err, errcap) != 0)
      return -1;
  } else if (o->badblock_passes > 0 && log) {
    snprintf(m, sizeof m, "+ bad-blocks check %d pass(es) on %s [dry-run]", o->badblock_passes, dst);
    log(m, luser);
  }
  if (!o->dry_run && !o->quick_format && is_block(dst) && !strcmp(o->mode, "dd")) {
    ProgMap zm = {prog, puser, 10, 5};
    if (zero_head(dst, log, luser, prog ? (RufuxCreateProgress)mapped : NULL, &zm,
                  err, errcap) != 0)
      return -1;
  }

  int rc;
  if (!strcmp(o->mode, "dd")) {
    rc = flow_dd(src, dst, o, prog, puser, err, errcap);
  } else if (!strcmp(o->mode, "extract")) {
    if (is_dir(dst)) rc = flow_extract_dir(src, dst, o, prog, puser, log, luser, err, errcap);
    else rc = flow_extract_disk(src, dst, o, prog, puser, log, luser, err, errcap);
  } else if (!strcmp(o->mode, "format")) {
    rc = flow_format(dst, o, prog, puser, log, luser, err, errcap);
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

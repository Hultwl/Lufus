// Rufux entry — Phase 3 stable CLI. No windows.h by design.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "linux/device.h"
#include "linux/iso_probe.h"
#include "linux/checksum.h"
#include "linux/writer.h"
#include "linux/partition.h"
#include "linux/mkfs_task.h"
#include "linux/extract.h"
#include "linux/boot.h"
#include "linux/persist.h"
#include "linux/badblocks.h"
#include "linux/mount.h"
#include "linux/priv.h"
#include "linux/secureboot.h"
#include "linux/update.h"
#include "linux/exec.h"
#include "linux/i18n.h"
#include "gui/gui_gtk.h"

#ifndef RUFUX_VERSION
#define RUFUX_VERSION "1.0.0"
#endif

static void usage(const char *p) {
  printf("Rufux %s (Stable)\n"
         "Usage:\n"
         "  %s list [--json] [--allow-fixed]\n"
         "  %s probe <file.iso> [--detail]\n"
         "  %s checksum <file>\n"
         "  %s write SRC DST [--dry-run|--real] [--verify] [--allow-fixed] [--allow-file] [--yes]\n"
         "  %s partition DST --scheme gpt|dos --layout single|esp+main [--dry-run|--real] [--allow-file] [--allow-fixed] [--yes]\n"
         "  %s format DST --fs vfat|ntfs|exfat|ext4|udf [--label L] [--dry-run|--real] [--allow-file] [--allow-fixed] [--yes]\n"
         "  %s extract SRC.iso DEST_DIR [--dry-run]\n"
         "  %s install-boot DST --mbr bios|gpt [--dry-run|--real] [--allow-file] [--allow-fixed] [--yes]\n"
         "  %s persist DIR --size MB [--label casper-rw] [--dry-run]\n"
         "  %s badblocks DST [--allow-file]\n"
         "  %s mount|umount DEV [--dry-run]\n"
         "  %s secureboot-status\n"
         "  %s validate-efi FILE\n"
         "  %s update-check\n"
         "  %s create SRC DST --mode dd|extract [--scheme gpt|dos] [--fs vfat|ntfs|exfat|ext4] [--label L] [--persist-mb N] [--dry-run|--real] [--allow-file] [--allow-fixed] [--yes] [--verify]\n"
         "  %s --gui [--theme system|dark|light]\n",
         RUFUX_VERSION, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p, p);
}

static void cli_progress(unsigned long long done, unsigned long long total, void *u) {
  (void)u;
  static int last = -1;
  int pct = total ? (int)(done * 100 / total) : 100;
  if (pct != last && (pct % 5 == 0 || done == total)) {
    fprintf(stderr, "\r%3d%%  %llu/%llu MB", pct, done >> 20, total >> 20);
    if (done == total) fprintf(stderr, "\n");
    last = pct;
  }
}

static int is_dir(const char *p) {
  struct stat st;
  return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

// End-to-end UEFI file flow on a whole disk (Phase 3):
// partition -> mkfs part1 -> udisks2 mount -> extract -> persist -> unmount -> MBR.
static int create_disk_extract(const char *src, const char *dst,
                               const char *scheme, const char *fs, const char *label,
                               unsigned long persist_mb, int dry,
                               int allow_fixed, char *err, unsigned long errcap) {
  char p1[160];
  rufux_part1(dst, p1, sizeof p1);
  printf("steps:\n  1. partition %s %s/esp+main (sfdisk)\n  2. format %s %s [%s]\n"
         "  3. mount %s (udisks2) + extract %s\n",
         dst, scheme, p1, fs, label, p1, src);
  if (persist_mb) printf("  4. persist casper-rw %luMB\n", persist_mb);
  printf("  5. install-boot %s (syslinux MBR)\n", dst);
  if (dry) { printf("create plan OK (dry-run)\n"); return 0; }

  if (rufux_need_root_for_block(dst, err, errcap) != 0) return -1;
  RufuxPartOpts po = {.scheme = scheme, .layout = "esp+main", .dry_run = 0,
                      .allow_fixed = allow_fixed, .allow_file = 0, .yes = 1};
  if (rufux_partition(dst, &po, err, errcap) != 0) return -1;
  // rescan + settle so the kernel exposes the new partition node
  const char *pp[] = {"/usr/bin/partprobe", dst, NULL};
  if (rufux_have("/usr/bin/partprobe")) rufux_run(pp, 0);
  const char *us[] = {"/usr/bin/udevadm", "settle", NULL};
  if (rufux_have("/usr/bin/udevadm")) rufux_run(us, 0);
  int waited = 0;
  while (access(p1, F_OK) != 0 && waited < 100) { usleep(100000); waited++; }
  if (access(p1, F_OK) != 0) {
    snprintf(err, errcap, "partition node '%s' did not appear", p1);
    return -1;
  }
  RufuxMkfsOpts mo = {.fs = fs, .label = label, .dry_run = 0,
                      .allow_fixed = allow_fixed, .allow_file = 0, .yes = 1};
  if (rufux_format(p1, &mo, err, errcap) != 0) return -1;
  char mnt[512] = {0};
  if (rufux_mount(p1, 0, mnt, sizeof mnt, err, errcap) != 0) return -1;
  int rc = 0;
  if (rufux_extract_iso(src, mnt, 0, err, errcap) != 0) rc = -1;
  if (!rc && persist_mb) {
    if (rufux_create_persist(mnt, "casper-rw", persist_mb, 0, err, errcap) != 0) rc = -1;
  }
  char uerr[512] = {0};
  if (rufux_unmount(p1, 0, uerr, sizeof uerr) != 0 && !rc) {
    snprintf(err, errcap, "extracted but unmount failed: %s", uerr);
    rc = -1;
  }
  if (!rc) {
    RufuxBootOpts bo = {.kind = !strcmp(scheme, "gpt") ? "gpt" : "bios",
                        .dry_run = 0, .allow_fixed = allow_fixed, .yes = 1};
    if (rufux_install_mbr(dst, &bo, err, errcap) != 0) rc = -1;
  }
  if (!rc) printf("create/extract OK: %s -> %s\n", src, dst);
  return rc;
}

int main(int argc, char **argv) {
  rufux_i18n_init();
  if (argc >= 2 && (!strcmp(argv[1], "--gui") || !strcmp(argv[1], "gui")))
    return rufux_gui_run(argc, argv);

  if (argc >= 2 && !strcmp(argv[1], "list")) {
    int json = 0, allow = 0;
    for (int i = 2; i < argc; i++) {
      if (!strcmp(argv[i], "--json")) json = 1;
      else if (!strcmp(argv[i], "--allow-fixed")) allow = 1;
    }
    RufuxDevice devs[128];
    int n = rufux_list_devices(devs, 128, allow);
    if (n < 0) { fprintf(stderr, "cannot scan /sys/block\n"); return 1; }
    if (json) rufux_print_devices_json(devs, n);
    else rufux_print_devices(devs, n);
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "probe")) {
    int detail = 0;
    for (int i = 3; i < argc; i++)
      if (!strcmp(argv[i], "--detail")) detail = 1;
    if (!detail) {
      char label[64] = {0};
      int rc = rufux_probe_iso(argv[2], label, sizeof label);
      printf("rc=%d label='%s'\n", rc, rc == 0 ? label : "?");
      return rc == 0 ? 0 : 2;
    }
    RufuxIsoInfo info;
    if (rufux_probe_iso_detail(argv[2], &info) != 0) {
      fprintf(stderr, "cannot probe '%s'\n", argv[2]);
      return 2;
    }
    rufux_print_iso_detail(argv[2], &info);
    return info.valid_iso ? 0 : 2;
  }
  if (argc >= 3 && !strcmp(argv[1], "checksum")) {
    unsigned char sum[32];
    char err[256] = {0};
    if (rufux_sha256_file(argv[2], sum, cli_progress, NULL, err, sizeof err) != 0) {
      fprintf(stderr, "checksum failed: %s\n", err);
      return 2;
    }
    char hex[65];
    rufux_hex32(sum, hex);
    printf("%s  %s\n", hex, argv[2]);
    return 0;
  }
  if (argc >= 4 && !strcmp(argv[1], "write")) {
    RufuxWriteOpts o = {0};
    o.dry_run = 1;
    for (int i = 4; i < argc; i++) {
      if (!strcmp(argv[i], "--dry-run")) o.dry_run = 1;
      else if (!strcmp(argv[i], "--real")) o.dry_run = 0;
      else if (!strcmp(argv[i], "--verify")) o.verify = 1;
      else if (!strcmp(argv[i], "--allow-fixed")) o.allow_fixed = 1;
      else if (!strcmp(argv[i], "--allow-file")) o.allow_file = 1;
      else if (!strcmp(argv[i], "--yes")) o.yes = 1;
    }
    char err[512] = {0};
    if (!o.dry_run && rufux_need_root_for_block(argv[3], err, sizeof err) != 0) {
      fprintf(stderr, "write failed: %s\n", err);
      return 3;
    }
    int rc = rufux_write_image(argv[2], argv[3], &o, cli_progress, NULL, err, sizeof err);
    if (rc != 0) { fprintf(stderr, "write failed: %s\n", err[0] ? err : "unknown"); return 3; }
    printf("%s OK: %s -> %s%s\n", o.dry_run ? "dry-run" : "write",
           argv[2], argv[3], o.verify ? " (verified)" : "");
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "partition")) {
    RufuxPartOpts o = {.scheme = "gpt", .layout = "single", .dry_run = 1};
    for (int i = 3; i < argc; i++) {
      if (!strcmp(argv[i], "--scheme") && i + 1 < argc) o.scheme = argv[++i];
      else if (!strcmp(argv[i], "--layout") && i + 1 < argc) o.layout = argv[++i];
      else if (!strcmp(argv[i], "--dry-run")) o.dry_run = 1;
      else if (!strcmp(argv[i], "--real")) o.dry_run = 0;
      else if (!strcmp(argv[i], "--allow-file")) o.allow_file = 1;
      else if (!strcmp(argv[i], "--allow-fixed")) o.allow_fixed = 1;
      else if (!strcmp(argv[i], "--yes")) o.yes = 1;
    }
    char plan[512], err[512] = {0};
    rufux_partition_plan(argv[2], &o, plan, sizeof plan);
    printf("%s\n", plan);
    if (!o.dry_run && rufux_need_root_for_block(argv[2], err, sizeof err) != 0) {
      fprintf(stderr, "partition failed: %s\n", err);
      return 3;
    }
    if (rufux_partition(argv[2], &o, err, sizeof err) != 0) {
      fprintf(stderr, "partition failed: %s\n", err);
      return 3;
    }
    printf("partition %s\n", o.dry_run ? "planned (dry-run)" : "OK");
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "format")) {
    RufuxMkfsOpts o = {.fs = "vfat", .dry_run = 1};
    for (int i = 3; i < argc; i++) {
      if (!strcmp(argv[i], "--fs") && i + 1 < argc) o.fs = argv[++i];
      else if (!strcmp(argv[i], "--label") && i + 1 < argc) o.label = argv[++i];
      else if (!strcmp(argv[i], "--dry-run")) o.dry_run = 1;
      else if (!strcmp(argv[i], "--real")) o.dry_run = 0;
      else if (!strcmp(argv[i], "--allow-file")) o.allow_file = 1;
      else if (!strcmp(argv[i], "--allow-fixed")) o.allow_fixed = 1;
      else if (!strcmp(argv[i], "--yes")) o.yes = 1;
    }
    char err[512] = {0};
    printf("format %s as %s%s\n", argv[2], o.fs, o.dry_run ? " [dry-run]" : "");
    if (!o.dry_run && rufux_need_root_for_block(argv[2], err, sizeof err) != 0) {
      fprintf(stderr, "format failed: %s\n", err);
      return 3;
    }
    if (rufux_format(argv[2], &o, err, sizeof err) != 0) {
      fprintf(stderr, "format failed: %s\n", err);
      return 3;
    }
    printf("format %s\n", o.dry_run ? "planned (dry-run)" : "OK");
    return 0;
  }
  if (argc >= 4 && !strcmp(argv[1], "extract")) {
    int dry = 0;
    for (int i = 4; i < argc; i++)
      if (!strcmp(argv[i], "--dry-run")) dry = 1;
    char err[512] = {0};
    printf("extract %s -> %s%s\n", argv[2], argv[3], dry ? " [dry-run]" : "");
    if (rufux_extract_iso(argv[2], argv[3], dry, err, sizeof err) != 0) {
      fprintf(stderr, "extract failed: %s\n", err);
      return 3;
    }
    printf("extract %s\n", dry ? "planned (dry-run)" : "OK");
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "install-boot")) {
    RufuxBootOpts o = {.kind = "bios", .dry_run = 1};
    for (int i = 3; i < argc; i++) {
      if (!strcmp(argv[i], "--mbr") && i + 1 < argc) o.kind = argv[++i];
      else if (!strcmp(argv[i], "--dry-run")) o.dry_run = 1;
      else if (!strcmp(argv[i], "--real")) o.dry_run = 0;
      else if (!strcmp(argv[i], "--allow-file")) o.allow_file = 1;
      else if (!strcmp(argv[i], "--allow-fixed")) o.allow_fixed = 1;
      else if (!strcmp(argv[i], "--yes")) o.yes = 1;
    }
    char err[512] = {0};
    printf("install-boot %s (%s)%s\n", argv[2], o.kind, o.dry_run ? " [dry-run]" : "");
    if (!o.dry_run && rufux_need_root_for_block(argv[2], err, sizeof err) != 0) {
      fprintf(stderr, "install-boot failed: %s\n", err);
      return 3;
    }
    if (rufux_install_mbr(argv[2], &o, err, sizeof err) != 0) {
      fprintf(stderr, "install-boot failed: %s\n", err);
      return 3;
    }
    printf("install-boot %s\n", o.dry_run ? "planned (dry-run)" : "OK");
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "persist")) {
    unsigned long mb = 0;
    const char *label = "casper-rw";
    int dry = 0;
    for (int i = 3; i < argc; i++) {
      if (!strcmp(argv[i], "--size") && i + 1 < argc) mb = strtoul(argv[++i], NULL, 10);
      else if (!strcmp(argv[i], "--label") && i + 1 < argc) label = argv[++i];
      else if (!strcmp(argv[i], "--dry-run")) dry = 1;
    }
    char err[512] = {0};
    if (rufux_create_persist(argv[2], label, mb, dry, err, sizeof err) != 0) {
      fprintf(stderr, "persist failed: %s\n", err);
      return 3;
    }
    printf("persist %s\n", dry ? "planned (dry-run)" : "OK");
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "badblocks")) {
    int allow_file = 0;
    for (int i = 3; i < argc; i++)
      if (!strcmp(argv[i], "--allow-file")) allow_file = 1;
    char err[512] = {0};
    unsigned long long bad = 0;
    if (rufux_badblocks(argv[2], allow_file, cli_progress, NULL, &bad, err, sizeof err) != 0) {
      fprintf(stderr, "badblocks failed: %s\n", err);
      return 3;
    }
    printf("badblocks: %llu bad regions (0 = clean)\n", bad);
    return bad == 0 ? 0 : 4;
  }
  if ((argc >= 3 && !strcmp(argv[1], "mount")) ||
      (argc >= 3 && !strcmp(argv[1], "umount"))) {
    int is_mount = !strcmp(argv[1], "mount");
    int dry = 0;
    for (int i = 3; i < argc; i++)
      if (!strcmp(argv[i], "--dry-run")) dry = 1;
    char err[512] = {0};
    if (is_mount) {
      char mnt[512] = {0};
      if (rufux_mount(argv[2], dry, mnt, sizeof mnt, err, sizeof err) != 0) {
        fprintf(stderr, "mount failed: %s\n", err);
        return 3;
      }
      printf("mounted %s at %s\n", argv[2], mnt);
    } else {
      if (rufux_unmount(argv[2], dry, err, sizeof err) != 0) {
        fprintf(stderr, "umount failed: %s\n", err);
        return 3;
      }
      printf("unmounted %s\n", argv[2]);
    }
    return 0;
  }
  if (argc >= 2 && !strcmp(argv[1], "secureboot-status")) {
    RufuxSbState s = rufux_sb_state();
    printf("secure-boot: %s\n", rufux_sb_string(s));
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "validate-efi")) {
    unsigned sub = 0;
    char err[512] = {0};
    if (rufux_validate_efi(argv[2], &sub, err, sizeof err) != 0) {
      fprintf(stderr, "validate-efi: %s\n", err);
      return 2;
    }
    printf("validate-efi: OK '%s' (PE subsystem %u = EFI)\n", argv[2], sub);
    return 0;
  }
  if (argc >= 2 && !strcmp(argv[1], "update-check")) {
    char latest[64] = {0}, err[256] = {0};
    if (rufux_update_check(RUFUX_VERSION, latest, sizeof latest, err, sizeof err) != 0) {
      fprintf(stderr, "update-check: %s\n", err);
      return 5; // distinct code: skipped/offline
    }
    if (!strcmp(latest, RUFUX_VERSION))
      printf("rufux %s is up to date\n", RUFUX_VERSION);
    else
      printf("rufux %s installed, latest is %s — see https://github.com/Hultwl/Rufux/releases\n",
             RUFUX_VERSION, latest);
    return 0;
  }
  if (argc >= 4 && !strcmp(argv[1], "create")) {
    const char *src = argv[2], *dst = argv[3];
    const char *mode = "dd", *scheme = "gpt", *fs = "vfat", *label = "RUFUX";
    unsigned long persist_mb = 0;
    int dry = 1, allow_file = 0, allow_fixed = 0, yes = 0, verify = 0;
    for (int i = 4; i < argc; i++) {
      if (!strcmp(argv[i], "--mode") && i + 1 < argc) mode = argv[++i];
      else if (!strcmp(argv[i], "--scheme") && i + 1 < argc) scheme = argv[++i];
      else if (!strcmp(argv[i], "--fs") && i + 1 < argc) fs = argv[++i];
      else if (!strcmp(argv[i], "--label") && i + 1 < argc) label = argv[++i];
      else if (!strcmp(argv[i], "--persist-mb") && i + 1 < argc) persist_mb = strtoul(argv[++i], NULL, 10);
      else if (!strcmp(argv[i], "--dry-run")) dry = 1;
      else if (!strcmp(argv[i], "--real")) dry = 0;
      else if (!strcmp(argv[i], "--allow-file")) allow_file = 1;
      else if (!strcmp(argv[i], "--allow-fixed")) allow_fixed = 1;
      else if (!strcmp(argv[i], "--yes")) yes = 1;
      else if (!strcmp(argv[i], "--verify")) verify = 1;
    }
    char err[1024] = {0};
    printf("create plan: %s -> %s [mode=%s scheme=%s fs=%s label=%s persist=%luMB]%s\n",
           src, dst, mode, scheme, fs, label, persist_mb, dry ? " [dry-run]" : "");
    if (!strcmp(mode, "dd")) {
      RufuxWriteOpts o = {.dry_run = dry, .verify = verify, .allow_fixed = allow_fixed,
                          .allow_file = allow_file, .yes = yes};
      if (!dry && rufux_need_root_for_block(dst, err, sizeof err) != 0) {
        fprintf(stderr, "create/dd failed: %s\n", err);
        return 3;
      }
      if (rufux_write_image(src, dst, &o, cli_progress, NULL, err, sizeof err) != 0) {
        fprintf(stderr, "create/dd failed: %s\n", err);
        return 3;
      }
      printf("create/dd %s\n", dry ? "planned" : "OK");
      return 0;
    }
    if (is_dir(dst)) {
      if (rufux_extract_iso(src, dst, dry, err, sizeof err) != 0) {
        fprintf(stderr, "create/extract failed: %s\n", err);
        return 3;
      }
      if (persist_mb && !dry) {
        if (rufux_create_persist(dst, "casper-rw", persist_mb, 0, err, sizeof err) != 0) {
          fprintf(stderr, "create/persist failed: %s\n", err);
          return 3;
        }
      } else if (persist_mb) {
        printf("+ persist casper-rw %luMB in %s [dry-run]\n", persist_mb, dst);
      }
      printf("create/extract %s\n", dry ? "planned" : "OK");
      return 0;
    }
    // whole-disk target: full auto flow (udisks2 mount)
    if (create_disk_extract(src, dst, scheme, fs, label, persist_mb, dry,
                            allow_fixed, err, sizeof err) != 0) {
      fprintf(stderr, "create/extract failed: %s\n", err[0] ? err : "unknown");
      return 3;
    }
    return 0;
  }
  usage(argv[0]);
  return 0;
}

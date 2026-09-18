#define _GNU_SOURCE
#include "partition.h"
#include "device.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

void rufux_partition_plan(const char *dst, const RufuxPartOpts *o,
                          char *out, unsigned long cap) {
  snprintf(out, cap, "partition %s as %s/%s (sfdisk)%s", dst, o->scheme,
           o->layout, o->dry_run ? " [dry-run]" : "");
}

int rufux_partition(const char *dst, const RufuxPartOpts *o,
                    char *err, unsigned long cap) {
  if (strcmp(o->scheme, "gpt") && strcmp(o->scheme, "dos") && strcmp(o->scheme, "mbr")) {
    snprintf(err, cap, "scheme must be gpt|dos (mbr alias dos)");
    return -1;
  }
  const char *scheme = !strcmp(o->scheme, "mbr") ? "dos" : o->scheme;
  if (strcmp(o->layout, "single") && strcmp(o->layout, "esp+main")) {
    snprintf(err, cap, "layout must be single|esp+main");
    return -1;
  }
  if (!o->dry_run && !o->yes) {
    snprintf(err, cap, "refusing real partition without --yes");
    return -1;
  }
  if (rufux_check_target(dst, o->allow_fixed, o->allow_file, err, cap) != 0)
    return -1;
  if (!rufux_have("sfdisk")) {
    snprintf(err, cap, "sfdisk not found");
    return -1;
  }
  // Explicit field syntax (works across sfdisk generations):
  // single: one Linux/UEFI-usable partition; esp+main: 512MiB ESP + rest.
  // ESP type GUID C12A7328-F81F-11D2-BA4B-00A0C93EC93B (was a placeholder).
  // MBR type follows the filesystem: BIOS boots FAT/NTFS partitions,
  // never type 83 (Linux). gpt layouts need no boot flag (UEFI ignores
  // it); every dos/MBR data partition gets flagged bootable.
  const char *mbr_type = "0c";
  if (o->fs_main) {
    if (!strcmp(o->fs_main, "ntfs") || !strcmp(o->fs_main, "exfat") ||
        !strcmp(o->fs_main, "udf"))
      mbr_type = "07";
    else if (!strcmp(o->fs_main, "ext4") || !strcmp(o->fs_main, "ext2") ||
             !strcmp(o->fs_main, "ext3"))
      mbr_type = "83";
  }
  char script[1024];
  if (!strcmp(o->layout, "single")) {
    if (!strcmp(scheme, "gpt"))
      snprintf(script, sizeof script,
               "label: gpt\nstart=1MiB, type=0FC63DAF-8483-4772-8E79-3D69D8477DE4\n");
    else
      snprintf(script, sizeof script, "label: dos\nstart=1MiB, type=%s, bootable\n",
               mbr_type);
  } else if (!strcmp(scheme, "gpt")) {
    // ESP + Windows/Linux data partition: the data volume carries a
    // Windows-readable filesystem (NTFS/exFAT), so like Rufus it gets
    // the Microsoft basic data type, not the Linux filesystem type.
    snprintf(script, sizeof script,
             "label: gpt\nsize=512MiB, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B\n"
             "type=EBD0A0A2-B938-11D2-B3FA-00A0C93EC93B\n");
  } else {
    snprintf(script, sizeof script,
             "label: dos\nsize=512MiB, type=ef\ntype=%s, bootable\n", mbr_type);
  }

  if (o->dry_run) {
    fprintf(stderr, "+ sfdisk --wipe always %s <<'%s'\n", dst, script);
    return 0;
  }
  // write script to temp and run sfdisk < script (no shell)
  char tmpl[] = "/tmp/rufux-sfdisk-XXXXXX";
  int fd = mkstemp(tmpl);
  if (fd < 0) { snprintf(err, cap, "mkstemp failed"); return -1; }
  size_t L = strlen(script);
  if (write(fd, script, L) != (ssize_t)L) { close(fd); unlink(tmpl); snprintf(err, cap, "tmp write failed"); return -1; }
  close(fd);
  const char *argv[] = {"sfdisk", "--wipe", "always", dst, NULL};
  // redirect stdin from tmpl
  int rc = 0;
  pid_t p = fork();
  if (p < 0) rc = -1;
  else if (p == 0) {
    FILE *f = freopen(tmpl, "r", stdin);
    (void)f;
    execvp(argv[0], (char *const *)argv);
    _exit(127);
  } else {
    int st = 0;
    while (waitpid(p, &st, 0) < 0) {}
    rc = (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
  }
  unlink(tmpl);
  // Tell the kernel to rescan (best effort; failures are non-fatal).
  // NOTE: dry_run here is 0 = really run. A previous revision passed 1
  // (log-only), which left standalone `partition` with a stale table.
  {
    const char *pa[] = {"partprobe", dst, NULL};
    if (rufux_have("partprobe")) rufux_run(pa, 0);
    const char *us[] = {"udevadm", "settle", NULL};
    if (rufux_have("udevadm")) rufux_run(us, 0);
  }
  if (rc != 0) snprintf(err, cap, "sfdisk failed on '%s'", dst);
  return rc;
}

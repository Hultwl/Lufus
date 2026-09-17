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

void lufus_partition_plan(const char *dst, const LufusPartOpts *o,
                          char *out, unsigned long cap) {
  snprintf(out, cap, "partition %s as %s/%s (sfdisk)%s", dst, o->scheme,
           o->layout, o->dry_run ? " [dry-run]" : "");
}

int lufus_partition(const char *dst, const LufusPartOpts *o,
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
  if (lufus_check_target(dst, o->allow_fixed, o->allow_file, err, cap) != 0)
    return -1;
  if (!lufus_have("/usr/bin/sfdisk")) {
    snprintf(err, cap, "sfdisk not found");
    return -1;
  }
  // feed script via --wipe always + layout
  // single: one Linux/UEFI-usable partition; esp+main: 512M ESP + rest
  char script[1024];
  if (!strcmp(o->layout, "single"))
    snprintf(script, sizeof script, "label: %s\n,;\n", scheme);
  else if (!strcmp(scheme, "gpt"))
    snprintf(script, sizeof script,
             "label: gpt\nsize=512M, type=[UUID_PLACEHOLDER_1]\n;\n");
  else
    snprintf(script, sizeof script,
             "label: dos\nsize=512M, type=ef\n;\n");

  if (o->dry_run) {
    fprintf(stderr, "+ /usr/bin/sfdisk --wipe always %s <<'%s'\n", dst, script);
    return 0;
  }
  // write script to temp and run sfdisk < script (no shell)
  char tmpl[] = "/tmp/lufus-sfdisk-XXXXXX";
  int fd = mkstemp(tmpl);
  if (fd < 0) { snprintf(err, cap, "mkstemp failed"); return -1; }
  size_t L = strlen(script);
  if (write(fd, script, L) != (ssize_t)L) { close(fd); unlink(tmpl); snprintf(err, cap, "tmp write failed"); return -1; }
  close(fd);
  const char *argv[] = {"/usr/bin/sfdisk", "--wipe", "always", dst, NULL};
  // redirect stdin from tmpl
  int rc = 0;
  pid_t p = fork();
  if (p < 0) rc = -1;
  else if (p == 0) {
    FILE *f = freopen(tmpl, "r", stdin);
    (void)f;
    execv(argv[0], (char *const *)argv);
    _exit(127);
  } else {
    int st = 0;
    while (waitpid(p, &st, 0) < 0) {}
    rc = (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
  }
  unlink(tmpl);
  // best-effort rescan
  {
    const char *pa[] = {"/usr/bin/partprobe", dst, NULL};
    if (lufus_have("/usr/bin/partprobe")) lufus_run(pa, 1 /*dry: just log, ignore*/);
  }
  if (rc != 0) snprintf(err, cap, "sfdisk failed on '%s'", dst);
  return rc;
}

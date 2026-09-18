#define _GNU_SOURCE
#include "extract.h"
#include "exec.h"
#include "iso_probe.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

// Directory content size in bytes (extraction progress polling).
// Pure libc via nftw: no du subprocess, no shell.
static unsigned long long du_acc;
static int du_cb(const char *path, const struct stat *sb, int type, struct FTW *ftw) {
  (void)path; (void)ftw;
  if (type == FTW_F) du_acc += (unsigned long long)sb->st_size;
  return 0;
}

static unsigned long long dir_size(const char *path) {
  du_acc = 0;
  struct stat st;
  if (stat(path, &st) != 0) return 0;
  if (!S_ISDIR(st.st_mode)) return (unsigned long long)st.st_size;
  nftw(path, du_cb, 16, FTW_PHYS);
  return du_acc;
}

// Run extractor in a child while the parent polls destination growth.
// total=0 disables progress (plain run). Returns extracted bytes via out.
static int extract_poll(const char *const av[], const char *dest_dir,
                        unsigned long long total,
                        RufuxExtractProgress prog, void *user,
                        unsigned long long *got_out,
                        char *err, unsigned long cap) {
  unsigned long long base = dir_size(dest_dir);
  pid_t pid = fork();
  if (pid < 0) { snprintf(err, cap, "fork failed"); return -1; }
  if (pid == 0) {
    execvp(av[0], (char *const *)av);
    _exit(127);
  }
  int rc = 0;
  for (;;) {
    int st = 0;
    pid_t w = waitpid(pid, &st, WNOHANG);
    if (w < 0) { rc = -1; break; }
    if (w == pid) {
      rc = (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
      break;
    }
    if (prog && total) {
      unsigned long long cur = dir_size(dest_dir);
      prog(cur > base ? cur - base : 0, total, user);
    }
    usleep(200000);
  }
  if (prog && total && rc == 0) {
    unsigned long long cur = dir_size(dest_dir);
    prog(cur > base ? cur - base : 0, total, user);
  }
  if (got_out) {
    unsigned long long cur = dir_size(dest_dir);
    *got_out = cur > base ? cur - base : 0;
  }
  if (rc != 0) snprintf(err, cap, "%s extract failed", av[0]);
  return rc;
}

int rufux_extract_iso_progress(const char *src, const char *dest_dir, int dry_run,
                               RufuxExtractProgress prog, void *user,
                               char *err, unsigned long cap) {
  struct stat st;
  if (stat(src, &st) != 0) { snprintf(err, cap, "source '%s' missing", src); return -1; }
  unsigned long long total = S_ISREG(st.st_mode) ? (unsigned long long)st.st_size : 0;
  if (!dry_run) {
    if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST) {
      snprintf(err, cap, "cannot mkdir '%s': %s", dest_dir, strerror(errno));
      return -1;
    }
  }
  // Backend routing: UDF images go straight to 7z (bsdtar silently
  // under-extracts some UDF layouts, e.g. Win11 media). Others prefer
  // bsdtar with a 7z fallback.
  int want_7z = rufux_iso_is_udf(src) > 0;
  int have_bsdtar = rufux_have("bsdtar");
  int rc = 0;
  unsigned long long got = 0;
  if (rufux_have("bsdtar") && !want_7z) {
    const char *av[] = {"bsdtar", "-xf", src, "-C", dest_dir, NULL};
    if (dry_run) { rufux_run(av, 1); return 0; }
    rc = extract_poll(av, dest_dir, (prog && total) ? total : 0, prog, user,
                      &got, err, cap);
    // bsdtar can exit 0 while under-extracting some UDF layouts: fall
    // through to 7z instead of declaring success on a partial tree.
    if (rc != 0 || (total > (50ULL << 20) && got * 4 < total)) {
      if (rc == 0 && rufux_have("7z")) {
        snprintf(err, cap, "bsdtar incomplete (%llu of %llu bytes), retrying with 7z",
                 got, total);
        have_bsdtar = 0; // force the 7z path below (keep err if it fails too)
      } else {
        if (rc != 0) return rc;
        snprintf(err, cap, "extract incomplete (%llu of %llu bytes) and no 7z available",
                 got, total);
        return -1;
      }
    } else {
      return rc;
    }
  }
  if (rufux_have("7z")) {
    char out[1152];
    snprintf(out, sizeof out, "-o%s", dest_dir);
    const char *av[] = {"7z", "x", src, "-y", out, NULL};
    if (dry_run) { rufux_run(av, 1); return 0; }
    rc = extract_poll(av, dest_dir, (prog && total) ? total : 0, prog, user,
                      &got, err, cap);
    if (rc == 0 && total > (50ULL << 20) && got * 4 < total) {
      snprintf(err, cap, "extract incomplete (%llu of %llu bytes)", got, total);
      return -1;
    }
    return rc;
  }
  if (!have_bsdtar && !rufux_have("7z"))
    snprintf(err, cap, "need bsdtar or 7z for extraction (7z required for UDF)");
  return -1;
}

int rufux_extract_iso(const char *src, const char *dest_dir, int dry_run,
                      char *err, unsigned long cap) {
  return rufux_extract_iso_progress(src, dest_dir, dry_run, NULL, NULL, err, cap);
}

int rufux_write_autorun(const char *dir, const char *label, int dry_run,
                        char *err, unsigned long cap) {
  if (!label || !label[0]) label = "RUFUX";
  char path[1024];
  snprintf(path, sizeof path, "%s/autorun.inf", dir);
  if (dry_run) {
    fprintf(stderr, "+ write %s (label '%s')\n", path, label);
    return 0;
  }
  FILE *f = fopen(path, "w");
  if (!f) { snprintf(err, cap, "cannot write '%s': %s", path, strerror(errno)); return -1; }
  fprintf(f, "[Autorun]\nLabel=%s\n", label);
  if (fclose(f) != 0) { snprintf(err, cap, "cannot close '%s'", path); return -1; }
  return 0;
}

#include "extract.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

// Directory size in bytes (for extraction progress polling).
static unsigned long long dir_size(const char *path) {
  char cmd[1152];
  snprintf(cmd, sizeof cmd, "du -sb --apparent-size '%s' 2>/dev/null", path);
  FILE *p = popen(cmd, "r");
  if (!p) return 0;
  unsigned long long n = 0;
  if (fscanf(p, "%llu", &n) != 1) n = 0;
  pclose(p);
  return n;
}

// Run extractor in a child while the parent polls destination growth.
// total=0 disables progress (plain run).
static int extract_poll(const char *const av[], const char *dest_dir,
                        unsigned long long total,
                        RufuxExtractProgress prog, void *user,
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
  if (rufux_have("bsdtar")) {
    const char *av[] = {"bsdtar", "-xf", src, "-C", dest_dir, NULL};
    if (dry_run) { rufux_run(av, 1); return 0; }
    return extract_poll(av, dest_dir, (prog && total) ? total : 0, prog, user, err, cap);
  }
  if (rufux_have("7z")) {
    char out[1152];
    snprintf(out, sizeof out, "-o%s", dest_dir);
    const char *av[] = {"7z", "x", src, out, NULL};
    if (dry_run) { rufux_run(av, 1); return 0; }
    return extract_poll(av, dest_dir, (prog && total) ? total : 0, prog, user, err, cap);
  }
  snprintf(err, cap, "need bsdtar or 7z for extraction");
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

#include "extract.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

int rufux_extract_iso(const char *src, const char *dest_dir, int dry_run,
                      char *err, unsigned long cap) {
  struct stat st;
  if (stat(src, &st) != 0) { snprintf(err, cap, "source '%s' missing", src); return -1; }
  if (!dry_run) {
    if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST) {
      snprintf(err, cap, "cannot mkdir '%s': %s", dest_dir, strerror(errno));
      return -1;
    }
  }
  if (rufux_have("bsdtar")) {
    const char *av[] = {"bsdtar", "-xf", src, "-C", dest_dir, NULL};
    if (rufux_run(av, dry_run) != 0) {
      if (!dry_run) snprintf(err, cap, "bsdtar extract failed");
      return dry_run ? 0 : -1;
    }
    return 0;
  }
  if (rufux_have("7z")) {
    const char *av[] = {"7z", "x", src, "-o", dest_dir, NULL};
    if (rufux_run(av, dry_run) != 0) {
      if (!dry_run) snprintf(err, cap, "7z extract failed");
      return dry_run ? 0 : -1;
    }
    return 0;
  }
  snprintf(err, cap, "need bsdtar or 7z for extraction");
  return -1;
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

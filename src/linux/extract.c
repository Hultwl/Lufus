#include "extract.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

int lufus_extract_iso(const char *src, const char *dest_dir, int dry_run,
                      char *err, unsigned long cap) {
  struct stat st;
  if (stat(src, &st) != 0) { snprintf(err, cap, "source '%s' missing", src); return -1; }
  if (!dry_run) {
    if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST) {
      snprintf(err, cap, "cannot mkdir '%s': %s", dest_dir, strerror(errno));
      return -1;
    }
  }
  if (lufus_have("/usr/bin/bsdtar")) {
    const char *av[] = {"/usr/bin/bsdtar", "-xf", src, "-C", dest_dir, NULL};
    if (lufus_run(av, dry_run) != 0) {
      if (!dry_run) snprintf(err, cap, "bsdtar extract failed");
      return dry_run ? 0 : -1;
    }
    return 0;
  }
  if (lufus_have("/usr/bin/7z")) {
    const char *av[] = {"/usr/bin/7z", "x", src, "-o", dest_dir, NULL};
    if (lufus_run(av, dry_run) != 0) {
      if (!dry_run) snprintf(err, cap, "7z extract failed");
      return dry_run ? 0 : -1;
    }
    return 0;
  }
  snprintf(err, cap, "need bsdtar or 7z for extraction");
  return -1;
}

#include "persist.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int rufux_create_persist(const char *dir, const char *label, unsigned long size_mb,
                         int dry_run, char *err, unsigned long cap) {
  if (!label || !label[0]) label = "casper-rw";
  if (size_mb == 0 || size_mb > 16384) { snprintf(err, cap, "size must be 1..16384 MB"); return -1; }
  struct stat st;
  if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) { snprintf(err, cap, "dir '%s' missing", dir); return -1; }
  char path[1024];
  snprintf(path, sizeof path, "%s/%s", dir, label);
  char sz[64];
  snprintf(sz, sizeof sz, "%luM", size_mb);
  // truncate + mkfs.ext4 -L label
  if (dry_run) {
    fprintf(stderr, "+ truncate -s %s %s && mkfs.ext4 -F -L %s %s\n", sz, path, label, path);
    return 0;
  }
  const char *a1[] = {"truncate", "-s", sz, path, NULL};
  if (rufux_run(a1, 0) != 0) { snprintf(err, cap, "truncate failed"); return -1; }
  char lab[256];
  snprintf(lab, sizeof lab, "%s", label);
  const char *a2[] = {"mkfs.ext4", "-F", "-L", lab, path, NULL};
  if (!rufux_have("mkfs.ext4")) { snprintf(err, cap, "mkfs.ext4 missing"); return -1; }
  if (rufux_run(a2, 0) != 0) { snprintf(err, cap, "mkfs.ext4 persist failed"); return -1; }
  return 0;
}

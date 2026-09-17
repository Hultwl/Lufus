#ifndef LUFUS_MKFS_H
#define LUFUS_MKFS_H
// mkfs dispatch (replaces format.c VDS/fmifs path for Phase 2).
typedef struct {
  const char *fs; // vfat|ntfs|exfat|ext4|udf
  const char *label; // may be NULL
  int dry_run;
  int allow_fixed;
  int allow_file;
  int yes;
} LufusMkfsOpts;
int lufus_format(const char *dst, const LufusMkfsOpts *o, char *err, unsigned long cap);
#endif

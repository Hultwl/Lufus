#ifndef RUFUX_MKFS_H
#define RUFUX_MKFS_H
// mkfs dispatch (replaces format.c VDS/fmifs path for Phase 2).
typedef struct {
  const char *fs; // vfat|ntfs|exfat|ext4|udf
  const char *label; // may be NULL
  int cluster_sectors; // 0 = default (vfat only: passed as mkfs.vfat -s)
  int dry_run;
  int allow_fixed;
  int allow_file;
  int yes;
} RufuxMkfsOpts;
int rufux_format(const char *dst, const RufuxMkfsOpts *o, char *err, unsigned long cap);
#endif

#ifndef LUFUS_PARTITION_H
#define LUFUS_PARTITION_H
// GPT/MBR layout via sfdisk (replaces drive.c VDS/IOCTL path for Phase 2).
// Layouts: "single" (one partition) or "esp+main" (ESP + main).
typedef struct {
  const char *scheme;  // "gpt" | "dos"
  const char *layout;  // "single" | "esp+main"
  const char *fs_main; // informational only (vfat|ntfs|exfat|ext4)
  int dry_run;
  int allow_fixed;
  int allow_file;
  int yes;
} LufusPartOpts;
int lufus_partition(const char *dst, const LufusPartOpts *o, char *err, unsigned long cap);
void lufus_partition_plan(const char *dst, const LufusPartOpts *o, char *out, unsigned long cap);
#endif

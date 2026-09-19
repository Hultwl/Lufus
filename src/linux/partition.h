#ifndef RUFUX_PARTITION_H
#define RUFUX_PARTITION_H
// GPT/MBR layout via sfdisk (replaces drive.c VDS/IOCTL path for Phase 2).
// Layouts: "single" (one partition), "esp+main" (ESP + main) or
// "main+uefintfs" (data partition + 1 MiB UEFI:NTFS partition at the end).
typedef struct {
  const char *scheme;  // "gpt" | "dos"
  const char *layout;  // "single" | "esp+main" | "main+uefintfs"
  const char *fs_main; // MBR partition type source (vfat->0c, ntfs/exfat->07); NULL = 0c
  int dry_run;
  int allow_fixed;
  int allow_file;
  int yes;
} RufuxPartOpts;
int rufux_partition(const char *dst, const RufuxPartOpts *o, char *err, unsigned long cap);
void rufux_partition_plan(const char *dst, const RufuxPartOpts *o, char *out, unsigned long cap);
#endif

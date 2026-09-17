#include "mkfs_task.h"
#include "device.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>

int rufux_format(const char *dst, const RufuxMkfsOpts *o,
                 char *err, unsigned long cap) {
  if (!o->fs || (!strcmp(o->fs, "") )) { snprintf(err, cap, "need --fs"); return -1; }
  if (!o->dry_run && !o->yes) { snprintf(err, cap, "refusing real format without --yes"); return -1; }
  if (rufux_check_target(dst, o->allow_fixed, o->allow_file, err, cap) != 0) return -1;

  const char *argv_vfat[] = {"/usr/bin/mkfs.vfat", "-F", "32", NULL, NULL, NULL};
  const char *argv_ntfs[] = {"/usr/bin/mkfs.ntfs", "-F", "-Q", NULL, NULL, NULL};
  const char *argv_exfat[] = {"/usr/bin/mkfs.exfat", NULL, NULL, NULL};
  const char *argv_ext4[] = {"/usr/bin/mkfs.ext4", "-F", NULL, NULL, NULL, NULL};
  const char *argv_udf[] = {"/usr/bin/mkfs.udf", NULL, NULL};
  const char **av = NULL;

  // build argv with optional label + target (static buffers, small)
  static char lab_vfat[160], lab_ntfs[160], lab_exfat[160], lab_ext4[160], sec_vfat[32];
  static const char *a_vfat[9], *a_ntfs[7], *a_exfat[5], *a_ext4[7], *a_udf[4];
  if (!strcmp(o->fs, "vfat") || !strcmp(o->fs, "fat32")) {
    if (!rufux_have("/usr/bin/mkfs.vfat")) { snprintf(err, cap, "mkfs.vfat missing"); return -1; }
    int i = 0;
    a_vfat[i++] = "/usr/bin/mkfs.vfat"; a_vfat[i++] = "-F"; a_vfat[i++] = "32";
    if (o->cluster_sectors > 0) {
      snprintf(sec_vfat, sizeof sec_vfat, "%d", o->cluster_sectors);
      a_vfat[i++] = "-s"; a_vfat[i++] = sec_vfat;
    }
    if (o->label && o->label[0]) { snprintf(lab_vfat, sizeof lab_vfat, "%s", o->label); a_vfat[i++] = "-n"; a_vfat[i++] = lab_vfat; }
    a_vfat[i++] = dst; a_vfat[i] = NULL; av = a_vfat;
    (void)argv_vfat;
  } else if (!strcmp(o->fs, "ntfs")) {
    if (!rufux_have("/usr/bin/mkfs.ntfs")) { snprintf(err, cap, "mkfs.ntfs missing (ntfsprogs)"); return -1; }
    int i = 0;
    a_ntfs[i++] = "/usr/bin/mkfs.ntfs"; a_ntfs[i++] = "-F"; a_ntfs[i++] = "-Q";
    if (o->label && o->label[0]) { snprintf(lab_ntfs, sizeof lab_ntfs, "%s", o->label); a_ntfs[i++] = "-L"; a_ntfs[i++] = lab_ntfs; }
    a_ntfs[i++] = dst; a_ntfs[i] = NULL; av = a_ntfs;
    (void)argv_ntfs;
  } else if (!strcmp(o->fs, "exfat")) {
    if (!rufux_have("/usr/bin/mkfs.exfat")) { snprintf(err, cap, "mkfs.exfat missing"); return -1; }
    int i = 0;
    a_exfat[i++] = "/usr/bin/mkfs.exfat";
    if (o->label && o->label[0]) { snprintf(lab_exfat, sizeof lab_exfat, "%s", o->label); a_exfat[i++] = "-n"; a_exfat[i++] = lab_exfat; }
    a_exfat[i++] = dst; a_exfat[i] = NULL; av = a_exfat;
    (void)argv_exfat;
  } else if (!strcmp(o->fs, "ext4") || !strcmp(o->fs, "ext2") || !strcmp(o->fs, "ext3")) {
    if (!rufux_have("/usr/bin/mkfs.ext4")) { snprintf(err, cap, "mkfs.ext4 missing"); return -1; }
    int i = 0;
    a_ext4[i++] = "/usr/bin/mkfs.ext4"; a_ext4[i++] = "-F";
    if (o->label && o->label[0]) { snprintf(lab_ext4, sizeof lab_ext4, "%s", o->label); a_ext4[i++] = "-L"; a_ext4[i++] = lab_ext4; }
    a_ext4[i++] = dst; a_ext4[i] = NULL; av = a_ext4;
    (void)argv_ext4;
  } else if (!strcmp(o->fs, "udf")) {
    if (!rufux_have("/usr/bin/mkfs.udf")) { snprintf(err, cap, "mkfs.udf missing"); return -1; }
    a_udf[0] = "/usr/bin/mkfs.udf"; a_udf[1] = dst; a_udf[2] = NULL; av = a_udf;
    (void)argv_udf;
  } else {
    snprintf(err, cap, "unsupported fs '%s' (vfat|ntfs|exfat|ext4|udf)", o->fs);
    return -1;
  }
  if (rufux_run(av, o->dry_run) != 0) {
    if (!o->dry_run) snprintf(err, cap, "mkfs.%s failed on '%s'", o->fs, dst);
    return o->dry_run ? 0 : -1;
  }
  return 0;
}

#include "iso_probe.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

int rufux_probe_iso(const char *path, char *label_out, unsigned long cap) {
  RufuxIsoInfo info = {0};
  int rc = rufux_probe_iso_detail(path, &info);
  if (rc != 0) return rc;
  snprintf(label_out, cap, "%s", info.label);
  return info.valid_iso ? 0 : -2;
}

int rufux_probe_iso_detail(const char *path, RufuxIsoInfo *info) {
  memset(info, 0, sizeof *info);
  struct stat st;
  if (stat(path, &st) != 0) return -1;
  info->size_bytes = (unsigned long long)st.st_size;
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  // PVD at sector 16: magic "CD001" at +1, label at +40 (32B)
  unsigned char sec[2048];
  if (fseek(f, 16 * 2048, SEEK_SET) != 0) { fclose(f); return -1; }
  if (fread(sec, 1, sizeof sec, f) != sizeof sec) { fclose(f); return -1; }
  if (sec[0] == 1 && !memcmp(sec + 1, "CD001", 5)) {
    info->valid_iso = 1;
    char label[33] = {0};
    memcpy(label, sec + 40, 32);
    label[32] = 0;
    for (int i = 31; i >= 0 && (label[i] == ' ' || label[i] == 0); i--) label[i] = 0;
    snprintf(info->label, sizeof info->label, "%s", label);
  }
  // Boot record at sector 17: "CD001", type 0, id "EL TORITO SPECIFICATION"
  if (fseek(f, 17 * 2048, SEEK_SET) == 0 &&
      fread(sec, 1, sizeof sec, f) == sizeof sec) {
    if (sec[0] == 0 && !memcmp(sec + 1, "CD001", 5) &&
        !memcmp(sec + 7, "EL TORITO SPECIFICATION", 23)) {
      info->bootable = 1;
      // crude EFI hint: scan first 512B of boot catalog area for 0xEF platform id
      for (int i = 0; i < (int)sizeof sec - 4; i++) {
        if (sec[i] == 0xEF) { info->has_efi = 1; break; }
      }
    }
  }
  fclose(f);
  return 0;
}

void rufux_print_iso_detail(const char *path, const RufuxIsoInfo *info) {
  printf("file: %s\nsize: %llu bytes (%.2f MB)\nlabel: %s\nvalid_iso: %s\nbootable: %s\nefi_hint: %s\n",
         path, info->size_bytes, info->size_bytes / 1048576.0,
         info->label[0] ? info->label : "(none)",
         info->valid_iso ? "yes" : "no",
         info->bootable ? "yes" : "no",
         info->has_efi ? "yes" : "no");
}

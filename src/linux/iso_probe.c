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
  // Boot record at sector 17: "CD001", type 0, id "EL TORITO SPECIFICATION".
  // Boot catalog pointer is a little-endian u32 at offset 0x47.
  if (fseek(f, 17 * 2048, SEEK_SET) == 0 &&
      fread(sec, 1, sizeof sec, f) == sizeof sec) {
    if (sec[0] == 0 && !memcmp(sec + 1, "CD001", 5) &&
        !memcmp(sec + 7, "EL TORITO SPECIFICATION", 23)) {
      info->bootable = 1;
      // Catalog validation entry: byte0 == 0x01 (header), byte1 ==
      // platform id (0x00 x86, 0x01 PPC, 0x02 Mac, 0xEF EFI).
      unsigned long cat_lba = (unsigned long)sec[0x47] |
                              ((unsigned long)sec[0x48] << 8) |
                              ((unsigned long)sec[0x49] << 16) |
                              ((unsigned long)sec[0x4A] << 24);
      unsigned long long cat_off = cat_lba * 2048ULL;
      if (cat_lba > 0 && cat_off + 32 <= info->size_bytes &&
          fseek(f, (long)cat_off, SEEK_SET) == 0 &&
          fread(sec, 1, 32, f) == 32) {
        if (sec[0] == 0x01 && sec[1] == 0xEF) info->has_efi = 1;
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

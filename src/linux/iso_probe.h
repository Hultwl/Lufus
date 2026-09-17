#ifndef LUFUS_ISO_H
#define LUFUS_ISO_H
// Phase 1: ISO detail probe.

typedef struct {
  char label[64];
  unsigned long long size_bytes;
  int valid_iso;   // PVD magic present
  int bootable;    // El Torito boot record present
  int has_efi;     // EFI boot image hint (eltorito platform 0xEF or BOOT.CAT ref)
} LufusIsoInfo;

int lufus_probe_iso(const char *path, char *label_out, unsigned long cap);
int lufus_probe_iso_detail(const char *path, LufusIsoInfo *info);
void lufus_print_iso_detail(const char *path, const LufusIsoInfo *info);
#endif

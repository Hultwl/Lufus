#ifndef RUFUX_ISO_H
#define RUFUX_ISO_H
// Phase 1: ISO detail probe.

typedef struct {
  char label[64];
  unsigned long long size_bytes;
  int valid_iso;   // PVD magic present
  int bootable;    // El Torito boot record present
  int has_efi;     // EFI boot image hint (eltorito platform 0xEF or BOOT.CAT ref)
  int is_windows;  // sources/install.wim|esd present (1) / no (-1 unknown if no bsdtar)
} RufuxIsoInfo;

int rufux_probe_iso(const char *path, char *label_out, unsigned long cap);
int rufux_probe_iso_detail(const char *path, RufuxIsoInfo *info);
void rufux_print_iso_detail(const char *path, const RufuxIsoInfo *info);
#endif

#include "iso_probe.h"
#include <stdio.h>
#include <string.h>

int lufus_probe_iso(const char *path, char *label_out, unsigned long cap) {
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  // ISO9660 PVD at 16*2048+40, 32 bytes volume label
  if (fseek(f, 16 * 2048 + 40, SEEK_SET) != 0) { fclose(f); return -1; }
  char label[33] = {0};
  if (fread(label, 1, 32, f) != 32) { fclose(f); return -1; }
  fclose(f);
  if (memcmp(label, "CD001", 5) != 0 && label[0] == 0) return -2;
  label[32] = 0;
  // trim trailing spaces
  for (int i = 31; i >= 0 && (label[i] == ' ' || label[i] == 0); i--) label[i] = 0;
  snprintf(label_out, cap, "%s", label);
  return 0;
}

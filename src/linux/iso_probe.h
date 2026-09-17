#ifndef LUFUS_ISO_H
#define LUFUS_ISO_H
// Minimal ISO probe (placeholder for reusing src/libcdio + src/iso.c logic).
int lufus_probe_iso(const char *path, char *label_out, unsigned long cap);
#endif

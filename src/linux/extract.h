#ifndef LUFUS_EXTRACT_H
#define LUFUS_EXTRACT_H
// ISO -> directory via bsdtar (preferred) or 7z.
int lufus_extract_iso(const char *src, const char *dest_dir, int dry_run,
                      char *err, unsigned long cap);
#endif

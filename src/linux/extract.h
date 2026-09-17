#ifndef RUFUX_EXTRACT_H
#define RUFUX_EXTRACT_H
// ISO -> directory via bsdtar (preferred) or 7z.
int rufux_extract_iso(const char *src, const char *dest_dir, int dry_run,
                      char *err, unsigned long cap);
// Extended label: write autorun.inf carrying the volume label (Rufus parity).
int rufux_write_autorun(const char *dir, const char *label, int dry_run,
                        char *err, unsigned long cap);
#endif

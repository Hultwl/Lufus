#ifndef RUFUX_EXTRACT_H
#define RUFUX_EXTRACT_H
// ISO -> directory via bsdtar (preferred) or 7z.
typedef void (*RufuxExtractProgress)(unsigned long long done,
                                     unsigned long long total, void *user);
int rufux_extract_iso(const char *src, const char *dest_dir, int dry_run,
                      char *err, unsigned long cap);
// Same, but reports destination growth against the image size.
int rufux_extract_iso_progress(const char *src, const char *dest_dir, int dry_run,
                               RufuxExtractProgress prog, void *user,
                               char *err, unsigned long cap);
// Extended label: write autorun.inf carrying the volume label (Rufus parity).
int rufux_write_autorun(const char *dir, const char *label, int dry_run,
                        char *err, unsigned long cap);
#endif

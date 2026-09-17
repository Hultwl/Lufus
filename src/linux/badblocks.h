#ifndef RUFUX_BADBLOCKS_H
#define RUFUX_BADBLOCKS_H
typedef void (*RufuxScanProgress)(unsigned long long done, unsigned long long total, void *user);
int rufux_badblocks(const char *path, int allow_file, RufuxScanProgress cb, void *user,
                    unsigned long long *bad_out, char *err, unsigned long cap);
#endif

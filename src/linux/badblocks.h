#ifndef LUFUS_BADBLOCKS_H
#define LUFUS_BADBLOCKS_H
typedef void (*LufusScanProgress)(unsigned long long done, unsigned long long total, void *user);
int lufus_badblocks(const char *path, int allow_file, LufusScanProgress cb, void *user,
                    unsigned long long *bad_out, char *err, unsigned long cap);
#endif

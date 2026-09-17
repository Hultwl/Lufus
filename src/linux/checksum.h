#ifndef LUFUS_CHECKSUM_H
#define LUFUS_CHECKSUM_H
// Phase 1: SHA-256 of files (self-contained, no openssl dep).
// cb(done,total,user) may be NULL.
typedef void (*LufusHashProgress)(unsigned long long done,
                                  unsigned long long total, void *user);
int lufus_sha256_file(const char *path, unsigned char out32[32],
                      LufusHashProgress cb, void *user,
                      char *err, unsigned long errcap);
void lufus_hex32(const unsigned char in32[32], char out65[65]);
#endif

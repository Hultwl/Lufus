// SHA-256 via OpenSSL EVP (audited primitive; no hand-rolled crypto).
#include "checksum.h"
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include <sys/stat.h>

void rufux_hex32(const unsigned char in32[32], char out65[65]) {
  static const char *H = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    out65[2*i] = H[in32[i] >> 4];
    out65[2*i+1] = H[in32[i] & 15];
  }
  out65[64] = 0;
}

int rufux_sha256_file(const char *path, unsigned char out32[32],
                      RufuxHashProgress cb, void *user,
                      char *err, unsigned long errcap) {
  struct stat st;
  if (stat(path, &st) != 0) {
    if (err) snprintf(err, errcap, "cannot stat '%s'", path);
    return -1;
  }
  FILE *f = fopen(path, "rb");
  if (!f) {
    if (err) snprintf(err, errcap, "cannot open '%s'", path);
    return -1;
  }
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) {
    if (err) snprintf(err, errcap, "EVP context allocation failed");
    fclose(f);
    return -1;
  }
  int rc = -1;
  unsigned char buf[1 << 16];
  size_t n;
  unsigned long long done = 0, total = (unsigned long long)st.st_size;
  unsigned int outlen = 0;
  if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == 1) {
    rc = 0;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
      if (EVP_DigestUpdate(ctx, buf, n) != 1) { rc = -1; break; }
      done += n;
      if (cb) cb(done, total, user);
    }
    if (ferror(f)) {
      if (err) snprintf(err, errcap, "read error on '%s'", path);
      rc = -1;
    }
    if (rc == 0 && EVP_DigestFinal_ex(ctx, out32, &outlen) != 1) rc = -1;
    if (rc == 0 && outlen != 32) rc = -1;
  } else if (err) {
    snprintf(err, errcap, "EVP init failed");
  }
  EVP_MD_CTX_free(ctx);
  fclose(f);
  if (rc == 0 && cb) cb(total, total, user);
  return rc;
}

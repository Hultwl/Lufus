// Unit test: DOS boot-record writers are byte-exact vs ms-sys blobs.
// Usage: ctest_dosboot <2MB-zeroed-file>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/linux/dosboot.h"
// Expected blobs live in dosboot.o; their lengths come in as argv
// (counted from the ms-sys headers by the test script).
extern unsigned char br_fat32_0x0[];
extern unsigned char br_fat32_0x52[];
extern unsigned char br_fat32_0x3f0[];
extern unsigned char mbr_dos_0x0[];

static int check(const char *path, unsigned long long off,
                 const unsigned char *blob, size_t len, const char *name) {
  FILE *f = fopen(path, "rb");
  if (!f) { printf("FAIL open %s\n", path); return 1; }
  unsigned char buf[1024] = {0};
  if (fseek(f, (long)off, SEEK_SET) || fread(buf, 1, len, f) != len) {
    printf("FAIL read %s\n", name);
    fclose(f);
    return 1;
  }
  fclose(f);
  if (memcmp(buf, blob, len)) { printf("FAIL bytes %s\n", name); return 1; }
  printf("ok %s\n", name);
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 7) return 2;
  size_t n0 = (size_t)strtoul(argv[2], NULL, 10);
  size_t n52 = (size_t)strtoul(argv[3], NULL, 10);
  size_t n3f0 = (size_t)strtoul(argv[4], NULL, 10);
  size_t nmbr = (size_t)strtoul(argv[5], NULL, 10);
  unsigned long long base = strtoull(argv[6], NULL, 10); // partition offset, like 1MiB
  if (nmbr > 440) nmbr = 440; // writer caps MBR code at 440 bytes
  char err[256] = {0};
  int fails = 0;
  if (rufux_dos_pbr_fd32(argv[1], base, "TESTLBL", err, sizeof err) != 0) {
    printf("FAIL pbr: %s\n", err);
    return 1;
  }
  fails += check(argv[1], base + 0x0, br_fat32_0x0, n0, "pbr-head");
  // label landed upper-cased + padded at 0x47
  {
    FILE *f = fopen(argv[1], "rb");
    char lab[12] = {0};
    fseek(f, (long)(base + 0x47), SEEK_SET);
    fread(lab, 1, 11, f);
    fclose(f);
    if (memcmp(lab, "TESTLBL    ", 11)) { printf("FAIL label\n"); fails++; }
    else printf("ok label\n");
  }
  fails += check(argv[1], base + 0x52, br_fat32_0x52, n52, "pbr-code");
  fails += check(argv[1], base + 0x3f0, br_fat32_0x3f0, n3f0, "pbr-tail");
  if (rufux_dos_mbr(argv[1], err, sizeof err) != 0) {
    printf("FAIL mbr: %s\n", err);
    return 1;
  }
  {
    size_t n = nmbr;
    fails += check(argv[1], 0, mbr_dos_0x0, n, "mbr-code");
  }
  // partition table area (446+) untouched: still zeros on a zeroed file
  {
    FILE *f = fopen(argv[1], "rb");
    unsigned char t[66] = {0};
    fseek(f, 446, SEEK_SET);
    fread(t, 1, sizeof t, f);
    fclose(f);
    for (int i = 0; i < 66; i++)
      if (t[i]) { printf("FAIL table-touched\n"); fails++; break; }
  }
  printf(fails ? "RESULT FAIL\n" : "RESULT OK\n");
  return fails != 0;
}

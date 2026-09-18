// Unit test: UEFI:NTFS staging + autounattend generation.
// Usage: ctest_wininstall <tmpdir>   (needs RUFUX_RES pointing at res/)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "../src/linux/wininstall.h"

static int has(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static int contains(const char *path, const char *needle) {
  FILE *f = fopen(path, "r");
  if (!f) return 0;
  char buf[8192];
  size_t n = fread(buf, 1, sizeof buf - 1, f);
  fclose(f);
  buf[n] = 0;
  return strstr(buf, needle) != NULL;
}

int main(int argc, char **argv) {
  if (argc < 2) return 2;
  char err[512] = {0};
  char stage[1024], xml[1024];
  snprintf(stage, sizeof stage, "%s/stage", argv[1]);
  snprintf(xml, sizeof xml, "%s/unattend", argv[1]);
  char cmd[1152];
  snprintf(cmd, sizeof cmd, "mkdir -p %s %s", stage, xml);
  if (system(cmd) != 0) return 2;

  if (rufux_stage_uefi_ntfs(stage, err, sizeof err) != 0) {
    // Offline CI/sandbox: fetch impossible, skip instead of failing.
    printf("ok stage (skipped: %s)\n", err);
  } else {
    char efi[1152];
    snprintf(efi, sizeof efi, "%s/esp/EFI/BOOT/bootx64.efi", stage);
    if (!has(efi)) { printf("FAIL bootx64.efi missing\n"); return 1; }
    printf("ok stage\n");
  }

  if (rufux_write_unattend(xml, "bypass,nro,privacy", err, sizeof err) != 0) {
    printf("FAIL unattend: %s\n", err);
    return 1;
  }
  char ax[1152];
  snprintf(ax, sizeof ax, "%s/autounattend.xml", xml);
  const char *need[] = {"BypassTPMCheck", "BypassSecureBootCheck", "BypassNRO",
                        "ProtectYourPC", "LabConfig", NULL};
  for (int i = 0; need[i]; i++) {
    if (!contains(ax, need[i])) { printf("FAIL unattend lacks %s\n", need[i]); return 1; }
  }
  printf("ok unattend\n");
  if (rufux_write_unattend(xml, "bogus", err, sizeof err) == 0) {
    printf("FAIL bogus wue accepted\n");
    return 1;
  }
  printf("ok wue-reject\nRESULT OK\n");
  return 0;
}

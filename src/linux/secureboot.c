#define _GNU_SOURCE
#include "secureboot.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

const char *rufux_sb_string(RufuxSbState s) {
  switch (s) {
    case RUFUX_SB_ENABLED: return "enabled";
    case RUFUX_SB_DISABLED: return "disabled";
    default: return "unknown";
  }
}

static int efivar_byte(const char *name, unsigned char *out) {
  char path[256];
  snprintf(path, sizeof path, "/sys/firmware/efi/efivars/%s", name);
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  unsigned char buf[8] = {0};
  size_t n = fread(buf, 1, sizeof buf, f);
  fclose(f);
  if (n < 5) return -1;
  *out = buf[4]; // 4B attributes + 1B value
  return 0;
}

RufuxSbState rufux_sb_state(void) {
  unsigned char sb = 0, setup = 0;
  if (efivar_byte("SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c", &sb) == 0) {
    if (sb == 0) return RUFUX_SB_DISABLED;
    // In SetupMode, keys are not enforced
    if (efivar_byte("SetupMode-8be4df61-93ca-11d2-aa0d-00e098032b8c", &setup) == 0 && setup == 1)
      return RUFUX_SB_DISABLED;
    return RUFUX_SB_ENABLED;
  }
  // fallback: bootctl (systemd), lines matched in C (no shell pipe)
  if (rufux_have("bootctl")) {
    const char *av[] = {"bootctl", "status", NULL};
    char out[4096] = {0};
    if (rufux_capture(av, out, sizeof out) == 0) {
      char *save = NULL, *line = strtok_r(out, "\n", &save);
      while (line) {
        if (strcasestr(line, "secure boot")) {
          if (strstr(line, "enabled")) return RUFUX_SB_ENABLED;
          if (strstr(line, "disabled")) return RUFUX_SB_DISABLED;
          break;
        }
        line = strtok_r(NULL, "\n", &save);
      }
    }
  }
  return RUFUX_SB_UNKNOWN;
}

int rufux_validate_efi(const char *path, unsigned *subsystem_out,
                       char *err, unsigned long cap) {
  FILE *f = fopen(path, "rb");
  if (!f) { snprintf(err, cap, "cannot open '%s'", path); return -1; }
  unsigned char mz[2];
  if (fread(mz, 1, 2, f) != 2 || mz[0] != 'M' || mz[1] != 'Z') {
    fclose(f);
    snprintf(err, cap, "'%s' is not a PE binary (no MZ header)", path);
    return -1;
  }
  uint32_t lfanew = 0;
  if (fseek(f, 0x3C, SEEK_SET) != 0 || fread(&lfanew, 4, 1, f) != 1) {
    fclose(f);
    snprintf(err, cap, "cannot read PE offset");
    return -1;
  }
  if (lfanew > (1u << 20)) { fclose(f); snprintf(err, cap, "bogus PE offset"); return -1; }
  if (fseek(f, lfanew, SEEK_SET) != 0) { fclose(f); snprintf(err, cap, "seek failed"); return -1; }
  unsigned char sig[4];
  if (fread(sig, 1, 4, f) != 4 || memcmp(sig, "PE\0\0", 4)) {
    fclose(f);
    snprintf(err, cap, "'%s' has no PE signature", path);
    return -1;
  }
  // COFF header: 20 bytes, then optional header magic
  if (fseek(f, lfanew + 4 + 20, SEEK_SET) != 0) { fclose(f); snprintf(err, cap, "seek failed"); return -1; }
  unsigned char magic[2];
  if (fread(magic, 1, 2, f) != 2) { fclose(f); snprintf(err, cap, "cannot read optional magic"); return -1; }
  int pe32plus = (magic[0] == 0x0b && magic[1] == 0x02);
  int pe32 = (magic[0] == 0x0b && magic[1] == 0x01);
  if (!pe32 && !pe32plus) { fclose(f); snprintf(err, cap, "unknown optional magic %02x%02x", magic[0], magic[1]); return -1; }
  // subsystem at optional+68 (both PE32 and PE32+)
  if (fseek(f, lfanew + 4 + 20 + 68, SEEK_SET) != 0) { fclose(f); snprintf(err, cap, "seek failed"); return -1; }
  unsigned char ss[2];
  if (fread(ss, 1, 2, f) != 2) { fclose(f); snprintf(err, cap, "cannot read subsystem"); return -1; }
  fclose(f);
  unsigned sub = (unsigned)ss[0] | ((unsigned)ss[1] << 8);
  if (subsystem_out) *subsystem_out = sub;
  // 2=CUI, 10=EFI app, 11=EFI boot service, 12=EFI runtime, 13=EFI ROM, 16=Xbox
  if (sub == 10 || sub == 11 || sub == 12 || sub == 13) return 0;
  snprintf(err, cap, "'%s' is PE but subsystem %u is not EFI (10/11/12/13)", path, sub);
  return -1;
}

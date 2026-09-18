#define _GNU_SOURCE
#include "wininstall.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>

int rufux_is_windows_iso(const char *iso, char *err, unsigned long cap) {
  struct stat st;
  if (stat(iso, &st) != 0) { snprintf(err, cap, "source '%s' missing", iso); return -1; }
  if (!rufux_have("bsdtar")) { snprintf(err, cap, "need bsdtar to inspect ISO"); return -1; }
  const char *av[] = {"bsdtar", "-tf", iso, NULL};
  char out[65536] = {0};
  if (rufux_capture(av, out, sizeof out) != 0) return 0; // unreadable: not windows
  char *save = NULL, *line = strtok_r(out, "\n", &save);
  while (line) {
    // sources/install.wim (or .esd/.swm) marks Windows install media
    if (strcasestr(line, "sources/install.wim") || strcasestr(line, "sources/install.esd") ||
        strcasestr(line, "sources/install.swm")) {
      // bsdtar prefixes ./ sometimes; match the tail explicitly
      const char *t = line + strlen(line);
      while (t > line && t[-1] != '/') t--;
      if (!strcasecmp(t, "install.wim") || !strcasecmp(t, "install.esd") ||
          !strcasecmp(t, "install.swm"))
        return 1;
    }
    line = strtok_r(NULL, "\n", &save);
  }
  return 0;
}

// UEFI:NTFS boot loader, fetched from upstream (pbatard/uefi-ntfs)
// and cached. The in-tree res/uefi/uefi-ntfs.img is only a directory
// skeleton, so it is never used as the payload.
#define UEFI_NTFS_URL "https://github.com/pbatard/uefi-ntfs/releases/latest/download/bootx64_signed.efi"

static int cache_dir(char *out, unsigned long cap) {
  const char *base = getenv("XDG_CACHE_HOME");
  if (base && base[0]) snprintf(out, cap, "%s/rufux", base);
  else {
    const char *home = getenv("HOME");
    if (!home || !home[0]) home = "/tmp";
    snprintf(out, cap, "%s/.cache/rufux", home);
  }
  char cmd[1152];
  snprintf(cmd, sizeof cmd, "%s", out);
  // mkdir -p equivalent
  char tmp[1152];
  snprintf(tmp, sizeof tmp, "%s", out);
  for (char *c = tmp + 1; *c; c++) {
    if (*c == '/') {
      *c = 0;
      mkdir(tmp, 0755);
      *c = '/';
    }
  }
  struct stat st;
  if (mkdir(out, 0755) != 0 && errno != EEXIST) return -1;
  if (stat(out, &st) != 0) return -1;
  return 0;
}

// Path of the cached loader, downloading it on first use.
static int uefi_loader_path(char *out, unsigned long cap, char *err, unsigned long errcap) {
  char dir[1024];
  if (cache_dir(dir, sizeof dir) != 0) {
    snprintf(err, errcap, "cannot create cache dir");
    return -1;
  }
  snprintf(out, cap, "%s/bootx64_signed.efi", dir);
  struct stat st;
  if (stat(out, &st) == 0 && st.st_size > 10000) return 0; // cached
  if (!rufux_have("curl")) {
    snprintf(err, errcap, "need curl to fetch the UEFI:NTFS loader (offline?)");
    return -1;
  }
  const char *av[] = {"curl", "-sL", "--max-time", "60", "-o", out, UEFI_NTFS_URL, NULL};
  if (rufux_run(av, 0) != 0 || stat(out, &st) != 0 || st.st_size < 10000) {
    snprintf(err, errcap, "download of UEFI:NTFS loader failed (offline?)");
    return -1;
  }
  return 0;
}

int rufux_stage_uefi_ntfs(const char *tmpdir, char *err, unsigned long cap) {
  char loader[1152];
  if (uefi_loader_path(loader, sizeof loader, err, cap) != 0) return -1;
  char efi[1152], boot[1152];
  snprintf(efi, sizeof efi, "%s/esp/EFI/BOOT", tmpdir);
  snprintf(boot, sizeof boot, "%s/esp/EFI/BOOT/bootx64.efi", tmpdir);
  // mkdir -p efi
  char tmp[1152];
  snprintf(tmp, sizeof tmp, "%s", efi);
  for (char *c = tmp + 1; *c; c++) {
    if (*c == '/') {
      *c = 0;
      mkdir(tmp, 0755);
      *c = '/';
    }
  }
  if (mkdir(efi, 0755) != 0 && errno != EEXIST) {
    snprintf(err, cap, "cannot mkdir '%s'", efi);
    return -1;
  }
  // copy loader -> bootx64.efi
  FILE *in = fopen(loader, "rb");
  FILE *f = fopen(boot, "wb");
  if (!in || !f) {
    if (in) fclose(in);
    if (f) fclose(f);
    snprintf(err, cap, "cannot stage bootloader");
    return -1;
  }
  char buf[1 << 16];
  size_t n;
  unsigned long long total = 0;
  while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
    if (fwrite(buf, 1, n, f) != n) {
      fclose(in); fclose(f);
      snprintf(err, cap, "write failed staging bootloader");
      return -1;
    }
    total += n;
  }
  fclose(in);
  if (fclose(f) != 0 || total < 10000) {
    snprintf(err, cap, "staged bootloader too small");
    return -1;
  }
  return 0;
}

int rufux_write_unattend(const char *dir, const char *wue,
                         char *err, unsigned long cap) {
  int bypass = 0, nro = 0, privacy = 0;
  if (wue && !strcmp(wue, "none")) return 0; // explicitly disabled: write nothing
  if (wue && wue[0]) {
    char tmp[256];
    snprintf(tmp, sizeof tmp, ",%s,", wue);
    for (char *p = tmp; *p; p++) *p = tolower((unsigned char)*p);
    bypass = !!strstr(tmp, ",bypass,") || !!strstr(tmp, ",all,");
    nro = !!strstr(tmp, ",nro,") || !!strstr(tmp, ",all,");
    privacy = !!strstr(tmp, ",privacy,") || !!strstr(tmp, ",all,");
    if (!bypass && !nro && !privacy) {
      snprintf(err, cap, "unknown --wue item (bypass,nro,privacy,all)");
      return -1;
    }
  } else {
    bypass = 1; // default: hardware requirement bypasses only
  }
  char path[1152];
  snprintf(path, sizeof path, "%s/autounattend.xml", dir);
  FILE *f = fopen(path, "w");
  if (!f) { snprintf(err, cap, "cannot write '%s': %s", path, strerror(errno)); return -1; }
  fprintf(f,
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
      "<!-- Generated by Rufux (Windows User Experience). -->\n"
      "<unattend xmlns=\"urn:schemas-microsoft-com:unattend\">\n"
      "  <settings pass=\"windowsPE\">\n"
      "    <component name=\"Microsoft-Windows-Setup\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\">\n");
  if (bypass) {
    fprintf(f,
      "      <LabConfig>\n"
      "        <BypassTPMCheck>true</BypassTPMCheck>\n"
      "        <BypassSecureBootCheck>true</BypassSecureBootCheck>\n"
      "        <BypassRAMCheck>true</BypassRAMCheck>\n"
      "        <BypassCPUCheck>true</BypassCPUCheck>\n"
      "        <BypassStorageCheck>true</BypassStorageCheck>\n"
      "      </LabConfig>\n");
  }
  fprintf(f,
      "    </component>\n"
      "  </settings>\n");
  if (nro) {
    fprintf(f,
      "  <settings pass=\"specialize\">\n"
      "    <component name=\"Microsoft-Windows-Deployment\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\">\n"
      "      <RunSynchronous>\n"
      "        <RunSynchronousCommand wcm:action=\"add\">\n"
      "          <Order>1</Order>\n"
      "          <Path>reg add HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OOBE /v BypassNRO /t REG_DWORD /d 1 /f</Path>\n"
      "        </RunSynchronousCommand>\n"
      "      </RunSynchronous>\n"
      "    </component>\n"
      "  </settings>\n");
  }
  if (privacy) {
    fprintf(f,
      "  <settings pass=\"oobeSystem\">\n"
      "    <component name=\"Microsoft-Windows-Shell-Setup\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\">\n"
      "      <OOBE>\n"
      "        <HideEULAPage>true</HideEULAPage>\n"
      "        <HideWirelessSetupInOOBE>true</HideWirelessSetupInOOBE>\n"
      "        <ProtectYourPC>3</ProtectYourPC>\n"
      "      </OOBE>\n"
      "    </component>\n"
      "  </settings>\n");
  }
  fprintf(f, "</unattend>\n");
  if (fclose(f) != 0) { snprintf(err, cap, "cannot close '%s'", path); return -1; }
  return 0;
}

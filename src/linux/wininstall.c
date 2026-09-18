#define _GNU_SOURCE
#include "wininstall.h"
#include "iso_probe.h"
#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

int rufux_is_windows_iso(const char *iso, char *err, unsigned long cap) {
  struct stat st;
  if (stat(iso, &st) != 0) { snprintf(err, cap, "source '%s' missing", iso); return -1; }
  // UDF images defeat bsdtar's listing the same way they defeat its
  // extraction: list those with 7z instead.
  if (rufux_iso_is_udf(iso) > 0) {
    if (!rufux_have("7z")) { snprintf(err, cap, "need 7z to inspect UDF image"); return -1; }
    const char *av[] = {"7z", "l", "-ba", iso, NULL};
    char out[65536] = {0};
    if (rufux_capture(av, out, sizeof out) != 0) return 0;
    char *save = NULL, *line = strtok_r(out, "\n", &save);
    while (line) {
      while (*line == ' ' || *line == '\t') line++;
      // trim trailing whitespace/CR (7z emits \r\n)
      char *e = line + strlen(line);
      while (e > line && (e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
      const char *t = line + strlen(line);
      while (t > line && t[-1] != '/' && t[-1] != '\\') t--;
      if (!strcasecmp(t, "install.wim") || !strcasecmp(t, "install.esd") ||
          !strcasecmp(t, "install.swm"))
        return 1;
      line = strtok_r(NULL, "\n", &save);
    }
    return 0;
  }
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

// UEFI:NTFS ESP payload: the full EFI tree (EFI/Boot/* loaders plus
// EFI/Rufus/ntfs_*.efi and exfat_*.efi drivers) from res/uefi/uefi-ntfs.img.
// The loader alone is not enough: it looks for \EFI\Rufus\ntfs_<arch>.efi
// beside itself and aborts with "couldn't find/load NTFS driver" when the
// driver file is missing, so the whole tree must be staged, like Rufus.
// The image ships in-tree (and installed under share/rufux); the upstream
// download is only a fallback when no local copy resolves.
#define UEFI_NTFS_IMG_URL "https://raw.githubusercontent.com/pbatard/rufus/master/res/uefi/uefi-ntfs.img"
#define UEFI_NTFS_IMG_SIZE 1048576UL

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

// mkdir -p helper (shared by the staging below).
static int mkdir_p(const char *path) {
  char tmp[1152];
  snprintf(tmp, sizeof tmp, "%s", path);
  for (char *c = tmp + 1; *c; c++) {
    if (*c == '/') {
      *c = 0;
      mkdir(tmp, 0755);
      *c = '/';
    }
  }
  if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
  return 0;
}

// Resolve the local UEFI:NTFS image, mirroring the FreeDOS payload lookup
// (RUFUX_RES override, build tree, installed shares, exe-relative).
static const char *uefi_img_local(void) {
  static char path[1152];
  const char *env = getenv("RUFUX_RES");
  if (env && env[0]) {
    snprintf(path, sizeof path, "%s/uefi/uefi-ntfs.img", env);
    struct stat st;
    if (stat(path, &st) == 0 && st.st_size > 100000) return path;
  }
  static char exedir[1024] = {0};
  if (!exedir[0]) {
    ssize_t n = readlink("/proc/self/exe", exedir, sizeof exedir - 1);
    if (n > 0) {
      exedir[n] = 0;
      char *slash = strrchr(exedir, '/');
      if (slash) *slash = 0;
    }
  }
  static const char *cands[] = {
    "res/uefi/uefi-ntfs.img", // build tree
    "/usr/share/rufux/uefi-ntfs.img",
    "/usr/local/share/rufux/uefi-ntfs.img",
    NULL,
  };
  char probe[1152];
  struct stat st;
  if (exedir[0]) {
    snprintf(probe, sizeof probe, "%s/../share/rufux/uefi-ntfs.img", exedir);
    if (stat(probe, &st) == 0 && st.st_size > 100000) {
      snprintf(path, sizeof path, "%s", probe);
      return path;
    }
  }
  for (int i = 0; cands[i]; i++) {
    if (stat(cands[i], &st) == 0 && st.st_size > 100000) {
      snprintf(path, sizeof path, "%s", cands[i]);
      return path;
    }
  }
  return NULL;
}

// Path of the UEFI:NTFS image: local copy first, upstream download cached
// as a fallback when nothing resolves (offline then fails loudly).
static int uefi_img_path(char *out, unsigned long cap, char *err, unsigned long errcap) {
  const char *local = uefi_img_local();
  if (local) { snprintf(out, cap, "%s", local); return 0; }
  char dir[1024];
  if (cache_dir(dir, sizeof dir) != 0) {
    snprintf(err, errcap, "cannot create cache dir");
    return -1;
  }
  snprintf(out, cap, "%s/uefi-ntfs.img", dir);
  struct stat st;
  if (stat(out, &st) == 0 && (unsigned long)st.st_size >= UEFI_NTFS_IMG_SIZE) return 0; // cached
  if (!rufux_have("curl")) {
    snprintf(err, errcap, "UEFI:NTFS image not found locally and no curl to fetch it (offline?)");
    return -1;
  }
  const char *av[] = {"curl", "-sL", "--max-time", "60", "-o", out, UEFI_NTFS_IMG_URL, NULL};
  if (rufux_run(av, 0) != 0 || stat(out, &st) != 0 ||
      (unsigned long)st.st_size < UEFI_NTFS_IMG_SIZE) {
    snprintf(err, errcap, "download of UEFI:NTFS image failed (offline?)");
    return -1;
  }
  return 0;
}

int rufux_stage_uefi_ntfs(const char *tmpdir, char *err, unsigned long cap) {
  char img[1152];
  if (uefi_img_path(img, sizeof img, err, cap) != 0) return -1;
  if (!rufux_have("7z")) {
    snprintf(err, cap, "need 7z to unpack the UEFI:NTFS image");
    return -1;
  }
  char esp[1152], bootd[1152], rufusd[1152], xtr[1152];
  snprintf(esp, sizeof esp, "%s/esp", tmpdir);
  snprintf(bootd, sizeof bootd, "%s/esp/EFI/BOOT", tmpdir);
  snprintf(rufusd, sizeof rufusd, "%s/esp/EFI/Rufus", tmpdir);
  snprintf(xtr, sizeof xtr, "%s/.uefintfs", tmpdir);
  if (mkdir_p(bootd) != 0 || mkdir_p(rufusd) != 0) {
    snprintf(err, cap, "cannot mkdir ESP staging dirs");
    return -1;
  }
  if (mkdir_p(xtr) != 0) { snprintf(err, cap, "cannot mkdir extract dir"); return -1; }
  // The image is a raw FAT filesystem; 7z reads it, bsdtar does not.
  char out[1250];
  snprintf(out, sizeof out, "-o%s", xtr);
  const char *av[] = {"7z", "x", "-y", img, out, NULL};
  if (rufux_run(av, 0) != 0) {
    snprintf(err, cap, "cannot extract UEFI:NTFS image");
    return -1;
  }
  // Copy the EFI tree, normalizing the loader dir to the BOOT casing the
  // rest of the code validates (FAT itself is case-insensitive).
  char s1[1250], t1[1250], s2[1250], t2[1250];
  snprintf(s1, sizeof s1, "%s/EFI/Boot/.", xtr);
  snprintf(t1, sizeof t1, "%s", bootd);
  snprintf(s2, sizeof s2, "%s/EFI/Rufus/.", xtr);
  snprintf(t2, sizeof t2, "%s", rufusd);
  const char *cp1[] = {"cp", "-a", s1, t1, NULL};
  const char *cp2[] = {"cp", "-a", s2, t2, NULL};
  if (rufux_run(cp1, 0) != 0 || rufux_run(cp2, 0) != 0) {
    snprintf(err, cap, "cannot stage UEFI:NTFS tree");
    return -1;
  }
  char rm[1250];
  snprintf(rm, sizeof rm, "%s", xtr);
  const char *rmv[] = {"rm", "-rf", rm, NULL};
  rufux_run(rmv, 0); // best effort cleanup
  // Both halves must have landed: the loader the firmware runs and the
  // NTFS driver it refuses to boot without.
  char boot[1152], drv[1152];
  snprintf(boot, sizeof boot, "%s/esp/EFI/BOOT/bootx64.efi", tmpdir);
  snprintf(drv, sizeof drv, "%s/esp/EFI/Rufus/ntfs_x64.efi", tmpdir);
  struct stat bst, dst;
  if (stat(boot, &bst) != 0 || bst.st_size < 10000) {
    snprintf(err, cap, "staged bootloader missing or too small");
    return -1;
  }
  if (stat(drv, &dst) != 0 || dst.st_size < 10000) {
    snprintf(err, cap, "staged NTFS driver missing (EFI/Rufus/ntfs_x64.efi)");
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
  // Shape mirrors upstream Rufus wue.c: components carry the wcm/xsi
  // namespaces, list items use wcm:action="add" (without it Setup
  // silently ignores them), HW bypasses run as reg commands, and the
  // WinPE pass carries an empty product key so Setup never prompts.
  static const char *ns =
    " xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\""
    " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"";
  fprintf(f,
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
      "<!-- Generated by Rufux (Windows User Experience). -->\n"
      "<unattend xmlns=\"urn:schemas-microsoft-com:unattend\">\n"
      "  <settings pass=\"windowsPE\">\n"
      "    <component name=\"Microsoft-Windows-Setup\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\"%s>\n"
      "      <UserData>\n"
      "        <AcceptEula>true</AcceptEula>\n"
      "        <ProductKey>\n"
      "          <Key />\n"
      "        </ProductKey>\n"
      "      </UserData>\n", ns);
  if (bypass) {
    static const char *keys[] = {
      "BypassTPMCheck", "BypassSecureBootCheck", "BypassRAMCheck",
      "BypassCPUCheck", "BypassStorageCheck", NULL };
    fprintf(f, "      <RunSynchronous>\n");
    for (int i = 0; keys[i]; i++) {
      fprintf(f,
        "        <RunSynchronousCommand wcm:action=\"add\">\n"
        "          <Order>%d</Order>\n"
        "          <Path>reg add HKLM\\SYSTEM\\Setup\\LabConfig /v %s /t REG_DWORD /d 1 /f</Path>\n"
        "        </RunSynchronousCommand>\n", i + 1, keys[i]);
    }
    fprintf(f, "      </RunSynchronous>\n");
  }
  fprintf(f,
      "    </component>\n"
      "  </settings>\n");
  if (nro) {
    fprintf(f,
      "  <settings pass=\"specialize\">\n"
      "    <component name=\"Microsoft-Windows-Deployment\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\"%s>\n"
      "      <RunSynchronous>\n"
      "        <RunSynchronousCommand wcm:action=\"add\">\n"
      "          <Order>1</Order>\n"
      "          <Path>reg add HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OOBE /v BypassNRO /t REG_DWORD /d 1 /f</Path>\n"
      "        </RunSynchronousCommand>\n"
      "      </RunSynchronous>\n"
      "    </component>\n"
      "  </settings>\n", ns);
  }
  if (privacy) {
    fprintf(f,
      "  <settings pass=\"oobeSystem\">\n"
      "    <component name=\"Microsoft-Windows-Shell-Setup\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\"%s>\n"
      "      <OOBE>\n"
      "        <HideEULAPage>true</HideEULAPage>\n"
      "        <HideWirelessSetupInOOBE>true</HideWirelessSetupInOOBE>\n"
      "        <ProtectYourPC>3</ProtectYourPC>\n"
      "      </OOBE>\n"
      "    </component>\n"
      "    <component name=\"Microsoft-Windows-SecureStartup-FilterDriver\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\"%s>\n"
      "      <PreventDeviceEncryption>true</PreventDeviceEncryption>\n"
      "    </component>\n"
      "    <component name=\"Microsoft-Windows-EnhancedStorage-Adm\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\"%s>\n"
      "      <TCGSecurityActivationDisabled>1</TCGSecurityActivationDisabled>\n"
      "    </component>\n"
      "  </settings>\n", ns, ns, ns);
  }
  fprintf(f, "</unattend>\n");
  if (fclose(f) != 0) { snprintf(err, cap, "cannot close '%s'", path); return -1; }
  return 0;
}

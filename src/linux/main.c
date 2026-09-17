// Lufus entry — Phase 1 CLI. No windows.h by design.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "linux/device.h"
#include "linux/iso_probe.h"
#include "linux/checksum.h"
#include "linux/writer.h"
#include "gui/gui_gtk.h"

static void usage(const char *p) {
  printf("Lufus 0.2.0 (Phase 1: Safe Core)\n"
         "Usage:\n"
         "  %s list [--json] [--allow-fixed]\n"
         "  %s probe <file.iso> [--detail]\n"
         "  %s checksum <file>\n"
         "  %s write SRC DST [--dry-run] [--verify] [--allow-fixed] [--allow-file] [--yes]\n"
         "  %s --gui\n",
         p, p, p, p, p);
}

static void cli_progress(unsigned long long done, unsigned long long total, void *u) {
  (void)u;
  static int last = -1;
  int pct = total ? (int)(done * 100 / total) : 100;
  if (pct != last && (pct % 5 == 0 || done == total)) {
    fprintf(stderr, "\r%3d%%  %llu/%llu MB", pct, done >> 20, total >> 20);
    if (done == total) fprintf(stderr, "\n");
    last = pct;
  }
}

int main(int argc, char **argv) {
  if (argc >= 2 && (!strcmp(argv[1], "--gui") || !strcmp(argv[1], "gui")))
    return lufus_gui_run(argc, argv);

  if (argc >= 2 && !strcmp(argv[1], "list")) {
    int json = 0, allow = 0;
    for (int i = 2; i < argc; i++) {
      if (!strcmp(argv[i], "--json")) json = 1;
      else if (!strcmp(argv[i], "--allow-fixed")) allow = 1;
    }
    LufusDevice devs[128];
    int n = lufus_list_devices(devs, 128, allow);
    if (n < 0) { fprintf(stderr, "cannot scan /sys/block\n"); return 1; }
    if (json) lufus_print_devices_json(devs, n);
    else lufus_print_devices(devs, n);
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "probe")) {
    int detail = 0;
    for (int i = 3; i < argc; i++)
      if (!strcmp(argv[i], "--detail")) detail = 1;
    if (!detail) {
      char label[64] = {0};
      int rc = lufus_probe_iso(argv[2], label, sizeof label);
      printf("rc=%d label='%s'\n", rc, rc == 0 ? label : "?");
      return rc == 0 ? 0 : 2;
    }
    LufusIsoInfo info;
    if (lufus_probe_iso_detail(argv[2], &info) != 0) {
      fprintf(stderr, "cannot probe '%s'\n", argv[2]);
      return 2;
    }
    lufus_print_iso_detail(argv[2], &info);
    return info.valid_iso ? 0 : 2;
  }
  if (argc >= 3 && !strcmp(argv[1], "checksum")) {
    unsigned char sum[32];
    char err[256] = {0};
    if (lufus_sha256_file(argv[2], sum, cli_progress, NULL, err, sizeof err) != 0) {
      fprintf(stderr, "checksum failed: %s\n", err);
      return 2;
    }
    char hex[65];
    lufus_hex32(sum, hex);
    printf("%s  %s\n", hex, argv[2]);
    return 0;
  }
  if (argc >= 4 && !strcmp(argv[1], "write")) {
    LufusWriteOpts o = {0};
    o.dry_run = 1; // default safe
    for (int i = 4; i < argc; i++) {
      if (!strcmp(argv[i], "--dry-run")) o.dry_run = 1;
      else if (!strcmp(argv[i], "--real")) o.dry_run = 0;
      else if (!strcmp(argv[i], "--verify")) o.verify = 1;
      else if (!strcmp(argv[i], "--allow-fixed")) o.allow_fixed = 1;
      else if (!strcmp(argv[i], "--allow-file")) o.allow_file = 1;
      else if (!strcmp(argv[i], "--yes")) o.yes = 1;
    }
    // --real without --yes is rejected inside writer; --dry-run never needs --yes
    char err[512] = {0};
    int rc = lufus_write_image(argv[2], argv[3], &o, cli_progress, NULL,
                               err, sizeof err);
    if (rc != 0) {
      fprintf(stderr, "write failed: %s\n", err[0] ? err : "unknown");
      return 3;
    }
    printf("%s OK: %s -> %s%s\n", o.dry_run ? "dry-run" : "write",
           argv[2], argv[3], o.verify ? " (verified)" : "");
    return 0;
  }
  usage(argv[0]);
  return 0;
}

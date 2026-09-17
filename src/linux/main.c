// Lufus entry point — Linux-native (replaces src/rufus.c WinMain).
// CLI first, --gui opens GTK window. No windows.h here by design.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "linux/device.h"
#include "linux/iso_probe.h"
#include "gui/gui_gtk.h"

static void usage(const char *p) {
  printf("Lufus 0.1.0 — Linux USB creator (Rufus port scaffold)\n"
         "Usage:\n"
         "  %s list [--allow-fixed]   list removable block devices\n"
         "  %s probe <file.iso>       probe ISO label\n"
         "  %s --gui                 open GTK GUI\n", p, p, p);
}

int main(int argc, char **argv) {
  if (argc >= 2 && (!strcmp(argv[1], "--gui") || !strcmp(argv[1], "gui")))
    return lufus_gui_run(argc, argv);
  if (argc >= 2 && !strcmp(argv[1], "list")) {
    int allow = (argc >= 3 && !strcmp(argv[2], "--allow-fixed"));
    LufusDevice devs[128];
    int n = lufus_list_devices(devs, 128, allow);
    if (n < 0) { fprintf(stderr, "cannot scan /sys/block\n"); return 1; }
    lufus_print_devices(devs, n);
    return 0;
  }
  if (argc >= 3 && !strcmp(argv[1], "probe")) {
    char label[64] = {0};
    int rc = lufus_probe_iso(argv[2], label, sizeof label);
    printf("rc=%d label='%s'\n", rc, rc == 0 ? label : "?");
    return rc == 0 ? 0 : 2;
  }
  usage(argv[0]);
  return 0;
}

#include "priv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

int rufux_need_root_for_block(const char *path, char *err, unsigned long cap) {
  struct stat st;
  if (stat(path, &st) != 0) return 0; // let caller report missing target
  if (!S_ISBLK(st.st_mode)) return 0; // files/dirs: no escalation needed
  if (geteuid() == 0) return 0;
  snprintf(err, cap, "target '%s' is a block device: re-run as root "
                     "(sudo rufux ... or pkexec rufux ...)", path);
  return -1;
}

// Display-session vars the root GUI needs to reach the user's screen.
static const char *keep_env[] = {
  "DISPLAY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "XAUTHORITY",
  "XDG_CURRENT_DESKTOP", "XDG_SESSION_TYPE", "DBUS_SESSION_BUS_ADDRESS",
  NULL,
};

void rufux_escalate_gui(int argc, char **argv) {
  if (geteuid() == 0) return;
  if (getenv("RUFUX_NO_ESCALATE")) return;
  // Re-exec: pkexec env VAR=val ... /proc/self/exe <orig args>
  char *exec_argv[64];
  int n = 0;
  exec_argv[n++] = "pkexec";
  exec_argv[n++] = "env";
  for (int i = 0; keep_env[i] && n < 40; i++) {
    const char *v = getenv(keep_env[i]);
    if (!v || !v[0]) continue;
    char *kv = malloc(strlen(keep_env[i]) + strlen(v) + 2);
    if (!kv) continue;
    sprintf(kv, "%s=%s", keep_env[i], v);
    exec_argv[n++] = kv;
  }
  exec_argv[n++] = "/proc/self/exe";
  for (int i = 1; i < argc && n < 63; i++) exec_argv[n++] = argv[i];
  exec_argv[n] = NULL;
  execvp("pkexec", exec_argv);
  // Only reached if pkexec is missing/broken: refuse, don't limp along.
  fprintf(stderr, "rufux: GUI needs root (block-device access) but pkexec failed: %s\n"
                  "Run instead: sudo rufux --gui\n", strerror(errno));
  exit(77);
}

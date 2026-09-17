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
  // Resolve OUR executable now: passing "/proc/self/exe" through
  // pkexec+env would resolve it in env's process (i.e. env itself,
  // which then just prints the environment and exits --empty window).
  char exe[1024] = {0};
  ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
  const char *self = (n > 0) ? exe : argv[0];
  // Re-exec: pkexec env VAR=val ... <self> <orig args>
  char *exec_argv[64];
  int k = 0;
  exec_argv[k++] = "pkexec";
  exec_argv[k++] = "env";
  for (int i = 0; keep_env[i] && k < 40; i++) {
    const char *v = getenv(keep_env[i]);
    if (!v || !v[0]) continue;
    char *kv = malloc(strlen(keep_env[i]) + strlen(v) + 2);
    if (!kv) continue;
    sprintf(kv, "%s=%s", keep_env[i], v);
    exec_argv[k++] = kv;
  }
  exec_argv[k++] = (char *)self;
  for (int i = 1; i < argc && k < 63; i++) exec_argv[k++] = argv[i];
  exec_argv[k] = NULL;
  execvp("pkexec", exec_argv);
  // Only reached if pkexec is missing/broken: refuse, don't limp along.
  fprintf(stderr, "rufux: GUI needs root (block-device access) but pkexec failed: %s\n"
                  "Run instead: sudo rufux --gui\n", strerror(errno));
  exit(77);
}

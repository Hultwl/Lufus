#ifndef RUFUX_PRIV_H
#define RUFUX_PRIV_H
// Privilege guard: block-device mutation needs root (sudo/pkexec).
// Returns 0 if allowed (regular file or root), -1 with err otherwise.
int rufux_need_root_for_block(const char *path, char *err, unsigned long cap);
// GUI entry: re-exec self as root via pkexec (preserving display env).
// Returns only on failure (prints why and exits); test hook:
// RUFUX_NO_ESCALATE=1 skips escalation entirely.
void rufux_escalate_gui(int argc, char **argv);
#endif

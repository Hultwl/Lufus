#ifndef RUFUX_PRIV_H
#define RUFUX_PRIV_H
// Privilege guard for the CLI: block-device mutation needs root.
// (The GUI runs unprivileged and escalates per-operation by spawning
// pkexec <self> create ... in on_start.)
// Returns 0 if allowed (regular file or root), -1 with err otherwise.
int rufux_need_root_for_block(const char *path, char *err, unsigned long cap);
#endif

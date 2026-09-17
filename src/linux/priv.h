#ifndef RUFUX_PRIV_H
#define RUFUX_PRIV_H
// Privilege guard: block-device mutation needs root (sudo/pkexec).
// Returns 0 if allowed (regular file or root), -1 with err otherwise.
int rufux_need_root_for_block(const char *path, char *err, unsigned long cap);
#endif

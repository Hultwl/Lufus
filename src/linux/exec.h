#ifndef RUFUX_EXEC_H
#define RUFUX_EXEC_H
// Run external helpers (sfdisk, mkfs.*, bsdtar...). dry_run=1 prints only.
int rufux_run(const char *const argv[], int dry_run);
int rufux_have(const char *bin); // 1 if executable in PATH
#endif

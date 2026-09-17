#ifndef LUFUS_EXEC_H
#define LUFUS_EXEC_H
// Run external helpers (sfdisk, mkfs.*, bsdtar...). dry_run=1 prints only.
int lufus_run(const char *const argv[], int dry_run);
int lufus_have(const char *bin); // 1 if executable in PATH
#endif

#include "priv.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

int lufus_need_root_for_block(const char *path, char *err, unsigned long cap) {
  struct stat st;
  if (stat(path, &st) != 0) return 0; // let caller report missing target
  if (!S_ISBLK(st.st_mode)) return 0; // files/dirs: no escalation needed
  if (geteuid() == 0) return 0;
  snprintf(err, cap, "target '%s' is a block device: re-run as root "
                     "(sudo lufus ... or pkexec lufus ...)", path);
  return -1;
}

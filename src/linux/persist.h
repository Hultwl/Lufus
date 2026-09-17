#ifndef RUFUX_PERSIST_H
#define RUFUX_PERSIST_H
// casper-rw / persistence file creation.
int rufux_create_persist(const char *dir, const char *label, unsigned long size_mb,
                         int dry_run, char *err, unsigned long cap);
#endif

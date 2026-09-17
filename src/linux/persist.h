#ifndef LUFUS_PERSIST_H
#define LUFUS_PERSIST_H
// casper-rw / persistence file creation.
int lufus_create_persist(const char *dir, const char *label, unsigned long size_mb,
                         int dry_run, char *err, unsigned long cap);
#endif

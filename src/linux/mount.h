#ifndef LUFUS_MOUNT_H
#define LUFUS_MOUNT_H
// udisks2 auto-mount via udisksctl (Phase 3). dry_run=1 prints only.
int lufus_mount(const char *dev, int dry_run, char *mnt_out, unsigned long cap,
                char *err, unsigned long errcap);
int lufus_unmount(const char *dev, int dry_run, char *err, unsigned long errcap);
// First partition node for a whole disk: /dev/sda->/dev/sda1, /dev/nvme0n1->p1.
void lufus_part1(const char *disk, char *out, unsigned long cap);
#endif

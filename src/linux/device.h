#ifndef LUFUS_DEVICE_H
#define LUFUS_DEVICE_H
// Phase 1: Linux device scan (replaces src/dev.c SetupDi path).

typedef struct {
  char sysname[64];   // e.g. sdb
  char devnode[128];  // e.g. /dev/sdb
  char vendor[128];
  char model[128];
  char serial[128];
  char transport[32]; // usb|mmc|sata|nvme|virt|unknown
  unsigned long long size_bytes;
  int removable;
  int is_usb;
  int mounted;        // 1 if any partition of this disk is mounted
} LufusDevice;

int lufus_list_devices(LufusDevice *out, int max, int include_fixed);
void lufus_print_devices(const LufusDevice *devs, int n);
void lufus_print_devices_json(const LufusDevice *devs, int n);
// Safety: 0 = ok to target. allow_file permits regular files (tests).
// err explains refusal (fixed disk, mounted, missing...).
int lufus_check_target(const char *path, int allow_fixed, int allow_file,
                       char *err, unsigned long cap);
void lufus_human_size(unsigned long long bytes, char *out, unsigned long cap);
#endif

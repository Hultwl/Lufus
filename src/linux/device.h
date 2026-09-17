#ifndef LUFUS_DEVICE_H
#define LUFUS_DEVICE_H
// Linux-native device enumeration (replaces src/dev.c SetupDi path).
// Uses sysfs + libudev when available, falls back to /sys/block scan.

typedef struct {
  char sysname[64];   // e.g. sdb
  char devnode[128];  // e.g. /dev/sdb
  char vendor[128];
  char model[128];
  unsigned long long size_bytes;
  int removable;      // 1 = USB/sd-style removable
  int is_usb;
} LufusDevice;

int lufus_list_devices(LufusDevice *out, int max, int include_fixed);
void lufus_print_devices(const LufusDevice *devs, int n);
#endif

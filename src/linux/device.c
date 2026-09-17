#define _GNU_SOURCE
#include "device.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

static int read_first_line(const char *path, char *out, size_t cap) {
  FILE *f = fopen(path, "r");
  if (!f) return -1;
  if (!fgets(out, (int)cap, f)) { fclose(f); return -1; }
  fclose(f);
  out[strcspn(out, "\r\n")] = 0;
  return 0;
}

static unsigned long long dev_size_bytes(const char *devnode) {
  int fd = open(devnode, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) return 0;
  unsigned long long bytes = 0;
  if (ioctl(fd, BLKGETSIZE64, &bytes) != 0) bytes = 0;
  close(fd);
  return bytes;
}

int lufus_list_devices(LufusDevice *out, int max, int include_fixed) {
  DIR *d = opendir("/sys/block");
  if (!d) return -1;
  struct dirent *e;
  int n = 0;
  while ((e = readdir(d)) && n < max) {
    if (e->d_name[0] == '.') continue;
    // skip loop/ram/dm/zram by default
    if (!strncmp(e->d_name, "loop", 4) || !strncmp(e->d_name, "ram", 3) ||
        !strncmp(e->d_name, "dm-", 3) || !strncmp(e->d_name, "zram", 4))
      continue;
    char p[256], buf[128] = {0};
    snprintf(p, sizeof p, "/sys/block/%s/removable", e->d_name);
    if (read_first_line(p, buf, sizeof buf) != 0) continue;
    int removable = (buf[0] == '1');
    if (!removable && !include_fixed) continue;

    LufusDevice *dev = &out[n];
    memset(dev, 0, sizeof *dev);
    snprintf(dev->sysname, sizeof dev->sysname, "%s", e->d_name);
    snprintf(dev->devnode, sizeof dev->devnode, "/dev/%s", e->d_name);
    snprintf(p, sizeof p, "/sys/block/%s/device/vendor", e->d_name);
    read_first_line(p, dev->vendor, sizeof dev->vendor);
    snprintf(p, sizeof p, "/sys/block/%s/device/model", e->d_name);
    read_first_line(p, dev->model, sizeof dev->model);
    dev->removable = removable;
    // crude USB check: driver symlink contains "usb"
    char link[256], target[256];
    snprintf(link, sizeof link, "/sys/block/%s/device", e->d_name);
    ssize_t l = readlink(link, target, sizeof target - 1);
    if (l > 0) {
      target[l] = 0;
      dev->is_usb = (strstr(target, "usb") != NULL);
    }
    dev->size_bytes = dev_size_bytes(dev->devnode);
    n++;
  }
  closedir(d);
  return n;
}

void lufus_print_devices(const LufusDevice *devs, int n) {
  printf("%-10s %-12s %8s  %s %s\n", "NODE", "SYS", "SIZE_GB", "USB", "MODEL");
  for (int i = 0; i < n; i++)
    printf("%-10s %-12s %8.2f  %s  %s %s\n", devs[i].devnode, devs[i].sysname,
           devs[i].size_bytes / 1073741824.0, devs[i].is_usb ? "yes" : "no",
           devs[i].vendor, devs[i].model);
}

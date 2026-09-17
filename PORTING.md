# Lufus Porting Plan — Rufus (Windows) → Lufus (Linux)

Upstream: pbatard/rufus @ 2ea79910, ~46k LOC C (gnu11), Win10+ Win32, GPLv3.

## What stays (portable, reuse as-is)

- `src/iso.c` + `src/libcdio/` (ISO9660/UDF), `src/bled/` (zstd/xz/gz), `src/wimlib/` minus `win32_*.c`, `src/hash.c`, `src/parser.c`, `src/xml.c`, `src/cregex*.c`
- `src/ms-sys/` (MBR/PBR writers), `src/format_fat32.c`, `src/format_ext.c` + `src/ext2fs/` minus `nt_io.c`
- Types: `mbr_types.h`, `gpt_types.h`, `efi.h`
- Payloads: `res/grub*/`, `res/syslinux/*.sys|*.bss|*.c32`, `res/uefi/uefi-ntfs.img`, `res/mbr/*.S`, `res/dbx/*`, `res/freedos/*`, `res/loc/*`

## What must be rewritten (Windows-coupled)

| Rufus file | Windows API | Linux replacement |
|---|---|---|
| `src/dev.c` | SetupDi + CfgMgr + `IOCTL_USB_*` | libudev + udisks2 + sysfs + usbfs reset |
| `src/drive.c` | `IOCTL_DISK_*_LAYOUT_EX`, MountMgr, VDS | libfdisk / parted, `BLKRRPART`, udisks2 mount |
| `src/format.c` | `fmifs!FormatEx`, `IVdsVolumeMF3_FormatEx2` | fork `mkfs.vfat/mkfs.ntfs/mkfs.exfat/mkfs.udf/mke2fs` |
| `src/winio.h`, `src/stdio.c` | `\\.\PhysicalDriveN`, `FSCTL_LOCK/DISMOUNT`, overlapped | `open(O_DIRECT\|O_EXCL)` + `flock` + `BLKGETSIZE64` |
| `src/rufus.c`, `src/ui.c`, `src/stdlg.c`, `rufus.rc`, `darkmode.c` | Win32 dialog/progress/statusbar | GTK4 + libadwaita (in `src/gui/`) |
| `src/stdfn.c`, `src/process.c`, `registry.h` | `SE_*` privs, ACLs, `NtQueryObject` | `geteuid` + polkit + `/proc` + `fuser` |
| `src/vhd.c` | `virtdisk.h` | `qemu-nbd` / libguestfs |
| `src/wue.c` | `bcdboot/mountvol/bcdedit` | `grub-install`, `efibootmgr`, `sbsign` |
| `src/net.c`, `src/pki.c` | `wininet`, `wincrypt/WinVerifyTrust` | `libcurl` + GnuTLS/openssl + PKCS#7 |
| `ext2fs/nt_io.c`, `syslinux/win/ntfssect.c` | Win32 I/O shims | drop, use native Linux I/O |

## Milestones

1. [x] Scaffold: CMake + `src/linux/` + `src/gui/` (this commit)
2. [ ] `lufus list` — real udev enumeration + size/transport (USB vs NVMe filter)
3. [ ] `lufus write --dry-run` — safe DD path with `O_EXCL`, progress, verify
4. [ ] Partition + mkfs dispatch (libfdisk + mkfs.*)
5. [ ] ISO extract (reuse libcdio/bled) + GRUB/syslinux install on Linux
6. [ ] GTK4 feature-parity UI (device picker, ISO picker, log, progress)
7. [ ] polkit privilege escalation, udisks2 mount, persistence, checksums
8. [ ] Drop all `windows.h` from default Linux build

## Safety rules

- Never touch a non-removable device without explicit `--allow-fixed`.
- Always `BLKRRPART` + verify + `fsync` before reporting success.

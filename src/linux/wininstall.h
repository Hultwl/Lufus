#ifndef RUFUX_WININSTALL_H
#define RUFUX_WININSTALL_H
// Windows installation media: ISO extract to NTFS + UEFI:NTFS boot files
// on the ESP + optional unattended-answer (WUE) XML. Full Windows-To-Go
// (WIM apply + BCD store) is out of scope: it needs Windows-licensed
// bits with no Linux-native path.
// 1 if the ISO looks like Windows install media (sources/install.wim|esd).
int rufux_is_windows_iso(const char *iso, char *err, unsigned long cap);
// Stage the UEFI:NTFS ESP payload (res/uefi/uefi-ntfs.img) into tmpdir
// via mtools (no mount needed). Asserts bootx64.efi lands.
int rufux_stage_uefi_ntfs(const char *tmpdir, char *err, unsigned long cap);
// Write autounattend.xml: LabConfig HW bypasses + optional NRO bypass +
// privacy screens off. `wue` is a comma list: bypass,nro,privacy (any
// subset; NULL/empty = bypass only... pass "" for bypass-only default).
int rufux_write_unattend(const char *dir, const char *wue,
                         char *err, unsigned long cap);
#endif

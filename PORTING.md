# How the port actually went

Upstream is pbatard/rufus at `2ea79910`: ~46k lines of C, Win32 only,
Windows 10 and up. I kept it in-tree for reference and wrote everything
Linux-native in `src/linux/` + `src/gui/`. No `windows.h` in our code,
ever — that was the one hard rule.

## What I kept

The portable guts: ISO9660/UDF parsing (`libcdio`), the `bled`
decompressors, `wimlib`, the checksum/parser/XML helpers, the MBR and
boot-record byte blobs from `ms-sys` (data, not code), and all the
payloads in `res/` — GRUB, syslinux binaries, the UEFI:NTFS image,
FreeDOS files, icons, upstream locales.

## What I rewrote (and with what)

No libfdisk, no libadwaita, none of the fancy stuff from the original
plan — plain tools called as subprocesses turned out to be enough and
a lot easier to debug:

| Instead of (Windows) | I used (Linux) |
|---|---|
| SetupDi device enumeration | sysfs scan + `BLKGETSIZE64`, `/sys/block/*/size` fallback so sizes work unprivileged |
| VDS + `fmifs!FormatEx` formatting | `mkfs.vfat/ntfs/exfat/ext4/udf` binaries |
| `IOCTL_DISK_*_LAYOUT_EX` partitioning | `sfdisk` scripts (explicit field syntax — bare `;` lines break older versions, learned the hard way) |
| `\\.\PhysicalDriveN` raw I/O | `open(O_DIRECT\|O_EXCL)` + `flock` + `fsync` + `BLKRRPART` |
| Win32 dialog UI | GTK4 (plain, no libadwaita), portal-native file pickers |
| Registry + services + `bcdboot` | dotfiles nowhere — polkit/`sudo`, udisks2 mounts, `grub-install` thinking (not yet needed) |
| `wininet` downloads | `curl` subprocess for update checks |
| VDS locking, `SE_*` privileges | `flock`, `geteuid` checks, pkexec-per-operation from the GUI |

Two rules that survived the whole project:

- Never touch a non-removable device without explicit `--allow-fixed`.
- Everything destructive dry-runs first; real runs need `--real --yes`.

## Rufus headline features: where each one stands

| Feature | Status | The real story |
|---|---|---|
| MD5 / SHA-1 / SHA-256 / SHA-512 | ✅ Done | OpenSSL EVP; `checksum --algo`, GUI `#` shows all four |
| Fixed VHD images | ✅ Done | Footer parsed and checksum-verified, payload written, footer skipped; dynamic/VHDX refused with a `qemu-img` pointer |
| Bad-blocks scan | ✅ Done | Read-only scan by default; `--write-patterns` does destructive 0xAA/0x55/0xFF/0x00 passes like Rufus |
| FreeDOS bootable USB | ✅ Done (1.2) | Real DOS boot records from ms-sys blobs via a Linux-native writer, KERNEL.SYS copied first — byte-exact tested, not just copied files |
| Windows install media | ✅ Done (1.2) | ESP + NTFS, ISO extract, UEFI:NTFS loader fetched from upstream, `autounattend.xml` with the Win11 bypasses |
| TPM/Secure-Boot bypass | ✅ Done as WUE (1.2) | Same answer-file mechanism Rufus uses; no registry hacking on our side |
| ReFS formatting | ❌ No | There is no Linux ReFS formatter — Microsoft never published one, the Linux driver is read-only. Refused with a message |
| Windows ISO downloader | ❌ No | Microsoft serves ISOs through an authenticated web flow with no sanctioned API. `rufux download-windows` tells you the manual path instead of pretending |
| Windows To Go (full OS on USB) | ⏸ Parked | Researched (`docs/wintogo-research.md`), feasible via wimlib + hivex-built BCD — but it needs a 32GB+ fast stick and multi-round boot testing I don't have lined up. See `docs/TODO.md` |
| 38-language UI | ⚠️ Partial | Our own strings are EN + FR + ES via gettext. `res/loc/` is upstream's translation set — inherited, not mine |

The pattern for the ❌ rows: I'd rather refuse with the real reason than
ship something that looks like the feature and isn't.

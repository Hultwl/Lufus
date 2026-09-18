# How Rufux got built

No master plan survived contact with real USB sticks. Roughly, though,
it went in three phases — each one ended with something I could
actually run.

## Phase 1 — Safe Core (v0.2.0) ✅

The rule from day one: nothing destructive happens by accident. Device
scanning from sysfs, ISO probing, SHA-256, and a writer that dry-runs
unless you say `--real --yes` twice, basically. Plus a GTK skeleton so
there was a window to look at.

Exit criteria, all met:

- [x] `rufux list [--json] [--allow-fixed]` — node/size/usb/vendor/model/serial/mounted
- [x] `rufux probe <iso> --detail` — label/size/bootable
- [x] `rufux checksum <file>` matches `sha256sum`
- [x] `rufux write SRC DST --dry-run` simulates, writes nothing
- [x] `rufux write SRC DST --allow-file --yes [--verify]` — refuses fixed disks and mounted targets
- [x] GTK GUI: device refresh, ISO pick + checksum, dry-run Start + progress + log
- [x] `tests/test_phase1.sh` passes

## Phase 2 — Bootable Parity (v0.3.0) ✅

Making sticks that actually boot: sfdisk partitioning, mkfs dispatch,
ISO extraction, syslinux, persistence files, bad-blocks checks, and a
`create` command tying it together. The GUI grew its mode/scheme/fs
pickers here.

## Phase 3 — Stable v1.0 ✅

The unglamorous stuff that makes it usable: udisks2 auto-mount,
privilege handling, Secure Boot status, update checks, translations,
packaging (PKGBUILD/Flatpak/man/desktop), CI, and a hardware matrix.

## After 1.0 — whatever hurt most

- **v1.0.1–1.0.2**: rename (Lufus was taken), GUI polish, themes
- **v1.0.3**: a reviewer QEMU-booted our output and found real bugs
  (stale partition tables, a stack overflow in update-check) — fixed
- **v1.1**: real progress bars, run safety, early guards
- **v1.1.1**: failures finally audible in the GUI log, worker dismounts first
- **v1.1.2**: UI stays alive during burns, system theme via portal
- **v1.1.3**: the whole reviewer deep-scan batch
- **v1.1.4**: Flathub attempt, then un-attempt (sandboxes and raw disks don't mix)
- **v1.2**: FreeDOS boots, Windows install media, WUE answers
- **v1.2.1**: MBR types + boot flags, syslinux wired in — legacy BIOS boots for real
- **v1.2.2**: a real user burned a Win11 USB and got one README — UDF
  under-extraction fixed with a loud failure, GUI form rebuilt

The pattern: someone (usually a reviewer with QEMU) finds something real,
it gets fixed with a regression test, it ships. That's the whole process.

# Hardware validation matrix (manual, Phase 3 stable)

Run automated suites first: `ctest --test-dir build` (file-backed, rootless).

For real USB sticks, use `tests/hw_smoke.sh` — it only plans by
default and needs explicit `--yes-i-understand` for destructive steps.

| # | Scenario | Command | Result |
|---|----------|---------|--------|
| H1 | List USB stick | `rufux list` shows node/size/usb | |
| H2 | Probe ISO | `rufux probe dl.iso --detail` valid+bootable | |
| H3 | Checksum | `rufux checksum dl.iso` == publisher sha256 | |
| H4 | DD write + verify | `sudo rufux write dl.iso /dev/sdX --real --verify --yes` boots | |
| H5 | Extract flow | `sudo rufux create dl.iso /dev/sdX --mode extract --scheme gpt --fs vfat --yes` boots UEFI | |
| H6 | Secure Boot | target boots with SB on; `validate-efi` OK on used bootloader | |
| H7 | Persistence | `--persist-mb 1024` creates working casper-rw | |
| H8 | Bad blocks | `rufux badblocks /dev/sdX` clean on known-good stick | |
| H9 | GUI | `rufux --gui` refresh/pick/dry-run; `--theme dark` | |

Fill in Result with device + ISO + pass/fail as you test.

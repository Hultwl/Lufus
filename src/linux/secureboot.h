#ifndef RUFUX_SECUREBOOT_H
#define RUFUX_SECUREBOOT_H
// Secure Boot status + EFI binary validation (Phase 3).
typedef enum { RUFUX_SB_ENABLED, RUFUX_SB_DISABLED, RUFUX_SB_UNKNOWN } RufuxSbState;
RufuxSbState rufux_sb_state(void);
const char *rufux_sb_string(RufuxSbState s);
// 0 = valid EFI PE binary. subsystem_out receives PE subsystem id.
int rufux_validate_efi(const char *path, unsigned *subsystem_out,
                       char *err, unsigned long cap);
#endif

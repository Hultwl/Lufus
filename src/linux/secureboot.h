#ifndef LUFUS_SECUREBOOT_H
#define LUFUS_SECUREBOOT_H
// Secure Boot status + EFI binary validation (Phase 3).
typedef enum { LUFUS_SB_ENABLED, LUFUS_SB_DISABLED, LUFUS_SB_UNKNOWN } LufusSbState;
LufusSbState lufus_sb_state(void);
const char *lufus_sb_string(LufusSbState s);
// 0 = valid EFI PE binary. subsystem_out receives PE subsystem id.
int lufus_validate_efi(const char *path, unsigned *subsystem_out,
                       char *err, unsigned long cap);
#endif

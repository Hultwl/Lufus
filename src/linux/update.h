#ifndef LUFUS_UPDATE_H
#define LUFUS_UPDATE_H
// Release check against GitHub API (needs curl binary + network).
int lufus_update_check(const char *current_version, char *latest_out,
                       unsigned long cap, char *err, unsigned long errcap);
#endif

#ifndef RUFUX_WRITER_H
#define RUFUX_WRITER_H
// Phase 1: safe image writer. Dry-run default; real writes need --yes.
// verify=1 re-reads and compares.

typedef void (*RufuxWriteProgress)(unsigned long long done,
                                   unsigned long long total, void *user);

typedef struct {
  int dry_run;
  int verify;
  int allow_fixed;
  int allow_file;
  int yes; // required for non-dry-run
  // Optional separate verify-phase reporting (else cb is reused).
  RufuxWriteProgress vprog;
  void *vuser;
} RufuxWriteOpts;

int rufux_write_image(const char *src, const char *dst,
                      const RufuxWriteOpts *opts,
                      RufuxWriteProgress cb, void *user,
                      char *err, unsigned long errcap);
#endif

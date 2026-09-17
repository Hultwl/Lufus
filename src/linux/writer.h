#ifndef LUFUS_WRITER_H
#define LUFUS_WRITER_H
// Phase 1: safe image writer. Dry-run default; real writes need --yes.
// verify=1 re-reads and compares.

typedef void (*LufusWriteProgress)(unsigned long long done,
                                   unsigned long long total, void *user);

typedef struct {
  int dry_run;
  int verify;
  int allow_fixed;
  int allow_file;
  int yes; // required for non-dry-run
} LufusWriteOpts;

int lufus_write_image(const char *src, const char *dst,
                      const LufusWriteOpts *opts,
                      LufusWriteProgress cb, void *user,
                      char *err, unsigned long errcap);
#endif

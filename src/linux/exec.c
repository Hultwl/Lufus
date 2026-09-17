#include "exec.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int lufus_have(const char *bin) {
  return access(bin, X_OK) == 0;
}

int lufus_run(const char *const argv[], int dry_run) {
  fprintf(stderr, "+");
  for (int i = 0; argv[i]; i++) fprintf(stderr, " %s", argv[i]);
  fprintf(stderr, "\n");
  if (dry_run) return 0;
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    execvp(argv[0], (char *const *)argv);
    _exit(127);
  }
  int st = 0;
  while (waitpid(p, &st, 0) < 0) {}
  return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
}

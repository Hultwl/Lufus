#include "exec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// PATH search (mirrors execvp): absolute paths checked directly,
// bare names resolved against each PATH component.
int rufux_have(const char *name) {
  if (!name || !name[0]) return 0;
  if (strchr(name, '/')) return access(name, X_OK) == 0;
  const char *path = getenv("PATH");
  if (!path || !path[0]) path = "/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin";
  char *copy = strdup(path);
  if (!copy) return 0;
  int found = 0;
  for (char *save = NULL, *dir = strtok_r(copy, ":", &save); dir;
       dir = strtok_r(NULL, ":", &save)) {
    char full[1024];
    snprintf(full, sizeof full, "%s/%s", dir, name);
    if (access(full, X_OK) == 0) { found = 1; break; }
  }
  free(copy);
  return found;
}

int rufux_run(const char *const argv[], int dry_run) {
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

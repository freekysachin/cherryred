#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHELL_PATH "/bin/shell"

static pid_t spawn_shell(void){
  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "init: fork failed: %s\n", strerror(errno));
    return -1;
  }

  if (pid == 0) {
    char *const args[] = {SHELL_PATH, NULL};
    execv(SHELL_PATH, args);
    fprintf(stderr, "init: cannot exec %s: %s\n", SHELL_PATH, strerror(errno));
    _exit(127);
  }

  return pid;
}

static pid_t ensure_shell_running(void){
  pid_t pid = spawn_shell();
  while (pid < 0) {
    sleep(1);
    pid = spawn_shell();
  }
  return pid;
}

int main(void) {
  setenv("PATH", "/bin:/sbin", 1);

  printf("init: starting as PID %d\n", getpid());
  pid_t shell_pid = ensure_shell_running();

  for (;;) {
    int status;
    pid_t died = wait(&status);

    if (died < 0) {
      if (errno == EINTR) continue;
      // No children at all - shouldn't happen since we always keep a shell running
      sleep(1);
      continue;
    }

    if (died == shell_pid) {
      printf("init: shell (pid %d) exited, respawning\n", died);
      sleep(1); // paces repeated respawns - e.g. while /bin/shell doesn't exist yet
      shell_pid = ensure_shell_running();
    }
  }

  return 0;
}

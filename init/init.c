#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

// The one thing init knows about its child: where to find it. Swapping the
// shell for something else later (a GUI launcher, say) is a one-line change
// here - init itself never needs to know what it's running.
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

// Keeps retrying (with a backoff so a persistently broken shell doesn't spin
// the CPU) until a shell is actually running, since init has nothing else to
// hand the console to.
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
      // No children at all - shouldn't happen since we always keep a shell
      // running, but avoid spinning if it ever does.
      sleep(1);
      continue;
    }

    if (died == shell_pid) {
      printf("init: shell (pid %d) exited, respawning\n", died);
      sleep(1); // paces repeated respawns - e.g. while /bin/shell doesn't exist yet
      shell_pid = ensure_shell_running();
    }

    // Any other pid is an orphan reparented to init when its own parent
    // exited first; wait() above already reaped it - that's the whole of
    // PID 1's obligation to processes it didn't start itself.
  }

  return 0;
}

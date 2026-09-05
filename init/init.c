#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <unistd.h>

static void emergency_shell(){
  fprintf(stderr, "\nemergency shell...\n");
  char *const args[] = {"/bin/sh", NULL};
  execve(args[0], args, NULL);

  fprintf(stderr, "Cannot exec emergency shell: %s\n", strerror(errno)); // if execve itself fails
  for(;;) {
    pause();
  }
}

int main() {
  // mount the our rootfs directories for initrmfs
  int procMount = mount("proc", "/proc", "proc", 0, NULL);
  if (procMount != 0) {
    fprintf(stderr, "Unable to mount /proc: %s\n", strerror(errno)); // Print the error message to stderr

    // Do not return as this is our PID 1 process and if we return, the kernel will panic and we will not be able to debug the issue
    emergency_shell();
  }

  int sysMount = mount("sysfs", "/sys", "sysfs", 0, NULL);
  if (sysMount != 0) {
    fprintf(stderr, "Unable to mount /sys: %s\n", strerror(errno));
    emergency_shell();
  }

  int devMount = mount("devtmpfs/", "/dev", "devtmpfs", 0, NULL);
  if (devMount != 0) {
    fprintf(stderr, "Unable to mount /dev: %s\n", strerror(errno));
    emergency_shell();
  }

  printf("\n==============\nMount Successful\n==============\n");

  setenv("PATH", "/bin:/sbin", 1);
  printf("bin setup Successful\n");

  // Command loop
  while (1) {
  }

  return 0;
}

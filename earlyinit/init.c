#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <unistd.h>
#include <string.h>

static void emergency_shell(){
  fprintf(stderr, "\nemergency shell...\n");
  char *const args[] = {"/bin/sh", NULL};
  execve(args[0], args, NULL);

  fprintf(stderr, "Cannot exec emergency shell: %s\n", strerror(errno)); // if execve itself fails
  for(;;) {
    pause();
  }
}

static void mount_vfs(char* src, char* dest, char* filetype, unsigned long flag){
  if (mount(src, dest, filetype, flag, NULL) != 0) {
    fprintf(stderr, "Unable to mount %s: %s\n", src, strerror(errno)); // Print the error message to stderr

    // Do not return as this is our PID 1 process and if we return, the kernel will panic and we will not be able to debug the issue
    emergency_shell();
  }
}

int main() {
  // mount the our rootfs directories for initrmfs
  mount_vfs("proc", "/proc", "proc", 0);
  mount_vfs("sysfs", "/sys", "sysfs", 0);
  mount_vfs("devtmpfs", "/dev", "devtmpfs", 0);

  printf("\n==============\nMount Successful\n==============\n");
  
  setenv("PATH", "/bin:/sbin", 1);
  printf("bin setup Successful\n");
  printf("Hello World\n");

  // Command loop
  for(;;){
    pause();
  }

  return 0;
}

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <unistd.h>

int main() {
  // mount the our rootfs directories for initrmfs
  int procMount = mount("proc", "/proc", "proc", 0, NULL);
  if (procMount != 0) {
    printf("Unable to mount proc/");

    return 0;
  }

  int sysMount = mount("sysfs", "/sys", "sysfs", 0, NULL);
  if (sysMount != 0) {
    printf("Unable to mount sys/");
  }

  int devMount = mount("devtmpfs/", "/dev", "devtmpfs", 0, NULL);
  if (devMount != 0) {
    printf("Unable to mount dev/");

    return errno;
  }

  printf("\n==============\nMount Successful\n==============\n");

  setenv("PATH", "/bin:/sbin", 1);
  printf("bin setup Successful\n");

  // Command loop
  while (1) {
  }

  return 0;
}

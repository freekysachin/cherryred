#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
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

static void mount_vfs(char* src, char* dest, char* filetype, unsigned long flag){
  if (mount(src, dest, filetype, flag, NULL) != 0) {
    fprintf(stderr, "Unable to mount %s: %s\n", src, strerror(errno)); // Print the error message to stderr

    // Do not return as this is our PID 1 process and if we return, the kernel will panic and we will not be able to debug the issue
    emergency_shell();
  }
}

static void move_mount(const char *src, const char *dest){
  if (mount(src, dest, NULL, MS_MOVE, NULL) != 0) {
    fprintf(stderr, "Unable to move mount %s to %s: %s\n", src, dest, strerror(errno));
    emergency_shell();
  }
}

/*  
  Finds "root=" on the kernel command line, e.g. "root=/dev/vda". Returns 
  NULL if it isn't there. The result points into a static buffer, so it's
  only valid until the next call. 
*/
static char *find_root_device(void){
  static char cmdline[4096];

  int fd = open("/proc/cmdline", O_RDONLY);
  if (fd < 0) return NULL;

  ssize_t n = read(fd, cmdline, sizeof(cmdline) - 1);
  close(fd);
  if (n < 0) return NULL;
  cmdline[n] = '\0';

  char *tok = strtok(cmdline, " \n");
  while (tok != NULL) {
    if (strncmp(tok, "root=", 5) == 0) {
      return tok + 5;
    }
    tok = strtok(NULL, " \n");
  }
  return NULL;
}

static void recursive_remove(const char *path, dev_t root_dev){
  DIR *dir = opendir(path);
  if (dir == NULL) return;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char child[PATH_MAX];
    snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);

    struct stat st;
    if (lstat(child, &st) != 0 || st.st_dev != root_dev) {
      continue; // not ours (or gone already) - leave it alone
    }

    if (S_ISDIR(st.st_mode)) {
      recursive_remove(child, root_dev);
      rmdir(child); // fails (harmlessly) for /mnt, which still holds /mnt/root
    } else {
      unlink(child);
    }
  }
  closedir(dir);
}

int main() {
  // mount the our rootfs directories for initrmfs
  mount_vfs("proc", "/proc", "proc", 0);
  mount_vfs("sysfs", "/sys", "sysfs", 0);
  mount_vfs("devtmpfs", "/dev", "devtmpfs", 0);

  // Giving some time for the mounts to settle before proceeding
  sleep(1);

  printf("\n==============\nMount Successful\n==============\n");

  char *root_dev = find_root_device();
  if (root_dev == NULL) {
    fprintf(stderr, "No root= on the kernel command line\n");
    emergency_shell();
  }

  printf("Mounting real root at %s\n", root_dev);
  mount_vfs(root_dev, "/mnt/root", "ext4", 0);

  // Move the already-mounted virtual filesystems across so /sbin/init
  // inherits them ready-made instead of remounting them itself.
  move_mount("/proc", "/mnt/root/proc");
  move_mount("/sys", "/mnt/root/sys");
  move_mount("/dev", "/mnt/root/dev");

  struct stat root_st;
  if (stat("/", &root_st) != 0) {
    fprintf(stderr, "Unable to stat /: %s\n", strerror(errno));
    emergency_shell();
  }
  recursive_remove("/", root_st.st_dev);

  /* 
    switch_root's actual mechanism: move the new root's mount onto "/",
    chroot into it, then exec the real init. Nothing past this point can
    safely return - if exec fails, PID 1 is gone and the kernel panics,
    hence emergency_shell rather than propagating the failure upward.
  */
  if (chdir("/mnt/root") != 0) {
    fprintf(stderr, "Unable to chdir to /mnt/root: %s\n", strerror(errno));
    emergency_shell();
  }
  move_mount(".", "/");
  if (chroot(".") != 0) {
    fprintf(stderr, "Unable to chroot: %s\n", strerror(errno));
    emergency_shell();
  }
  if (chdir("/") != 0) {
    fprintf(stderr, "Unable to chdir to /: %s\n", strerror(errno));
    emergency_shell();
  }

  printf("Handing over to /sbin/init\n");
  char *const init_args[] = {"/sbin/init", NULL};
  execv(init_args[0], init_args);

  fprintf(stderr, "Cannot exec /sbin/init: %s\n", strerror(errno));
  emergency_shell();

  return 0;
}

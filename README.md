# cherryred

A hobby operating system project — booting the real Linux kernel with a from-scratch custom userspace, built one feature at a time.

## What this is

cherryred is not a fork of an existing Linux distro, and it does not repackage Ubuntu/Debian/Arch. It compiles the actual upstream Linux kernel from kernel.org and pairs it with a completely custom userspace — starting from a single "hello world" `init` binary and growing outward into a fork/exec shell, hand-written coreutils-style tools (`touch`, `rm`, `cat`, ...), and eventually real GNU coreutils once dynamic linking is working.

It boots the way a real distro boots: in **two stages**. A tiny `initramfs` in RAM comes up first, brings up the kernel's virtual filesystems, finds the real root filesystem on disk, and then hands the machine over to it — replacing itself with the on-disk `init`. Everything runs under QEMU.

## Why

To understand, from first principles, what actually happens between "kernel loads" and "you have a shell prompt" on a real Linux system — by building each piece by hand instead of taking it for granted.

## How it boots

**earlyinit — the initramfs, in RAM.**

1. QEMU loads `build/bzImage` (the kernel), `build/initramfs.cpio.gz` (the RAM disk), and attaches `build/rootfs.img` as a virtio disk.
2. The kernel decompresses the initramfs into a RAM-backed filesystem, mounts it as `/`, opens `/dev/console`, and executes `/init` as PID 1.
3. `earlyinit` mounts `/proc`, `/sys`, and `/dev`.
4. It reads `root=` from `/proc/cmdline` and mounts that device at `/mnt/root`.
5. It `MS_MOVE`s `/proc`, `/sys`, and `/dev` across into the new root, so `init` inherits them already mounted.
6. It deletes the initramfs contents, freeing that RAM permanently.
7. It moves the new root's mount onto `/`, `chroot`s into it, and `exec`s `/sbin/init`.

**init — the real root, on disk.**

8. `/sbin/init` is now PID 1 — the `earlyinit` process no longer exists, having been replaced in place by `exec`.
9. It sets up the environment, spawns a shell, reaps orphaned children, and respawns the shell if it dies. It never exits.

### Why `switch_root` and not `pivot_root`

The initramfs is special: it *is* `rootfs`, the kernel's built-in root filesystem, and the kernel will not let you unmount it or `pivot_root` away from it — there is no parent mount to move it to. `pivot_root(2)` simply fails there.

`switch_root` is the workaround, and steps 5–7 above are what it actually does: move the new root's mount onto `/`, `chroot` into it, and reclaim the old root's memory by *deleting its files* — since the mount itself can never go away. That deletion is the only way to get the initramfs RAM back, which is why it's a step at all.

## Kernel configuration

`earlyinit` has no module loader, so **everything needed to reach the disk must be built in (`=y`, not `=m`)**:

| Option | Why |
| --- | --- |
| `CONFIG_BLK_DEV_INITRD` | unpack the initramfs at boot |
| `CONFIG_VIRTIO_BLK`, `CONFIG_VIRTIO_PCI` | see QEMU's disk as `/dev/vda` |
| `CONFIG_EXT4_FS` | read the root filesystem |
| `CONFIG_DEVTMPFS` | populate `/dev` |
| `CONFIG_PROC_FS`, `CONFIG_SYSFS` | `/proc` and `/sys` |

A missing `=y` here looks like a kernel panic with no useful message, so it's the first thing to check when the boot dies.

## Folder structure

```
cherryred/
├── earlyinit/              # initramfs PID 1 — mount, find root, switch_root
│   ├── init.c
│   └── Makefile
├── init/                   # on-disk /sbin/init — shell + process supervisor
│   ├── init.c
│   └── Makefile
├── tools/                 # hand-written userspace tools — land in /bin on the disk
│   ├── touch.c
│   ├── rm.c
│   ├── cat.c
│   └── Makefile
├── kernel/                # kernel CONFIG and patches only — not the kernel source tree
│   ├── .config
│   └── patches/
├── initramfs/             # staging tree for earlyinit — packed into the cpio
│   ├── dev/console         # real device node, made at build time — see note below
│   ├── proc/               # empty — mountpoint
│   ├── sys/                # empty — mountpoint
│   └── mnt/root/           # empty — where the real root gets mounted
├── rootfs/                # staging tree for the disk image — init's world
│   ├── sbin/               # /sbin/init lands here at build time
│   ├── bin/                # compiled tools land here at build time
│   ├── etc/
│   └── proc/  sys/  dev/   # empty — mountpoints, moved in from earlyinit
├── scripts/
│   ├── build-kernel.sh
│   ├── build-initramfs.sh
│   ├── build-rootfs-img.sh
│   └── run-qemu.sh
├── build/                 # gitignored — bzImage, initramfs.cpio.gz, rootfs.img, objects
├── .gitignore
└── README.md
```

`kernel/` holds only what's *yours* to version — config and patches — since the actual kernel source is huge and lives upstream, fetched by `build-kernel.sh` rather than committed.

`initramfs/` and `rootfs/` are both staging trees, but they are not the same thing: `initramfs/` is a tiny RAM disk that exists only to reach the real root, while `rootfs/` becomes the persistent disk image. Compiled binaries in either are build output, regenerated on every build, the same way you'd never commit a `.o` file.

> **`initramfs/dev/console` must be a real device node** (`mknod c 5 1`), created by `build-initramfs.sh`. The kernel opens `/dev/console` to give PID 1 its stdin/stdout/stderr *before* it runs `/init` — it reads that node from the initramfs itself, and `CONFIG_DEVTMPFS_MOUNT` explicitly does not apply to initramfs. Without it, `earlyinit` boots with no file descriptors and every message you print silently vanishes.

## Building & running

```bash
./scripts/build-kernel.sh       # fetches and builds the Linux kernel
./scripts/build-initramfs.sh    # builds earlyinit, packages the initramfs cpio
./scripts/build-rootfs-img.sh   # builds init + tools, formats the disk image
./scripts/run-qemu.sh           # boots kernel + initramfs + disk in QEMU
```

The disk image is built with `mke2fs -d`, which populates a filesystem image directly from a staging directory — so no `sudo`, no `losetup`, and no loop mounts anywhere in the build.

Binaries are linked with `-static`, because there is no dynamic linker in the rootfs until the final stage of the project.

Scripts are filled in incrementally as each stage is built — see Progress below.

## Progress

Built incrementally, one feature at a time, with each stage tagged in git:

- [ ] `v0.1` — skeleton `earlyinit` prints "hello world," boots as PID 1 via initramfs
- [ ] `v0.2` — `earlyinit` mounts `/proc`, `/sys`, `/dev`
- [ ] `v0.3` — **two-stage boot**: build a disk image, mount `root=`, move mounts, `switch_root`, exec `init`
- [ ] `v0.4` — `init` becomes a real shell: read input, fork/exec, reap orphans, respawn
- [ ] `v0.5` — hand-written tools: `touch`, `rm`, `cat`
- [ ] `v0.6` — swap in BusyBox for broader tool coverage
- [ ] `v0.7` — dynamic linking + real GNU coreutils

## Status

Hobby project, built for learning rather than stability. Expect things to be broken between stages.

Currently pre-`v0.1`: `earlyinit` has mount logic written, but the build scripts don't exist yet, so nothing has actually booted.

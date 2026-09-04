# cherryred

A hobby operating system project — booting the real Linux kernel with a from-scratch custom userspace, built one feature at a time.

## What this is

cherryred is not a fork of an existing Linux distro, and it does not repackage Ubuntu/Debian/Arch. It compiles the actual upstream Linux kernel from kernel.org and pairs it with a completely custom userspace — starting from a single "hello world" `init` binary and growing outward into a fork/exec shell, hand-written coreutils-style tools (`touch`, `rm`, `cat`, ...), and eventually real GNU coreutils once dynamic linking is working.

Everything boots through an **initramfs**: a compressed `cpio` archive that the kernel unpacks into RAM at boot and executes as PID 1. There's no installed disk image and no package manager — just a kernel and a ramdisk, run under QEMU.

## Why

To understand, from first principles, what actually happens between "kernel loads" and "you have a shell prompt" on a real Linux system — by building each piece by hand instead of taking it for granted.

## How it boots

1. QEMU loads `build/bzImage` (the compiled kernel) and `build/initramfs.cpio.gz` (the packaged rootfs).
2. The kernel decompresses the initramfs into a RAM-backed filesystem and mounts it as `/`.
3. The kernel executes `/init` as PID 1.
4. `/init` mounts `/proc`, `/sys`, `/dev`, then starts reading commands and `fork`/`exec`ing tools from `/bin`.

## Folder structure

```
cherryred/
├── init/                 # source for the init program (PID 1 / shell)
│   ├── init.c
│   └── Makefile
├── tools/                # source for hand-written userspace tools
│   ├── touch.c
│   ├── rm.c
│   ├── cat.c
│   └── Makefile
├── kernel/               # kernel CONFIG and patches only — not the kernel source tree
│   ├── .config
│   └── patches/
├── rootfs/               # staging tree, packaged as-is into the initramfs
│   ├── bin/               # compiled tools land here at build time
│   ├── dev/                # empty — populated by devtmpfs at boot
│   ├── proc/               # empty — mountpoint for procfs
│   ├── sys/                # empty — mountpoint for sysfs
│   └── etc/
├── scripts/
│   ├── build-kernel.sh 
│   ├── build-initramfs.sh
│   └── run-qemu.sh  
├── build/                 # gitignored — bzImage, initramfs.cpio.gz, object files
├── .gitignore
└── README.md
```

`kernel/` and `rootfs/` are asymmetric on purpose. `kernel/` holds only what's *yours* to version — config and patches — since the actual kernel source is huge and lives upstream, fetched by `build-kernel.sh` rather than committed. `rootfs/` is a disposable build output, regenerated from `init/` and `tools/` on every build, the same way you'd never commit a `.o` file.

## Building & running

```bash
./scripts/build-kernel.sh       # fetches and builds the Linux kernel
./scripts/build-initramfs.sh    # builds init + tools, packages the initramfs
./scripts/run-qemu.sh           # boots it in QEMU
```

Scripts are filled in incrementally as each stage of the project is built — see Progress below.

## Progress

Built incrementally, one feature at a time, with each stage tagged in git:

- [ ] `v0.1` — skeleton `init` prints "hello world," boots as PID 1 via initramfs
- [ ] `v0.2` — `init` becomes a real shell: read input, fork/exec commands
- [ ] `v0.3` — mount `/proc`, `/sys`, `/dev` from init
- [ ] `v0.4` — hand-written tools: `touch`, `rm`, `cat`
- [ ] `v0.5` — swap in BusyBox for broader tool coverage
- [ ] `v0.6` — dynamic linking + real GNU coreutils

## Status

Hobby project, built for learning rather than stability. Expect things to be broken between stages.
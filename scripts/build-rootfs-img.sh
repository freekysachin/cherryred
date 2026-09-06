#!/usr/bin/env bash
#
# Builds init + tools and formats them into build/rootfs.img — the persistent
# ext4 disk that earlyinit hands the machine over to.
#
# Needed from v0.3 onward. Before that there is nothing to switch_root into.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
STAGING="$BUILD/rootfs-staging"
IMG="$BUILD/rootfs.img"

IMG_SIZE="${IMG_SIZE:-64M}"
CC="${CC:-gcc}"
CFLAGS="${CFLAGS:--static -Wall -Wextra -O2}"

die() { echo "error: $*" >&2; exit 1; }

command -v mke2fs >/dev/null || die "mke2fs not found — sudo apt install e2fsprogs"

if [ ! -f "$ROOT/init/init.c" ]; then
  die "init/init.c does not exist yet.
  That's the on-disk /sbin/init — the second half of the two-stage boot, due at v0.3.
  Until then this script has nothing to put on the disk."
fi

# --- assemble the tree -----------------------------------------------------
rm -rf "$STAGING"
# proc/sys/dev are empty mountpoints: earlyinit MS_MOVEs the already-mounted
# filesystems onto them during the handover, so init inherits them ready to use.
mkdir -p "$STAGING"/{sbin,bin,etc,proc,sys,dev,mnt,root,tmp}

echo ">> compiling init"
$CC $CFLAGS -o "$STAGING/sbin/init" "$ROOT/init/init.c"

shopt -s nullglob
tools=("$ROOT"/tools/*.c)
if [ ${#tools[@]} -gt 0 ]; then
  for src in "${tools[@]}"; do
    name="$(basename "$src" .c)"
    echo ">> compiling tool $name"
    $CC $CFLAGS -o "$STAGING/bin/$name" "$src"
  done
else
  echo ">> no tools/*.c yet, skipping (due at v0.5)"
fi
shopt -u nullglob

# Anything committed under rootfs/ is overlaid on top of the compiled output.
[ -d "$ROOT/rootfs" ] && cp -a "$ROOT/rootfs/." "$STAGING/"

# --- format ----------------------------------------------------------------
# mke2fs -d populates the filesystem image directly from a directory, so this
# needs no loop mount and no root. fakeroot makes the files land owned by
# 0:0 rather than by whoever ran the build.
rm -f "$IMG"
echo ">> formatting $IMG_SIZE ext4 image"
if command -v fakeroot >/dev/null; then
  fakeroot -- bash -c "
    set -e
    chown -R 0:0 '$STAGING'
    mke2fs -q -t ext4 -L cherryred -d '$STAGING' -F '$IMG' '$IMG_SIZE'
  "
else
  echo "   (fakeroot missing — files will be owned by uid $(id -u), harmless since init runs as root)"
  mke2fs -q -t ext4 -L cherryred -d "$STAGING" -F "$IMG" "$IMG_SIZE"
fi

echo
echo "built  build/rootfs.img  ($(du -h "$IMG" | cut -f1))"

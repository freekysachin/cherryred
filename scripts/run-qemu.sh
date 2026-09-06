#!/usr/bin/env bash
#
# Boots cherryred under QEMU.
#
# Works with only a kernel + initramfs (v0.1), and picks up the disk
# automatically once build-rootfs-img.sh has produced one (v0.3).
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"

# Override KERNEL to boot against your distro's kernel before you've built one:
#   KERNEL=/boot/vmlinuz-$(uname -r) ./scripts/run-qemu.sh
KERNEL="${KERNEL:-$BUILD/bzImage}"
INITRD="${INITRD:-$BUILD/initramfs.cpio.gz}"
DISK="${DISK:-$BUILD/rootfs.img}"
MEM="${MEM:-512M}"

die() { echo "error: $*" >&2; exit 1; }

[ -f "$KERNEL" ] || die "no kernel at $KERNEL — run ./scripts/build-kernel.sh"
[ -f "$INITRD" ] || die "no initramfs at $INITRD — run ./scripts/build-initramfs.sh"

# console=ttyS0 sends kernel and userspace output to the serial port, which
# -nographic wires to this terminal. Without it you get a graphical window
# and see nothing here.
CMDLINE="console=ttyS0"

args=(
  -kernel "$KERNEL"
  -initrd "$INITRD"
  -m "$MEM"
  -nographic
  # A panic should stop and show you the trace, not silently reboot into
  # the same panic forever.
  -no-reboot
)

if [ -f "$DISK" ]; then
  args+=(-drive "file=$DISK,if=virtio,format=raw")
  # root= is read by earlyinit out of /proc/cmdline, not by the kernel:
  # the kernel's own root= handling is bypassed entirely when an initramfs
  # provides /init.
  CMDLINE="$CMDLINE root=/dev/vda rw"
  echo ">> disk attached as /dev/vda"
else
  echo ">> no rootfs.img yet — initramfs-only boot (pre-v0.3)"
fi

if [ -w /dev/kvm ]; then
  args+=(-enable-kvm -cpu host)
else
  echo ">> no KVM access, falling back to emulation (slower)"
fi

args+=(-append "$CMDLINE")

echo ">> booting — press Ctrl-A then X to quit QEMU"
echo
exec qemu-system-x86_64 "${args[@]}"

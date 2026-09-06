#!/usr/bin/env bash
#
# Fetches the upstream Linux kernel source, configures it for cherryred,
# and builds a bzImage into build/.
#
# The kernel source is NOT committed to this repo (it's ~1.5GB extracted).
# kernel/.config is deliberately NOT committed either — it's per-developer, so
# everyone can pick their own options on top of cherryred's actual requirement:
# REQUIRED_CONFIGS below, which IS versioned (it's code, right here in this
# script) and gets force-enabled on whatever config you start from, every run.
# Only kernel/patches/ is shared, versioned content.
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"

KERNEL_VERSION="${KERNEL_VERSION:-6.12.108}"     # override: KERNEL_VERSION=6.18.49 ./scripts/build-kernel.sh
JOBS="${JOBS:-$(nproc)}"

KSRC="$BUILD/linux-$KERNEL_VERSION"
TARBALL="$BUILD/linux-$KERNEL_VERSION.tar.xz"
URL="https://cdn.kernel.org/pub/linux/kernel/v${KERNEL_VERSION%%.*}.x/linux-$KERNEL_VERSION.tar.xz"

# Options cherryred cannot boot without. All must be =y, never =m: earlyinit has
# no module loader, so anything needed to reach the disk must already be in the
# kernel image by the time PID 1 runs.
REQUIRED_CONFIGS=(
  BLK_DEV_INITRD          # unpack the initramfs cpio at boot
  BINFMT_ELF              # execute our static ELF binaries
  DEVTMPFS                # /dev, so we can see the disk node at all
  PROC_FS                 # /proc, needed to read /proc/cmdline for root=
  SYSFS                   # /sys
  VIRTIO                  # QEMU paravirtualised device core
  VIRTIO_PCI              # ...on the PCI bus
  VIRTIO_BLK              # ...exposing the disk as /dev/vda
  EXT4_FS                 # read the root filesystem
  TTY                     # console infrastructure
  SERIAL_8250             # the serial port QEMU gives us
  SERIAL_8250_CONSOLE     # ...usable as console=ttyS0
)

die() { echo "error: $*" >&2; exit 1; }

check_deps() {
  local missing=()
  for t in make gcc flex bison bc curl tar; do
    command -v "$t" >/dev/null || missing+=("$t")
  done
  [ -f /usr/include/openssl/ssl.h ] || missing+=("libssl-dev")
  [ -f /usr/include/libelf.h ]      || missing+=("libelf-dev")
  if [ "${MENUCONFIG:-0}" = "1" ]; then
    [ -f /usr/include/ncurses.h ] || missing+=("libncurses-dev")
  fi

  if [ ${#missing[@]} -gt 0 ]; then
    echo "Missing kernel build dependencies: ${missing[*]}" >&2
    local hint="build-essential flex bison bc libssl-dev libelf-dev"
    [ "${MENUCONFIG:-0}" = "1" ] && hint="$hint libncurses-dev"
    echo "  sudo apt install $hint" >&2
    exit 1
  fi
}

check_deps
mkdir -p "$BUILD" "$ROOT/kernel"

# --- fetch -----------------------------------------------------------------
if [ ! -f "$TARBALL" ]; then
  echo ">> downloading $URL"
  curl -fL --progress-bar -o "$TARBALL.part" "$URL" \
    || die "download failed. Set KERNEL_VERSION to a version that exists on kernel.org."
  mv "$TARBALL.part" "$TARBALL"
else
  echo ">> using cached $(basename "$TARBALL")"
fi

if [ ! -d "$KSRC" ]; then
  echo ">> extracting (this takes a minute)"
  tar -xf "$TARBALL" -C "$BUILD"
fi

# --- patches ---------------------------------------------------------------
# Ours to version; the source tree is not. Applied fresh on every extract.
shopt -s nullglob
for p in "$ROOT"/kernel/patches/*.patch; do
  echo ">> applying $(basename "$p")"
  patch -d "$KSRC" -p1 -N -r - < "$p" || echo "   (already applied, skipping)"
done
shopt -u nullglob

# --- configure -------------------------------------------------------------
if [ -f "$ROOT/kernel/.config" ]; then
  echo ">> starting from kernel/.config"
  cp "$ROOT/kernel/.config" "$KSRC/.config"
else
  echo ">> no kernel/.config yet, starting from x86_64_defconfig"
  make -C "$KSRC" x86_64_defconfig
fi

if [ "${MENUCONFIG:-0}" = "1" ]; then
  echo ">> launching menuconfig — save and exit ('/' to search, then Q) when done"
  make -C "$KSRC" menuconfig
fi

echo ">> forcing cherryred's required options on"
for opt in "${REQUIRED_CONFIGS[@]}"; do
  "$KSRC/scripts/config" --file "$KSRC/.config" --enable "$opt"
done

# Resolves any dependencies our --enable calls pulled in, and fills in
# defaults for anything new since the .config was last written.
make -C "$KSRC" olddefconfig

# Verify, because scripts/config will happily set an option that a later
# olddefconfig turns straight back off if its dependencies aren't met.
for opt in "${REQUIRED_CONFIGS[@]}"; do
  grep -q "^CONFIG_$opt=y" "$KSRC/.config" \
    || die "CONFIG_$opt is not =y after olddefconfig — it likely has an unmet dependency."
done

# --- build -----------------------------------------------------------------
echo ">> building bzImage with $JOBS jobs"
make -C "$KSRC" -j"$JOBS" bzImage

cp "$KSRC/arch/x86/boot/bzImage" "$BUILD/bzImage"
# Save the config back so the exact kernel we just built is reproducible.
cp "$KSRC/.config" "$ROOT/kernel/.config"

# kernel/source is a convenience symlink for browsing headers next to
# kernel/.config — it must be repointed on every build, since KERNEL_VERSION
# is overridable and a stale symlink fails silently rather than erroring.
ln -sfn "../build/linux-$KERNEL_VERSION" "$ROOT/kernel/source"

echo
echo "built  build/bzImage  ($(du -h "$BUILD/bzImage" | cut -f1))"
echo "linked kernel/source -> build/linux-$KERNEL_VERSION"
echo "saved  kernel/.config  — yours to customize, not committed (see REQUIRED_CONFIGS above)"

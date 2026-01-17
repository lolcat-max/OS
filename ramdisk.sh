#!/usr/bin/env bash
set -euo pipefail

BUSYBOX_BINARY="${BUSYBOX_BINARY:-/usr/bin/busybox}"
KERNEL_IMAGE="${KERNEL_IMAGE:-/path/to/your/kernel/Image}"

WORKDIR="${WORKDIR:-$PWD/min_linux}"
ROOTFS_DIR="$WORKDIR/rootfs"
INITRAMFS="$WORKDIR/initramfs.cpio.gz"

if [ ! -f "$BUSYBOX_BINARY" ]; then
  echo "ERROR: $BUSYBOX_BINARY not found"
  exit 1
fi

echo "Using: $BUSYBOX_BINARY"
rm -rf "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"

cd "$ROOTFS_DIR"

# -------- Essential directories --------
mkdir -p bin sbin dev proc sys tmp run
chmod 1777 tmp

# -------- Copy busybox --------
cp "$BUSYBOX_BINARY" bin/busybox
chmod 755 bin/busybox

# -------- Create symlinks --------
cd bin
ESSENTIAL="sh ls cat ps date id pwd mkdir rmdir cp mv rm touch sync kill true false dd head tail grep cut sed"
for applet in $ESSENTIAL; do
  ln -sf busybox "$applet" 2>/dev/null || true
done
cd ..

cd sbin
for applet in init switch_root poweroff reboot halt mdev mount umount; do
  ln -sf ../bin/busybox "$applet" 2>/dev/null || true
done
cd ..

# -------- Device nodes --------
sudo rm -f dev/console dev/null dev/zero dev/tty 2>/dev/null || true
sudo mknod -m 600 dev/console c 5 1
sudo mknod -m 666 dev/null    c 1 3
sudo mknod -m 666 dev/zero    c 1 5
sudo mknod -m 666 dev/tty     c 5 0

# -------- /init script --------
cat > init << 'EOF'
#!/bin/sh
export PATH=/bin:/sbin

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /tmp
mount -t tmpfs tmpfs /dev

echo "/sbin/mdev" > /proc/sys/kernel/hotplug 2>/dev/null || true
/sbin/mdev -s 2>/dev/null || true

echo "✓ Tiny Linux ready"
exec /bin/sh
EOF
chmod 755 init

# -------- FIXED: Build initramfs from correct directory --------
echo "Building initramfs..."
find . | cpio -o --format=newc | gzip -9 > "$INITRAMFS"

cd ..
echo "✓ SUCCESS: $INITRAMFS ($(du -h "$INITRAMFS" | cut -f1))"
echo "  BusyBox: $(ls -lh "$ROOTFS_DIR/bin/busybox")"
echo "  Symlinks: $(find "$ROOTFS_DIR" -type l | wc -l)"

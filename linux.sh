#!/usr/bin/env bash
set -euo pipefail

# Complete ISO builder: BusyBox initramfs + kernel + GRUB
OUTDIR="${1:-$PWD/min_linux_iso}"
ISO="$OUTDIR/linux.iso"
KERNEL_IMAGE="$OUTDIR/vmlinuz"
INITRAMFS="$OUTDIR/initramfs.cpio.gz"
BUSYBOX_BINARY="${BUSYBOX_BINARY:-/usr/bin/busybox}"

echo "Building complete bootable ISO -> $ISO"

# -------- 1. Create minimal initramfs --------
rm -rf "$OUTDIR"
mkdir -p "$OUTDIR/rootfs" "$OUTDIR/iso/boot/grub"

cd "$OUTDIR/rootfs"

# Structure
mkdir -p bin sbin dev proc sys tmp run
chmod 1777 tmp

# Copy BusyBox
cp "$BUSYBOX_BINARY" bin/busybox
chmod 755 bin/busybox

# Essential symlinks
cd bin
for applet in sh ls cat ps date id pwd mkdir rmdir cp mv rm touch sync kill true false dd head tail grep cut sed mount umount mknod; do
  ln -sf busybox "$applet"
done
cd ..

cd sbin
for applet in init switch_root poweroff reboot halt mdev; do
  ln -sf ../bin/busybox "$applet"
done
cd ..

# Devices
sudo mknod -m 600 dev/console c 5 1
sudo mknod -m 666 dev/null    c 1 3
sudo mknod -m 666 dev/zero    c 1 5
sudo mknod -m 666 dev/tty     c 5 0

# Init script
cat > init << 'EOF'
#!/bin/sh
export PATH=/bin:/sbin
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /tmp
mount -t tmpfs tmpfs /dev
echo "/sbin/mdev" > /proc/sys/kernel/hotplug 2>/dev/null || true
/sbin/mdev -s 2>/dev/null || true
echo "✓ Booted Tiny Linux shell!"
exec /bin/sh
EOF
chmod 755 init

# Pack initramfs
cd "$OUTDIR/rootfs"
find . | cpio -o --format=newc | gzip -9 > "$INITRAMFS"

# -------- 2. Get kernel (download prebuilt) --------
cd "$OUTDIR"
if [ ! -f "$KERNEL_IMAGE" ]; then
  echo "Downloading kernel..."
  wget -q https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.13.tar.xz
  tar -xf linux-6.13.tar.xz linux-6.13/arch/x86/boot/bzImage
  mv linux-6.13/arch/x86/boot/bzImage vmlinuz
  rm -rf linux-6.13*
fi

# -------- 3. Create ISO structure --------
cd "$OUTDIR/iso/boot"
cp "../../vmlinuz" .
cp "../../initramfs.cpio.gz" .

# GRUB config
cat > grub/grub.cfg << 'EOF'
set timeout=5
set default=0

menuentry 'Tiny Linux (BusyBox)' {
    linux /boot/vmlinuz console=ttyS0,115200n8 init=/init
    initrd /boot/initramfs.cpio.gz
}
EOF

# -------- 4. Build ISO --------
cd "$OUTDIR/iso"
sudo grub-mkrescue -o "../../linux.iso" .

cd "$OUTDIR"
echo "=============================================="
echo "✓ COMPLETE! Bootable ISO created:"
echo "  ISO: $(du -h linux.iso | cut -f1) $ISO"
echo "  Kernel: $(du -h vmlinuz | cut -f1)"
echo "  Initramfs: $(du -h initramfs.cpio.gz | cut -f1)"
echo ""
echo "Test with QEMU:"
echo "  qemu-system-x86_64 -cdrom linux.iso -m 512M -nographic"
echo ""
echo "Burn to USB:"
echo "  sudo dd if=linux.iso of=/dev/sdX bs=4M status=progress"
echo "=============================================="


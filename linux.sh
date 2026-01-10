#!/bin/bash
set -e

OUTDIR="min_linux_iso"
INITDIR="$OUTDIR/initramfs"

mkdir -p "$OUTDIR"

# -------------------------------
# Find kernel
# -------------------------------
KERNEL=$(ls -t /boot/vmlinuz* 2>/dev/null | head -1)
if [ -z "$KERNEL" ]; then
  echo "No kernel found in /boot"
  exit 1
fi

echo "Using kernel: $KERNEL"

# -------------------------------
# Build initramfs with BusyBox
# -------------------------------
echo "Building initramfs..."

rm -rf "$INITDIR"
mkdir -p "$INITDIR"/{bin,sbin,proc,sys,dev}

# Copy busybox
if ! command -v busybox >/dev/null; then
  echo "busybox not found. Install with: sudo apt install busybox-static"
  exit 1
fi

cp "$(command -v busybox)" "$INITDIR/bin/"
chmod +x "$INITDIR/bin/busybox"

# Create applets
ln -s busybox "$INITDIR/bin/sh"
ln -s busybox "$INITDIR/bin/mount"
ln -s busybox "$INITDIR/bin/echo"

# /init
cat > "$INITDIR/init" << 'EOF'
#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev

echo
echo "🔥 Tiny Linux booted successfully"
echo

exec /bin/sh
EOF

chmod +x "$INITDIR/init"

# Pack initramfs
(
  cd "$INITDIR"
  find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
)

# -------------------------------
# Build ISO
# -------------------------------
rm -rf "$OUTDIR/iso"
mkdir -p "$OUTDIR/iso/boot/grub"

cp "$KERNEL" "$OUTDIR/iso/vmlinuz"
cp "$OUTDIR/initramfs.cpio.gz" "$OUTDIR/iso/initramfs.cpio.gz"

cat > "$OUTDIR/iso/boot/grub/grub.cfg" << 'EOF'
set timeout=0
set default=0

menuentry 'Tiny Linux' {
  linux /vmlinuz console=ttyS0 init=/init panic=1
  initrd /initramfs.cpio.gz
}
EOF

cd "$OUTDIR/iso"
sudo grub-mkrescue -o ../linux.iso . >/dev/null
cd - >/dev/null

echo
echo "🎉 ISO created: $OUTDIR/linux.iso"
echo "Run:"
echo "qemu-system-x86_64 -cdrom $OUTDIR/linux.iso -nographic"


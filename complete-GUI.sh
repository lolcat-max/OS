#!/bin/bash
################################################################################
# MINIMAL WAYLAND LIVE ISO - PROPER APPROACH WITH SQUASHFS
#
# This version uses squashfs for the rootfs (like real live CDs do)
# instead of embedding everything in initramfs which causes OOM
################################################################################
set -euo pipefail

WORK_DIR="$HOME/custom-linux-build"
ROOTFS_DIR="$HOME/custom-rootfs"
ISO_DIR="$HOME/iso-staging"
ISO_OUT="$HOME/custom-linux-wayland.iso"
JOBS="$(nproc)"

sudo rm -rf "$WORK_DIR" "$ROOTFS_DIR" "$ISO_DIR" "$ISO_OUT"

###############################################################################
# 1) Host Dependencies
###############################################################################
sudo apt-get update
sudo apt-get install -y \
  build-essential flex bison libncurses-dev libssl-dev bc dwarves pahole \
  git rsync cpio xorriso grub-pc-bin mtools squashfs-tools \
  mmdebstrap busybox-static kmod coreutils

###############################################################################
# 2) Kernel (DRM + Virtio + Initramfs + SquashFS support)
###############################################################################
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"
git clone --depth 1 https://github.com/torvalds/linux.git
cd linux

make defconfig

# KMS / input / initramfs basics
./scripts/config --enable CONFIG_DEVTMPFS
./scripts/config --enable CONFIG_DEVTMPFS_MOUNT
./scripts/config --enable CONFIG_PROC_FS
./scripts/config --enable CONFIG_SYSFS
./scripts/config --enable CONFIG_TMPFS
./scripts/config --enable CONFIG_BLK_DEV_INITRD

./scripts/config --enable CONFIG_INPUT
./scripts/config --enable CONFIG_INPUT_EVDEV

# Virtio core (CRITICAL when using -vga virtio)
./scripts/config --enable CONFIG_VIRTIO
./scripts/config --enable CONFIG_VIRTIO_PCI
./scripts/config --enable CONFIG_VIRTIO_MMIO
./scripts/config --enable CONFIG_HW_RANDOM_VIRTIO

# DRM / virtio GPU (KMS)
./scripts/config --enable CONFIG_DRM
./scripts/config --enable CONFIG_DRM_KMS_HELPER
./scripts/config --enable CONFIG_DRM_VIRTIO_GPU

./scripts/config --enable CONFIG_DRM_SIMPLEDRM
./scripts/config --enable CONFIG_FB
./scripts/config --enable CONFIG_FRAMEBUFFER_CONSOLE

# SquashFS support (CRITICAL for live CD)
./scripts/config --enable CONFIG_SQUASHFS
./scripts/config --enable CONFIG_SQUASHFS_XZ
./scripts/config --enable CONFIG_SQUASHFS_ZLIB

# ISO9660 / loop device support
./scripts/config --enable CONFIG_ISO9660_FS
./scripts/config --enable CONFIG_JOLIET
./scripts/config --enable CONFIG_BLK_DEV_LOOP

make olddefconfig
make -j"$JOBS" bzImage modules

###############################################################################
# 3) Root filesystem (Wayland + Sway)
###############################################################################
sudo mmdebstrap \
  --architecture=amd64 \
  --variant=minbase \
  --include="\
systemd systemd-sysv udev dbus seatd \
sway foot \
libwayland-client0 libwayland-server0 wayland-protocols \
libgl1 libegl1 libgles2 libgl1-mesa-dri mesa-vulkan-drivers \
xwayland \
kmod coreutils bash \
" \
  bookworm "$ROOTFS_DIR"

# Install kernel modules into the rootfs
sudo make modules_install -C "$WORK_DIR/linux" INSTALL_MOD_PATH="$ROOTFS_DIR"

###############################################################################
# 4) User + services
###############################################################################
sudo chroot "$ROOTFS_DIR" /bin/bash <<'EOF'
set -e
groupadd -f video
groupadd -f render
groupadd -f input
useradd -m -u 1000 -s /bin/bash -G video,render,input user
echo user:password | chpasswd
systemctl enable seatd
systemctl enable getty@tty1
EOF

###############################################################################
# 5) Autologin + Sway
###############################################################################
sudo mkdir -p "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d"
cat <<'EOF' | sudo tee "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d/override.conf" >/dev/null
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin user --noclear %I $TERM
EOF

cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/.bash_profile" >/dev/null
if [ -z "${WAYLAND_DISPLAY:-}" ] && [ "$(tty)" = "/dev/tty1" ]; then
  export XDG_RUNTIME_DIR=/run/user/1000
  mkdir -p "$XDG_RUNTIME_DIR"
  chmod 0700 "$XDG_RUNTIME_DIR"
  exec dbus-run-session sway
fi
EOF
sudo chroot "$ROOTFS_DIR" chown user:user /home/user/.bash_profile

###############################################################################
# 6) Create SquashFS image (compressed rootfs)
###############################################################################
mkdir -p "$ISO_DIR/live"
echo "=== Creating SquashFS image (this may take a few minutes) ==="
sudo mksquashfs "$ROOTFS_DIR" "$ISO_DIR/live/filesystem.squashfs" -comp xz -b 1M
echo "=== SquashFS created ==="
ls -lh "$ISO_DIR/live/filesystem.squashfs"

###############################################################################
# 7) Create minimal initramfs
###############################################################################
# Create boot directory first
mkdir -p "$ISO_DIR/boot"

INITRAMFS_DIR=$(mktemp -d)
cd "$INITRAMFS_DIR"

# Get busybox
BUSYBOX_HOST="$(command -v busybox || echo /bin/busybox)"
cp "$BUSYBOX_HOST" busybox
chmod +x busybox

# Create directory structure
mkdir -p bin sbin dev proc sys mnt/root newroot

# Create symlinks for busybox applets
ln -s /busybox bin/sh
ln -s /busybox bin/mount
ln -s /busybox bin/mkdir
ln -s /busybox bin/sleep
ln -s /busybox bin/ls
ln -s /busybox bin/cat
ln -s /busybox sbin/switch_root

# Create init script
cat <<'INIT_EOF' > init
#!/bin/sh
# Minimal init for live CD

# Create and mount essential filesystems
mkdir -p /dev /proc /sys /mnt/cdrom /mnt/squash /mnt/root

mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys

echo "Scanning for CD-ROM..."
sleep 2

# Find and mount the CD-ROM
for dev in /dev/sr0 /dev/cdrom /dev/scd0; do
    if [ -b "$dev" ]; then
        echo "Trying to mount $dev..."
        if mount -t iso9660 -o ro "$dev" /mnt/cdrom 2>/dev/null; then
            echo "Mounted CD-ROM from $dev"
            break
        fi
    fi
done

# Check if we found the squashfs
if [ ! -f /mnt/cdrom/live/filesystem.squashfs ]; then
    echo "ERROR: Could not find filesystem.squashfs on CD-ROM"
    echo "Dropping to shell for debugging..."
    exec /bin/sh
fi

# Mount the squashfs
echo "Mounting squashfs filesystem..."
mount -t squashfs -o loop,ro /mnt/cdrom/live/filesystem.squashfs /mnt/squash

# Copy to tmpfs root (overlay would be better but more complex)
echo "Setting up root filesystem..."
mount -t tmpfs tmpfs /mnt/root
mkdir -p /mnt/root/{dev,proc,sys,run,tmp}

# Copy essential directories from squashfs
for dir in bin sbin lib lib64 usr etc opt var home root; do
    if [ -d "/mnt/squash/$dir" ]; then
        cp -a "/mnt/squash/$dir" /mnt/root/ 2>/dev/null || true
    fi
done

# Prepare new root
mount -t proc proc /mnt/root/proc
mount -t sysfs sysfs /mnt/root/sys
mount -t devtmpfs devtmpfs /mnt/root/dev
mount -t tmpfs tmpfs /mnt/root/run
mount -t tmpfs tmpfs /mnt/root/tmp

echo "Switching to new root..."
exec switch_root /mnt/root /sbin/init

# Should never reach here
echo "ERROR: switch_root failed"
exec /bin/sh
INIT_EOF

chmod +x init

# Build initramfs
echo "=== Building initramfs ==="
find . | cpio -o -H newc | gzip > "$ISO_DIR/boot/initrd.img"
cd /
rm -rf "$INITRAMFS_DIR"

echo "=== Initramfs created ==="
ls -lh "$ISO_DIR/boot/initrd.img"

###############################################################################
# 8) Copy kernel
###############################################################################
mkdir -p "$ISO_DIR/boot"
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"

###############################################################################
# 9) GRUB config
###############################################################################
mkdir -p "$ISO_DIR/boot/grub"
cat <<'EOF' > "$ISO_DIR/boot/grub/grub.cfg"
set default=0
set timeout=3

menuentry "Sway Wayland Live" {
  linux /boot/vmlinuz console=tty1 console=ttyS0,115200 loglevel=7
  initrd /boot/initrd.img
}
EOF

###############################################################################
# 10) Build ISO
###############################################################################
cd "$ISO_DIR"
grub-mkrescue -o "$ISO_OUT" .

echo
echo "✓ ISO built: $ISO_OUT"
echo
echo "ISO size:"
ls -lh "$ISO_OUT"
echo
echo "Boot with QEMU:"
echo "qemu-system-x86_64 -m 4G -cdrom \"$ISO_OUT\" -boot d -enable-kvm -cpu host -smp 2 -vga virtio -serial mon:stdio"

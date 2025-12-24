#!/bin/bash
################################################################################
# CUSTOM LINUX BUILD WITH WAYLAND (SWAY)
# Resolves Kernel Panic (0x9) and enables GUI via RAM-based RootFS
################################################################################

set -euo pipefail

# Configuration
WORK_DIR="$HOME/custom-linux-build"
ROOTFS_DIR="$HOME/custom-rootfs"
ISO_DIR="$HOME/iso-staging"
KERNEL_JOBS=$(nproc)

# 1. Install Dependencies
echo "Installing build dependencies..."
sudo apt-get update
sudo apt-get install -y \
    build-essential flex bison libncurses-dev libssl-dev libelf-dev bc \
    dwarves pahole cpio rsync wget git mmdebstrap xorriso \
    grub-pc-bin grub-efi-amd64-bin mtools busybox-static

# 2. Kernel Build with Graphics Support
echo "Building Linux Kernel..."
mkdir -p "$WORK_DIR" && cd "$WORK_DIR"
[ -d linux ] || git clone --depth 1 https://github.com/torvalds/linux.git linux
cd linux
make defconfig

# Enable drivers for Wayland/QEMU
./scripts/config --enable CONFIG_DRM_VIRTIO_GPU
./scripts/config --enable CONFIG_DRM_BOCHS
./scripts/config --enable CONFIG_INPUT_EVDEV
./scripts/config --enable CONFIG_DEVTMPFS
./scripts/config --enable CONFIG_DEVTMPFS_MOUNT
./scripts/config --enable CONFIG_TMPFS
./scripts/config --enable CONFIG_TMPFS_POSIX_ACL
./scripts/config --enable CONFIG_BLK_DEV_INITRD

make olddefconfig
make -j"$KERNEL_JOBS" bzImage

# 3. Build RootFS (Debian Bookworm)
echo "Creating Root Filesystem..."
sudo rm -rf "$ROOTFS_DIR"
mkdir -p "$ROOTFS_DIR"
sudo mmdebstrap --architecture=amd64 --variant=minbase \
    --include=sway,foot,seatd,dbus-user-session,systemd,systemd-sysv,libgl1-mesa-dri,udev \
    bookworm "$ROOTFS_DIR" http://deb.debian.org/debian/

# Configure User and Autologin
sudo chroot "$ROOTFS_DIR" useradd -m -s /bin/bash -G video,render,user
echo "user:password" | sudo chroot "$ROOTFS_DIR" chpasswd

# Setup Systemd Autologin on TTY1
sudo mkdir -p "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d"
cat <<EOF | sudo tee "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d/override.conf"
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin user --noclear %I \$TERM
EOF

# Setup Sway Autostart from Bash Profile
cat <<EOF | sudo tee "$ROOTFS_DIR/home/user/.bash_profile"
if [ -z "\$WAYLAND_DISPLAY" ] && [ "\$(tty)" = "/dev/tty1" ]; then
    export XDG_RUNTIME_DIR=/tmp/runtime-user
    mkdir -p \$XDG_RUNTIME_DIR
    chmod 700 \$XDG_RUNTIME_DIR
    exec dbus-run-session sway
fi
EOF
sudo chroot "$ROOTFS_DIR" chown user:user /home/user/.bash_profile

# 4. Create Bootable ISO
echo "Assembling ISO..."
mkdir -p "$ISO_DIR/boot/grub"
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"

# Pack RootFS into Initrd (Avoids mount errors)
cd "$ROOTFS_DIR"
sudo find . | sudo cpio -o -H newc | gzip > "$ISO_DIR/boot/initrd.img"

cat <<EOF > "$ISO_DIR/boot/grub/grub.cfg"
set timeout=0
menuentry "Custom Linux Wayland" {
    linux /boot/vmlinuz quiet splash root=/dev/ram0 rw
    initrd /boot/initrd.img
}
EOF

grub-mkrescue -o "$HOME/custom-linux-wayland.iso" "$ISO_DIR"

echo "BUILD SUCCESSFUL! ISO: $HOME/custom-linux-wayland.iso"

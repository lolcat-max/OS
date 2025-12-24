#!/bin/bash
################################################################################
# CUSTOM LINUX BUILD WITH WAYLAND (SWAY)
################################################################################

set -euo pipefail

# Configuration
WORK_DIR="$HOME/custom-linux-build"
ROOTFS_DIR="$HOME/custom-rootfs"
ISO_DIR="$HOME/iso-staging"
KERNEL_JOBS=$(nproc)

# Cleanup previous builds
sudo rm -rf "$WORK_DIR" "$ROOTFS_DIR" "$ISO_DIR" "$HOME/custom-linux-wayland.iso"

# 1. Install Dependencies
echo "Installing build dependencies..."
sudo apt-get update
sudo apt-get install -y \
    build-essential flex bison libncurses-dev libssl-dev libelf-dev bc \
    dwarves pahole cpio rsync wget git mmdebstrap xorriso \
    grub-pc-bin grub-efi-amd64-bin mtools busybox-static kmod

# 2. Kernel Build with Graphics Support
echo "Building Linux Kernel..."
mkdir -p "$WORK_DIR" && cd "$WORK_DIR"
git clone --depth 1 https://github.com/torvalds/linux.git linux
cd linux
make defconfig

# Enable drivers for Wayland/QEMU/VMware
./scripts/config --enable CONFIG_DRM_VIRTIO_GPU
./scripts/config --enable CONFIG_DRM_BOCHS
./scripts/config --enable CONFIG_DRM_VMWGFX
./scripts/config --enable CONFIG_INPUT_EVDEV
./scripts/config --enable CONFIG_DEVTMPFS
./scripts/config --enable CONFIG_DEVTMPFS_MOUNT
./scripts/config --enable CONFIG_TMPFS
./scripts/config --enable CONFIG_TMPFS_POSIX_ACL
./scripts/config --enable CONFIG_BLK_DEV_INITRD
# SCSI and Disk Support for VMware
./scripts/config --enable CONFIG_SCSI
./scripts/config --enable CONFIG_BLK_DEV_SD
./scripts/config --enable CONFIG_SCSI_VMW_PVSCSI
./scripts/config --enable CONFIG_VMWARE_PVSCSI
./scripts/config --enable CONFIG_VMW_BALLOON
./scripts/config --enable CONFIG_NVME_CORE
./scripts/config --enable CONFIG_BLK_DEV_NVME

make olddefconfig
make -j"$KERNEL_JOBS" bzImage

# 3. Build RootFS (Debian Bookworm)
echo "Creating Root Filesystem..."
mkdir -p "$ROOTFS_DIR"
sudo mmdebstrap --architecture=amd64 --variant=minbase \
    --include=sway,foot,seatd,dbus-user-session,systemd,systemd-sysv,libgl1-mesa-dri,udev,kmod \
    bookworm "$ROOTFS_DIR" http://deb.debian.org/debian/

# Create required groups and user
echo "Configuring user and groups..."

sudo chroot "$ROOTFS_DIR" useradd -m -u 1000 -g users -s /bin/bash -G video,render,input user
echo "user:password" | sudo chroot "$ROOTFS_DIR" chpasswd

# Enable seatd service
sudo chroot "$ROOTFS_DIR" systemctl enable seatd

# Setup Systemd Autologin on TTY1
sudo mkdir -p "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d"
cat <<EOF | sudo tee "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d/override.conf"
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin user --noclear %I \$TERM
EOF

# Setup Sway Autostart from Bash Profile
cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/.bash_profile"
if [ -z "${WAYLAND_DISPLAY}" ] && [ "$(tty)" = "/dev/tty1" ]; then
    export XDG_RUNTIME_DIR=/run/user/$(id -u)
    mkdir -p ${XDG_RUNTIME_DIR}
    chmod 700 ${XDG_RUNTIME_DIR}
    export XDG_SESSION_TYPE=wayland
    export XDG_CURRENT_DESKTOP=sway
    exec dbus-run-session -- sway
fi
EOF
sudo chroot "$ROOTFS_DIR" chown user:users /home/user/.bash_profile

# 4. Create Bootable ISO with Proper Initramfs
echo "Assembling ISO with proper initramfs..."
mkdir -p "$ISO_DIR/boot/grub"
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"

# Create structured initramfs
INITRAMFS_DIR="$ISO_DIR/initramfs"
mkdir -p "$INITRAMFS_DIR"/{bin,sbin,etc,dev,proc,sys,run,tmp,home/user,lib,lib64,usr/bin,usr/sbin}
sudo cp -a "$ROOTFS_DIR"/{bin/{bash,sh,su},sbin/{init,agetty},lib/x86_64-linux-gnu/ld-linux-x86-64.so.2} "$INITRAMFS_DIR/lib/"
sudo cp -a "$ROOTFS_DIR/lib/x86_64-linux-gnu/"*.so* "$INITRAMFS_DIR/lib/" 2>/dev/null || true
sudo cp -a "$ROOTFS_DIR/usr/bin/{dbus-run-session,dbus-launch}" "$INITRAMFS_DIR/usr/bin/" 2>/dev/null || true
sudo cp -a "$ROOTFS_DIR/home/user" "$INITRAMFS_DIR/home/"
sudo chroot "$ROOTFS_DIR" ls /bin/modprobe >/dev/null 2>&1 && sudo cp -a "$ROOTFS_DIR/bin/modprobe" "$INITRAMFS_DIR/bin/" || sudo cp /sbin/modprobe "$INITRAMFS_DIR/bin/" 2>/dev/null || true

# Create init script
cat <<'EOF' | sudo tee "$INITRAMFS_DIR/init"
#!/bin/bash
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin

mount -t devtmpfs none /dev
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /run
mkdir -p /run/user/1000 /tmp/runtime-user

# Load kernel modules for graphics
modprobe drm 2>/dev/null || true
modprobe virtio_gpu 2>/dev/null || true
modprobe vmwgfx 2>/dev/null || true
modprobe drm_kms_helper 2>/dev/null || true

# Wait for devices
sleep 3

# Switch to new root and exec systemd
exec switch_root /home/user /sbin/init
EOF
sudo chmod +x "$INITRAMFS_DIR/init"

# Copy entire rootfs to initramfs home
sudo rsync -a "$ROOTFS_DIR/" "$INITRAMFS_DIR/home/"

# Pack initramfs
cd "$INITRAMFS_DIR"
sudo find . | sudo cpio -o -H newc | gzip -9 > "$ISO_DIR/boot/initrd.img"

# GRUB Config
cat <<EOF > "$ISO_DIR/boot/grub/grub.cfg"
set timeout=5
set default=0

menuentry "Custom Linux Wayland (Sway)" {
    linux /boot/vmlinuz root=/dev/ram0 rw quiet splash console=tty1 loglevel=3
    initrd /boot/initrd.img
}

menuentry "Custom Linux (Debug - no splash)" {
    linux /boot/vmlinuz root=/dev/ram0 rw console=tty1 loglevel=7
    initrd /boot/initrd.img
}
EOF

# Create ISO
cd "$ISO_DIR"
grub-mkrescue -o "$HOME/custom-linux-wayland.iso" .

echo "BUILD SUCCESSFUL! ISO created: $HOME/custom-linux-wayland.iso"
echo "Boot with the 'Custom Linux Wayland (Sway)' entry."
echo "Login: user / password"
echo "Should auto-launch Sway on TTY1."

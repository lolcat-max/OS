#!/bin/bash
################################################################################
# WAYLAND DESKTOP LIVE ISO - COMPLETE BUILD SCRIPT
# 
# Fixes:
# 1. Adds CONFIG_BLK_DEV_SR to kernel (CD-ROM support)
# 2. Adds a retry loop in initramfs to wait for hardware detection
# 3. Builds full GNOME Desktop on Wayland
################################################################################
set -euo pipefail

# Configuration
WORK_DIR="$HOME/custom-linux-build"
ROOTFS_DIR="$HOME/custom-rootfs"
ISO_DIR="$HOME/iso-staging"
ISO_OUT="$HOME/custom-linux-wayland-desktop.iso"
JOBS="$(nproc)"

# Clean previous builds
echo "=== Cleaning up previous build artifacts ==="
sudo rm -rf "$WORK_DIR" "$ROOTFS_DIR" "$ISO_DIR" "$ISO_OUT"

###############################################################################
# 1) Host Dependencies
###############################################################################
echo "=== Installing Host Dependencies ==="
sudo apt-get update
sudo apt-get install -y \
  build-essential flex bison libncurses-dev libssl-dev bc dwarves pahole \
  git rsync cpio xorriso grub-pc-bin mtools squashfs-tools \
  mmdebstrap busybox-static kmod coreutils \
  debian-archive-keyring

###############################################################################
# 2) Kernel Compilation
###############################################################################
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

echo "=== Cloning Linux Kernel ==="
# We use depth 1 to save download time
git clone --depth 1 https://github.com/torvalds/linux.git
cd linux

echo "=== Configuring Kernel ==="
make defconfig

# --- Core Filesystem Support ---
./scripts/config --enable CONFIG_DEVTMPFS
./scripts/config --enable CONFIG_DEVTMPFS_MOUNT
./scripts/config --enable CONFIG_SQUASHFS
./scripts/config --enable CONFIG_SQUASHFS_XZ
./scripts/config --enable CONFIG_OVERLAY_FS
./scripts/config --enable CONFIG_TMPFS

# --- CD-ROM & ISO Support (CRITICAL FIX) ---
./scripts/config --enable CONFIG_SCSI
./scripts/config --enable CONFIG_BLK_DEV_SD
./scripts/config --enable CONFIG_BLK_DEV_SR    # Enables /dev/sr0
./scripts/config --enable CONFIG_CDROM
./scripts/config --enable CONFIG_ISO9660_FS
./scripts/config --enable CONFIG_JOLIET
./scripts/config --enable CONFIG_ZISOFS

# --- Graphics & DRM (Wayland Requirements) ---
./scripts/config --enable CONFIG_DRM
./scripts/config --enable CONFIG_DRM_KMS_HELPER
./scripts/config --enable CONFIG_DRM_FBDEV_EMULATION
./scripts/config --enable CONFIG_DRM_I915       # Intel
./scripts/config --enable CONFIG_DRM_AMDGPU     # AMD
./scripts/config --enable CONFIG_DRM_NOUVEAU    # Nvidia (Open Source)
./scripts/config --enable CONFIG_DRM_VIRTIO_GPU # QEMU/VirtIO
./scripts/config --enable CONFIG_DRM_BOCHS      # QEMU/Standard
./scripts/config --enable CONFIG_DRM_QXL        # QEMU/QXL
./scripts/config --enable CONFIG_FB_EFI
./scripts/config --enable CONFIG_FB_VESA

# --- Input Devices ---
./scripts/config --enable CONFIG_INPUT_EVDEV

# --- EFI Support ---
./scripts/config --enable CONFIG_EFI
./scripts/config --enable CONFIG_EFI_STUB

echo "=== Compiling Kernel (This will take time) ==="
make olddefconfig
make -j"$JOBS" bzImage modules

###############################################################################
# 3) Root Filesystem (GNOME Desktop)
###############################################################################
echo "=== Building Root Filesystem with mmdebstrap ==="

sudo mmdebstrap \
  --architecture=amd64 \
  --variant=minbase \
  --include="\
systemd systemd-sysv udev dbus \
gnome-shell gnome-session gnome-terminal nautilus gnome-control-center \
gnome-tweaks gnome-backgrounds \
gdm3 \
network-manager \
wayland-protocols xwayland \
mesa-utils mesa-vulkan-drivers libgl1-mesa-dri \
kmod coreutils bash util-linux procps psmisc \
sudo iproute2 \
firmware-linux-free \
fonts-dejavu-core fonts-cantarell \
" \
  bookworm "$ROOTFS_DIR"

echo "=== Installing Kernel Modules ==="
sudo make modules_install -C "$WORK_DIR/linux" INSTALL_MOD_PATH="$ROOTFS_DIR"

###############################################################################
# 4) Configure the Desktop System
###############################################################################
echo "=== Configuring System Settings ==="

sudo chroot "$ROOTFS_DIR" /bin/bash <<'EOF'
set -e

# 1. User Setup
# Create user 'user' with password 'password'
useradd -m -u 1000 -s /bin/bash -G sudo,video,render,input,audio,netdev,plugdev user
echo "user:password" | chpasswd

# 2. Network
echo "wayland-live" > /etc/hostname
systemctl enable NetworkManager

# 3. GDM (Login Manager) Configuration
# Force Wayland and Auto-login
mkdir -p /etc/gdm3
cat > /etc/gdm3/custom.conf <<'GDM_CONF'
[daemon]
WaylandEnable=true
AutomaticLoginEnable=true
AutomaticLogin=user
[security]
[xdmcp]
[chooser]
[debug]
GDM_CONF

# 4. Environment Variables for Wayland
mkdir -p /etc/environment.d
cat > /etc/environment.d/gnome-wayland.conf <<'ENV_CONF'
GDK_BACKEND=wayland
QT_QPA_PLATFORM=wayland
MOZ_ENABLE_WAYLAND=1
XDG_SESSION_TYPE=wayland
ENV_CONF

# 5. Udev Permissions for GPU
cat > /etc/udev/rules.d/70-drm.rules <<'UDEV_CONF'
SUBSYSTEM=="drm", KERNEL=="card[0-9]*", TAG+="seat", TAG+="master-of-seat", GROUP="video", MODE="0666"
SUBSYSTEM=="drm", KERNEL=="renderD[0-9]*", GROUP="render", MODE="0666"
UDEV_CONF

# 6. Locale generation (fixes weird characters)
apt-get update && apt-get install -y locales
echo "en_US.UTF-8 UTF-8" > /etc/locale.gen
locale-gen
EOF

# Create a Welcome File on Desktop
sudo mkdir -p "$ROOTFS_DIR/home/user/Desktop"
cat <<'WELCOME' | sudo tee "$ROOTFS_DIR/home/user/Desktop/README.txt" >/dev/null
Welcome to your Custom Wayland ISO!
-----------------------------------
This system is running entirely from RAM.
Changes will be lost upon reboot.

User: user
Pass: password
WELCOME
sudo chroot "$ROOTFS_DIR" chown -R user:user /home/user

###############################################################################
# 5) Create SquashFS
###############################################################################
mkdir -p "$ISO_DIR/live"
echo "=== Compressing Filesystem (SquashFS) ==="
# We use xz compression for better size, though it's slower to build
sudo mksquashfs "$ROOTFS_DIR" "$ISO_DIR/live/filesystem.squashfs" -comp xz -b 1M -noappend

###############################################################################
# 6) Create Initramfs (With Wait-For-Root Fix)
###############################################################################
echo "=== Building Initramfs ==="
mkdir -p "$ISO_DIR/boot"

INITRAMFS_DIR=$(mktemp -d)
cd "$INITRAMFS_DIR"

# Copy busybox
BUSYBOX_HOST="$(command -v busybox || echo /bin/busybox)"
cp "$BUSYBOX_HOST" busybox
chmod +x busybox

# Layout
mkdir -p bin sbin dev proc sys mnt/root mnt/cdrom mnt/squash
ln -s /busybox bin/sh
ln -s /busybox bin/mount
ln -s /busybox bin/umount
ln -s /busybox bin/mkdir
ln -s /busybox bin/ls
ln -s /busybox bin/sleep
ln -s /busybox bin/cp
ln -s /busybox bin/echo
ln -s /busybox bin/mknod
ln -s /busybox sbin/switch_root

# THE INIT SCRIPT
cat <<'INIT_EOF' > init
#!/bin/sh

# 1. Mount virtual filesystems
mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys

echo "----------------------------------------"
echo "   Wayland Live ISO Boot Sequence"
echo "----------------------------------------"

# 2. Wait for CD-ROM (The Fix)
echo "Searching for boot media..."
FOUND_DEV=""
RETRIES=0

# Loop for 10 seconds waiting for /dev/sr0 or similar to appear
while [ -z "$FOUND_DEV" ] && [ $RETRIES -lt 10 ]; do
    sleep 1
    for dev in /dev/sr0 /dev/cdrom /dev/scd0; do
        if [ -b "$dev" ]; then
            # Try mounting
            if mount -t iso9660 -o ro "$dev" /mnt/cdrom 2>/dev/null; then
                if [ -f "/mnt/cdrom/live/filesystem.squashfs" ]; then
                    FOUND_DEV="$dev"
                    echo "Found boot media at $dev"
                    break
                else
                    umount /mnt/cdrom
                fi
            fi
        fi
    done
    RETRIES=$((RETRIES+1))
done

if [ -z "$FOUND_DEV" ]; then
    echo "CRITICAL ERROR: Boot media not found!"
    echo "Dropping to shell..."
    exec /bin/sh
fi

# 3. Mount SquashFS
mount -t squashfs -o loop,ro /mnt/cdrom/live/filesystem.squashfs /mnt/squash

# 4. Copy to RAM (Simple/Robust method)
# Note: This requires 6GB+ RAM allocation in VM
echo "Allocating RAM disk (Size: 6G)..."
mount -t tmpfs -o size=6G tmpfs /mnt/root

echo "Copying system to RAM (Please wait ~1 minute)..."
cp -a /mnt/squash/* /mnt/root/

# 5. Move Mounts and Switch
mkdir -p /mnt/root/dev /mnt/root/proc /mnt/root/sys
mount -t devtmpfs devtmpfs /mnt/root/dev
mount -t proc proc /mnt/root/proc
mount -t sysfs sysfs /mnt/root/sys

echo "Unmounting transition layers..."
umount /mnt/squash
umount /mnt/cdrom

echo "Switching to GNOME Desktop..."
exec switch_root /mnt/root /sbin/init
INIT_EOF

chmod +x init

# Pack initramfs
find . | cpio -o -H newc | gzip > "$ISO_DIR/boot/initrd.img"
cd /
rm -rf "$INITRAMFS_DIR"

###############################################################################
# 7) Final ISO Assembly
###############################################################################
echo "=== Assembling ISO ==="

# Copy Kernel
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"

# Configure GRUB
mkdir -p "$ISO_DIR/boot/grub"
cat <<'GRUB_EOF' > "$ISO_DIR/boot/grub/grub.cfg"
set default=0
set timeout=5

menuentry "GNOME Wayland Live Desktop" {
    linux /boot/vmlinuz console=tty1 quiet splash
    initrd /boot/initrd.img
}

menuentry "GNOME Wayland (Debug Mode)" {
    linux /boot/vmlinuz console=tty1 loglevel=7
    initrd /boot/initrd.img
}
GRUB_EOF

# Build ISO
cd "$ISO_DIR"
grub-mkrescue -o "$ISO_OUT" .

echo ""
echo "#########################################################"
echo "Build Complete: $ISO_OUT"
echo "#########################################################"
echo ""
echo "To run this ISO, you MUST allocate enough RAM (for copy-to-RAM mode):"
echo ""
echo "qemu-system-x86_64 -m 8G -cdrom $ISO_OUT -boot d -enable-kvm -cpu host -smp 4 -vga virtio"
echo ""

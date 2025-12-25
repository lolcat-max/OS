#!/bin/bash
################################################################################
# WAYLAND DESKTOP LIVE ISO - Full Desktop Environment
#
# Uses GNOME on Wayland for a complete desktop experience with:
# - Desktop icons
# - Taskbar/panel
# - System tray
# - File manager
# - Application menu
################################################################################
set -euo pipefail

WORK_DIR="$HOME/custom-linux-build"
ROOTFS_DIR="$HOME/custom-rootfs"
ISO_DIR="$HOME/iso-staging"
ISO_OUT="$HOME/custom-linux-wayland-desktop.iso"
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
# 2) Kernel - Universal GPU support
###############################################################################
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"
git clone --depth 1 https://github.com/torvalds/linux.git
cd linux

make defconfig

# Basic system
./scripts/config --enable CONFIG_DEVTMPFS
./scripts/config --enable CONFIG_DEVTMPFS_MOUNT
./scripts/config --enable CONFIG_PROC_FS
./scripts/config --enable CONFIG_SYSFS
./scripts/config --enable CONFIG_TMPFS
./scripts/config --enable CONFIG_BLK_DEV_INITRD

# Input
./scripts/config --enable CONFIG_INPUT
./scripts/config --enable CONFIG_INPUT_EVDEV
./scripts/config --enable CONFIG_INPUT_KEYBOARD
./scripts/config --enable CONFIG_INPUT_MOUSE

# PCI
./scripts/config --enable CONFIG_PCI
./scripts/config --enable CONFIG_PCIEPORTBUS

# Virtio
./scripts/config --enable CONFIG_VIRTIO
./scripts/config --enable CONFIG_VIRTIO_PCI
./scripts/config --enable CONFIG_VIRTIO_MMIO
./scripts/config --enable CONFIG_VIRTIO_BLK
./scripts/config --enable CONFIG_HW_RANDOM_VIRTIO

# DRM
./scripts/config --enable CONFIG_DRM
./scripts/config --enable CONFIG_DRM_KMS_HELPER
./scripts/config --enable CONFIG_DRM_FBDEV_EMULATION
./scripts/config --enable CONFIG_DRM_FBDEV_LEAK_PHYS_SMEM

# GPU drivers
./scripts/config --enable CONFIG_DRM_I915
./scripts/config --enable CONFIG_DRM_AMDGPU
./scripts/config --enable CONFIG_DRM_RADEON
./scripts/config --enable CONFIG_DRM_NOUVEAU
./scripts/config --enable CONFIG_DRM_VMWGFX
./scripts/config --enable CONFIG_DRM_VMWGFX_FBCON
./scripts/config --enable CONFIG_DRM_VBOXVIDEO
./scripts/config --enable CONFIG_DRM_VIRTIO_GPU
./scripts/config --enable CONFIG_DRM_SIMPLEDRM
./scripts/config --enable CONFIG_DRM_BOCHS

# Framebuffer
./scripts/config --enable CONFIG_FB
./scripts/config --enable CONFIG_FB_SIMPLE
./scripts/config --enable CONFIG_FB_EFI
./scripts/config --enable CONFIG_FB_VESA
./scripts/config --enable CONFIG_FRAMEBUFFER_CONSOLE
./scripts/config --enable CONFIG_FRAMEBUFFER_CONSOLE_DETECT_PRIMARY
./scripts/config --enable CONFIG_LOGO

# VT
./scripts/config --enable CONFIG_VT
./scripts/config --enable CONFIG_VT_CONSOLE
./scripts/config --enable CONFIG_VT_HW_CONSOLE_BINDING

# SquashFS
./scripts/config --enable CONFIG_SQUASHFS
./scripts/config --enable CONFIG_SQUASHFS_XZ

# ISO
./scripts/config --enable CONFIG_ISO9660_FS
./scripts/config --enable CONFIG_JOLIET
./scripts/config --enable CONFIG_BLK_DEV_LOOP

# EFI
./scripts/config --enable CONFIG_EFI
./scripts/config --enable CONFIG_EFI_STUB
./scripts/config --enable CONFIG_EFI_VARS

make olddefconfig
make -j"$JOBS" bzImage modules

###############################################################################
# 3) Root filesystem - GNOME Desktop Environment
###############################################################################
echo "=== Building root filesystem with GNOME desktop ==="
echo "    This will take several minutes..."

sudo mmdebstrap \
  --architecture=amd64 \
  --variant=minbase \
  --include="\
systemd systemd-sysv udev dbus \
gnome-shell gnome-session gnome-terminal nautilus gnome-control-center \
gnome-tweaks gnome-backgrounds gnome-text-editor \
gdm3 \
network-manager \
libwayland-client0 libwayland-server0 wayland-protocols \
libgl1 libegl1 libgles2 libglx0 libglapi-mesa \
libgl1-mesa-dri libegl-mesa0 libglx-mesa0 \
mesa-vulkan-drivers libgbm1 \
libdrm2 libdrm-common libdrm-amdgpu1 libdrm-intel1 libdrm-nouveau2 libdrm-radeon1 \
xwayland \
kmod coreutils bash util-linux procps psmisc \
libpixman-1-0 libpng16-16 libxkbcommon0 libinput10 \
ca-certificates pciutils usbutils \
firmware-linux-free \
fonts-dejavu-core \
" \
  bookworm "$ROOTFS_DIR"

# Install kernel modules
sudo make modules_install -C "$WORK_DIR/linux" INSTALL_MOD_PATH="$ROOTFS_DIR"

###############################################################################
# 4) System configuration
###############################################################################
sudo chroot "$ROOTFS_DIR" /bin/bash <<'EOF'
set -e

# Groups
groupadd -f video
groupadd -f render
groupadd -f input

# User with proper groups for desktop
useradd -m -u 1000 -s /bin/bash -G video,render,input,audio,netdev user
echo user:password | chpasswd

# Enable services
systemctl enable gdm3
systemctl enable NetworkManager

# Rebuild library cache
ldconfig

# Udev rules
mkdir -p /etc/udev/rules.d
cat > /etc/udev/rules.d/70-drm.rules <<'UDEV_EOF'
SUBSYSTEM=="drm", KERNEL=="card[0-9]*", TAG+="seat", TAG+="master-of-seat", GROUP="video", MODE="0666"
SUBSYSTEM=="drm", KERNEL=="renderD[0-9]*", GROUP="render", MODE="0666"
SUBSYSTEM=="input", TAG+="seat", GROUP="input", MODE="0660"
KERNEL=="event[0-9]*", TAG+="seat", GROUP="input", MODE="0660"
UDEV_EOF

# Configure GDM to use Wayland
mkdir -p /etc/gdm3
cat > /etc/gdm3/custom.conf <<'GDM_EOF'
[daemon]
WaylandEnable=true
AutomaticLoginEnable=true
AutomaticLogin=user

[security]

[xdmcp]

[chooser]

[debug]
GDM_EOF

# Set GNOME to prefer Wayland
mkdir -p /etc/environment.d
cat > /etc/environment.d/gnome-wayland.conf <<'ENV_EOF'
GDK_BACKEND=wayland
QT_QPA_PLATFORM=wayland
MOZ_ENABLE_WAYLAND=1
ENV_EOF

EOF

# Create a welcome message on desktop
sudo mkdir -p "$ROOTFS_DIR/home/user/Desktop"
cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/Desktop/Welcome.txt" >/dev/null
╔════════════════════════════════════════╗
║     Welcome to Wayland Live Desktop    ║
╚════════════════════════════════════════╝

This is a live Debian system running GNOME on Wayland.

Features:
• Full desktop environment with taskbar and system tray
• File manager (Nautilus)
• Terminal (GNOME Terminal)
• Settings panel
• Application launcher (Activities)

Quick Tips:
• Press the Super/Windows key to open Activities
• Click "Show Applications" (9 dots) for the app menu
• Top bar shows system status and date/time
• Files are accessible through the file manager

Default user: user
Password: password

Enjoy your Wayland desktop!
EOF

sudo chroot "$ROOTFS_DIR" chown -R user:user /home/user

###############################################################################
# 5) SquashFS
###############################################################################
mkdir -p "$ISO_DIR/live"
echo "=== Creating SquashFS (this will take several minutes) ==="
sudo mksquashfs "$ROOTFS_DIR" "$ISO_DIR/live/filesystem.squashfs" -comp xz -b 1M -noappend
echo "=== SquashFS created ==="
ls -lh "$ISO_DIR/live/filesystem.squashfs"

###############################################################################
# 6) Initramfs
###############################################################################
mkdir -p "$ISO_DIR/boot"

INITRAMFS_DIR=$(mktemp -d)
cd "$INITRAMFS_DIR"

BUSYBOX_HOST="$(command -v busybox || echo /bin/busybox)"
cp "$BUSYBOX_HOST" busybox
chmod +x busybox

mkdir -p bin sbin dev proc sys mnt/root

ln -s /busybox bin/sh
ln -s /busybox bin/mount
ln -s /busybox bin/mkdir
ln -s /busybox bin/sleep
ln -s /busybox bin/ls
ln -s /busybox bin/cat
ln -s /busybox bin/echo
ln -s /busybox bin/cp
ln -s /busybox sbin/switch_root

cat <<'INIT_EOF' > init
#!/bin/sh

mkdir -p /dev /proc /sys /mnt/cdrom /mnt/squash /mnt/root

mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys

echo "╔════════════════════════════════════════╗"
echo "║   Booting Wayland Desktop Live CD      ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Please wait, loading desktop environment..."
sleep 2

# Check for GPU
if [ -c /dev/dri/card0 ]; then
    echo "✓ Graphics hardware detected"
else
    echo "! Using software rendering"
fi

# Mount CD-ROM
for dev in /dev/sr0 /dev/cdrom /dev/scd0; do
    if [ -b "$dev" ]; then
        if mount -t iso9660 -o ro "$dev" /mnt/cdrom 2>/dev/null; then
            break
        fi
    fi
done

if [ ! -f /mnt/cdrom/live/filesystem.squashfs ]; then
    echo "ERROR: filesystem.squashfs not found"
    exec /bin/sh
fi

mount -t squashfs -o loop,ro /mnt/cdrom/live/filesystem.squashfs /mnt/squash

# Need larger tmpfs for full desktop
mount -t tmpfs -o size=6G tmpfs /mnt/root

echo "Copying desktop system (may take 1-2 minutes)..."
for dir in bin sbin lib lib64 usr etc opt var home root; do
    if [ -d "/mnt/squash/$dir" ]; then
        mkdir -p "/mnt/root/$dir"
        cp -a "/mnt/squash/$dir"/* "/mnt/root/$dir/" 2>/dev/null || true
    fi
done

mkdir -p /mnt/root/{dev,proc,sys,run,tmp}
mount -t proc proc /mnt/root/proc
mount -t sysfs sysfs /mnt/root/sys
mount -t devtmpfs devtmpfs /mnt/root/dev
mount -t tmpfs tmpfs /mnt/root/run
mount -t tmpfs tmpfs /mnt/root/tmp

echo "Starting desktop..."
exec switch_root /mnt/root /sbin/init
INIT_EOF

chmod +x init

find . | cpio -o -H newc | gzip > "$ISO_DIR/boot/initrd.img"
cd /
rm -rf "$INITRAMFS_DIR"

###############################################################################
# 7) Kernel
###############################################################################
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"

###############################################################################
# 8) GRUB
###############################################################################
mkdir -p "$ISO_DIR/boot/grub"
cat <<'EOF' > "$ISO_DIR/boot/grub/grub.cfg"
set default=0
set timeout=5

menuentry "GNOME Wayland Desktop (Live)" {
  linux /boot/vmlinuz console=tty1 quiet splash
  initrd /boot/initrd.img
}

menuentry "GNOME Wayland Desktop (safe mode)" {
  linux /boot/vmlinuz console=tty1 quiet nomodeset
  initrd /boot/initrd.img
}

menuentry "GNOME Wayland Desktop (debug)" {
  linux /boot/vmlinuz console=tty1 console=ttyS0,115200 loglevel=7
  initrd /boot/initrd.img
}
EOF

###############################################################################
# 9) Build ISO
###############################################################################
cd "$ISO_DIR"
grub-mkrescue -o "$ISO_OUT" .

echo ""
echo "╔════════════════════════════════════════╗"
echo "║   DESKTOP ISO BUILD COMPLETE! 🎉      ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "ISO: $ISO_OUT"
ls -lh "$ISO_OUT"
echo ""
echo "This ISO includes:"
echo "  • GNOME Desktop Environment"
echo "  • Wayland display server"
echo "  • Automatic login"
echo "  • Desktop icons and taskbar"
echo "  • File manager, terminal, settings"
echo ""
echo "Test with QEMU (recommend 6GB+ RAM):"
echo "  qemu-system-x86_64 -m 6G -cdrom \"$ISO_OUT\" -boot d \\"
echo "    -enable-kvm -cpu host -smp 4 -vga virtio"
echo ""
echo "Note: Full desktop requires more resources than minimal WM"
echo "      Minimum: 4GB RAM, Recommended: 6-8GB RAM"
echo ""

#!/bin/bash
################################################################################
# MINIMAL WAYLAND LIVE ISO - FIXED DISPLAY
#
# Fixes:
# - Proper kernel DRM/KMS configuration
# - Video group permissions for /dev/dri
# - Seatd socket permissions
# - XDG_RUNTIME_DIR setup
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
# 2) Kernel (Complete DRM/KMS + Virtio setup)
###############################################################################
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"
git clone --depth 1 https://github.com/torvalds/linux.git
cd linux

make defconfig

# Basic system support
./scripts/config --enable CONFIG_DEVTMPFS
./scripts/config --enable CONFIG_DEVTMPFS_MOUNT
./scripts/config --enable CONFIG_PROC_FS
./scripts/config --enable CONFIG_SYSFS
./scripts/config --enable CONFIG_TMPFS
./scripts/config --enable CONFIG_BLK_DEV_INITRD

# Input devices
./scripts/config --enable CONFIG_INPUT
./scripts/config --enable CONFIG_INPUT_EVDEV
./scripts/config --enable CONFIG_INPUT_KEYBOARD
./scripts/config --enable CONFIG_INPUT_MOUSE

# Virtio support (CRITICAL for QEMU)
./scripts/config --enable CONFIG_VIRTIO
./scripts/config --enable CONFIG_VIRTIO_PCI
./scripts/config --enable CONFIG_VIRTIO_MMIO
./scripts/config --enable CONFIG_VIRTIO_BLK
./scripts/config --enable CONFIG_VIRTIO_NET
./scripts/config --enable CONFIG_HW_RANDOM_VIRTIO

# DRM core (CRITICAL for graphics)
./scripts/config --enable CONFIG_DRM
./scripts/config --enable CONFIG_DRM_KMS_HELPER
./scripts/config --enable CONFIG_DRM_FBDEV_EMULATION

# Virtio GPU driver (for QEMU -vga virtio)
./scripts/config --enable CONFIG_DRM_VIRTIO_GPU

# Additional DRM drivers for broader compatibility
./scripts/config --enable CONFIG_DRM_SIMPLEDRM
./scripts/config --enable CONFIG_DRM_BOCHS

# Framebuffer console
./scripts/config --enable CONFIG_FB
./scripts/config --enable CONFIG_FB_SIMPLE
./scripts/config --enable CONFIG_FRAMEBUFFER_CONSOLE
./scripts/config --enable CONFIG_FRAMEBUFFER_CONSOLE_DETECT_PRIMARY

# VT (Virtual Terminal) support
./scripts/config --enable CONFIG_VT
./scripts/config --enable CONFIG_VT_CONSOLE
./scripts/config --enable CONFIG_VT_HW_CONSOLE_BINDING

# SquashFS support
./scripts/config --enable CONFIG_SQUASHFS
./scripts/config --enable CONFIG_SQUASHFS_XZ
./scripts/config --enable CONFIG_SQUASHFS_ZLIB

# ISO9660 / loop device support
./scripts/config --enable CONFIG_ISO9660_FS
./scripts/config --enable CONFIG_JOLIET
./scripts/config --enable CONFIG_BLK_DEV_LOOP

# Build as modules where possible for troubleshooting
./scripts/config --module CONFIG_DRM_VIRTIO_GPU

make olddefconfig
make -j"$JOBS" bzImage modules

###############################################################################
# 3) Root filesystem (Wayland + Sway with all dependencies)
###############################################################################
sudo mmdebstrap \
  --architecture=amd64 \
  --variant=minbase \
  --include="\
systemd systemd-sysv udev dbus seatd \
sway foot \
libwayland-client0 libwayland-server0 wayland-protocols \
libgl1 libegl1 libgles2 libgl1-mesa-dri mesa-vulkan-drivers \
libgbm1 libdrm2 libdrm-common \
xwayland \
kmod coreutils bash util-linux \
" \
  bookworm "$ROOTFS_DIR"

# Install kernel modules
sudo make modules_install -C "$WORK_DIR/linux" INSTALL_MOD_PATH="$ROOTFS_DIR"

###############################################################################
# 4) User + services + permissions
###############################################################################
sudo chroot "$ROOTFS_DIR" /bin/bash <<'EOF'
set -e

# Create groups
groupadd -f video
groupadd -f render
groupadd -f input
groupadd -f seat

# Create user with proper groups
useradd -m -u 1000 -s /bin/bash -G video,render,input,seat user
echo user:password | chpasswd

# Enable services
systemctl enable seatd
systemctl enable getty@tty1

# Create udev rules for seat permissions
mkdir -p /etc/udev/rules.d
cat > /etc/udev/rules.d/70-seat.rules <<'UDEV_EOF'
# DRI devices
SUBSYSTEM=="drm", KERNEL=="card[0-9]*", TAG+="seat", TAG+="master-of-seat"
SUBSYSTEM=="drm", KERNEL=="renderD[0-9]*", GROUP="render", MODE="0660"

# Input devices
SUBSYSTEM=="input", TAG+="seat"
KERNEL=="event[0-9]*", TAG+="seat"

# Graphics devices
SUBSYSTEM=="graphics", TAG+="seat"
UDEV_EOF

# Create seatd config
mkdir -p /etc/seatd
cat > /etc/seatd/seatd.conf <<'SEATD_EOF'
# Allow user group to access seatd
user = root
group = seat
SEATD_EOF

EOF

###############################################################################
# 5) Autologin + Sway startup script
###############################################################################
sudo mkdir -p "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d"
cat <<'EOF' | sudo tee "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d/override.conf" >/dev/null
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin user --noclear %I $TERM
Type=simple
EOF

cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/.bash_profile" >/dev/null
# Auto-start Sway on tty1
if [ -z "${WAYLAND_DISPLAY:-}" ] && [ "$(tty)" = "/dev/tty1" ]; then
  # Set up XDG_RUNTIME_DIR
  export XDG_RUNTIME_DIR=/run/user/1000
  mkdir -p "$XDG_RUNTIME_DIR"
  chmod 0700 "$XDG_RUNTIME_DIR"
  chown 1000:1000 "$XDG_RUNTIME_DIR"
  
  # Set required environment variables
  export XDG_SESSION_TYPE=wayland
  export XDG_SESSION_CLASS=user
  export XDG_CURRENT_DESKTOP=sway
  
  # Make sure DRI devices are accessible
  if [ -e /dev/dri/card0 ]; then
    echo "Found GPU at /dev/dri/card0"
  else
    echo "WARNING: No GPU device found at /dev/dri/card0"
  fi
  
  # Start sway with dbus and seatd
  echo "Starting Sway..."
  exec dbus-run-session sway 2>&1 | tee /tmp/sway.log
fi
EOF
sudo chroot "$ROOTFS_DIR" chown user:user /home/user/.bash_profile

# Create a basic sway config
sudo mkdir -p "$ROOTFS_DIR/home/user/.config/sway"
cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/.config/sway/config" >/dev/null
# Sway config for live CD

# Set mod key (Mod4 = Super/Windows key)
set $mod Mod4

# Start a terminal with Mod+Enter
bindsym $mod+Return exec foot

# Kill focused window with Mod+Shift+Q
bindsym $mod+Shift+q kill

# Reload config with Mod+Shift+C
bindsym $mod+Shift+c reload

# Exit sway with Mod+Shift+E
bindsym $mod+Shift+e exit

# Moving around
bindsym $mod+Left focus left
bindsym $mod+Down focus down
bindsym $mod+Up focus up
bindsym $mod+Right focus right

# Output configuration (auto-detect)
output * bg #000000 solid_color

# Status bar
bar {
    status_command while date +'%Y-%m-%d %H:%M:%S'; do sleep 1; done
    position top
}

# Auto-start message
exec foot sh -c 'echo "Welcome to Sway on Wayland!"; echo ""; echo "Useful keybindings:"; echo "  Mod+Enter: Open terminal"; echo "  Mod+Shift+Q: Close window"; echo "  Mod+Shift+E: Exit Sway"; echo ""; echo "Check /tmp/sway.log for any errors"; exec bash'
EOF
sudo chroot "$ROOTFS_DIR" chown -R user:user /home/user/.config

###############################################################################
# 6) Create SquashFS image
###############################################################################
mkdir -p "$ISO_DIR/live"
echo "=== Creating SquashFS image ==="
sudo mksquashfs "$ROOTFS_DIR" "$ISO_DIR/live/filesystem.squashfs" -comp xz -b 1M
echo "=== SquashFS created ==="
ls -lh "$ISO_DIR/live/filesystem.squashfs"

###############################################################################
# 7) Create minimal initramfs with module loading
###############################################################################
mkdir -p "$ISO_DIR/boot"

INITRAMFS_DIR=$(mktemp -d)
cd "$INITRAMFS_DIR"

# Get busybox
BUSYBOX_HOST="$(command -v busybox || echo /bin/busybox)"
cp "$BUSYBOX_HOST" busybox
chmod +x busybox

# Create directory structure
mkdir -p bin sbin dev proc sys mnt/root newroot lib/modules

# Create symlinks for busybox applets
ln -s /busybox bin/sh
ln -s /busybox bin/mount
ln -s /busybox bin/mkdir
ln -s /busybox bin/sleep
ln -s /busybox bin/ls
ln -s /busybox bin/cat
ln -s /busybox bin/echo
ln -s /busybox bin/modprobe
ln -s /busybox sbin/switch_root

# Create init script with module loading
cat <<'INIT_EOF' > init
#!/bin/sh
# Live CD init with GPU module loading

# Mount essential filesystems
mkdir -p /dev /proc /sys /mnt/cdrom /mnt/squash /mnt/root

mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys

echo "=== Wayland Live CD Boot ==="
echo "Waiting for devices..."
sleep 2

# Find and mount the CD-ROM
for dev in /dev/sr0 /dev/cdrom /dev/scd0; do
    if [ -b "$dev" ]; then
        echo "Mounting CD-ROM from $dev..."
        if mount -t iso9660 -o ro "$dev" /mnt/cdrom 2>/dev/null; then
            echo "CD-ROM mounted successfully"
            break
        fi
    fi
done

# Check for squashfs
if [ ! -f /mnt/cdrom/live/filesystem.squashfs ]; then
    echo "ERROR: Could not find filesystem.squashfs"
    exec /bin/sh
fi

# Mount squashfs
echo "Mounting root filesystem..."
mount -t squashfs -o loop,ro /mnt/cdrom/live/filesystem.squashfs /mnt/squash

# Setup tmpfs root
echo "Setting up tmpfs root..."
mount -t tmpfs -o size=2G tmpfs /mnt/root

# Copy filesystem
echo "Copying system files (this may take a moment)..."
for dir in bin sbin lib lib64 usr etc opt var home root; do
    if [ -d "/mnt/squash/$dir" ]; then
        mkdir -p "/mnt/root/$dir"
        cp -a "/mnt/squash/$dir"/* "/mnt/root/$dir/" 2>/dev/null || true
    fi
done

# Prepare mounts
mkdir -p /mnt/root/{dev,proc,sys,run,tmp}
mount -t proc proc /mnt/root/proc
mount -t sysfs sysfs /mnt/root/sys
mount -t devtmpfs devtmpfs /mnt/root/dev
mount -t tmpfs tmpfs /mnt/root/run
mount -t tmpfs tmpfs /mnt/root/tmp

echo "Switching to new root and starting systemd..."
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

###############################################################################
# 8) Copy kernel
###############################################################################
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"

###############################################################################
# 9) GRUB config
###############################################################################
mkdir -p "$ISO_DIR/boot/grub"
cat <<'EOF' > "$ISO_DIR/boot/grub/grub.cfg"
set default=0
set timeout=5

menuentry "Sway Wayland Live" {
  linux /boot/vmlinuz console=tty1 quiet splash
  initrd /boot/initrd.img
}

menuentry "Sway Wayland Live (verbose)" {
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
echo "✓✓✓ ISO BUILD COMPLETE ✓✓✓"
echo
echo "ISO: $ISO_OUT"
ls -lh "$ISO_OUT"
echo
echo "=== QEMU Test Commands ==="
echo
echo "Standard boot:"
echo "  qemu-system-x86_64 -m 2G -cdrom \"$ISO_OUT\" -boot d -enable-kvm -cpu host -smp 2 -vga virtio"
echo
echo "With serial console (for debugging):"
echo "  qemu-system-x86_64 -m 4G -cdrom \"$ISO_OUT\" -boot d -enable-kvm -cpu host -smp 2 -vga virtio -serial mon:stdio"
echo

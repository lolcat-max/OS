#!/bin/bash
################################################################################
# MINIMAL WAYLAND LIVE ISO - COMPLETE RENDERER FIX
#
# This version ensures all renderer requirements are met:
# - Complete wlroots dependencies
# - Proper Mesa/EGL/GBM setup
# - Fallback compositor (cage) if sway fails
# - Software rendering support
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
# 2) Kernel - All graphics drivers BUILT-IN (not modules)
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

# Virtio (CRITICAL - all built-in)
./scripts/config --enable CONFIG_VIRTIO
./scripts/config --enable CONFIG_VIRTIO_PCI
./scripts/config --enable CONFIG_VIRTIO_MMIO
./scripts/config --enable CONFIG_VIRTIO_BLK
./scripts/config --enable CONFIG_HW_RANDOM_VIRTIO

# DRM - ALL BUILT-IN, NOT MODULES
./scripts/config --enable CONFIG_DRM
./scripts/config --enable CONFIG_DRM_KMS_HELPER
./scripts/config --enable CONFIG_DRM_FBDEV_EMULATION
./scripts/config --enable CONFIG_DRM_FBDEV_LEAK_PHYS_SMEM

# Virtio GPU - BUILT-IN
./scripts/config --enable CONFIG_DRM_VIRTIO_GPU

# Fallback drivers - BUILT-IN
./scripts/config --enable CONFIG_DRM_SIMPLEDRM
./scripts/config --enable CONFIG_DRM_BOCHS

# Framebuffer
./scripts/config --enable CONFIG_FB
./scripts/config --enable CONFIG_FB_SIMPLE
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

make olddefconfig
make -j"$JOBS" bzImage modules

###############################################################################
# 3) Root filesystem - COMPLETE stack with fallback compositor
###############################################################################
sudo mmdebstrap \
  --architecture=amd64 \
  --variant=minbase \
  --include="\
systemd systemd-sysv udev dbus seatd \
sway foot cage weston \
libwayland-client0 libwayland-server0 wayland-protocols \
libgl1 libegl1 libgles2 libglx0 libglapi-mesa \
libgl1-mesa-dri libegl-mesa0 libglx-mesa0 \
mesa-vulkan-drivers libgbm1 \
libdrm2 libdrm-common libdrm-amdgpu1 libdrm-intel1 libdrm-nouveau2 libdrm-radeon1 \
xwayland \
kmod coreutils bash util-linux procps psmisc \
libpixman-1-0 libpng16-16 libxkbcommon0 libinput10 \
libxcb1 libx11-6 libxfixes3 libxcb-render0 libxcb-shm0 libxcb-composite0 \
libjson-c5 libpcre2-8-0 libcairo2 libpango-1.0-0 \
ca-certificates pciutils usbutils \
firmware-linux-free strace \
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
groupadd -f seat

# User
useradd -m -u 1000 -s /bin/bash -G video,render,input,seat user
echo user:password | chpasswd

# Services
systemctl enable seatd
systemctl enable getty@tty1

# Rebuild library cache (CRITICAL)
ldconfig

# Udev rules for DRI devices
mkdir -p /etc/udev/rules.d
cat > /etc/udev/rules.d/70-drm.rules <<'UDEV_EOF'
# DRI devices - make accessible
SUBSYSTEM=="drm", KERNEL=="card[0-9]*", TAG+="seat", TAG+="master-of-seat", GROUP="video", MODE="0666"
SUBSYSTEM=="drm", KERNEL=="renderD[0-9]*", GROUP="render", MODE="0666"
SUBSYSTEM=="input", TAG+="seat", GROUP="input", MODE="0660"
KERNEL=="event[0-9]*", TAG+="seat", GROUP="input", MODE="0660"
UDEV_EOF

# Seatd config
mkdir -p /etc/seatd
cat > /etc/seatd/seatd.conf <<'SEATD_EOF'
user = root
group = seat
SEATD_EOF

EOF

###############################################################################
# 5) Startup script with multiple compositor fallbacks
###############################################################################
sudo mkdir -p "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d"
cat <<'EOF' | sudo tee "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d/override.conf" >/dev/null
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin user --noclear %I $TERM
Type=simple
EOF

cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/.bash_profile" >/dev/null
# Auto-start Wayland compositor on tty1
if [ -z "${WAYLAND_DISPLAY:-}" ] && [ "$(tty)" = "/dev/tty1" ]; then
  
  echo "╔════════════════════════════════════════╗"
  echo "║   Wayland Compositor Auto-Start       ║"
  echo "╚════════════════════════════════════════╝"
  echo ""
  
  # Runtime directory
  export XDG_RUNTIME_DIR=/run/user/1000
  mkdir -p "$XDG_RUNTIME_DIR"
  chmod 0700 "$XDG_RUNTIME_DIR"
  
  # Session variables
  export XDG_SESSION_TYPE=wayland
  export XDG_SESSION_CLASS=user
  export XDG_CURRENT_DESKTOP=sway
  
  # Seatd
  export SEATD_SOCK=/run/seatd.sock
  
  # Start seatd if not running
  if ! systemctl is-active --quiet seatd; then
    echo "Starting seatd..."
    sudo systemctl start seatd
    sleep 1
  fi
  
  # Diagnostics
  echo "System diagnostics:"
  echo "-------------------"
  
  if [ -e /dev/dri/card0 ]; then
    echo "✓ GPU: /dev/dri/card0"
    ls -la /dev/dri/ 2>/dev/null | head -5
  else
    echo "✗ No /dev/dri/card0 - will use software rendering"
  fi
  
  if [ -e /dev/dri/renderD128 ]; then
    echo "✓ Render node: /dev/dri/renderD128"
  fi
  
  if [ -S /run/seatd.sock ]; then
    echo "✓ Seatd socket: /run/seatd.sock"
    ls -la /run/seatd.sock
  else
    echo "✗ No seatd socket!"
  fi
  
  echo ""
  echo "Mesa drivers:"
  find /usr/lib -name "*_dri.so" 2>/dev/null | head -3 || echo "  (none found)"
  echo ""
  
  # Try compositors in order
  echo "Attempting to start compositor..."
  echo ""
  
  # Function to try a compositor
  try_compositor() {
    local name=$1
    local cmd=$2
    
    echo "→ Trying $name..."
    
    # Set environment for this attempt
    if [ -e /dev/dri/card0 ]; then
      export WLR_RENDERER=gles2
      export LIBGL_ALWAYS_SOFTWARE=0
      export WLR_NO_HARDWARE_CURSORS=1
    else
      export WLR_RENDERER=pixman
      export LIBGL_ALWAYS_SOFTWARE=1
    fi
    
    export WLR_BACKENDS=drm,libinput
    
    # Try to start
    if eval "$cmd" 2>&1 | tee "/tmp/$name.log" & then
      local pid=$!
      sleep 3
      if kill -0 $pid 2>/dev/null; then
        echo "✓ $name started successfully (PID: $pid)"
        wait $pid
        return 0
      else
        echo "✗ $name failed to start"
        return 1
      fi
    fi
    return 1
  }
  
  # Try Sway first
  if try_compositor "sway" "dbus-run-session sway -d"; then
    exit 0
  fi
  
  echo ""
  echo "Sway failed, trying Cage with foot terminal..."
  
  # Try Cage (simpler compositor) with a terminal
  if try_compositor "cage" "dbus-run-session cage -d -- foot"; then
    exit 0
  fi
  
  echo ""
  echo "Cage failed, trying Weston..."
  
  # Try Weston (most compatible)
  if try_compositor "weston" "weston --backend=drm-backend.so"; then
    exit 0
  fi
  
  # All failed
  echo ""
  echo "╔════════════════════════════════════════╗"
  echo "║  All compositors failed to start!     ║"
  echo "╚════════════════════════════════════════╝"
  echo ""
  echo "Logs saved to:"
  echo "  /tmp/sway.log"
  echo "  /tmp/cage.log"
  echo "  /tmp/weston.log"
  echo ""
  echo "Dropping to shell for debugging..."
  echo "You can try manually:"
  echo "  WLR_RENDERER=pixman LIBGL_ALWAYS_SOFTWARE=1 sway"
  echo ""
  
  exec /bin/bash
fi
EOF
sudo chroot "$ROOTFS_DIR" chown user:user /home/user/.bash_profile

# Sway config
sudo mkdir -p "$ROOTFS_DIR/home/user/.config/sway"
cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/.config/sway/config" >/dev/null
# Minimal Sway config

set $mod Mod4

# Terminal
bindsym $mod+Return exec foot

# Kill
bindsym $mod+Shift+q kill

# Exit
bindsym $mod+Shift+e exit

# Movement
bindsym $mod+Left focus left
bindsym $mod+Down focus down
bindsym $mod+Up focus up
bindsym $mod+Right focus right

# Output
output * bg #1a1a2e solid_color

# Bar
bar {
    position top
    status_command while date +'%Y-%m-%d %H:%M:%S - Sway Wayland'; do sleep 1; done
    colors {
        background #000000
        statusline #ffffff
    }
}

# Welcome
exec foot sh -c 'echo "╔════════════════════════════════════════╗"; echo "║        Sway Wayland Live CD           ║"; echo "╚════════════════════════════════════════╝"; echo ""; echo "Keys: Mod+Enter=terminal, Mod+Shift+Q=close, Mod+Shift+E=exit"; echo ""; exec bash'
EOF
sudo chroot "$ROOTFS_DIR" chown -R user:user /home/user/.config

###############################################################################
# 6) SquashFS
###############################################################################
mkdir -p "$ISO_DIR/live"
echo "=== Creating SquashFS ==="
sudo mksquashfs "$ROOTFS_DIR" "$ISO_DIR/live/filesystem.squashfs" -comp xz -b 1M -noappend
ls -lh "$ISO_DIR/live/filesystem.squashfs"

###############################################################################
# 7) Initramfs
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

echo "=== Booting Wayland Live CD ==="
sleep 3

# Check for GPU
if [ -c /dev/dri/card0 ]; then
    echo "✓ Found GPU device"
    ls -la /dev/dri/
else
    echo "! No GPU hardware detected (will use software rendering)"
fi

# Mount CD
for dev in /dev/sr0 /dev/cdrom /dev/scd0; do
    if [ -b "$dev" ]; then
        if mount -t iso9660 -o ro "$dev" /mnt/cdrom 2>/dev/null; then
            echo "✓ Mounted CD-ROM"
            break
        fi
    fi
done

if [ ! -f /mnt/cdrom/live/filesystem.squashfs ]; then
    echo "ERROR: No filesystem.squashfs"
    exec /bin/sh
fi

mount -t squashfs -o loop,ro /mnt/cdrom/live/filesystem.squashfs /mnt/squash

mount -t tmpfs -o size=4G tmpfs /mnt/root

echo "Copying system files..."
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

echo "Starting system..."
exec switch_root /mnt/root /sbin/init
INIT_EOF

chmod +x init

find . | cpio -o -H newc | gzip > "$ISO_DIR/boot/initrd.img"
cd /
rm -rf "$INITRAMFS_DIR"

###############################################################################
# 8) Kernel
###############################################################################
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"

###############################################################################
# 9) GRUB
###############################################################################
mkdir -p "$ISO_DIR/boot/grub"
cat <<'EOF' > "$ISO_DIR/boot/grub/grub.cfg"
set default=0
set timeout=5

menuentry "Wayland Live (Sway/Cage/Weston)" {
  linux /boot/vmlinuz console=tty1 quiet video=efifb:off video=simplefb:off
  initrd /boot/initrd.img
}

menuentry "Wayland Live (verbose output)" {
  linux /boot/vmlinuz console=tty1 console=ttyS0,115200 loglevel=7 video=efifb:off video=simplefb:off
  initrd /boot/initrd.img
}
EOF

###############################################################################
# 10) Build ISO
###############################################################################
cd "$ISO_DIR"
grub-mkrescue -o "$ISO_OUT" .

echo ""
echo "╔════════════════════════════════════════╗"
echo "║         BUILD COMPLETE! 🎉            ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "ISO: $ISO_OUT"
ls -lh "$ISO_OUT"
echo ""
echo "Test command:"
echo "  qemu-system-x86_64 -m 4G -cdrom \"$ISO_OUT\" -boot d \\"
echo "    -enable-kvm -cpu host -smp 4 -vga virtio"
echo ""
echo "Note: This ISO will try Sway → Cage → Weston in sequence"
echo "      Logs saved to /tmp/{sway,cage,weston}.log"
echo ""

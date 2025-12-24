#!/bin/bash
################################################################################
# WAYLAND LIVE ISO - REAL HARDWARE & VMWARE SUPPORT
#
# This version includes:
# - Intel/AMD/NVIDIA graphics drivers
# - SATA/AHCI/NVMe storage controllers
# - VMware vmwgfx driver
# - USB storage and keyboard/mouse
# - Network drivers
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
# 2) Kernel - COMPREHENSIVE HARDWARE SUPPORT
###############################################################################
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"
git clone --depth 1 https://github.com/torvalds/linux.git
cd linux

make defconfig

echo "Configuring kernel for real hardware support..."

# ============================================================================
# BASIC SYSTEM
# ============================================================================
./scripts/config --enable CONFIG_DEVTMPFS
./scripts/config --enable CONFIG_DEVTMPFS_MOUNT
./scripts/config --enable CONFIG_PROC_FS
./scripts/config --enable CONFIG_SYSFS
./scripts/config --enable CONFIG_TMPFS
./scripts/config --enable CONFIG_BLK_DEV_INITRD

# ============================================================================
# STORAGE CONTROLLERS (CRITICAL FOR BOOTING)
# ============================================================================
# SATA/AHCI (most common on desktops/laptops)
./scripts/config --enable CONFIG_ATA
./scripts/config --enable CONFIG_ATA_ACPI
./scripts/config --enable CONFIG_SATA_AHCI
./scripts/config --enable CONFIG_ATA_PIIX
./scripts/config --enable CONFIG_SATA_AHCI_PLATFORM

# NVMe (modern SSDs)
./scripts/config --enable CONFIG_BLK_DEV_NVME

# SCSI support (required by many drivers)
./scripts/config --enable CONFIG_SCSI
./scripts/config --enable CONFIG_BLK_DEV_SD
./scripts/config --enable CONFIG_SCSI_MOD

# USB storage
./scripts/config --enable CONFIG_USB_STORAGE

# CD-ROM support
./scripts/config --enable CONFIG_BLK_DEV_SR
./scripts/config --enable CONFIG_CHR_DEV_SG

# ============================================================================
# USB SUPPORT (keyboards, mice, storage)
# ============================================================================
./scripts/config --enable CONFIG_USB
./scripts/config --enable CONFIG_USB_XHCI_HCD
./scripts/config --enable CONFIG_USB_EHCI_HCD
./scripts/config --enable CONFIG_USB_OHCI_HCD
./scripts/config --enable CONFIG_USB_UHCI_HCD

# ============================================================================
# INPUT DEVICES
# ============================================================================
./scripts/config --enable CONFIG_INPUT
./scripts/config --enable CONFIG_INPUT_EVDEV
./scripts/config --enable CONFIG_INPUT_KEYBOARD
./scripts/config --enable CONFIG_INPUT_MOUSE
./scripts/config --enable CONFIG_USB_HID
./scripts/config --enable CONFIG_HID
./scripts/config --enable CONFIG_HID_GENERIC
./scripts/config --enable CONFIG_HID_CHERRY
./scripts/config --enable CONFIG_HID_LOGITECH
./scripts/config --enable CONFIG_HID_MICROSOFT

# ============================================================================
# PCI/ACPI SUPPORT
# ============================================================================
./scripts/config --enable CONFIG_PCI
./scripts/config --enable CONFIG_ACPI
./scripts/config --enable CONFIG_ACPI_VIDEO
./scripts/config --enable CONFIG_PCIE_ASPM

# ============================================================================
# NETWORK (for updates/troubleshooting)
# ============================================================================
./scripts/config --enable CONFIG_NETDEVICES
./scripts/config --enable CONFIG_ETHERNET
./scripts/config --enable CONFIG_NET_VENDOR_INTEL
./scripts/config --enable CONFIG_E1000
./scripts/config --enable CONFIG_E1000E
./scripts/config --enable CONFIG_NET_VENDOR_REALTEK
./scripts/config --enable CONFIG_R8169
./scripts/config --enable CONFIG_NET_VENDOR_BROADCOM

# Wireless (common on laptops)
./scripts/config --enable CONFIG_WLAN
./scripts/config --enable CONFIG_CFG80211
./scripts/config --enable CONFIG_MAC80211

# ============================================================================
# DRM CORE
# ============================================================================
./scripts/config --enable CONFIG_DRM
./scripts/config --enable CONFIG_DRM_KMS_HELPER
./scripts/config --enable CONFIG_DRM_FBDEV_EMULATION
./scripts/config --enable CONFIG_DRM_FBDEV_LEAK_PHYS_SMEM

# ============================================================================
# INTEL GRAPHICS (most common on laptops/desktops)
# ============================================================================
./scripts/config --enable CONFIG_DRM_I915
./scripts/config --enable CONFIG_DRM_I915_GVT

# ============================================================================
# AMD GRAPHICS (Radeon and AMDGPU)
# ============================================================================
./scripts/config --enable CONFIG_DRM_RADEON
./scripts/config --enable CONFIG_DRM_AMDGPU
./scripts/config --enable CONFIG_DRM_AMDGPU_SI
./scripts/config --enable CONFIG_DRM_AMDGPU_CIK

# ============================================================================
# NVIDIA (Nouveau open-source driver)
# ============================================================================
./scripts/config --enable CONFIG_DRM_NOUVEAU

# ============================================================================
# VMWARE GRAPHICS
# ============================================================================
./scripts/config --enable CONFIG_DRM_VMWGFX
./scripts/config --enable CONFIG_DRM_VMWGFX_FBCON

# ============================================================================
# QEMU/VIRTIO (for testing)
# ============================================================================
./scripts/config --enable CONFIG_VIRTIO
./scripts/config --enable CONFIG_VIRTIO_PCI
./scripts/config --enable CONFIG_VIRTIO_MMIO
./scripts/config --enable CONFIG_VIRTIO_BLK
./scripts/config --enable CONFIG_VIRTIO_NET
./scripts/config --enable CONFIG_HW_RANDOM_VIRTIO
./scripts/config --enable CONFIG_DRM_VIRTIO_GPU

# ============================================================================
# OTHER GRAPHICS DRIVERS
# ============================================================================
./scripts/config --enable CONFIG_DRM_SIMPLEDRM
./scripts/config --enable CONFIG_DRM_BOCHS
./scripts/config --enable CONFIG_DRM_VGEM
./scripts/config --enable CONFIG_DRM_VKMS

# ============================================================================
# FRAMEBUFFER CONSOLE
# ============================================================================
./scripts/config --enable CONFIG_FB
./scripts/config --enable CONFIG_FB_SIMPLE
./scripts/config --enable CONFIG_FB_EFI
./scripts/config --enable CONFIG_FRAMEBUFFER_CONSOLE
./scripts/config --enable CONFIG_FRAMEBUFFER_CONSOLE_DETECT_PRIMARY
./scripts/config --enable CONFIG_LOGO

# ============================================================================
# VT/TTY
# ============================================================================
./scripts/config --enable CONFIG_VT
./scripts/config --enable CONFIG_VT_CONSOLE
./scripts/config --enable CONFIG_VT_HW_CONSOLE_BINDING

# ============================================================================
# FILESYSTEMS
# ============================================================================
./scripts/config --enable CONFIG_EXT4_FS
./scripts/config --enable CONFIG_XFS_FS
./scripts/config --enable CONFIG_BTRFS_FS
./scripts/config --enable CONFIG_VFAT_FS
./scripts/config --enable CONFIG_FAT_FS
./scripts/config --enable CONFIG_SQUASHFS
./scripts/config --enable CONFIG_SQUASHFS_XZ
./scripts/config --enable CONFIG_ISO9660_FS
./scripts/config --enable CONFIG_JOLIET
./scripts/config --enable CONFIG_BLK_DEV_LOOP

# ============================================================================
# EFI SUPPORT (for modern systems)
# ============================================================================
./scripts/config --enable CONFIG_EFI
./scripts/config --enable CONFIG_EFI_STUB
./scripts/config --enable CONFIG_EFI_VARS
./scripts/config --enable CONFIG_EFIVAR_FS

make olddefconfig
echo "Building kernel (this will take several minutes)..."
make -j"$JOBS" bzImage modules

###############################################################################
# 3) Root filesystem
###############################################################################
echo "Creating root filesystem..."
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
firmware-linux-free \
" \
  bookworm "$ROOTFS_DIR"

# Install kernel modules (CRITICAL for hardware)
echo "Installing kernel modules..."
sudo make modules_install -C "$WORK_DIR/linux" INSTALL_MOD_PATH="$ROOTFS_DIR"

###############################################################################
# 4) System configuration
###############################################################################
echo "Configuring system..."
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

# Library cache
ldconfig

# Udev rules
mkdir -p /etc/udev/rules.d
cat > /etc/udev/rules.d/70-drm.rules <<'UDEV_EOF'
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
# 5) Startup script
###############################################################################
sudo mkdir -p "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d"
cat <<'EOF' | sudo tee "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d/override.conf" >/dev/null
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin user --noclear %I $TERM
Type=simple
EOF

cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/.bash_profile" >/dev/null
if [ -z "${WAYLAND_DISPLAY:-}" ] && [ "$(tty)" = "/dev/tty1" ]; then
  
  echo "╔════════════════════════════════════════╗"
  echo "║      Wayland Live CD - Welcome        ║"
  echo "╚════════════════════════════════════════╝"
  echo ""
  
  # Runtime directory
  export XDG_RUNTIME_DIR=/run/user/1000
  mkdir -p "$XDG_RUNTIME_DIR"
  chmod 0700 "$XDG_RUNTIME_DIR"
  
  # Session
  export XDG_SESSION_TYPE=wayland
  export XDG_SESSION_CLASS=user
  export XDG_CURRENT_DESKTOP=sway
  export SEATD_SOCK=/run/seatd.sock
  
  # Start seatd
  if ! systemctl is-active --quiet seatd; then
    sudo systemctl start seatd
    sleep 1
  fi
  
  # Hardware detection
  echo "Hardware Detection:"
  echo "-------------------"
  echo "GPU:"
  lspci -v | grep -i vga || echo "  No VGA device found via lspci"
  echo ""
  
  if [ -e /dev/dri/card0 ]; then
    echo "✓ DRM device: /dev/dri/card0"
    ls -la /dev/dri/ 2>/dev/null | grep -v total
  else
    echo "✗ No /dev/dri/card0"
  fi
  
  if [ -S /run/seatd.sock ]; then
    echo "✓ Seatd running"
  fi
  
  echo ""
  echo "Available DRI drivers:"
  ls /usr/lib/x86_64-linux-gnu/dri/*_dri.so 2>/dev/null | xargs -n1 basename | sed 's/_dri.so//' || echo "  (none found)"
  echo ""
  
  # Graphics environment
  if [ -e /dev/dri/card0 ]; then
    export WLR_RENDERER=gles2
    export LIBGL_ALWAYS_SOFTWARE=0
  else
    export WLR_RENDERER=pixman
    export LIBGL_ALWAYS_SOFTWARE=1
    echo "⚠ No GPU detected - using software rendering"
  fi
  
  export WLR_BACKENDS=drm,libinput
  export WLR_NO_HARDWARE_CURSORS=1
  
  echo "Starting Sway..."
  echo "(Logs: /tmp/sway.log)"
  echo ""
  
  exec dbus-run-session sway -d 2>&1 | tee /tmp/sway.log
fi
EOF
sudo chroot "$ROOTFS_DIR" chown user:user /home/user/.bash_profile

# Sway config
sudo mkdir -p "$ROOTFS_DIR/home/user/.config/sway"
cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/.config/sway/config" >/dev/null
set $mod Mod4

bindsym $mod+Return exec foot
bindsym $mod+Shift+q kill
bindsym $mod+Shift+e exit

bindsym $mod+Left focus left
bindsym $mod+Down focus down
bindsym $mod+Up focus up
bindsym $mod+Right focus right

output * bg #1a1a2e solid_color

bar {
    position top
    status_command while date +'%Y-%m-%d %H:%M:%S - Sway Live'; do sleep 1; done
    colors {
        background #000000
        statusline #ffffff
    }
}

exec foot sh -c 'echo "╔════════════════════════════════════════╗"; echo "║      Sway Wayland Live CD             ║"; echo "╚════════════════════════════════════════╝"; echo ""; echo "Mod+Enter: Terminal"; echo "Mod+Shift+Q: Close window"; echo "Mod+Shift+E: Exit"; echo ""; lspci -v | grep -i vga; echo ""; exec bash'
EOF
sudo chroot "$ROOTFS_DIR" chown -R user:user /home/user/.config

###############################################################################
# 6) SquashFS
###############################################################################
echo "Creating SquashFS image..."
mkdir -p "$ISO_DIR/live"
sudo mksquashfs "$ROOTFS_DIR" "$ISO_DIR/live/filesystem.squashfs" -comp xz -b 1M -noappend
echo "SquashFS size:"
ls -lh "$ISO_DIR/live/filesystem.squashfs"

###############################################################################
# 7) Initramfs with hardware module loading
###############################################################################
echo "Creating initramfs..."
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
ln -s /busybox bin/find
ln -s /busybox sbin/switch_root

cat <<'INIT_EOF' > init
#!/bin/sh

mkdir -p /dev /proc /sys /mnt/cdrom /mnt/squash /mnt/root

mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys

echo "=== Wayland Live CD Boot ==="
echo ""
echo "Waiting for hardware initialization..."
sleep 3

# Try to mount CD from various devices
echo "Searching for CD-ROM..."
for dev in /dev/sr0 /dev/sr1 /dev/cdrom /dev/scd0 /dev/disk/by-label/CDROM; do
    if [ -b "$dev" ]; then
        echo "  Trying $dev..."
        if mount -t iso9660 -o ro "$dev" /mnt/cdrom 2>/dev/null; then
            echo "  ✓ Mounted from $dev"
            break
        fi
    fi
done

if [ ! -f /mnt/cdrom/live/filesystem.squashfs ]; then
    echo "ERROR: Cannot find filesystem.squashfs"
    echo ""
    echo "Available block devices:"
    ls -la /dev/sd* /dev/sr* /dev/nvme* 2>/dev/null || echo "  (none)"
    echo ""
    exec /bin/sh
fi

echo "Mounting root filesystem..."
mount -t squashfs -o loop,ro /mnt/cdrom/live/filesystem.squashfs /mnt/squash

echo "Setting up tmpfs root (4GB)..."
mount -t tmpfs -o size=4G tmpfs /mnt/root

echo "Copying system files (this may take 30-60 seconds)..."
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

echo "Starting systemd..."
exec switch_root /mnt/root /sbin/init
INIT_EOF

chmod +x init

find . | cpio -o -H newc | gzip > "$ISO_DIR/boot/initrd.img"
cd /
rm -rf "$INITRAMFS_DIR"

###############################################################################
# 8) Kernel
###############################################################################
echo "Copying kernel..."
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"

###############################################################################
# 9) GRUB (BIOS and EFI support)
###############################################################################
echo "Configuring bootloader..."
mkdir -p "$ISO_DIR/boot/grub"
cat <<'EOF' > "$ISO_DIR/boot/grub/grub.cfg"
set default=0
set timeout=5

menuentry "Wayland Live CD (Sway)" {
  linux /boot/vmlinuz console=tty1 quiet splash
  initrd /boot/initrd.img
}

menuentry "Wayland Live CD (verbose)" {
  linux /boot/vmlinuz console=tty1 loglevel=7
  initrd /boot/initrd.img
}

menuentry "Wayland Live CD (safe mode - software rendering)" {
  linux /boot/vmlinuz console=tty1 nomodeset
  initrd /boot/initrd.img
}
EOF

###############################################################################
# 10) Build ISO
###############################################################################
echo "Building ISO..."
cd "$ISO_DIR"
grub-mkrescue -o "$ISO_OUT" .

echo ""
echo "╔════════════════════════════════════════╗"
echo "║       BUILD COMPLETE! 🎉              ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "ISO: $ISO_OUT"
ls -lh "$ISO_OUT"
echo ""
echo "This ISO includes drivers for:"
echo "  • Intel/AMD/NVIDIA graphics"
echo "  • VMware (vmwgfx)"
echo "  • SATA, NVMe, USB storage"
echo "  • Most common hardware"
echo ""
echo "Test commands:"
echo "  QEMU:   qemu-system-x86_64 -m 4G -cdrom \"$ISO_OUT\" -boot d -enable-kvm -vga virtio"
echo "  VMware: Boot from ISO in VM settings"
echo "  Real:   Burn to USB/CD and boot"
echo ""

#!/bin/bash
################################################################################
# MINIMAL WAYLAND LIVE ISO (systemd + sway) — FIXED (initramfs shell + virtio + modules)
#
# Fixes common kernel panics:
#   - /init has #!/bin/sh but initramfs had no /bin/sh  -> panic
#   - virtio-gpu needs virtio core config               -> early crash/panic
#   - modprobe needs modules present                    -> failures later
#
# Notes:
# - This builds a Debian bookworm rootfs, bundles it *inside* initramfs under /rootfs,
#   then bind-mounts it as /newroot and switch_root into systemd.
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
  git rsync cpio xorriso grub-pc-bin mtools \
  mmdebstrap busybox-static kmod coreutils

###############################################################################
# 2) Kernel (DRM + Virtio + Initramfs support)
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

# Install kernel modules into the rootfs so modprobe works later (CRITICAL)
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
# 6) ISO staging + initramfs (with a real /bin/sh in initramfs root)
###############################################################################
mkdir -p "$ISO_DIR"/{boot,rootfs}
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"
sudo rsync -a "$ROOTFS_DIR/" "$ISO_DIR/rootfs/"

# Put static BusyBox in initramfs root and provide /bin/sh so /init can run
BUSYBOX_HOST="$(command -v busybox || true)"
if [ -z "$BUSYBOX_HOST" ]; then
  # busybox-static typically installs /bin/busybox
  BUSYBOX_HOST="/bin/busybox"
fi

sudo cp "$BUSYBOX_HOST" "$ISO_DIR/busybox"
sudo chmod +x "$ISO_DIR/busybox"
sudo mkdir -p "$ISO_DIR/bin" "$ISO_DIR/sbin"

# The kernel executes /init; the shebang needs /bin/sh in the initramfs
sudo ln -sf /busybox "$ISO_DIR/bin/sh"
# Ensure these work even before we switch_root (busybox applets)
sudo ln -sf /busybox "$ISO_DIR/bin/mount"
sudo ln -sf /busybox "$ISO_DIR/bin/mkdir"
sudo ln -sf /busybox "$ISO_DIR/bin/cp"
sudo ln -sf /busybox "$ISO_DIR/bin/ln"
sudo ln -sf /busybox "$ISO_DIR/sbin/switch_root"

# Init script: bind-mount embedded /rootfs as /newroot, mount /run tmpfs, then switch_root
cat <<'EOF' | sudo tee "$ISO_DIR/init" >/dev/null
#!/bin/sh
set -e

# Minimal early mounts
mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys

# Our Debian tree is embedded in the initramfs under /rootfs
# Make it the real root via bind mount to preserve metadata
mkdir -p /newroot
mount --bind /rootfs /newroot

# Prepare required mount points for systemd
mkdir -p /newroot/{dev,proc,sys,run}
mount --move /dev  /newroot/dev
mount --move /proc /newroot/proc
mount --move /sys  /newroot/sys

# systemd expects /run to be a tmpfs
mount -t tmpfs tmpfs /newroot/run

# Optional: make boot logs visible
echo "initramfs: switching to real root..." > /newroot/dev/kmsg 2>/dev/null || true

# Hand off to systemd
exec switch_root /newroot /sbin/init
EOF
sudo chmod +x "$ISO_DIR/init"

# Build initrd
cd "$ISO_DIR"
sudo find init busybox bin sbin rootfs | cpio -o -H newc | gzip > boot/initrd.img

###############################################################################
# 7) GRUB config (ISO only)
###############################################################################
mkdir -p boot/grub
cat <<'EOF' > boot/grub/grub.cfg
set default=0
set timeout=0

menuentry "Sway Wayland" {
  linux /boot/vmlinuz console=tty1 console=ttyS0,115200 earlyprintk=serial,ttyS0,115200 loglevel=7 panic=30
  initrd /boot/initrd.img
}
EOF

###############################################################################
# 8) Build ISO
###############################################################################
grub-mkrescue -o "$ISO_OUT" .

echo
echo "✓ ISO built: $ISO_OUT"
echo
echo "Boot with QEMU (serial enabled so you can SEE panics):"
echo
echo "qemu-system-x86_64 \\"
echo "  -m 2G -cdrom \"$ISO_OUT\" -boot d \\"
echo "  -enable-kvm -cpu host -smp 2 -vga virtio \\"
echo "  -serial mon:stdio -no-reboot"

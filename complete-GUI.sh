#!/bin/bash
################################################################################
# MINIMAL WAYLAND LIVE ISO (SYSTEMD + SWAY) — WORKING VERSION
################################################################################
set -euo pipefail

WORK_DIR="$HOME/custom-linux-build"
ROOTFS_DIR="$HOME/custom-rootfs"
ISO_DIR="$HOME/iso-staging"
ISO_OUT="$HOME/custom-linux-wayland.iso"
JOBS=$(nproc)

sudo rm -rf "$WORK_DIR" "$ROOTFS_DIR" "$ISO_DIR" "$ISO_OUT"

###############################################################################
# 1. Dependencies
###############################################################################
sudo apt-get update
sudo apt-get install -y \
  build-essential flex bison libncurses-dev libssl-dev bc dwarves pahole \
  git rsync cpio xorriso grub-pc-bin mtools \
  mmdebstrap busybox-static kmod coreutils

###############################################################################
# 2. Kernel (DRM + Virtio)
###############################################################################
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"
git clone --depth 1 https://github.com/torvalds/linux.git
cd linux

make defconfig
./scripts/config --enable CONFIG_DRM
./scripts/config --enable CONFIG_DRM_VIRTIO_GPU
./scripts/config --enable CONFIG_DRM_KMS_HELPER
./scripts/config --enable CONFIG_INPUT_EVDEV
./scripts/config --enable CONFIG_DEVTMPFS
./scripts/config --enable CONFIG_DEVTMPFS_MOUNT
./scripts/config --enable CONFIG_TMPFS
./scripts/config --enable CONFIG_BLK_DEV_INITRD
make olddefconfig
make -j"$JOBS" bzImage

###############################################################################
# 3. Root filesystem (Wayland + Sway)
###############################################################################
sudo mmdebstrap \
  --architecture=amd64 \
  --variant=minbase \
  --include="
systemd systemd-sysv udev dbus seatd
sway foot
libwayland-client0 libwayland-server0 wayland-protocols
libgl1 libegl1 libgles2 libgl1-mesa-dri mesa-vulkan-drivers
xwayland
kmod coreutils bash
" \
  bookworm "$ROOTFS_DIR"

###############################################################################
# 4. User + services
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
# 5. Autologin + Sway
###############################################################################
sudo mkdir -p "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d"
cat <<'EOF' | sudo tee "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d/override.conf"
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin user --noclear %I $TERM
EOF

cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/.bash_profile"
if [ -z "${WAYLAND_DISPLAY:-}" ] && [ "$(tty)" = "/dev/tty1" ]; then
  export XDG_RUNTIME_DIR=/run/user/1000
  mkdir -p "$XDG_RUNTIME_DIR"
  chmod 0700 "$XDG_RUNTIME_DIR"
  exec dbus-run-session sway
fi
EOF
sudo chroot "$ROOTFS_DIR" chown user:user /home/user/.bash_profile

###############################################################################
# 6. Initramfs (CORRECT switch_root)
###############################################################################
mkdir -p "$ISO_DIR"/{boot,rootfs}
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"
sudo rsync -a "$ROOTFS_DIR/" "$ISO_DIR/rootfs/"

cat <<'EOF' | sudo tee "$ISO_DIR/init"
#!/bin/sh
set -e

mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys

mkdir /newroot
mount -t tmpfs tmpfs /newroot
cp -a /rootfs/* /newroot/

mkdir -p /newroot/{proc,sys,dev,run}
mount --move /dev /newroot/dev
mount --move /proc /newroot/proc
mount --move /sys /newroot/sys

for m in drm virtio_gpu drm_kms_helper; do
  modprobe $m 2>/dev/null || true
done

exec switch_root /newroot /sbin/init
EOF
sudo chmod +x "$ISO_DIR/init"

cd "$ISO_DIR"
sudo find init rootfs | cpio -o -H newc | gzip > boot/initrd.img

###############################################################################
# 7. GRUB (ISO only — NO grub-install)
###############################################################################
mkdir -p boot/grub
cat <<'EOF' > boot/grub/grub.cfg
set default=0
set timeout=0
set hidden_timeout=0
set hidden_timeout_quiet=true

menuentry "Sway Wayland" {
  linux /boot/vmlinuz console=tty1 loglevel=7
  initrd /boot/initrd.img
}
EOF

###############################################################################
# 8. Build ISO
###############################################################################
grub-mkrescue -o "$ISO_OUT" .

echo
echo "✓ ISO built: $ISO_OUT"
echo "✓ Boot with QEMU using the launcher below"

qemu-system-x86_64 -m 2G -cdrom "$HOME/custom-linux-wayland.iso" -boot d -enable-kvm -cpu host -smp 2 -vga virtio -net nic -net user

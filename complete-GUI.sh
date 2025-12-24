#!/bin/bash
################################################################################
# CUSTOM LINUX WAYLAND ISO - FIXED GRUB PATH VERSION
################################################################################

set -euo pipefail

WORK_DIR="$HOME/custom-linux-build"
ROOTFS_DIR="$HOME/custom-rootfs"
ISO_DIR="$HOME/iso-staging"
KERNEL_JOBS=$(nproc)

sudo rm -rf "$WORK_DIR" "$ROOTFS_DIR" "$ISO_DIR" "$HOME/custom-linux-wayland.iso"

# 1. Dependencies
echo "Installing dependencies..."
sudo apt-get update && sudo apt-get install -y \
    build-essential flex bison libncurses-dev libssl-dev bc dwarves pahole cpio \
    rsync git mmdebstrap xorriso grub-pc-bin mtools busybox-static kmod coreutils

# 2. Kernel with graphics
echo "Building kernel..."
mkdir -p "$WORK_DIR" && cd "$WORK_DIR"
git clone --depth 1 --branch master https://github.com/torvalds/linux.git linux
cd linux && make defconfig

./scripts/config --enable CONFIG_DRM_VIRTIO_GPU --enable CONFIG_DRM_VMWGFX
./scripts/config --enable CONFIG_DRM_BOCHS --enable CONFIG_INPUT_EVDEV
./scripts/config --enable CONFIG_DEVTMPFS --enable CONFIG_DEVTMPFS_MOUNT
./scripts/config --enable CONFIG_TMPFS --enable CONFIG_BLK_DEV_INITRD
make olddefconfig -j"$KERNEL_JOBS" bzImage

# 3. Minimal rootfs
echo "Building rootfs..."
sudo rm -rf "$ROOTFS_DIR" && mkdir -p "$ROOTFS_DIR"
sudo mmdebstrap --architecture=amd64 --variant=minbase \
    --include='systemd systemd-sysv udev libgl1-mesa-dri sway foot seatd dbus kmod coreutils bash' \
    bookworm "$ROOTFS_DIR"

# 4. Fix groups and user
echo "Setting up user..."
sudo chroot "$ROOTFS_DIR" /bin/bash -c "
groupadd -g 44 video && groupadd -g 109 render && groupadd -g 24 input
useradd -m -u 1000 -g users -s /bin/bash -G video,render,input user
echo 'user:password' | chpasswd
systemctl enable seatd getty@tty1
"

# 5. Autostart sway
sudo mkdir -p "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d"
cat <<'EOF' | sudo tee "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d/override.conf"
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin user --noclear %I $TERM
EOF

cat <<'EOF' | sudo tee "$ROOTFS_DIR/home/user/.bash_profile"
if [ -z "${WAYLAND_DISPLAY:-}" ] && [ "$(tty)" = '/dev/tty1' ]; then
  export XDG_RUNTIME_DIR=/run/user/1000
  mkdir -p "$XDG_RUNTIME_DIR" && chmod 0700 "$XDG_RUNTIME_DIR"
  exec dbus-run-session sway
fi
EOF
sudo chroot "$ROOTFS_DIR" chown user:users /home/user/.bash_profile

# 6. Create PROPER initramfs (entire rootfs in RAM)
echo "Creating initramfs..."
mkdir -p "$ISO_DIR"/{boot,boot/grub,rootfs}
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_DIR/boot/vmlinuz"

# Copy entire rootfs to initramfs structure
sudo rsync -a "$ROOTFS_DIR/" "$ISO_DIR/rootfs/"

# Create init script
cat <<'EOF' | sudo tee "$ISO_DIR/init"
#!/init
mount -t devtmpfs devtmpfs /dev
mount -t proc     proc     /proc
mount -t sysfs    sysfs    /sys 
mount -t tmpfs    tmpfs    /run
mount -t tmpfs    tmpfs    /tmp

# Graphics modules
for mod in drm virtio_gpu vmwgfx drm_kms_helper; do
  modprobe $mod 2>/dev/null || true
done

exec switch_root /rootfs /sbin/init systemd.unit=multi-user.target
EOF
sudo chmod +x "$ISO_DIR/init"

# Pack initramfs
cd "$ISO_DIR"
sudo find rootfs init | cpio -o -H newc | gzip > boot/initrd.img

# 7. GRUB - FIXED PATH (create full boot/grub structure first)
mkdir -p boot/grub
cat <<'EOF' | tee boot/grub/grub.cfg
set timeout=3
menuentry "Sway Wayland" {
  linux /boot/vmlinuz root=/dev/ram0 rw console=tty1 loglevel=3 quiet splash
  initrd /boot/initrd.img
}
menuentry "Debug" {
  linux /boot/vmlinuz root=/dev/ram0 rw console=tty1 loglevel=7
  initrd /boot/initrd.img
}
EOF

# Create ISO
grub-mkrescue -o "$HOME/custom-linux-wayland.iso" .

echo "✓ ISO: $HOME/custom-linux-wayland.iso"
echo "Boot 'Sway Wayland' → auto-login → Sway"


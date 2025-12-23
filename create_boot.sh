#!/bin/bash

set -euo pipefail

# Configuration
WORK_DIR="$HOME/custom-linux-build"
BOOT_FILES_DIR="$HOME/boot-files"
ROOTFS_DIR="$HOME/custom-rootfs"
DISK_IMAGE="$BOOT_FILES_DIR/linux-gui-efi.img"

# Create a new disk image
dd if=/dev/zero of="$DISK_IMAGE" bs=1M count=4096 status=none

# Create partition table and partitions
parted -s "$DISK_IMAGE" mklabel gpt
parted -s "$DISK_IMAGE" mkpart primary fat32 1MiB 257MiB
parted -s "$DISK_IMAGE" set 1 esp on
parted -s "$DISK_IMAGE" mkpart primary ext4 257MiB 100%

# Set up loop device
LOOP_DEV=$(losetup -f --show "$DISK_IMAGE")
partprobe "$LOOP_DEV"

# Format partitions
mkfs.fat -F32 "${LOOP_DEV}p1" > /dev/null
mkfs.ext4 -F "${LOOP_DEV}p2" > /dev/null

# Mount partitions
mkdir -p /mnt/efi /mnt/root
mount "${LOOP_DEV}p1" /mnt/efi
mount "${LOOP_DEV}p2" /mnt/root

# Copy root filesystem
rsync -a "$ROOTFS_DIR/" /mnt/root/

# Create boot and EFI directories
mkdir -p /mnt/root/boot /mnt/efi/EFI/boot

# Copy kernel and initramfs to boot directory
cp "$WORK_DIR/linux/arch/x86/boot/bzImage" /mnt/root/boot/
cp "$BOOT_FILES_DIR/initramfs.cpio.gz" /mnt/root/boot/

# Install GRUB-EFI
grub-install --target=x86_64-efi --efi-directory=/mnt/efi --boot-directory=/mnt/root/boot --removable

# Generate GRUB configuration
grub-mkconfig -o /mnt/root/boot/grub/grub.cfg

# Copy grubx64.efi to EFI boot directory (required for some UEFI firmware)
cp /mnt/efi/EFI/grub/grubx64.efi /mnt/efi/EFI/boot/bootx64.efi

# Unmount and clean up
umount /mnt/efi
umount /mnt/root
losetup -d "$LOOP_DEV"

echo "UEFI bootable disk image created: $DISK_IMAGE"

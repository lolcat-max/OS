.POSIX:

# Variables
KERNEL_VERSION = 6.1
KERNEL_URL = https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$(KERNEL_VERSION).tar.xz
GTK_APP = gtk_app
ISO = gtk_os.iso
INITRAMFS = initramfs.cpio.gz
KERNEL = vmlinuz
GRUB_CFG = grub.cfg
ISO_ROOT = iso_root

# Directories
KERNEL_DIR = linux-$(KERNEL_VERSION)
INITRAMFS_DIR = initramfs

# Tools
WGET = wget
TAR = tar
MAKE = make
GRUB_MKRESCUE = grub-mkrescue
CC = gcc
CXX = g++
PKG_CONFIG = pkg-config

# Download and extract kernel
kernel:
	$(WGET) $(KERNEL_URL)
	$(TAR) -xf linux-$(KERNEL_VERSION).tar.xz

# Compile kernel
kernel_compile:
	cd linux-$(KERNEL_VERSION) && make defconfig
	cd linux-$(KERNEL_VERSION) && make -j$(shell nproc) WERROR=0

# Create initramfs directory
initramfs_dir:
	mkdir -p $(INITRAMFS_DIR)/{bin,sbin,etc,proc,sys,dev}

# Copy GTK application to initramfs
initramfs_copy:
	cp $(GTK_APP) $(INITRAMFS_DIR)/usr/bin/
	chmod +x $(INITRAMFS_DIR)/usr/bin/$(GTK_APP)

# Create init script
initramfs_init:
	echo '#!/bin/sh' > $(INITRAMFS_DIR)/init
	echo 'mount -t proc proc /proc' >> $(INITRAMFS_DIR)/init
	echo 'mount -t sysfs sysfs /sys' >> $(INITRAMFS_DIR)/init
	echo 'mount -t devtmpfs devtmpfs /dev' >> $(INITRAMFS_DIR)/init
	echo 'export GDK_BACKEND=fb' >> $(INITRAMFS_DIR)/init
	echo 'exec /usr/bin/$(GTK_APP)' >> $(INITRAMFS_DIR)/init
	chmod +x $(INITRAMFS_DIR)/init

# Create initramfs
initramfs:
	cd $(INITRAMFS_DIR) && find . | cpio -o -H newc | gzip > ../$(INITRAMFS)

# Create ISO directory
iso_dir:
	mkdir -p $(ISO_ROOT)/boot/grub

# Copy kernel and initramfs to ISO directory
iso_copy:
	cp $(KERNEL_DIR)/arch/x86/boot/bzImage $(ISO_ROOT)/$(KERNEL)
	cp $(INITRAMFS) $(ISO_ROOT)/$(INITRAMFS)

# Create GRUB configuration
iso_grub:
	echo 'set timeout=0' > $(ISO_ROOT)/boot/grub/$(GRUB_CFG)
	echo 'set default=0' >> $(ISO_ROOT)/boot/grub/$(GRUB_CFG)
	echo '' >> $(ISO_ROOT)/boot/grub/$(GRUB_CFG)
	echo 'menuentry "GTK OS" {' >> $(ISO_ROOT)/boot/grub/$(GRUB_CFG)
	echo '  linux /$(KERNEL) root=/dev/ram0 init=/init' >> $(ISO_ROOT)/boot/grub/$(GRUB_CFG)
	echo '  initrd /$(INITRAMFS)' >> $(ISO_ROOT)/boot/grub/$(GRUB_CFG)
	echo '}' >> $(ISO_ROOT)/boot/grub/$(GRUB_CFG)

# Create ISO
iso:
	$(GRUB_MKRESCUE) -o $(ISO) $(ISO_ROOT)
# Apply patch
patch_kernel:
	cd linux-$(KERNEL_VERSION) && patch -p1 < ../kernel_fixes.patch
		
	# Fix all instances of: for (i = 0; i < tp->irq_max; i++)
	sed -i 's/for (i = 0; i < tp->irq_max; i++)/for (i = 0; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++)/g' "$FILE"

	# Fix: for (i = 1; i < tp->irq_max; i++)
	sed -i 's/for (i = 1; i < tp->irq_max; i++)/for (i = 1; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++)/g' "$FILE"

	# Fix: for (; i < tp->irq_max; i++, ...)
	sed -i 's/for (; i < tp->irq_max; i++,/for (; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++,/g' "$FILE"

	# Fix: for (; i < tp->irq_max; i++)
	sed -i 's/for (; i < tp->irq_max; i++)/for (; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++)/g' "$FILE"
# Clean up
clean:
	rm -rf $(KERNEL_DIR) $(INITRAMFS_DIR) $(ISO_ROOT) $(INITRAMFS) $(ISO)

# Default target
all: kernel patch_kernel kernel_compile initramfs_dir initramfs_copy initramfs_init initramfs iso_dir iso_copy iso_grub iso

.PHONY: all kernel patch_kernel kernel_compile initramfs_dir initramfs_copy initramfs_init initramfs iso_dir iso_copy iso_grub iso clean

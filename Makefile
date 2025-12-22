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

# GTK flags
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

# Build GTK application
$(GTK_APP): $(GTK_APP).c
	$(CC) $(GTK_CFLAGS) -o $(GTK_APP) $(GTK_APP).c $(GTK_LIBS)

# Download and extract kernel
kernel:
	$(WGET) $(KERNEL_URL)
	$(TAR) -xf linux-$(KERNEL_VERSION).tar.xz
	cd $(KERNEL_DIR) && \
	sed -i 's/seq_printf(sf, "%s %u\\n", dname, iocg->cfg_weight \/ WEIGHT_ONE);/seq_printf(sf, "%s %lu\\n", dname, iocg->cfg_weight \/ WEIGHT_ONE);/g' block/blk-iocost.c && \
	sed -i 's/seq_printf(sf, "default %u\\n", iocc->dfl_weight \/ WEIGHT_ONE);/seq_printf(sf, "default %lu\\n", iocc->dfl_weight \/ WEIGHT_ONE);/g' block/blk-iocost.c
	
	cd $(KERNEL_DIR) && \
	sed -i 's/for (i = 0; i < tp->irq_max; i++)/for (i = 0; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++)/g' drivers/net/ethernet/broadcom/tg3.c && \
	sed -i 's/for (i = 1; i < tp->irq_max; i++)/for (i = 1; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++)/g' drivers/net/ethernet/broadcom/tg3.c && \
	sed -i 's/for (; i < tp->irq_max; i++,/for (; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++,/g' drivers/net/ethernet/broadcom/tg3.c && \
	sed -i 's/for (; i < tp->irq_max; i++)/for (; i < tp->irq_max \&\& i < TG3_IRQ_MAX_VECS; i++)/g' drivers/net/ethernet/broadcom/tg3.c && \
	sed -i 's/xt_TCPMSS/xt_tcpmss/g' net/netfilter/Makefile

# Compile kernel
kernel_compile:
	cd $(KERNEL_DIR) && make defconfig
	cd $(KERNEL_DIR) && make -j$(shell nproc) WERROR=0

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

# Clean up
clean:
	rm -rf $(KERNEL_DIR) $(INITRAMFS_DIR) $(ISO_ROOT) $(INITRAMFS) $(ISO) $(GTK_APP)

# Default target: build GTK app, patch, compile kernel, package ISO
all: $(GTK_APP) kernel kernel_compile initramfs_dir initramfs_copy initramfs_init initramfs iso_dir iso_copy iso_grub iso

.PHONY: all $(GTK_APP) kernel kernel_compile initramfs_dir initramfs_copy initramfs_init initramfs iso_dir iso_copy iso_grub iso clean

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
	find . | cpio -o -H newc | gzip > ../initramfs.cpio.gz

# Compile kernel
kernel_compile:
	cd $(KERNEL_DIR) && make defconfig
	cd $(KERNEL_DIR) && make -j$(shell nproc) WERROR=0
initramfs:
	cd $(INITRAMFS_DIR) && find . | cpio -o -H newc | gzip > ../$(INITRAMFS)
# Create initramfs directory
initramfs_dir:
	mkdir -p initramfs/{bin,sbin,etc,proc,sys,dev,usr/bin}

initramfs_copy: initramfs_dir
	cp $(GTK_APP) initramfs/usr/bin/
	chmod +x initramfs/usr/bin/$(GTK_APP)
	chmod +x initramfs/usr/bin/$(GTK_APP)
	
initramfs_init: initramfs_dir
	echo '#!/bin/sh' > $(INITRAMFS_DIR)/init
	echo '/bin/busybox mount -t proc proc /proc' >> $(INITRAMFS_DIR)/init
	echo '/bin/busybox mount -t sysfs sysfs /sys' >> $(INITRAMFS_DIR)/init
	echo '/bin/busybox mount -t devtmpfs devtmpfs /dev' >> $(INITRAMFS_DIR)/init
	echo 'export GDK_BACKEND=fb' >> $(INITRAMFS_DIR)/init
	echo 'exec /usr/bin/$(GTK_APP)' >> $(INITRAMFS_DIR)/init
	chmod +x $(INITRAMFS_DIR)/init

# Create initramfs
initramfs_dir:
	mkdir -p initramfs/{bin,sbin,etc,proc,sys,dev,usr/bin}

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

BUSYBOX := $(shell command -v busybox 2>/dev/null)

initramfs_busybox: initramfs_dir
	@test -n "$(BUSYBOX)" || (echo "busybox not found. Install busybox/busybox-static."; exit 1)
	cp "$(BUSYBOX)" $(INITRAMFS_DIR)/bin/busybox
	chmod +x $(INITRAMFS_DIR)/bin/busybox
	ln -sf busybox $(INITRAMFS_DIR)/bin/sh
	ln -sf busybox $(INITRAMFS_DIR)/bin/mount
# Create ISO
iso:
	$(GRUB_MKRESCUE) -o $(ISO) $(ISO_ROOT)

# Clean up
clean:
	rm -rf $(KERNEL_DIR) initramfs $(ISO_ROOT) $(INITRAMFS) $(ISO) $(GTK_APP)

all: $(GTK_APP) kernel kernel_compile initramfs_dir initramfs_busybox initramfs_copy initramfs_init initramfs iso_dir iso_copy iso_grub iso
.PHONY: all $(GTK_APP) kernel kernel_compile initramfs_dir initramfs_copy initramfs_init initramfs iso_dir iso_copy iso_grub iso clean

TCC_VERSION = 0.9.27
TCC_URL = https://download-mirror.savannah.gnu.org/releases/tinycc/tcc-$(TCC_VERSION).tar.bz2
TCC_DIR = tcc-$(TCC_VERSION)

all: main.iso

# 1. Build TCC (Cross-Compiler)
$(TCC_DIR)/i386-tcc:
	rm -rf $(TCC_DIR)
	wget -c $(TCC_URL)
	tar xf tcc-$(TCC_VERSION).tar.bz2
	# Fix bcheck.c for glibc >= 2.34
	find $(TCC_DIR) -name "bcheck.c" -exec sed -i 's/#define CONFIG_TCC_MALLOC_HOOKS//g' {} +
	find $(TCC_DIR) -name "bcheck.c" -exec sed -i 's/#define HAVE_MEMALIGN//g' {} +
	# Configure & Build
	cd $(TCC_DIR) && ./configure --cpu=i386 --enable-cross
	cd $(TCC_DIR) && make

# 2. Build libtcc.a (Runtime Support)
libtcc.a: $(TCC_DIR)/i386-tcc
	cd $(TCC_DIR) && ar rcs ../libtcc.a libtcc.o

# 3. Compile Kernel to Object File (kernel.o)
kernel.o: kernel.c $(TCC_DIR)/i386-tcc
	$(TCC_DIR)/i386-tcc -c -m32 -nostdlib \
		-I $(TCC_DIR)/include \
		kernel.c -o kernel.o

# 4. Link Kernel (Use system LD for robust linker script support)
kernel.bin: kernel.o libtcc.a linker.ld
	ld -m elf_i386 -T linker.ld -o kernel.bin kernel.o libtcc.a

# 5. Create ISO
main.iso: kernel.bin grub.cfg
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/
	cp grub.cfg isodir/boot/grub/
	grub-mkrescue -o main.iso isodir

run: main.iso
	qemu-system-i386 -cdrom main.iso -m 512M

clean:
	rm -rf *.bin *.o *.a main.iso isodir

distclean: clean
	rm -rf $(TCC_DIR)
# Add this rule to your Makefile
grub.cfg:
	echo 'menuentry "TCC OS" { multiboot /boot/kernel.bin }' > grub.cfg
.PHONY: all run clean distclean

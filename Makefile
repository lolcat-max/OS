TCC_VERSION = 0.9.27
TCC_URL = https://download-mirror.savannah.gnu.org/releases/tinycc/tcc-$(TCC_VERSION).tar.bz2
TCC_DIR = tcc-$(TCC_VERSION)

all: main.iso

$(TCC_DIR)/i386-tcc:
	rm -rf $(TCC_DIR)
	wget -c $(TCC_URL)
	tar xf tcc-$(TCC_VERSION).tar.bz2
	find $(TCC_DIR) -name "bcheck.c" -exec sed -i 's/#define CONFIG_TCC_MALLOC_HOOKS//g' {} +
	find $(TCC_DIR) -name "bcheck.c" -exec sed -i 's/#define HAVE_MEMALIGN//g' {} +
	cd $(TCC_DIR) && ./configure --cpu=i386 --enable-cross
	cd $(TCC_DIR) && make

libtcc.a: $(TCC_DIR)/i386-tcc
	cd $(TCC_DIR) && ar rcs ../libtcc.a libtcc.o

# Assemble the boot stub (Use system assembler 'as' for reliability, or TCC)
boot.o: boot.S
	as --32 boot.S -o boot.o

# Compile Kernel
kernel.o: kernel.c $(TCC_DIR)/i386-tcc
	$(TCC_DIR)/i386-tcc -c -m32 -nostdlib \
		-I $(TCC_DIR)/include \
		kernel.c -o kernel.o

# Link (boot.o MUST be first!)
kernel.bin: boot.o kernel.o libtcc.a linker.ld
	ld -m elf_i386 -T linker.ld -o kernel.bin boot.o kernel.o libtcc.a

grub.cfg:
	echo 'menuentry "TCC OS" { multiboot /boot/kernel.bin }' > grub.cfg

main.iso: kernel.bin grub.cfg
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/
	cp grub.cfg isodir/boot/grub/
	grub-mkrescue -o main.iso isodir

run: main.iso
	qemu-system-i386 -cdrom main.iso -m 512M

clean:
	rm -rf *.bin *.o *.a main.iso grub.cfg isodir

distclean: clean
	rm -rf $(TCC_DIR)

.PHONY: all run clean distclean

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

# 2. Build COMPLETE libtcc.a from ALL source files (fixes undefined refs)
# 2. Build MINIMAL libtcc.a - Essential files only
# 2. Build MINIMAL libtcc.a - CORRECT TCC 0.9.27 files
libtcc.a: $(TCC_DIR)/i386-tcc
	cd $(TCC_DIR) && rm -f ../../libtcc.a *.o
	cd $(TCC_DIR) && ../$(TCC_DIR)/i386-tcc -c -m32 -nostdlib -I include \
		-DTCC_TARGET_I386=1 -DTCC_ARM=0 libtcc.c
	cd $(TCC_DIR) && ar rcs ../../libtcc.a libtcc.o
	cd $(TCC_DIR) && ../$(TCC_DIR)/i386-tcc -c -m32 -nostdlib -I include \
		-DTCC_TARGET_I386=1 tccgen.c
	cd $(TCC_DIR) && ar rcs ../../libtcc.a tccgen.o
	cd $(TCC_DIR) && ../$(TCC_DIR)/i386-tcc -c -m32 -nostdlib -I include \
		-DTCC_TARGET_I386=1 i386-gen.c
	cd $(TCC_DIR) && ar rcs ../../libtcc.a i386-gen.o
	cd $(TCC_DIR) && ../$(TCC_DIR)/i386-tcc -c -m32 -nostdlib -I include \
		-DTCC_TARGET_I386=1 i386-asm.c
	cd $(TCC_DIR) && ar rcs ../../libtcc.a i386-asm.o
# 3. Compile Kernel (with stack protector disabled)
kernel.o: kernel.c $(TCC_DIR)/i386-tcc
	$(TCC_DIR)/i386-tcc -c -m32 -nostdlib -fno-stack-protector \
		-I $(TCC_DIR)/include \
		kernel.c -o kernel.o

# 3. Compile boot.S
boot.o: boot.S $(TCC_DIR)/i386-tcc
	$(TCC_DIR)/i386-tcc -c -m32 -nostdlib boot.S -o boot.o
	
# 4. Link Kernel with COMPLETE TCC runtime
kernel.bin: boot.o kernel.o libtcc.a linker.ld
	ld -m elf_i386 -T linker.ld -o kernel.bin boot.o kernel.o $(TCC_DIR)/libtcc.a

# 5. Create ISO
main.iso: kernel.bin grub.cfg
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/
	cp grub.cfg isodir/boot/grub/
	grub-mkrescue -o main.iso isodir

run: main.iso
	qemu-system-i386 -cdrom main.iso -m 512M

clean:
	rm -rf *.bin *.o *.a main.iso isodir $(TCC_DIR)

distclean: clean
	rm -rf $(TCC_DIR)

grub.cfg:
	echo 'menuentry "TCC OS" { multiboot /boot/kernel.bin }' > grub.cfg

.PHONY: all run clean distclean

.POSIX:

ISODIR := iso
MULTIBOOT := $(ISODIR)/boot/main.elf
GRUB_CFG := grub.cfg
MAIN := main.iso

# Added -fno-pic to disable Position Independent Code
CXXFLAGS := -ffreestanding -O2 -Wall -Wextra -std=c++17 -fno-exceptions -fno-rtti -Iinclude -fno-pic
.PHONY: clean run

$(MAIN):
	# Create directories
	mkdir -p $(ISODIR)/boot/grub
	
	# 1. Assemble boot.S
	as -32 boot.S -o boot.o

	# 2. Compile kernel.cpp with CXXFLAGS (includes -fno-pic)
	# Using g++ instead of gcc is better for .cpp files
	g++ -c kernel.cpp -m32 $(CXXFLAGS) -o kernel.o 

	# 3. Link with -no-pie to prevent relocatable executable generation
	gcc -ffreestanding -m32 -nostdlib -no-pie -o '$(MULTIBOOT)' -T linker.ld boot.o kernel.o -lgcc

	# 4. Copy config
	cp $(GRUB_CFG) $(ISODIR)/boot/grub/grub.cfg

	# 5. Create ISO
	grub-mkrescue -o '$@' '$(ISODIR)'

clean:
	rm -f *.o '$(MULTIBOOT)' '$(MAIN)'
	rm -rf $(ISODIR)

run: $(MAIN)
	qemu-system-i386 -cdrom '$(MAIN)'

.POSIX:

TARGET = gtk_app.iso
APP = gtk_app

CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Iinclude

.PHONY: all clean run iso

all: $(APP)

$(APP): gtk_app.cpp
	$(CXX) $(CXXFLAGS) -o $(APP) gtk_app.cpp $(shell pkg-config --cflags --libs gtk+-3.0)

clean:
	rm -f $(APP) $(TARGET)

run: $(TARGET)
	qemu-system-x86_64 -cdrom $(TARGET)

iso: $(APP)
	mkdir -p iso_root/boot/grub
	cp $(APP) iso_root/
	cp /boot/vmlinuz iso_root/boot/
	cp /boot/initrd.img iso_root/boot/
	echo 'set timeout=0\nset default=0\n\nmenuentry "GTK App" {\n  linux /boot/vmlinuz root=/dev/sr0\n  initrd /boot/initrd.img\n}' > iso_root/boot/grub/grub.cfg
	genisoimage -o $(TARGET) -b boot/grub/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table iso_root/
	rm -rf iso_root

make -j$(nproc)
mkdir -p iso/boot/grub
cp arch/x86/boot/bzImage iso/boot/vmlinuz
grub-mkrescue -o kernel.iso iso/
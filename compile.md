sudo apt update

sudo apt install make gcc gcc-multilib binutils grub-common xorriso qemu-system-x86 build-essential nasm gcc binutils qemu-system-x86 xorriso grub-pc-bin mtools g++-multilib libgtk-3-dev libgtk-3-0 flex bison build-essential libelf-dev libelf1 libssl-dev busybox

make

in terminal:

The file you downloaded still has Windows line endings. Let me give you the commands to run directly in your terminal instead:
bash# Step 1: Install busybox-static
sudo apt-get install -y busybox-static

# Step 2: Clean and create structure
rm -rf initramfs initramfs.cpio.gz
mkdir -p initramfs/bin
mkdir -p initramfs/dev
mkdir -p initramfs/proc
mkdir -p initramfs/sys
mkdir -p initramfs/usr/bin

# Step 3: Copy busybox (find the right path)
BUSYBOX=$(which busybox)
echo "Using busybox at: $BUSYBOX"
cp "$BUSYBOX" initramfs/bin/busybox
chmod +x initramfs/bin/busybox

# Step 4: Create init script (copy this entire block including EOF markers)
cat > initramfs/init << 'EOF'
#!/bin/busybox sh
echo "=== Init Started ==="
/bin/busybox --install -s /bin
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev
echo "Boot successful! Kernel: $(uname -r)"
if [ -x /usr/bin/gtk_app ]; then
    export GDK_BACKEND=fb
    export PATH=/bin:/sbin:/usr/bin:/usr/sbin
    exec /usr/bin/gtk_app
else
    echo "No gtk_app found, launching shell..."
    exec /bin/sh
fi
EOF

chmod +x initramfs/init

# Step 5: Copy gtk_app if it exists
if [ -f gtk_app ]; then
    cp gtk_app initramfs/usr/bin/gtk_app
    chmod +x initramfs/usr/bin/gtk_app
    echo "GTK app copied"
fi

# Step 6: Create the archive
cd initramfs
find . | cpio -o -H newc | gzip > ../initramfs.cpio.gz
cd ..

# Step 7: Verify it worked
echo "=== VERIFICATION ==="
gunzip -c initramfs.cpio.gz | cpio -t | head -20
echo ""
echo "Checking for init at root:"
gunzip -c initramfs.cpio.gz | cpio -t | grep "^init$" && echo "SUCCESS!" || echo "FAILED!"

ls -lh initramfs.cpio.gz

make iso

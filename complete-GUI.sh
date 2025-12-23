#!/bin/bash

################################################################################
# WSL CUSTOM LINUX BUILD WITH WAYLAND GUI
# Builds a complete Linux system with Sway (Wayland compositor) and GUI apps
################################################################################
set -e
apt-get install tar gzip coreutils
# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# Configuration
KERNEL_JOBS=${KERNEL_JOBS:-$(nproc)}
BUSYBOX_JOBS=${BUSYBOX_JOBS:-$(nproc)}
BOOT_SIZE_MB=${BOOT_SIZE_MB:-500}  # Increased for GUI

# Use Linux native paths (avoid /tmp on Windows mount)
WORK_DIR="$HOME/custom-linux-build"
BOOT_FILES_DIR="$HOME/boot-files"
ROOTFS_DIR="$HOME/custom-rootfs"

print_banner() {
    clear
    echo -e "${CYAN}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║     CUSTOM LINUX BUILD WITH WAYLAND GUI                      ║
║     Includes Sway Compositor + Desktop Applications          ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
}

print_step() {
    echo ""
    echo -e "${MAGENTA}════════════════════════════════════════════════════════${NC}"
    echo -e "${MAGENTA}▶ STEP $1${NC}: ${CYAN}$2${NC}"
    echo -e "${MAGENTA}════════════════════════════════════════════════════════${NC}"
    echo ""
}

print_status() { echo -e "${GREEN}[✓]${NC} $1"; }
print_error() { echo -e "${RED}[✗]${NC} $1"; }
print_warning() { echo -e "${YELLOW}[⚠]${NC} $1"; }
print_info() { echo -e "${BLUE}[ℹ]${NC} $1"; }

check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root"
        print_warning "Run: sudo bash $0"
        exit 1
    fi
}
install_dependencies() {
    print_step 1 "Installing Build Dependencies"
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        build-essential flex bison libncurses-dev libssl-dev libelf-dev \
        bc dwarves pahole cpio rsync wget git busybox-static \
        mmdebstrap zstd binutils dpkg-dev qemu-system-x86
    print_status "Dependencies installed"
}



clone_kernel() {
    print_step 2 "Downloading Linux Kernel"
    rm -rf "$WORK_DIR"
    mkdir -p "$WORK_DIR"
    cd "$WORK_DIR"
    
    if [ ! -d "linux" ]; then
        print_info "Cloning kernel (this takes 2-5 minutes)..."
        git clone --depth 1 https://github.com/torvalds/linux.git
        print_status "Kernel downloaded"
    else
        print_warning "Kernel already downloaded"
    fi
}

configure_kernel() {
    print_step 3 "Configuring Kernel with Graphics Support"
    
    cd "$WORK_DIR/linux"
    
    print_info "Creating configuration with GPU and graphics support..."
    make defconfig > /dev/null 2>&1
    
    # Essential options
    scripts/config --enable CONFIG_DEVTMPFS
    scripts/config --enable CONFIG_DEVTMPFS_MOUNT
    scripts/config --enable CONFIG_BLK_DEV_INITRD
    scripts/config --enable CONFIG_TMPFS
    scripts/config --enable CONFIG_TMPFS_POSIX_ACL
    
    # Graphics and DRM support
    scripts/config --enable CONFIG_DRM
    scripts/config --enable CONFIG_DRM_FBDEV_EMULATION
    scripts/config --enable CONFIG_FB
    scripts/config --enable CONFIG_FRAMEBUFFER_CONSOLE
    scripts/config --enable CONFIG_FB_VESA
    scripts/config --enable CONFIG_FB_EFI
    
    # Virtual GPU drivers for QEMU/VirtualBox
    scripts/config --enable CONFIG_DRM_VIRTIO_GPU
    scripts/config --enable CONFIG_DRM_BOCHS
    scripts/config --enable CONFIG_DRM_QXL
    
    # Input devices
    scripts/config --enable CONFIG_INPUT_KEYBOARD
    scripts/config --enable CONFIG_INPUT_MOUSE
    scripts/config --enable CONFIG_INPUT_EVDEV
    
    # USB support
    scripts/config --enable CONFIG_USB_SUPPORT
    scripts/config --enable CONFIG_USB
    scripts/config --enable CONFIG_USB_XHCI_HCD
    scripts/config --enable CONFIG_USB_EHCI_HCD
    scripts/config --enable CONFIG_USB_OHCI_HCD
    scripts/config --enable CONFIG_USB_STORAGE
    scripts/config --enable CONFIG_USB_HID
    
    # Networking
    scripts/config --enable CONFIG_NET
    scripts/config --enable CONFIG_INET
    scripts/config --enable CONFIG_PACKET
    scripts/config --enable CONFIG_UNIX
    
    # Filesystem support
    scripts/config --enable CONFIG_EXT4_FS
    scripts/config --enable CONFIG_PROC_FS
    scripts/config --enable CONFIG_SYSFS
    
    make olddefconfig > /dev/null 2>&1
    
    print_status "Kernel configured with graphics support"
}

build_kernel() {
    print_step 4 "Building Linux Kernel"
    
    cd "$WORK_DIR/linux"
    
    print_info "Compiling kernel with $KERNEL_JOBS jobs..."
    print_warning "This takes 15-30 minutes on WSL"
    echo ""
    
    local start_time=$(date +%s)
    
    if make -j "$KERNEL_JOBS" 2>&1 | tee /tmp/kernel-build.log | grep --line-buffered -E '(CC|LD|AR).*\.(o|a|ko)$'; then
        local end_time=$(date +%s)
        local minutes=$(((end_time - start_time) / 60))
        local seconds=$(((end_time - start_time) % 60))
        
        if [ -f arch/x86/boot/bzImage ]; then
            echo ""
            print_status "Kernel built in ${minutes}m ${seconds}s"
            print_info "Kernel size: $(du -h arch/x86/boot/bzImage | cut -f1)"
        else
            print_error "Kernel build failed!"
            exit 1
        fi
    else
        print_error "Kernel compilation failed!"
        exit 1
    fi
}
create_rootfs() {
    print_step 5 "Creating Root Filesystem (Extractor: ar)"
    mkdir -p "$ROOTFS_DIR"
    
    # 2. Run debootstrap with clean arguments

	# Usage inside your create_rootfs function
	mmdebstrap --architecture=amd64 \
		--variant=minbase \
		--include=sway,foot,firefox-esr,network-manager,sudo \
		bookworm "$ROOTFS_DIR" http://deb.debian.org/debian/
    
    # Mount necessary filesystems
    mount -t proc none "$ROOTFS_DIR/proc"
    mount -t sysfs none "$ROOTFS_DIR/sys"
    mount -o bind /dev "$ROOTFS_DIR/dev"
    
    # Install GUI packages in chroot
    cat > "$ROOTFS_DIR/install_gui.sh" << 'INSTALLSCRIPT'
#!/bin/bash
export DEBIAN_FRONTEND=noninteractive

# Update package lists
apt-get update

# Install Wayland and Sway
apt-get install -y \
    sway \
    swaylock \
    swayidle \
    waybar \
    wofi \
    foot \
    wl-clipboard \
    xwayland

# Install basic utilities
apt-get install -y \
    network-manager \
    pulseaudio \
    vim \
    nano \
    firefox-esr \
    pcmanfm \
    grim \
    slurp \
    imv \
    mpv \
    htop

# Clean up
apt-get clean
rm -rf /var/lib/apt/lists/*

echo "GUI components installed"
INSTALLSCRIPT
    
    chmod +x "$ROOTFS_DIR/install_gui.sh"
    chroot "$ROOTFS_DIR" /install_gui.sh
    rm "$ROOTFS_DIR/install_gui.sh"
    
    print_status "GUI components installed"
    
    # Configure Sway
    print_info "Configuring Sway..."
    
    mkdir -p "$ROOTFS_DIR/etc/sway"
    cat > "$ROOTFS_DIR/etc/sway/config" << 'SWAYCONFIG'
# Sway Configuration

# Logo key as modifier
set $mod Mod4

# Terminal
bindsym $mod+Return exec foot

# Application launcher
bindsym $mod+d exec wofi --show drun

# Kill focused window
bindsym $mod+Shift+q kill

# Exit sway
bindsym $mod+Shift+e exec swaynag -t warning -m 'Exit sway?' -b 'Yes' 'swaymsg exit'

# Reload configuration
bindsym $mod+Shift+c reload

# Move focus
bindsym $mod+Left focus left
bindsym $mod+Down focus down
bindsym $mod+Up focus up
bindsym $mod+Right focus right

# Move windows
bindsym $mod+Shift+Left move left
bindsym $mod+Shift+Down move down
bindsym $mod+Shift+Up move up
bindsym $mod+Shift+Right move right

# Fullscreen
bindsym $mod+f fullscreen

# Screenshot
bindsym $mod+Print exec grim -g "$(slurp)" - | wl-copy

# Volume
bindsym XF86AudioRaiseVolume exec pactl set-sink-volume @DEFAULT_SINK@ +5%
bindsym XF86AudioLowerVolume exec pactl set-sink-volume @DEFAULT_SINK@ -5%
bindsym XF86AudioMute exec pactl set-sink-mute @DEFAULT_SINK@ toggle

# Workspaces
bindsym $mod+1 workspace 1
bindsym $mod+2 workspace 2
bindsym $mod+3 workspace 3
bindsym $mod+4 workspace 4

# Status bar
bar {
    position top
    status_command while date +'%Y-%m-%d %H:%M:%S'; do sleep 1; done
    
    colors {
        statusline #ffffff
        background #323232
        inactive_workspace #32323200 #32323200 #5c5c5c
    }
}

# Output configuration
output * bg #003366 solid_color

# Auto-start applications
exec dbus-daemon --session --address=unix:path=$XDG_RUNTIME_DIR/bus
SWAYCONFIG
    
    # Create user and configure auto-login
    print_info "Creating user 'user' with auto-login..."
    
    chroot "$ROOTFS_DIR" useradd -m -s /bin/bash -G sudo user
    chroot "$ROOTFS_DIR" bash -c 'echo "user:password" | chpasswd'
    
    # Auto-start Sway
    cat > "$ROOTFS_DIR/home/user/.bash_profile" << 'BASHPROFILE'
# Auto-start Sway on login
if [ -z "$WAYLAND_DISPLAY" ] && [ "$XDG_VTNR" -eq 1 ]; then
    export XDG_RUNTIME_DIR=/tmp/runtime-user
    mkdir -p $XDG_RUNTIME_DIR
    chmod 700 $XDG_RUNTIME_DIR
    exec sway
fi
BASHPROFILE
    
    chroot "$ROOTFS_DIR" chown -R user:user /home/user
    
    # Configure auto-login
    mkdir -p "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d"
    cat > "$ROOTFS_DIR/etc/systemd/system/getty@tty1.service.d/override.conf" << 'AUTOLOGIN'
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin user --noclear %I $TERM
AUTOLOGIN
    
    # Unmount filesystems
    umount "$ROOTFS_DIR/dev/pts"
    umount "$ROOTFS_DIR/dev"
    umount "$ROOTFS_DIR/sys"
    umount "$ROOTFS_DIR/proc"
    
    print_status "Root filesystem configured"
}

create_initramfs() {
    print_step 6 "Creating Initramfs"
    
    mkdir -p "$BOOT_FILES_DIR/initramfs"
    
    print_info "Creating init script..."
    cat > "$BOOT_FILES_DIR/initramfs/init" << 'INITSCRIPT'
#!/bin/sh

# Mount essential filesystems
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

# Create mount points
mkdir -p /dev/pts /newroot

# Mount devpts
mount -t devpts none /dev/pts

# Wait for disk
sleep 2

# Mount root filesystem
mount -t ext4 /dev/sda1 /newroot

# Switch to new root
exec switch_root /newroot /sbin/init
INITSCRIPT
    
    chmod +x "$BOOT_FILES_DIR/initramfs/init"
    
    # Copy necessary binaries
    mkdir -p "$BOOT_FILES_DIR/initramfs"/{bin,sbin,proc,sys,dev,newroot}
    
    # Use busybox-static package or static binary
    print_info "Installing busybox-static in initramfs..."
    if command -v busybox >/dev/null 2>&1; then
        cp /bin/busybox "$BOOT_FILES_DIR/initramfs/bin/"
    else
        apt-get install -y busybox-static 2>/dev/null || true
        cp /bin/busybox-static "$BOOT_FILES_DIR/initramfs/bin/busybox" 2>/dev/null || true
    fi

    # Fallback: Download static busybox binary
    if [ ! -f "$BOOT_FILES_DIR/initramfs/bin/busybox" ]; then
        print_warning "Downloading static busybox binary..."
        wget -qO- https://busybox.net/downloads/binaries/1.36.1-x86_64-linux-musl/busybox \
            > "$BOOT_FILES_DIR/initramfs/bin/busybox"
        chmod +x "$BOOT_FILES_DIR/initramfs/bin/busybox"
    fi

    # Create all symlinks
    cd "$BOOT_FILES_DIR/initramfs/bin"
    ./busybox --install -s .

    # Create /init symlink
    ln -sf busybox "$BOOT_FILES_DIR/initramfs/init"

    # Create initramfs archive
    cd "$BOOT_FILES_DIR/initramfs"
    find . | cpio -o -H newc 2>/dev/null | gzip > ../initramfs.cpio.gz
    
    print_status "Initramfs created"
}

create_disk_image() {
    print_step 7 "Creating Bootable Disk Image"
    
    cd "$BOOT_FILES_DIR"
    
    print_info "Creating 4GB disk image..."
    dd if=/dev/zero of=linux-gui.img bs=1M count=4096 2>/dev/null
    
    # Install parted only for this step, then remove
    if ! command -v parted >/dev/null 2>&1; then
        print_info "Installing parted temporarily..."
        apt-get install -y parted || true
    fi
    
    print_info "Creating partition..."
    parted -s linux-gui.img mklabel msdos
    parted -s linux-gui.img mkpart primary ext4 1MiB 100%
    
    # Clean up parted after use
    apt-get remove --purge -y parted -qq 2>/dev/null || true
    
    print_info "Creating partition..."
    parted -s linux-gui.img mklabel msdos
    parted -s linux-gui.img mkpart primary ext4 1MiB 100%
    
    print_info "Setting up loop device..."
    LOOP_DEV=$(losetup -f --show linux-gui.img)
    partprobe "$LOOP_DEV"
    
    print_info "Formatting partition..."
    mkfs.ext4 -F "${LOOP_DEV}p1"
    
    print_info "Mounting and copying rootfs..."
    mkdir -p /mnt/guiroot
    mount "${LOOP_DEV}p1" /mnt/guiroot
    
    rsync -a "$ROOTFS_DIR/" /mnt/guiroot/
	
    find "$ROOTFS_DIR" -type f ! -readable -exec chmod +r {} +
	
    print_info "Installing bootloader..."
    # Copy kernel
    cp "$WORK_DIR/linux/arch/x86/boot/bzImage" /mnt/guiroot/boot/
    cp "$BOOT_FILES_DIR/initramfs.cpio.gz" /mnt/guiroot/boot/
    
    # Install GRUB
    apt-get install -y grub-pc-bin 2>&1 | grep "Setting up" || true
    
    grub-install --target=i386-pc --boot-directory=/mnt/guiroot/boot "$LOOP_DEV"
    
    # Configure GRUB
    cat > /mnt/guiroot/boot/grub/grub.cfg << 'GRUBCFG'
set timeout=5
set default=0

menuentry "Custom Linux with Wayland GUI" {
    linux /boot/bzImage root=/dev/sda1 rw quiet
    initrd /boot/initramfs.cpio.gz
}
GRUBCFG
    
    sync
    umount /mnt/guiroot
    losetup -d "$LOOP_DEV"
    
    print_status "Bootable disk image created: $BOOT_FILES_DIR/linux-gui.img"
}

create_test_scripts() {
    print_step 8 "Creating Test Scripts"
    
    cd "$BOOT_FILES_DIR"
    
    cat > test-gui.sh << 'TESTSCRIPT'
#!/bin/bash

echo "Starting Custom Linux with Wayland GUI..."
echo ""
echo "This will launch QEMU with:"
echo "  - 2GB RAM"
echo "  - GPU acceleration"
echo "  - USB tablet for better mouse"
echo ""
echo "Login credentials:"
echo "  Username: user"
echo "  Password: password"
echo ""
echo "Keyboard shortcuts in Sway:"
echo "  Super+Enter    : Open terminal"
echo "  Super+D        : Application launcher"
echo "  Super+Q        : Close window"
echo "  Super+Shift+E  : Exit Sway"
echo ""
echo "Starting in 3 seconds..."
sleep 3

qemu-system-x86_64 \
    -drive file=linux-gui.img,format=raw \
    -m 2048M \
    -smp 2 \
    -vga virtio \
    -display gtk,gl=on \
    -device virtio-tablet-pci \
    -enable-kvm 2>/dev/null || \
    qemu-system-x86_64 \
        -drive file=linux-gui.img,format=raw \
        -m 2048M \
        -smp 2 \
        -vga virtio \
        -display gtk \
        -device virtio-tablet-pci
TESTSCRIPT
    
    chmod +x test-gui.sh
    
    cat > README-GUI.txt << 'README'
CUSTOM LINUX WITH WAYLAND GUI
==============================

Your custom Linux system with Sway Wayland compositor is ready!

FILES:
  linux-gui.img  - 4GB bootable disk image with GUI
  test-gui.sh    - Launch in QEMU
  
TO TEST:
  cd $BOOT_FILES_DIR
  bash test-gui.sh

LOGIN:
  Username: user
  Password: password

INCLUDED SOFTWARE:
  - Sway (Wayland compositor)
  - Waybar (status bar)
  - Foot (terminal)
  - Wofi (application launcher)
  - Firefox ESR (web browser)
  - PCManFM (file manager)
  - Grim/Slurp (screenshots)
  - Imv (image viewer)
  - MPV (video player)

KEYBOARD SHORTCUTS:
  Super+Enter      - Terminal
  Super+D          - App launcher  
  Super+Q          - Close window
  Super+F          - Fullscreen
  Super+1,2,3,4    - Switch workspace
  Super+Shift+E    - Exit

TO COPY TO WINDOWS:
  cp -r $BOOT_FILES_DIR /mnt/c/Users/YOUR_USERNAME/Desktop/

TO BOOT ON REAL HARDWARE:
  dd if=linux-gui.img of=/dev/sdX bs=4M status=progress
  (Replace /dev/sdX with your USB drive)

README
    
    print_status "Test scripts created"
}

show_summary() {
    echo ""
    echo -e "${GREEN}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║         🎉 GUI LINUX SYSTEM BUILD COMPLETE! 🎉               ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    
    echo -e "${CYAN}Build Summary:${NC}"
    echo ""
    echo "  📁 Location: $BOOT_FILES_DIR"
    echo ""
    
    if [ -f "$BOOT_FILES_DIR/linux-gui.img" ]; then
        echo "  💾 Disk Image: $(du -h "$BOOT_FILES_DIR/linux-gui.img" | cut -f1)"
    fi
    
    echo ""
    echo -e "${CYAN}What's Included:${NC}"
    echo "  🐧 Custom Linux Kernel with graphics drivers"
    echo "  🖥️  Sway Wayland Compositor"
    echo "  🌐 Firefox Web Browser"
    echo "  📁 PCManFM File Manager"
    echo "  🖼️  Image & Video Viewers"
    echo "  💻 Full desktop environment"
    echo ""
    echo -e "${CYAN}Next Steps:${NC}"
    echo ""
    echo "  1. Test in QEMU:"
    echo "     ${YELLOW}cd $BOOT_FILES_DIR && bash test-gui.sh${NC}"
    echo ""
    echo "  2. Login with:"
    echo "     Username: ${YELLOW}user${NC}"
    echo "     Password: ${YELLOW}password${NC}"
    echo ""
    echo "  3. Use Super+Enter for terminal, Super+D for apps"
    echo ""
    echo "  4. Copy to Windows:"
    echo "     ${YELLOW}cp -r $BOOT_FILES_DIR /mnt/c/Users/\$USER/Desktop/${NC}"
    echo ""
    echo -e "${GREEN}Enjoy your custom Linux with GUI! 🎊${NC}"
    echo ""
}

main() {
    print_banner
    
    check_root
    
    print_info "Configuration:"
    echo "  Build Directory: $WORK_DIR"
    echo "  Output Directory: $BOOT_FILES_DIR"
    echo "  CPU Cores: $KERNEL_JOBS"
    echo "  Disk Image Size: 4GB"
    echo ""
    print_warning "This build will take 30-60 minutes on WSL"
    print_warning "It will download ~500MB of packages"
    echo ""
    
    read -p "Press Enter to start or Ctrl+C to cancel..."
    
    local total_start=$(date +%s)
    
    install_dependencies
    clone_kernel
    configure_kernel
    build_kernel
    create_rootfs
    create_initramfs
    create_disk_image
    create_test_scripts
    
    local total_end=$(date +%s)
    local total_minutes=$(((total_end - total_start) / 60))
    local total_seconds=$(((total_end - total_start) % 60))
    
    print_info "Total build time: ${total_minutes}m ${total_seconds}s"
    
    show_summary
}

main "$@"

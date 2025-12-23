#!/bin/bash

################################################################################
# CUSTOM LINUX BUILD WITH WAYLAND GUI
# Builds a complete Linux system with Sway (Wayland compositor) and GUI apps
################################################################################

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
# Initial minimal deps (in case they are missing)
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y tar gzip coreutils xorriso

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
BOOT_SIZE_MB=${BOOT_SIZE_MB:-500}  # Unused but kept for compatibility

# Use Linux native paths
WORK_DIR="$HOME/custom-linux-build"
BOOT_FILES_DIR="$HOME/boot-files"
ROOTFS_DIR="$HOME/custom-rootfs"

print_banner() {
    clear || true
    echo -e "${CYAN}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║     CUSTOM LINUX BUILD WITH WAYLAND GUI                       ║
║     Includes Sway Compositor + Desktop Applications           ║
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
print_error()  { echo -e "${RED}[✗]${NC} $1"; }
print_warning(){ echo -e "${YELLOW}[⚠]${NC} $1"; }
print_info()   { echo -e "${BLUE}[ℹ]${NC} $1"; }

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
        bc dwarves pahole cpio rsync wget git \
        busybox-static mmdebstrap zstd binutils dpkg-dev \
        qemu-system-x86 parted grub-pc-bin
    print_status "Dependencies installed"
}

clone_kernel() {
    print_step 2 "Downloading Linux Kernel"
    rm -rf "$WORK_DIR"
    mkdir -p "$WORK_DIR"
    cd "$WORK_DIR"

    if [ ! -d "linux" ]; then
        print_info "Cloning kernel (this takes 2-5 minutes)..."
        git clone --depth 1 https://github.com/torvalds/linux.git linux
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
    scripts/config --enable CONFIG_INPUT
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
    print_warning "This may take 15-30 minutes depending on your system"
    echo ""

    local start_time
    start_time=$(date +%s)

    if make -j "$KERNEL_JOBS" 2>&1 | tee /tmp/kernel-build.log | \
        grep --line-buffered -E '(CC|LD|AR).*\.(o|a|ko)$' || true; then

        local end_time
        end_time=$(date +%s)
        local minutes=$(((end_time - start_time) / 60))
        local seconds=$(((end_time - start_time) % 60))

        if [ -f arch/x86/boot/bzImage ]; then
            echo ""
            print_status "Kernel built in ${minutes}m ${seconds}s"
            print_info "Kernel size: $(du -h arch/x86/boot/bzImage | cut -f1)"
        else
            print_error "Kernel build failed (no bzImage found)!"
            exit 1
        fi
    else
        print_error "Kernel compilation failed!"
        exit 1
    fi
}

create_rootfs() {
    print_step 5 "Creating Root Filesystem (Debian bookworm)"

    mkdir -p "$ROOTFS_DIR"

    mmdebstrap --architecture=amd64 \
        --variant=minbase \
        --include=sway,foot,firefox-esr,network-manager,sudo,wofi,dbus-user-session,seatd,alsa-utils,pulseaudio,grim,slurp,wl-clipboard \
        bookworm "$ROOTFS_DIR" http://deb.debian.org/debian/

    # Mount necessary filesystems
    mount -t proc proc "$ROOTFS_DIR/proc"
    mount -t sysfs sys "$ROOTFS_DIR/sys"
    mount -o bind /dev "$ROOTFS_DIR/dev"
    mkdir -p "$ROOTFS_DIR/dev/pts"
    mount -t devpts devpts "$ROOTFS_DIR/dev/pts"

    # Expect install_gui.sh in the *host* user home
    HOST_USER="${SUDO_USER:-$USER}"
    if [ ! -f "/home/$HOST_USER/install_gui.sh" ]; then
        print_error "install_gui.sh script not found at /home/$HOST_USER/install_gui.sh"
        print_warning "Create that script to add extra GUI packages/config, or touch an empty one."
        touch "/home/$HOST_USER/install_gui.sh"
    fi

    cp "/home/$HOST_USER/install_gui.sh" "$ROOTFS_DIR/install_gui.sh"
    chmod +x "$ROOTFS_DIR/install_gui.sh"

    chroot "$ROOTFS_DIR" /bin/bash /install_gui.sh || true

    print_status "GUI components (base) installed"

    # Configure Sway
    print_info "Configuring Sway..."
    mkdir -p "$ROOTFS_DIR/etc/sway"
    cat > "$ROOTFS_DIR/etc/sway/config" << 'SWAYCONFIG'
# Sway Configuration

set $mod Mod4

bindsym $mod+Return exec foot
bindsym $mod+d exec wofi --show drun
bindsym $mod+Shift+q kill
bindsym $mod+Shift+e exec swaynag -t warning -m 'Exit sway?' -b 'Yes' 'swaymsg exit'
bindsym $mod+Shift+c reload

# Focus movement
bindsym $mod+Left focus left
bindsym $mod+Down focus down
bindsym $mod+Up focus up
bindsym $mod+Right focus right

# Window movement
bindsym $mod+Shift+Left move left
bindsym $mod+Shift+Down move down
bindsym $mod+Shift+Up move up
bindsym $mod+Shift+Right move right

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

bar {
    position top
    status_command while date +'%Y-%m-%d %H:%M:%S'; do sleep 1; done

    colors {
        statusline #ffffff
        background #323232
        inactive_workspace #32323200 #32323200 #5c5c5c
    }
}

output * bg #003366 solid_color

exec dbus-daemon --session --address=unix:path=$XDG_RUNTIME_DIR/bus
SWAYCONFIG

    # Create user and configure auto-login
    print_info "Creating user 'user' with auto-login..."

    chroot "$ROOTFS_DIR" useradd -m -s /bin/bash -G sudo user || true
    chroot "$ROOTFS_DIR" bash -c 'echo "user:password" | chpasswd'

    # Auto-start Sway
    mkdir -p "$ROOTFS_DIR/home/user"
    cat > "$ROOTFS_DIR/home/user/.bash_profile" << 'BASHPROFILE'
# Auto-start Sway on login
if [ -z "$WAYLAND_DISPLAY" ] && [ "${XDG_VTNR:-1}" -eq 1 ]; then
    export XDG_RUNTIME_DIR=/tmp/runtime-user
    mkdir -p "$XDG_RUNTIME_DIR"
    chmod 700 "$XDG_RUNTIME_DIR"
    exec sway
fi
BASHPROFILE

    chroot "$ROOTFS_DIR" chown -R user:user /home/user

    # Configure auto-login on tty1
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

mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

mkdir -p /dev/pts /newroot
mount -t devpts none /dev/pts

sleep 2

mount -t ext4 /dev/sda1 /newroot

exec switch_root /newroot /sbin/init
INITSCRIPT

    chmod +x "$BOOT_FILES_DIR/initramfs/init"

    mkdir -p "$BOOT_FILES_DIR/initramfs"/{bin,sbin,proc,sys,dev,newroot}

    print_info "Installing busybox-static in initramfs..."
    if command -v busybox >/dev/null 2>&1; then
        cp "$(command -v busybox)" "$BOOT_FILES_DIR/initramfs/bin/"
    else
        apt-get install -y busybox-static 2>/dev/null || true
        cp /bin/busybox-static "$BOOT_FILES_DIR/initramfs/bin/busybox" 2>/dev/null || true
    fi

    if [ ! -f "$BOOT_FILES_DIR/initramfs/bin/busybox" ]; then
        print_warning "Downloading static busybox binary..."
        wget -qO "$BOOT_FILES_DIR/initramfs/bin/busybox" \
            https://busybox.net/downloads/binaries/1.36.1-x86_64-linux-musl/busybox
        chmod +x "$BOOT_FILES_DIR/initramfs/bin/busybox"
    fi

    cd "$BOOT_FILES_DIR/initramfs"
    ./bin/busybox --install -s ./bin

    # Ensure /init is our script, not the busybox symlink
    # (already correct from cat > init, so do not overwrite)

    cd "$BOOT_FILES_DIR/initramfs"
    find . | cpio -o -H newc 2>/dev/null | gzip > ../initramfs.cpio.gz

    print_status "Initramfs created"
}
create_iso_image() {
    print_step 7 "Creating UEFI bootable ISO"

    mkdir -p "$BOOT_FILES_DIR"
    ISO_STAGING="$BOOT_FILES_DIR/iso-root"
    ISO_OUTPUT="$BOOT_FILES_DIR/linux-gui-efi.iso"

    rm -rf "$ISO_STAGING"
    mkdir -p "$ISO_STAGING/EFI/boot" \
             "$ISO_STAGING/boot/grub"

    # Copy kernel and initramfs
    cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$ISO_STAGING/boot/"
    cp "$BOOT_FILES_DIR/initramfs.cpio.gz" "$ISO_STAGING/boot/"

    # Create minimal grub.cfg
    cat > "$ISO_STAGING/boot/grub/grub.cfg" << 'GRUBCFG'
set timeout=5
set default=0

menuentry "Custom Linux with Wayland GUI (ISO)" {
    linux /boot/bzImage root=/dev/ram0 rw quiet
    initrd /boot/initramfs.cpio.gz
}
GRUBCFG

    # Create the ISO using grub-mkrescue (needs xorriso)
    grub-mkrescue -o "$ISO_OUTPUT" "$ISO_STAGING"

    print_status "Bootable ISO created: $ISO_OUTPUT"
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
echo "  - GPU acceleration (virtio)"
echo "  - USB tablet for better mouse"
echo ""
echo "Login credentials:"
echo "  Username: user"
echo "  Password: password"
echo ""
echo "Keyboard shortcuts in Sway:"
echo "  Super+Enter    : Open terminal"
echo "  Super+D        : Application launcher"
echo "  Super+Shift+Q  : Close window"
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

    print_status "Test script created: $BOOT_FILES_DIR/test-gui.sh"
    echo "Run it with: cd $BOOT_FILES_DIR && ./test-gui.sh"
}

main() {
    print_banner
    check_root
    install_dependencies
    clone_kernel
    configure_kernel
    build_kernel
    create_rootfs
    create_initramfs
    create_disk_image
    create_test_scripts
    print_status "Linux build with Wayland GUI completed successfully!"
}

main "$@"

#!/bin/bash

################################################################################
# WSL-COMPATIBLE CUSTOM LINUX BUILD SCRIPT
# Special version for Windows Subsystem for Linux (WSL)
################################################################################

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# WSL-specific configuration
KERNEL_JOBS=${KERNEL_JOBS:-$(nproc)}
BUSYBOX_JOBS=${BUSYBOX_JOBS:-$(nproc)}
BOOT_SIZE_MB=${BOOT_SIZE_MB:-50}

# Use Linux native paths, not Windows paths
WORK_DIR="/tmp/custom-linux-build"
BOOT_FILES_DIR="/tmp/boot-files"

print_banner() {
    clear
    echo -e "${CYAN}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║     CUSTOM LINUX BUILD - WSL OPTIMIZED VERSION              ║
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

check_wsl() {
    if grep -qi microsoft /proc/version; then
        print_status "Detected Windows Subsystem for Linux (WSL)"
        return 0
    else
        print_warning "Not running on WSL, but will work anyway"
        return 0
    fi
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root"
        print_warning "Run: sudo bash $0"
        exit 1
    fi
}

install_dependencies() {
    print_step 1 "Installing Dependencies"
    
    print_info "Updating package lists..."
    apt-get update -qq
    
    print_info "Installing build tools (this may take a few minutes)..."
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        build-essential \
        libncurses-dev \
        bison \
        flex \
        libssl-dev \
        libelf-dev \
        bc \
        cpio \
        git \
        wget \
        syslinux \
        dosfstools \
        qemu-system-x86 2>&1 | grep -E "(Setting up|Unpacking)" || true
    
    print_status "All dependencies installed"
}

clone_sources() {
    print_step 2 "Downloading Linux Kernel and Busybox"
    
    mkdir -p "$WORK_DIR"
    cd "$WORK_DIR"
    
    if [ ! -d "linux" ]; then
        print_info "Downloading Linux kernel (latest stable, ~200MB)..."
        print_warning "This will take 2-5 minutes depending on your connection"
        git clone --depth 1 https://github.com/torvalds/linux.git
        print_status "Kernel source downloaded"
    else
        print_warning "Kernel source already exists, skipping download"
    fi
    
    if [ ! -d "busybox" ]; then
        print_info "Downloading Busybox..."
        git clone --depth 1 https://git.busybox.net/busybox
        print_status "Busybox source downloaded"
    else
        print_warning "Busybox source already exists, skipping download"
    fi
}

configure_kernel() {
    print_step 3 "Configuring Kernel"
    
    cd "$WORK_DIR/linux"
    
    print_info "Creating minimal kernel configuration..."
    make defconfig > /dev/null 2>&1
    
    # Enable required options
    scripts/config --enable CONFIG_DEVTMPFS
    scripts/config --enable CONFIG_DEVTMPFS_MOUNT
    scripts/config --enable CONFIG_BLK_DEV_INITRD
    scripts/config --enable CONFIG_RD_GZIP
    scripts/config --enable CONFIG_RD_BZIP2
    scripts/config --enable CONFIG_RD_LZMA
    scripts/config --enable CONFIG_RD_XZ
    
    # Disable debug to reduce size
    scripts/config --disable CONFIG_DEBUG_KERNEL
    scripts/config --disable CONFIG_DEBUG_INFO
    
    make olddefconfig > /dev/null 2>&1
    
    print_status "Kernel configured"
}

build_kernel() {
    print_step 4 "Building Linux Kernel"
    
    cd "$WORK_DIR/linux"
    
    print_info "Compiling kernel with $KERNEL_JOBS parallel jobs..."
    print_warning "THIS WILL TAKE 15-30 MINUTES on WSL"
    print_warning "You will see compilation messages below..."
    echo ""
    echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    
    local start_time=$(date +%s)
    
    # Build kernel with visible output
    if make -j "$KERNEL_JOBS" 2>&1 | tee /tmp/kernel-build-wsl.log | grep --line-buffered -E '(CC|LD|AR|HOSTCC|OBJCOPY|Building)'; then
        echo ""
        echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        echo ""
        
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        local minutes=$((duration / 60))
        local seconds=$((duration % 60))
        
        if [ -f arch/x86/boot/bzImage ]; then
            print_status "Kernel built successfully in ${minutes}m ${seconds}s"
            local size=$(du -h arch/x86/boot/bzImage | cut -f1)
            print_info "Kernel size: $size"
        else
            print_error "Kernel built but bzImage not found!"
            print_error "Check log: /tmp/kernel-build-wsl.log"
            exit 1
        fi
    else
        print_error "Kernel compilation failed!"
        print_error "Check the full log: /tmp/kernel-build-wsl.log"
        print_error "Common WSL issues:"
        echo "  - Low memory (increase WSL memory in .wslconfig)"
        echo "  - Disk space (need 10GB free)"
        exit 1
    fi
}

configure_busybox() {
    print_step 5 "Configuring Busybox"
    
    cd "$WORK_DIR/busybox"
    
    print_info "Creating busybox configuration..."
    make defconfig > /dev/null 2>&1
    
    # Enable static build
    sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
    
    make oldconfig > /dev/null 2>&1
    
    print_status "Busybox configured for static build"
}

build_busybox() {
    print_step 6 "Building Busybox"
    
    cd "$WORK_DIR/busybox"
    
    print_info "Compiling busybox..."
    
    if make -j "$BUSYBOX_JOBS" 2>&1 | tee /tmp/busybox-build-wsl.log; then
        if [ -f busybox ]; then
            print_status "Busybox built successfully"
            local size=$(du -h busybox | cut -f1)
            print_info "Busybox size: $size"
        else
            print_error "Busybox binary not created!"
            exit 1
        fi
    else
        print_error "Busybox build failed!"
        exit 1
    fi
}

create_initramfs() {
    print_step 7 "Creating Root Filesystem"
    
    mkdir -p "$BOOT_FILES_DIR/initramfs"
    
    print_info "Installing busybox..."
    cd "$WORK_DIR/busybox"
    make CONFIG_PREFIX="$BOOT_FILES_DIR/initramfs" install > /dev/null 2>&1
    
    cd "$BOOT_FILES_DIR/initramfs"
    [ -f linuxrc ] && rm linuxrc
    
    print_info "Creating init script..."
    cat > init << 'INITSCRIPT'
#!/bin/sh

mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

mkdir -p /dev/pts /dev/shm /tmp
mount -t devpts devpts /dev/pts
mount -t tmpfs tmpfs /dev/shm
mount -t tmpfs tmpfs /tmp

clear
cat << 'EOF'

╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║          Welcome to Your Custom Linux System!                ║
║                                                               ║
║          Built on WSL - Running Native Linux                 ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝

EOF

echo "System Info:"
echo "  Kernel: $(uname -r)"
echo "  Arch:   $(uname -m)"
echo "  Memory: $(free -h | grep Mem | awk '{print $2}')"
echo ""
echo "Type 'busybox --list' to see available commands"
echo ""

exec /bin/sh
INITSCRIPT
    
    chmod +x init
    
    print_info "Packing initramfs..."
    find . | cpio -o -H newc 2>/dev/null > ../init.cpio
    
    print_status "Initramfs created: $BOOT_FILES_DIR/init.cpio"
}

create_boot_image() {
    print_step 8 "Creating Bootable Image"
    
    cd "$BOOT_FILES_DIR"
    
    # Copy kernel
    print_info "Copying kernel..."
    cp "$WORK_DIR/linux/arch/x86/boot/bzImage" .
    
    print_info "Creating boot disk image..."
    dd if=/dev/zero of=boot.img bs=1M count="$BOOT_SIZE_MB" 2>&1 | grep -E "(copied|MB)" || true
    
    print_info "Formatting..."
    mkfs.fat boot.img > /dev/null 2>&1
    
    print_info "Installing bootloader..."
    syslinux boot.img
    
    print_info "Copying files to boot image..."
    mkdir -p mnt
    mount boot.img mnt
    cp bzImage init.cpio mnt/
    
    cat > mnt/syslinux.cfg << 'SYSLINUXCFG'
DEFAULT linux
PROMPT 0
TIMEOUT 10

LABEL linux
    KERNEL bzImage
    APPEND initrd=init.cpio
SYSLINUXCFG
    
    umount mnt
    rmdir mnt
    
    print_status "Boot image created: $BOOT_FILES_DIR/boot.img"
}

create_test_scripts() {
    print_step 9 "Creating Test Scripts"
    
    cd "$BOOT_FILES_DIR"
    
    cat > test-qemu.sh << 'TESTSCRIPT'
#!/bin/bash
echo "Testing with QEMU..."
if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "ERROR: QEMU not found"
    exit 1
fi

BOOT_DIR="/tmp/boot-files"
echo "Starting VM..."
echo "Press Ctrl+Alt+G to release mouse, close window to exit"
echo ""

qemu-system-x86_64 \
    -kernel "$BOOT_DIR/bzImage" \
    -initrd "$BOOT_DIR/init.cpio" \
    -m 512M \
    -append "console=tty0"
TESTSCRIPT
    
    chmod +x test-qemu.sh
    
    cat > README.txt << 'README'
CUSTOM LINUX BUILD - WSL VERSION
=================================

Your custom Linux system has been built!

Files created in: /tmp/boot-files/
  - bzImage      : Linux kernel
  - init.cpio    : Root filesystem
  - boot.img     : Bootable disk image
  - test-qemu.sh : Test script

To test:
  cd /tmp/boot-files
  bash test-qemu.sh

To copy to Windows:
  cp -r /tmp/boot-files /mnt/c/Users/YOUR_USERNAME/Desktop/

Build logs:
  - /tmp/kernel-build-wsl.log
  - /tmp/busybox-build-wsl.log

README
    
    print_status "Test scripts created"
}

show_summary() {
    echo ""
    echo -e "${GREEN}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║              🎉 BUILD COMPLETED SUCCESSFULLY! 🎉             ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    
    echo -e "${CYAN}Build Summary:${NC}"
    echo ""
    echo "  📁 Output Directory:  $BOOT_FILES_DIR"
    echo ""
    
    if [ -f "$BOOT_FILES_DIR/bzImage" ]; then
        echo "  🐧 Kernel:     $(du -h "$BOOT_FILES_DIR/bzImage" | cut -f1)"
    fi
    
    if [ -f "$BOOT_FILES_DIR/init.cpio" ]; then
        echo "  📦 Initramfs:  $(du -h "$BOOT_FILES_DIR/init.cpio" | cut -f1)"
    fi
    
    if [ -f "$BOOT_FILES_DIR/boot.img" ]; then
        echo "  💾 Boot Image: $(du -h "$BOOT_FILES_DIR/boot.img" | cut -f1)"
    fi
    
    echo ""
    echo -e "${CYAN}Next Steps:${NC}"
    echo ""
    echo "  1. Test with QEMU:"
    echo "     ${YELLOW}cd $BOOT_FILES_DIR && bash test-qemu.sh${NC}"
    echo ""
    echo "  2. Copy to Windows Desktop:"
    echo "     ${YELLOW}cp -r $BOOT_FILES_DIR /mnt/c/Users/\$USER/Desktop/${NC}"
    echo ""
    echo "  3. View build logs:"
    echo "     ${YELLOW}less /tmp/kernel-build-wsl.log${NC}"
    echo ""
    echo -e "${GREEN}🎊 Congratulations! Your custom Linux is ready! 🎊${NC}"
    echo ""
}

main() {
    print_banner
    
    check_wsl
    check_root
    
    print_info "Configuration:"
    echo "  Working Directory: $WORK_DIR"
    echo "  Output Directory:  $BOOT_FILES_DIR"
    echo "  CPU Cores:         $KERNEL_JOBS"
    echo "  Boot Image Size:   ${BOOT_SIZE_MB}MB"
    echo ""
    
    read -p "Press Enter to start build or Ctrl+C to cancel..."
    
    local total_start=$(date +%s)
    
    install_dependencies
    clone_sources
    configure_kernel
    build_kernel
    configure_busybox
    build_busybox
    create_initramfs
    create_boot_image
    create_test_scripts
    
    local total_end=$(date +%s)
    local total_duration=$((total_end - total_start))
    local total_minutes=$((total_duration / 60))
    local total_seconds=$((total_duration % 60))
    
    print_info "Total build time: ${total_minutes}m ${total_seconds}s"
    
    show_summary
}

main "$@"

#!/bin/bash

################################################################################
# COMPLETE AUTOMATED CUSTOM LINUX BUILD SCRIPT
# One script to rule them all - fully automated from start to finish
################################################################################

set -e  # Exit on any error

# Color codes
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
BOOT_SIZE_MB=${BOOT_SIZE_MB:-50}
WORK_DIR=${WORK_DIR:-$(pwd)/custom-linux-build}
BOOT_FILES_DIR="/boot-files"
AUTO_MODE=false
SKIP_DEPS=false
SKIP_TESTS=false

################################################################################
# Helper Functions
################################################################################

print_banner() {
    echo -e "${CYAN}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║     CUSTOM LINUX KERNEL & BUSYBOX BUILD AUTOMATION          ║
║                                                               ║
║     Build a minimal Linux system from scratch                ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
}

print_step() {
    echo -e "\n${MAGENTA}▶ STEP $1${NC}: ${CYAN}$2${NC}\n"
}

print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[⚠]${NC} $1"
}

print_info() {
    echo -e "${BLUE}[ℹ]${NC} $1"
}

progress_bar() {
    local duration=$1
    local message=$2
    local progress=0
    local bar_length=50
    
    while [ $progress -le 100 ]; do
        local filled=$((progress * bar_length / 100))
        local empty=$((bar_length - filled))
        printf "\r${BLUE}[ℹ]${NC} $message ["
        printf "%${filled}s" | tr ' ' '='
        printf "%${empty}s" | tr ' ' '-'
        printf "] ${progress}%%"
        progress=$((progress + 2))
        sleep $(echo "$duration / 50" | bc -l)
    done
    echo ""
}

spinner() {
    local pid=$1
    local message=$2
    local spin='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    local i=0
    
    while kill -0 $pid 2>/dev/null; do
        i=$(( (i+1) %10 ))
        printf "\r${BLUE}[${spin:$i:1}]${NC} $message"
        sleep .1
    done
    printf "\r"
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root"
        print_warning "Please run: sudo $0"
        exit 1
    fi
}

detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
        DISTRO_VERSION=$VERSION_ID
        print_info "Detected: $NAME $VERSION_ID"
    else
        print_error "Cannot detect distribution"
        exit 1
    fi
}

check_disk_space() {
    local required_gb=10
    local available_gb=$(df -BG $(pwd) | tail -1 | awk '{print $4}' | sed 's/G//')
    
    if [ "$available_gb" -lt "$required_gb" ]; then
        print_error "Insufficient disk space. Required: ${required_gb}GB, Available: ${available_gb}GB"
        exit 1
    fi
    
    print_status "Disk space check: ${available_gb}GB available (${required_gb}GB required)"
}

check_internet() {
    print_info "Checking internet connectivity..."
    if ping -c 1 github.com >/dev/null 2>&1 || ping -c 1 google.com >/dev/null 2>&1; then
        print_status "Internet connection: OK"
        return 0
    else
        print_error "No internet connection detected"
        exit 1
    fi
}

################################################################################
# STEP 1: System Preparation
################################################################################

prepare_system() {
    print_step 1 "System Preparation"
    
    check_root
    detect_distro
    check_disk_space
    check_internet
    
    print_status "System preparation complete"
}

################################################################################
# STEP 2: Install Dependencies
################################################################################

install_dependencies() {
    print_step 2 "Installing Dependencies"
    
    if [ "$SKIP_DEPS" = true ]; then
        print_warning "Skipping dependency installation"
        return
    fi
    
    case $DISTRO in
        debian|ubuntu)
            print_info "Installing packages for Debian/Ubuntu..."
            apt-get update -qq
            DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
                bzip2 git vim make gcc libncurses-dev flex bison \
                bc cpio libelf-dev libssl-dev syslinux dosfstools \
                nano wget curl > /dev/null 2>&1 &
            spinner $! "Installing dependencies"
            wait $!
            ;;
        fedora|rhel|centos)
            print_info "Installing packages for Fedora/RHEL..."
            dnf install -y -q \
                bzip2 git vim make gcc ncurses-devel flex bison \
                bc cpio elfutils-libelf-devel openssl-devel \
                syslinux dosfstools nano wget curl > /dev/null 2>&1 &
            spinner $! "Installing dependencies"
            wait $!
            ;;
        arch)
            print_info "Installing packages for Arch Linux..."
            pacman -Sy --noconfirm --quiet \
                base-devel git vim bc cpio syslinux dosfstools > /dev/null 2>&1 &
            spinner $! "Installing dependencies"
            wait $!
            ;;
        *)
            print_error "Unsupported distribution: $DISTRO"
            print_warning "Please install dependencies manually"
            exit 1
            ;;
    esac
    
    print_status "Dependencies installed successfully"
}

################################################################################
# STEP 3: Clone Repositories
################################################################################

clone_repositories() {
    print_step 3 "Cloning Source Repositories"
    
    mkdir -p "$WORK_DIR"
    cd "$WORK_DIR"
    
    # Clone Linux kernel
    if [ -d "linux" ]; then
        print_warning "Linux kernel directory exists, skipping clone"
    else
        print_info "Cloning Linux kernel (this may take a few minutes)..."
        git clone --depth 1 https://github.com/torvalds/linux.git > /dev/null 2>&1 &
        spinner $! "Cloning Linux kernel repository"
        wait $!
        print_status "Linux kernel cloned successfully"
    fi
    
    # Clone Busybox
    if [ -d "busybox" ]; then
        print_warning "Busybox directory exists, skipping clone"
    else
        print_info "Cloning Busybox..."
        git clone --depth 1 https://git.busybox.net/busybox > /dev/null 2>&1 &
        spinner $! "Cloning Busybox repository"
        wait $!
        print_status "Busybox cloned successfully"
    fi
}

################################################################################
# STEP 4: Configure Kernel (Automated)
################################################################################

configure_kernel() {
    print_step 4 "Configuring Linux Kernel"
    
    cd "$WORK_DIR/linux"
    
    if [ -f .config ]; then
        print_warning "Kernel configuration exists, using existing config"
        return
    fi
    
    print_info "Using default configuration with minimal settings..."
    
    # Create a minimal config
    make defconfig > /dev/null 2>&1
    
    # Enable essential options
    scripts/config --enable CONFIG_DEVTMPFS
    scripts/config --enable CONFIG_DEVTMPFS_MOUNT
    scripts/config --enable CONFIG_BLK_DEV_INITRD
    scripts/config --enable CONFIG_RD_GZIP
    scripts/config --enable CONFIG_RD_BZIP2
    scripts/config --enable CONFIG_RD_LZMA
    scripts/config --enable CONFIG_RD_XZ
    scripts/config --enable CONFIG_RD_LZO
    scripts/config --enable CONFIG_RD_LZ4
    
    # Disable unnecessary features for smaller kernel
    scripts/config --disable CONFIG_DEBUG_KERNEL
    scripts/config --disable CONFIG_DEBUG_INFO
    
    make olddefconfig > /dev/null 2>&1
    
    print_status "Kernel configured with minimal settings"
}

################################################################################
# STEP 5: Build Kernel
################################################################################

build_kernel() {
    print_step 5 "Building Linux Kernel"
    
    cd "$WORK_DIR/linux"
    
    if [ -f arch/x86/boot/bzImage ]; then
        print_warning "Kernel image exists, rebuilding..."
    fi
    
    print_info "Building kernel with $KERNEL_JOBS parallel jobs..."
    print_warning "This will take 10-30 minutes depending on your system"
    
    local start_time=$(date +%s)
    
    make -j "$KERNEL_JOBS" > /tmp/kernel-build.log 2>&1 &
    local build_pid=$!
    
    spinner $build_pid "Compiling kernel ($(nproc) cores active)"
    wait $build_pid
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    local minutes=$((duration / 60))
    local seconds=$((duration % 60))
    
    if [ -f arch/x86/boot/bzImage ]; then
        print_status "Kernel built successfully in ${minutes}m ${seconds}s"
        local size=$(du -h arch/x86/boot/bzImage | cut -f1)
        print_info "Kernel size: $size"
    else
        print_error "Kernel build failed"
        print_error "Check log: /tmp/kernel-build.log"
        exit 1
    fi
}

################################################################################
# STEP 6: Copy Kernel Image
################################################################################

copy_kernel() {
    print_step 6 "Copying Kernel Image"
    
    mkdir -p "$BOOT_FILES_DIR"
    cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$BOOT_FILES_DIR/"
    
    print_status "Kernel image copied to $BOOT_FILES_DIR/bzImage"
}

################################################################################
# STEP 7: Configure Busybox (Automated)
################################################################################

configure_busybox() {
    print_step 7 "Configuring Busybox"
    
    cd "$WORK_DIR/busybox"
    
    if [ -f .config ]; then
        print_warning "Busybox configuration exists, using existing config"
        return
    fi
    
    print_info "Creating static busybox configuration..."
    
    make defconfig > /dev/null 2>&1
    
    # Enable static build
    sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
    
    make oldconfig > /dev/null 2>&1
    
    print_status "Busybox configured for static build"
}

################################################################################
# STEP 8: Build Busybox
################################################################################

build_busybox() {
    print_step 8 "Building Busybox"
    
    cd "$WORK_DIR/busybox"
    
    print_info "Building busybox with $BUSYBOX_JOBS parallel jobs..."
    
    local start_time=$(date +%s)
    
    make -j "$BUSYBOX_JOBS" > /tmp/busybox-build.log 2>&1 &
    local build_pid=$!
    
    spinner $build_pid "Compiling busybox"
    wait $build_pid
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    if [ -f busybox ]; then
        print_status "Busybox built successfully in ${duration}s"
        local size=$(du -h busybox | cut -f1)
        print_info "Busybox size: $size"
    else
        print_error "Busybox build failed"
        print_error "Check log: /tmp/busybox-build.log"
        exit 1
    fi
}

################################################################################
# STEP 9: Create Initramfs
################################################################################

create_initramfs() {
    print_step 9 "Creating Initial RAM Filesystem"
    
    print_info "Installing busybox to initramfs..."
    mkdir -p "$BOOT_FILES_DIR/initramfs"
    
    cd "$WORK_DIR/busybox"
    make CONFIG_PREFIX="$BOOT_FILES_DIR/initramfs" install > /dev/null 2>&1
    
    cd "$BOOT_FILES_DIR/initramfs"
    
    # Remove linuxrc
    [ -f linuxrc ] && rm linuxrc
    
    print_info "Creating init script..."
    cat > init << 'INITSCRIPT'
#!/bin/sh

# Mount essential filesystems
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

# Create additional mount points
mkdir -p /dev/pts /dev/shm /tmp /run

# Mount additional filesystems
mount -t devpts devpts /dev/pts
mount -t tmpfs tmpfs /dev/shm
mount -t tmpfs tmpfs /tmp
mount -t tmpfs tmpfs /run

# Set up hostname
echo "custom-linux" > /proc/sys/kernel/hostname

# Clear screen and show banner
clear
cat << 'EOF'

╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║              Welcome to Custom Linux System                  ║
║                                                               ║
║              Built from scratch with love                    ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝

EOF

echo "System Information:"
echo "  Kernel: $(uname -r)"
echo "  Architecture: $(uname -m)"
echo "  Hostname: $(hostname)"
echo "  Memory: $(free -h | grep Mem | awk '{print $2}') total"
echo ""
echo "Available commands: Type 'busybox --list' to see all"
echo ""

# Start shell
exec /bin/sh
INITSCRIPT
    
    chmod +x init
    
    print_info "Creating initramfs archive..."
    find . | cpio -o -H newc 2>/dev/null | gzip > ../init.cpio.gz
    
    # Also create uncompressed version
    find . | cpio -o -H newc > ../init.cpio 2>/dev/null
    
    print_status "Initramfs created successfully"
}

################################################################################
# STEP 10: Create Bootable Image
################################################################################

create_bootable_image() {
    print_step 10 "Creating Bootable Disk Image"
    
    cd "$BOOT_FILES_DIR"
    
    print_info "Creating ${BOOT_SIZE_MB}MB disk image..."
    dd if=/dev/zero of=boot bs=1M count="$BOOT_SIZE_MB" 2>/dev/null
    
    print_info "Formatting as FAT filesystem..."
    mkfs.fat boot > /dev/null 2>&1
    
    print_info "Installing bootloader (syslinux)..."
    syslinux boot
    
    print_info "Mounting disk image..."
    mkdir -p m
    mount boot m
    
    print_info "Copying kernel and initramfs..."
    cp bzImage init.cpio m/
    
    print_info "Creating bootloader configuration..."
    cat > m/syslinux.cfg << 'SYSLINUXCFG'
DEFAULT linux
PROMPT 0
TIMEOUT 10

LABEL linux
    KERNEL bzImage
    APPEND initrd=init.cpio quiet
SYSLINUXCFG
    
    print_info "Unmounting disk image..."
    umount m
    rmdir m
    
    local size=$(du -h boot | cut -f1)
    print_status "Bootable image created: $BOOT_FILES_DIR/boot ($size)"
}

################################################################################
# STEP 11: Generate Boot Scripts
################################################################################

generate_scripts() {
    print_step 11 "Generating Helper Scripts"
    
    cd "$BOOT_FILES_DIR"
    
    # Test script
    cat > test-qemu.sh << 'TESTSCRIPT'
#!/bin/bash

echo "Testing Custom Linux with QEMU..."
echo ""

if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "ERROR: QEMU not installed"
    echo "Install with: sudo apt-get install qemu-system-x86"
    exit 1
fi

echo "Starting VM with 512MB RAM..."
echo "Press Ctrl+Alt+G to release mouse, then close window to exit"
echo ""

qemu-system-x86_64 \
    -kernel /boot-files/bzImage \
    -initrd /boot-files/init.cpio \
    -m 512M \
    -append "console=tty0"
TESTSCRIPT
    
    chmod +x test-qemu.sh
    
    # Boot from image script
    cat > test-boot-image.sh << 'BOOTSCRIPT'
#!/bin/bash

echo "Testing Boot Image with QEMU..."
echo ""

if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "ERROR: QEMU not installed"
    exit 1
fi

qemu-system-x86_64 \
    -drive file=/boot-files/boot,format=raw \
    -m 512M
BOOTSCRIPT
    
    chmod +x test-boot-image.sh
    
    # USB writer script
    cat > write-to-usb.sh << 'USBSCRIPT'
#!/bin/bash

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root: sudo $0"
    exit 1
fi

echo "WARNING: This will ERASE ALL DATA on the target device!"
echo ""
lsblk
echo ""
read -p "Enter device (e.g., /dev/sdb): " DEVICE

if [ ! -b "$DEVICE" ]; then
    echo "ERROR: $DEVICE is not a valid block device"
    exit 1
fi

read -p "Are you ABSOLUTELY SURE you want to write to $DEVICE? (type 'yes'): " CONFIRM

if [ "$CONFIRM" != "yes" ]; then
    echo "Aborted"
    exit 0
fi

echo "Writing to $DEVICE..."
dd if=/boot-files/boot of=$DEVICE bs=4M status=progress
sync

echo "Done! You can now boot from $DEVICE"
USBSCRIPT
    
    chmod +x write-to-usb.sh
    
    print_status "Helper scripts created"
}

################################################################################
# STEP 12: Run Tests
################################################################################

run_tests() {
    print_step 12 "System Verification"
    
    if [ "$SKIP_TESTS" = true ]; then
        print_warning "Skipping tests"
        return
    fi
    
    print_info "Verifying build outputs..."
    
    local all_good=true
    
    # Check kernel
    if [ -f "$BOOT_FILES_DIR/bzImage" ]; then
        print_status "Kernel image: OK"
    else
        print_error "Kernel image: MISSING"
        all_good=false
    fi
    
    # Check initramfs
    if [ -f "$BOOT_FILES_DIR/init.cpio" ]; then
        print_status "Initramfs: OK"
    else
        print_error "Initramfs: MISSING"
        all_good=false
    fi
    
    # Check boot image
    if [ -f "$BOOT_FILES_DIR/boot" ]; then
        print_status "Boot image: OK"
    else
        print_error "Boot image: MISSING"
        all_good=false
    fi
    
    # Check scripts
    if [ -f "$BOOT_FILES_DIR/test-qemu.sh" ]; then
        print_status "Test scripts: OK"
    else
        print_error "Test scripts: MISSING"
        all_good=false
    fi
    
    if [ "$all_good" = true ]; then
        print_status "All verifications passed!"
    else
        print_error "Some verifications failed"
        return 1
    fi
}

################################################################################
# Final Summary
################################################################################

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
    echo "  📁 Output Directory: $BOOT_FILES_DIR"
    echo "  📁 Source Directory: $WORK_DIR"
    echo ""
    
    if [ -f "$BOOT_FILES_DIR/bzImage" ]; then
        local kernel_size=$(du -h "$BOOT_FILES_DIR/bzImage" | cut -f1)
        echo "  🐧 Kernel Image:     $BOOT_FILES_DIR/bzImage ($kernel_size)"
    fi
    
    if [ -f "$BOOT_FILES_DIR/init.cpio" ]; then
        local initramfs_size=$(du -h "$BOOT_FILES_DIR/init.cpio" | cut -f1)
        echo "  📦 Initramfs:        $BOOT_FILES_DIR/init.cpio ($initramfs_size)"
    fi
    
    if [ -f "$BOOT_FILES_DIR/boot" ]; then
        local boot_size=$(du -h "$BOOT_FILES_DIR/boot" | cut -f1)
        echo "  💾 Boot Image:       $BOOT_FILES_DIR/boot ($boot_size)"
    fi
    
    echo ""
    echo -e "${CYAN}Next Steps:${NC}"
    echo ""
    echo "  1. Test with QEMU:"
    echo "     ${YELLOW}cd $BOOT_FILES_DIR && ./test-qemu.sh${NC}"
    echo ""
    echo "  2. Test boot image:"
    echo "     ${YELLOW}cd $BOOT_FILES_DIR && ./test-boot-image.sh${NC}"
    echo ""
    echo "  3. Write to USB drive:"
    echo "     ${YELLOW}cd $BOOT_FILES_DIR && sudo ./write-to-usb.sh${NC}"
    echo ""
    echo "  4. Customize init script:"
    echo "     ${YELLOW}nano $BOOT_FILES_DIR/initramfs/init${NC}"
    echo ""
    echo -e "${GREEN}Happy Hacking! 🚀${NC}"
    echo ""
}

################################################################################
# Usage and Main
################################################################################

show_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build a complete custom Linux system from scratch automatically.

Options:
    --auto              Fully automatic mode (no interaction)
    --skip-deps         Skip dependency installation
    --skip-tests        Skip verification tests
    --kernel-jobs N     Parallel jobs for kernel (default: $(nproc))
    --busybox-jobs N    Parallel jobs for busybox (default: $(nproc))
    --boot-size N       Boot image size in MB (default: 50)
    --work-dir DIR      Working directory (default: ./custom-linux-build)
    --help              Show this help

Environment Variables:
    KERNEL_JOBS         Same as --kernel-jobs
    BUSYBOX_JOBS        Same as --busybox-jobs
    BOOT_SIZE_MB        Same as --boot-size
    WORK_DIR            Same as --work-dir

Examples:
    # Fully automatic build
    sudo $0 --auto

    # Use all CPU cores
    sudo $0 --kernel-jobs \$(nproc)

    # Custom directory and size
    sudo $0 --work-dir /tmp/build --boot-size 100

EOF
}

main() {
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --auto)
                AUTO_MODE=true
                shift
                ;;
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            --skip-tests)
                SKIP_TESTS=true
                shift
                ;;
            --kernel-jobs)
                KERNEL_JOBS="$2"
                shift 2
                ;;
            --busybox-jobs)
                BUSYBOX_JOBS="$2"
                shift 2
                ;;
            --boot-size)
                BOOT_SIZE_MB="$2"
                shift 2
                ;;
            --work-dir)
                WORK_DIR="$2"
                shift 2
                ;;
            --help)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Show banner
    clear
    print_banner
    
    print_info "Configuration:"
    echo "  Working Directory:  $WORK_DIR"
    echo "  Output Directory:   $BOOT_FILES_DIR"
    echo "  Kernel Jobs:        $KERNEL_JOBS"
    echo "  Busybox Jobs:       $BUSYBOX_JOBS"
    echo "  Boot Image Size:    ${BOOT_SIZE_MB}MB"
    echo "  Auto Mode:          $AUTO_MODE"
    echo ""
    
    if [ "$AUTO_MODE" = false ]; then
        read -p "Press Enter to continue or Ctrl+C to cancel..."
    fi
    
    local total_start=$(date +%s)
    
    # Execute build steps
    prepare_system
    install_dependencies
    clone_repositories
    configure_kernel
    build_kernel
    copy_kernel
    configure_busybox
    build_busybox
    create_initramfs
    create_bootable_image
    generate_scripts
    run_tests
    
    local total_end=$(date +%s)
    local total_duration=$((total_end - total_start))
    local total_minutes=$((total_duration / 60))
    local total_seconds=$((total_duration % 60))
    
    echo ""
    print_info "Total build time: ${total_minutes}m ${total_seconds}s"
    
    show_summary
}

# Run main function
main "$@"
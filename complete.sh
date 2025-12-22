#!/bin/bash

################################################################################
# COMPLETE AUTOMATED CUSTOM LINUX BUILD SCRIPT - VERBOSE VERSION
# Shows real-time build output so you know what's happening
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

################################################################################
# Helper Functions
################################################################################

print_banner() {
    clear
    echo -e "${CYAN}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║     CUSTOM LINUX KERNEL & BUSYBOX BUILD AUTOMATION          ║
║              (VERBOSE MODE - Shows Progress)                 ║
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

check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root"
        print_warning "Please run: sudo bash $0"
        exit 1
    fi
}

detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        DISTRO=$ID
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
    
    print_status "Disk space: ${available_gb}GB available (need ${required_gb}GB)"
}

check_internet() {
    print_info "Checking internet connectivity..."
    if ping -c 1 -W 2 github.com >/dev/null 2>&1 || ping -c 1 -W 2 google.com >/dev/null 2>&1; then
        print_status "Internet connection: OK"
        return 0
    else
        print_error "No internet connection detected"
        exit 1
    fi
}

################################################################################
# Build Steps
################################################################################

prepare_system() {
    print_step 1 "System Preparation"
    check_root
    detect_distro
    check_disk_space
    check_internet
    print_status "System preparation complete"
}

install_dependencies() {
    print_step 2 "Installing Dependencies"
    
    if [ "$SKIP_DEPS" = true ]; then
        print_warning "Skipping dependency installation"
        return
    fi
    
    case $DISTRO in
        debian|ubuntu)
            print_info "Updating package lists..."
            apt-get update
            print_info "Installing build dependencies..."
            DEBIAN_FRONTEND=noninteractive apt-get install -y \
                bzip2 git vim make gcc libncurses-dev flex bison \
                bc cpio libelf-dev libssl-dev syslinux dosfstools nano wget curl
            ;;
        fedora|rhel|centos)
            print_info "Installing dependencies..."
            dnf install -y \
                bzip2 git vim make gcc ncurses-devel flex bison \
                bc cpio elfutils-libelf-devel openssl-devel \
                syslinux dosfstools nano wget curl
            ;;
        *)
            print_error "Unsupported distribution: $DISTRO"
            exit 1
            ;;
    esac
    
    print_status "Dependencies installed successfully"
}

clone_repositories() {
    print_step 3 "Cloning Source Repositories"
    
    mkdir -p "$WORK_DIR"
    cd "$WORK_DIR"
    
    if [ -d "linux" ]; then
        print_warning "Linux kernel directory exists, skipping clone"
    else
        print_info "Cloning Linux kernel (this may take 2-5 minutes)..."
        git clone --depth 1 https://github.com/torvalds/linux.git
        print_status "Linux kernel cloned successfully"
    fi
    
    if [ -d "busybox" ]; then
        print_warning "Busybox directory exists, skipping clone"
    else
        print_info "Cloning Busybox..."
        git clone --depth 1 https://git.busybox.net/busybox
        print_status "Busybox cloned successfully"
    fi
}

configure_kernel() {
    print_step 4 "Configuring Linux Kernel"
    
    cd "$WORK_DIR/linux"
    
    if [ -f .config ]; then
        print_warning "Using existing kernel configuration"
        return
    fi
    
    print_info "Creating default configuration..."
    make defconfig
    
    print_info "Enabling essential options..."
    scripts/config --enable CONFIG_DEVTMPFS
    scripts/config --enable CONFIG_DEVTMPFS_MOUNT
    scripts/config --enable CONFIG_BLK_DEV_INITRD
    scripts/config --disable CONFIG_DEBUG_KERNEL
    scripts/config --disable CONFIG_DEBUG_INFO
    
    make olddefconfig > /dev/null 2>&1
    
    print_status "Kernel configured successfully"
}

build_kernel() {
    print_step 5 "Building Linux Kernel"
    
    cd "$WORK_DIR/linux"
    
    print_info "Starting kernel compilation with $KERNEL_JOBS parallel jobs..."
    print_warning "This will take 10-30 minutes. You'll see compilation output below."
    echo ""
    echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    
    local start_time=$(date +%s)
    
    # Show actual build output
    if make -j "$KERNEL_JOBS" 2>&1 | tee /tmp/kernel-build.log | grep --line-buffered -E '(CC|LD|AR|HOSTCC|OBJCOPY)'; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        local minutes=$((duration / 60))
        local seconds=$((duration % 60))
        
        echo ""
        echo -e "${YELLOW}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        echo ""
        print_status "Kernel built successfully in ${minutes}m ${seconds}s"
        
        if [ -f arch/x86/boot/bzImage ]; then
            local size=$(du -h arch/x86/boot/bzImage | cut -f1)
            print_info "Kernel size: $size"
        fi
    else
        print_error "Kernel build failed! Check /tmp/kernel-build.log"
        exit 1
    fi
}

copy_kernel() {
    print_step 6 "Copying Kernel Image"
    mkdir -p "$BOOT_FILES_DIR"
    cp "$WORK_DIR/linux/arch/x86/boot/bzImage" "$BOOT_FILES_DIR/"
    print_status "Kernel copied to $BOOT_FILES_DIR/bzImage"
}

configure_busybox() {
    print_step 7 "Configuring Busybox"
    
    cd "$WORK_DIR/busybox"
    
    if [ -f .config ]; then
        print_warning "Using existing busybox configuration"
        return
    fi
    
    print_info "Creating default configuration..."
    make defconfig > /dev/null 2>&1
    
    print_info "Enabling static build..."
    sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
    
    make oldconfig > /dev/null 2>&1
    
    print_status "Busybox configured successfully"
}

build_busybox() {
    print_step 8 "Building Busybox"
    
    cd "$WORK_DIR/busybox"
    
    print_info "Building busybox with $BUSYBOX_JOBS parallel jobs..."
    echo ""
    
    local start_time=$(date +%s)
    
    if make -j "$BUSYBOX_JOBS" 2>&1 | tee /tmp/busybox-build.log; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        
        echo ""
        print_status "Busybox built in ${duration}s"
        
        if [ -f busybox ]; then
            local size=$(du -h busybox | cut -f1)
            print_info "Busybox size: $size"
        fi
    else
        print_error "Busybox build failed! Check /tmp/busybox-build.log"
        exit 1
    fi
}

create_initramfs() {
    print_step 9 "Creating Initial RAM Filesystem"
    
    print_info "Installing busybox..."
    mkdir -p "$BOOT_FILES_DIR/initramfs"
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

mkdir -p /dev/pts /dev/shm /tmp /run
mount -t devpts devpts /dev/pts
mount -t tmpfs tmpfs /dev/shm
mount -t tmpfs tmpfs /tmp
mount -t tmpfs tmpfs /run

echo "custom-linux" > /proc/sys/kernel/hostname

clear
cat << 'EOF'

╔═══════════════════════════════════════════════════════════════╗
║              Welcome to Custom Linux System                  ║
╚═══════════════════════════════════════════════════════════════╝

EOF

echo "Kernel: $(uname -r)"
echo "Memory: $(free -h | grep Mem | awk '{print $2}')"
echo ""

exec /bin/sh
INITSCRIPT
    
    chmod +x init
    
    print_info "Creating initramfs archive..."
    find . | cpio -o -H newc 2>/dev/null > ../init.cpio
    
    print_status "Initramfs created successfully"
}

create_bootable_image() {
    print_step 10 "Creating Bootable Disk Image"
    
    cd "$BOOT_FILES_DIR"
    
    print_info "Creating ${BOOT_SIZE_MB}MB disk image..."
    dd if=/dev/zero of=boot bs=1M count="$BOOT_SIZE_MB" 2>&1 | grep -E "(copied|MB)"
    
    print_info "Formatting filesystem..."
    mkfs.fat boot
    
    print_info "Installing bootloader..."
    syslinux boot
    
    print_info "Mounting and copying files..."
    mkdir -p m
    mount boot m
    cp bzImage init.cpio m/
    
    cat > m/syslinux.cfg << 'SYSLINUXCFG'
DEFAULT linux
PROMPT 0
TIMEOUT 10

LABEL linux
    KERNEL bzImage
    APPEND initrd=init.cpio quiet
SYSLINUXCFG
    
    umount m
    rmdir m
    
    print_status "Boot image created successfully"
}

generate_scripts() {
    print_step 11 "Generating Helper Scripts"
    
    cd "$BOOT_FILES_DIR"
    
    cat > test-qemu.sh << 'TESTSCRIPT'
#!/bin/bash
if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "ERROR: QEMU not installed"
    echo "Install: sudo apt-get install qemu-system-x86"
    exit 1
fi
echo "Starting QEMU test..."
qemu-system-x86_64 -kernel /boot-files/bzImage -initrd /boot-files/init.cpio -m 512M
TESTSCRIPT
    
    chmod +x test-qemu.sh
    print_status "Helper scripts created"
}

show_summary() {
    echo ""
    echo -e "${GREEN}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║              🎉 BUILD COMPLETED SUCCESSFULLY! 🎉             ║
╚═══════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
    
    echo -e "${CYAN}Output Files:${NC}"
    echo ""
    if [ -f "$BOOT_FILES_DIR/bzImage" ]; then
        echo "  🐧 Kernel:    $(du -h "$BOOT_FILES_DIR/bzImage" | cut -f1) - $BOOT_FILES_DIR/bzImage"
    fi
    if [ -f "$BOOT_FILES_DIR/init.cpio" ]; then
        echo "  📦 Initramfs: $(du -h "$BOOT_FILES_DIR/init.cpio" | cut -f1) - $BOOT_FILES_DIR/init.cpio"
    fi
    if [ -f "$BOOT_FILES_DIR/boot" ]; then
        echo "  💾 Boot Image: $(du -h "$BOOT_FILES_DIR/boot" | cut -f1) - $BOOT_FILES_DIR/boot"
    fi
    
    echo ""
    echo -e "${CYAN}Test Your Build:${NC}"
    echo "  cd $BOOT_FILES_DIR && bash test-qemu.sh"
    echo ""
}

show_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Options:
    --auto              Fully automatic mode
    --skip-deps         Skip dependency installation
    --kernel-jobs N     Parallel jobs (default: $(nproc))
    --boot-size N       Boot size in MB (default: 50)
    --work-dir DIR      Working directory
    --help              Show help

EOF
}

main() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --auto) AUTO_MODE=true; shift ;;
            --skip-deps) SKIP_DEPS=true; shift ;;
            --kernel-jobs) KERNEL_JOBS="$2"; shift 2 ;;
            --boot-size) BOOT_SIZE_MB="$2"; shift 2 ;;
            --work-dir) WORK_DIR="$2"; shift 2 ;;
            --help) show_usage; exit 0 ;;
            *) echo "Unknown: $1"; show_usage; exit 1 ;;
        esac
    done
    
    print_banner
    
    print_info "Configuration:"
    echo "  Working Directory: $WORK_DIR"
    echo "  Kernel Jobs: $KERNEL_JOBS"
    echo "  Boot Size: ${BOOT_SIZE_MB}MB"
    echo ""
    
    if [ "$AUTO_MODE" = false ]; then
        read -p "Press Enter to start or Ctrl+C to cancel..."
    fi
    
    local total_start=$(date +%s)
    
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
    
    local total_end=$(date +%s)
    local total_duration=$((total_end - total_start))
    local total_minutes=$((total_duration / 60))
    local total_seconds=$((total_duration % 60))
    
    print_info "Total time: ${total_minutes}m ${total_seconds}s"
    show_summary
}

main "$@"

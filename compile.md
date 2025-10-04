sudo apt update

sudo apt install make gcc gcc-multilib binutils grub-common xorriso qemu-system-x86 build-essential nasm gcc binutils qemu-system-x86 xorriso grub-pc-bin mtools g++-multilib build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo


*enter OS-main*






mkdir ~/cross && cd ~/cross

wget https://ftp.gnu.org/gnu/binutils/binutils-2.43.tar.xz

wget https://ftp.gnu.org/gnu/gcc/gcc-14.2.0/gcc-14.2.0.tar.xz


tar xf binutils-2.43.tar.xz

tar xf gcc-14.2.0.tar.xz

mkdir build-binutils && cd build-binutils

../binutils-2.43/configure --target=i686-elf --prefix=$HOME/cross --with-sysroot --disable-nls --disable-werror

make -j$(nproc)

make install




cd ..

mkdir build-gcc && cd build-gcc

../gcc-14.2.0/configure --target=i686-elf --prefix=$HOME/cross --disable-nls --enable-languages=c,c++ --without-headers

make all-gcc -j$(nproc)

make install-gcc



Afterwards, add this line to your shell’s startup file (~/.bashrc):

export PATH="$HOME/cross/bin:$PATH"

Then reload:

source ~/.bashrc


Now check:

i686-elf-gcc --version


Now compile:

make clean

make libtcc.a CONFIG_BCHECK=no CC="i686-elf-gcc -ffreestanding -fno-builtin -fno-stack-protector -nostdlib"

sudo make

use VMware with SATA drive, port 0

<img src="https://r2cdn.perplexity.ai/pplx-full-logo-primary-dark%402x.png" style="height:64px;margin-right:32px"/>

## Complete Guide: Compile Static BusyBox from https://github.com/mirror/busybox

### Prerequisites (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential libncurses-dev bison flex git bc libssl-dev
```


### Step-by-Step Compilation

```bash
# 1. Clone the mirror
git clone https://github.com/mirror/busybox.git
cd busybox

# 2. Generate default config for x86_64
make x86_64_defconfig

# 3. Enable static linking (CRITICAL)
make menuconfig
```

**menuconfig navigation:**

```
Settings  ──> Build Options
[*] Build static binary (no shared libs)  ← Enable this
Coreutils ──> [*] sh (ash shell)
          ──> [*] ls, cat, echo, ps, mount, mkdir, rm, cp (basic set)
Networking ──> Disable (your kernel lacks networking)
```

**Save \& Exit**

**Or force static via config edit:**

```bash
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
```


### 4. Build (5-10 minutes)

```bash
# Clean previous builds
make clean

# Compile static binary
make -j$(nproc) LDFLAGS="--static"
```

Produces `./busybox` (~1.5MB static ELF).

### 5. Verify Static Linking

```bash
file busybox
# Output: ELF 64-bit LSB executable, x86-64, statically linked

ldd busybox
# Output: not a dynamic executable
```


### 6. Copy to Your FAT32 Disk

```bash
# Strip for smaller size
strip busybox

# Copy to your kernel's disk image
sudo mkdir -p mnt/bin
sudo mount -o loop disk.img mnt
sudo cp busybox mnt/bin/
sudo ln -s busybox mnt/bin/sh mnt/bin/ls mnt/bin/cat mnt/bin/ps mnt/bin/mount
sudo umount mnt
```


### 7. Test Commands Available

```bash
./busybox --list  # Shows all applets: sh, ls, cat, ps, etc.
./busybox sh      # Interactive shell
```


### Troubleshooting Common Errors

**`cannot find -lcrypt -lm`:**

```bash
# Install dev libs or add to .config:
echo 'CONFIG_EXTRA_LDLIBS="crypt m resolv rt"' >> .config
make clean && make -j$(nproc)
```

**Cross-compile for 32-bit (if your kernel is i386):**

```bash
make i386_defconfig
export CFLAGS="-m32"
export LDFLAGS="-m32 --static"
make -j$(nproc)
sudo apt install gcc-multilib  # Required
```

**Minimal config for your kernel:**

```
make defconfig
make menuconfig
# Disable: Networking, SELinux, IPv6, NFS, USB, etc.
# Enable: Coreutils (sh, ls, cat, echo), Editutils (vi?), Procps (ps)
```


### Integration with Your Kernel

Once compiled, your kernel's `fat32listfiles` shows `/bin/busybox`. Type `sh` in terminal → loads ELF via `cmdrun("/bin/busybox")` → BusyBox shell runs in `TerminalWindow`.

**Result:** `busybox sh` gives full POSIX shell (`ls`, `cat /proc/cpuinfo`, `ps`, `mount`, etc.) printing to your `console_print` via syscall stubs.[^1][^2][^3]

Recompile kernel with ELF loader (previous response) and boot—your OS now has a complete Unix shell![^4]
<span style="display:none">[^10][^11][^12][^13][^14][^15][^16][^5][^6][^7][^8][^9]</span>

<div align="center">⁂</div>

[^1]: paste.txt

[^2]: https://haoict.github.io/operating-systems/linux/make-your-own-linux-distro-from-scratch/3-busybox/

[^3]: https://github.com/ukanth/afwall/wiki/HOWTO-Compiling-busybox

[^4]: https://re-ws.pl/2020/11/busybox-based-linux-distro-from-scratch/

[^5]: paste.txt

[^6]: https://xdaforums.com/t/guide-bin-compile-busybox-on-linux.2857650/

[^7]: https://community.nxp.com/t5/-/-/m-p/508058

[^8]: https://gist.github.com/bluedragon1221/a58b0e1ed4492b44aa530f4db0ffef85

[^9]: https://github.com/EXALAB/Busybox-static

[^10]: https://github.com/mirror/busybox/blob/master/Config.in

[^11]: https://subscription.packtpub.com/book/iot-and-hardware/9781783289851/1/ch01lvl1sec09/compiling-busybox-simple

[^12]: https://dockermirror.com/en/detail/image/library%2Fbusybox

[^13]: https://busybox.net/FAQ.html

[^14]: https://stackoverflow.com/questions/77002233/how-to-build-busybox-with-static-binary

[^15]: https://risc-v-machines.readthedocs.io/en/latest/linux/simple/

[^16]: https://github.com/mranv/minimalOS


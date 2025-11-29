# TCC Bare Metal Makefile (Fixed Headers & Syntax)

CC      = gcc
LD      = ld
QEMU    = qemu-system-i386

# Flags for 32-bit Bare Metal
CFLAGS  = -m32 -fno-stack-protector -fno-builtin -nostdinc -fno-pie
LDFLAGS = -m elf_i386 -T linker.ld

# TCC Compilation Flags
# -w : Suppress warnings from our fake headers
# -I... : Force it to look in our fake_inc first
TCC_FLAGS = $(CFLAGS) \
	-DTCC_TARGET_I386 -DCONFIG_TCC_STATIC -DONE_SOURCE \
	-D_FORTIFY_SOURCE=0 -w \
	-I tcc-0.9.27/fake_inc \
	-I tcc-0.9.27 \
	-I tcc-0.9.27/include

TCC_SRC = tcc-0.9.27/libtcc.c
LIBTCC  = libtcc_i386.a

all: kernel.bin

# 1. Download TCC
tcc-0.9.27:
	@echo "[DOWNLOAD] Fetching TCC..."
	wget -c https://download-mirror.savannah.gnu.org/releases/tinycc/tcc-0.9.27.tar.bz2
	tar xf tcc-0.9.27.tar.bz2
	@echo "[CONFIG] Creating dummy config.h..."
	@echo '#define TCC_VERSION "0.9.27"' > tcc-0.9.27/config.h

# 2. Create Fake Headers (The Fix)
# 2. Create Fake Headers (Complete & Fixed)
headers: tcc-0.9.27
	@echo "[HEADERS] Creating fake libc headers..."
	@mkdir -p tcc-0.9.27/fake_inc/sys

	# stdint.h
	@echo '#ifndef _STDINT_H' > tcc-0.9.27/fake_inc/stdint.h
	@echo '#define _STDINT_H' >> tcc-0.9.27/fake_inc/stdint.h
	@echo 'typedef signed char int8_t; typedef short int16_t; typedef int int32_t; typedef long long int64_t;' >> tcc-0.9.27/fake_inc/stdint.h
	@echo 'typedef unsigned char uint8_t; typedef unsigned short uint16_t; typedef unsigned int uint32_t; typedef unsigned long long uint64_t;' >> tcc-0.9.27/fake_inc/stdint.h
	@echo 'typedef int intptr_t; typedef unsigned int uintptr_t;' >> tcc-0.9.27/fake_inc/stdint.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/stdint.h
	@cp tcc-0.9.27/fake_inc/stdint.h tcc-0.9.27/fake_inc/inttypes.h

	# stdlib.h
	@echo '#ifndef _STDLIB_H' > tcc-0.9.27/fake_inc/stdlib.h
	@echo '#define _STDLIB_H' >> tcc-0.9.27/fake_inc/stdlib.h
	@echo '#define NULL ((void*)0)' >> tcc-0.9.27/fake_inc/stdlib.h
	@echo 'typedef unsigned int size_t;' >> tcc-0.9.27/fake_inc/stdlib.h
	@echo 'void* malloc(size_t); void free(void*); void* realloc(void*, size_t); void* calloc(size_t, size_t);' >> tcc-0.9.27/fake_inc/stdlib.h
	@echo 'void exit(int); double strtod(const char*, char**); unsigned long strtoul(const char*, char**, int);' >> tcc-0.9.27/fake_inc/stdlib.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/stdlib.h

	# stdio.h
	@echo '#ifndef _STDIO_H' > tcc-0.9.27/fake_inc/stdio.h
	@echo '#define _STDIO_H' >> tcc-0.9.27/fake_inc/stdio.h
	@echo '#define SEEK_SET 0' >> tcc-0.9.27/fake_inc/stdio.h
	@echo '#define SEEK_CUR 1' >> tcc-0.9.27/fake_inc/stdio.h
	@echo '#define SEEK_END 2' >> tcc-0.9.27/fake_inc/stdio.h
	@echo '#define NULL ((void*)0)' >> tcc-0.9.27/fake_inc/stdio.h
	@echo 'typedef void FILE; extern FILE *stdout, *stderr;' >> tcc-0.9.27/fake_inc/stdio.h
	@echo 'int printf(const char*, ...); int sprintf(char*, const char*, ...); int snprintf(char*, int, const char*, ...);' >> tcc-0.9.27/fake_inc/stdio.h
	@echo 'int fprintf(FILE*, const char*, ...); int fputc(int, FILE*); int fputs(const char*, FILE*);' >> tcc-0.9.27/fake_inc/stdio.h
	@echo 'int fflush(FILE*); int fclose(FILE*); FILE* fopen(const char*, const char*); int fwrite(const void*, int, int, FILE*);' >> tcc-0.9.27/fake_inc/stdio.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/stdio.h

	# string.h
	@echo '#ifndef _STRING_H' > tcc-0.9.27/fake_inc/string.h
	@echo '#define _STRING_H' >> tcc-0.9.27/fake_inc/string.h
	@echo '#define NULL ((void*)0)' >> tcc-0.9.27/fake_inc/string.h
	@echo 'typedef unsigned int size_t;' >> tcc-0.9.27/fake_inc/string.h
	@echo 'void* memset(void*, int, size_t); int strlen(const char*); char* strcpy(char*, const char*);' >> tcc-0.9.27/fake_inc/string.h
	@echo 'char* strcat(char*, const char*); char* strchr(const char*, int); char* strrchr(const char*, int);' >> tcc-0.9.27/fake_inc/string.h
	@echo 'int strcmp(const char*, const char*); int strncmp(const char*, const char*, size_t);' >> tcc-0.9.27/fake_inc/string.h
	@echo 'void* memcpy(void*, const void*, size_t); void* memmove(void*, const void*, size_t); char* strdup(const char*); char* strerror(int);' >> tcc-0.9.27/fake_inc/string.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/string.h

	# stdarg.h
	@echo '#ifndef _STDARG_H' > tcc-0.9.27/fake_inc/stdarg.h
	@echo '#define _STDARG_H' >> tcc-0.9.27/fake_inc/stdarg.h
	@echo 'typedef __builtin_va_list va_list;' >> tcc-0.9.27/fake_inc/stdarg.h
	@echo '#define va_start(v,l) __builtin_va_start(v,l)' >> tcc-0.9.27/fake_inc/stdarg.h
	@echo '#define va_end(v) __builtin_va_end(v)' >> tcc-0.9.27/fake_inc/stdarg.h
	@echo '#define va_arg(v,l) __builtin_va_arg(v,l)' >> tcc-0.9.27/fake_inc/stdarg.h
	@echo '#define va_copy(d,s) __builtin_va_copy(d,s)' >> tcc-0.9.27/fake_inc/stdarg.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/stdarg.h

	# setjmp.h (CRITICAL - WAS MISSING GUARDS)
	@echo '#ifndef _SETJMP_H' > tcc-0.9.27/fake_inc/setjmp.h
	@echo '#define _SETJMP_H' >> tcc-0.9.27/fake_inc/setjmp.h
	@echo 'typedef int jmp_buf[16];' >> tcc-0.9.27/fake_inc/setjmp.h
	@echo 'int setjmp(jmp_buf);' >> tcc-0.9.27/fake_inc/setjmp.h
	@echo 'void longjmp(jmp_buf, int);' >> tcc-0.9.27/fake_inc/setjmp.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/setjmp.h

	# errno.h
	@echo '#ifndef _ERRNO_H' > tcc-0.9.27/fake_inc/errno.h
	@echo '#define _ERRNO_H' >> tcc-0.9.27/fake_inc/errno.h
	@echo 'extern int errno;' >> tcc-0.9.27/fake_inc/errno.h
	@echo '#define ERANGE 34' >> tcc-0.9.27/fake_inc/errno.h
	@echo '#define ENOENT 2' >> tcc-0.9.27/fake_inc/errno.h
	@echo '#define EACCES 13' >> tcc-0.9.27/fake_inc/errno.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/errno.h

	# time.h
	@echo '#ifndef _TIME_H' > tcc-0.9.27/fake_inc/time.h
	@echo '#define _TIME_H' >> tcc-0.9.27/fake_inc/time.h
	@echo 'typedef long time_t;' >> tcc-0.9.27/fake_inc/time.h
	@echo 'struct tm { int tm_sec; int tm_min; int tm_hour; int tm_mday; int tm_mon; int tm_year; };' >> tcc-0.9.27/fake_inc/time.h
	@echo 'time_t time(time_t *t); struct tm *localtime(const time_t *timep);' >> tcc-0.9.27/fake_inc/time.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/time.h

	# math.h
	@echo '#ifndef _MATH_H' > tcc-0.9.27/fake_inc/math.h
	@echo '#define _MATH_H' >> tcc-0.9.27/fake_inc/math.h
	@echo '#define HUGE_VAL 1e999' >> tcc-0.9.27/fake_inc/math.h
	@echo 'double ldexp(double x, int exp);' >> tcc-0.9.27/fake_inc/math.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/math.h

	# fcntl.h
	@echo '#ifndef _FCNTL_H' > tcc-0.9.27/fake_inc/fcntl.h
	@echo '#define _FCNTL_H' >> tcc-0.9.27/fake_inc/fcntl.h
	@echo '#define O_RDONLY 0' >> tcc-0.9.27/fake_inc/fcntl.h
	@echo '#define O_WRONLY 1' >> tcc-0.9.27/fake_inc/fcntl.h
	@echo '#define O_CREAT 64' >> tcc-0.9.27/fake_inc/fcntl.h
	@echo '#define O_TRUNC 512' >> tcc-0.9.27/fake_inc/fcntl.h
	@echo '#define O_BINARY 0' >> tcc-0.9.27/fake_inc/fcntl.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/fcntl.h

	# unistd.h
	@echo '#ifndef _UNISTD_H' > tcc-0.9.27/fake_inc/unistd.h
	@echo '#define _UNISTD_H' >> tcc-0.9.27/fake_inc/unistd.h
	@echo 'typedef int ssize_t;' >> tcc-0.9.27/fake_inc/unistd.h
	@echo 'int open(const char*, int, ...); int close(int); int read(int, void*, int); int lseek(int, int, int);' >> tcc-0.9.27/fake_inc/unistd.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/unistd.h

	# sys headers
	@echo 'struct timeval{long tv_sec;};' > tcc-0.9.27/fake_inc/sys/time.h
	@echo 'struct stat{int st_mode;};' > tcc-0.9.27/fake_inc/sys/stat.h
	@echo 'typedef unsigned int size_t; void* mmap(void*,size_t,int,int,int,int);' > tcc-0.9.27/fake_inc/sys/mman.h
	
	# signal.h
	@echo '#ifndef _SIGNAL_H' > tcc-0.9.27/fake_inc/signal.h
	@echo '#define _SIGNAL_H' >> tcc-0.9.27/fake_inc/signal.h
	@echo 'typedef void (*sighandler_t)(int);' >> tcc-0.9.27/fake_inc/signal.h
	@echo 'typedef struct { int si_signo; int si_code; } siginfo_t;' >> tcc-0.9.27/fake_inc/signal.h
	@echo 'typedef struct { unsigned long __val[32]; } sigset_t;' >> tcc-0.9.27/fake_inc/signal.h
	@echo 'struct sigaction { void (*sa_sigaction)(int, siginfo_t *, void *); sigset_t sa_mask; int sa_flags; };' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define SIGINT 2' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define SIGSEGV 11' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define SIGILL 4' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define SIGFPE 8' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define SIGBUS 7' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define SIGABRT 6' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define SIG_DFL ((sighandler_t)0)' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define SIG_IGN ((sighandler_t)1)' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define SA_SIGINFO 4' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define SA_RESETHAND 0x80000000' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define FPE_INTDIV 1' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#define FPE_FLTDIV 3' >> tcc-0.9.27/fake_inc/signal.h
	@echo 'sighandler_t signal(int signum, sighandler_t handler);' >> tcc-0.9.27/fake_inc/signal.h
	@echo 'int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);' >> tcc-0.9.27/fake_inc/signal.h
	@echo 'int sigemptyset(sigset_t *set);' >> tcc-0.9.27/fake_inc/signal.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/signal.h


	# sys headers
	@echo 'struct timeval{long tv_sec;};' > tcc-0.9.27/fake_inc/sys/time.h
	@echo 'struct stat{int st_mode;};' > tcc-0.9.27/fake_inc/sys/stat.h
	@echo 'typedef unsigned int size_t; void* mmap(void*,size_t,int,int,int,int);' > tcc-0.9.27/fake_inc/sys/mman.h
	
	# ucontext
	@echo '#ifndef _SYS_UCONTEXT_H' > tcc-0.9.27/fake_inc/sys/ucontext.h
	@echo '#define _SYS_UCONTEXT_H' >> tcc-0.9.27/fake_inc/sys/ucontext.h
	@echo '#define EIP 14' >> tcc-0.9.27/fake_inc/sys/ucontext.h
	@echo '#define EBP 6' >> tcc-0.9.27/fake_inc/sys/ucontext.h
	@echo 'typedef struct { int gregs[23]; } mcontext_t;' >> tcc-0.9.27/fake_inc/sys/ucontext.h
	@echo 'typedef struct { mcontext_t uc_mcontext; } ucontext_t;' >> tcc-0.9.27/fake_inc/sys/ucontext.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/sys/ucontext.h
	
	#mman
	@echo '#ifndef _SYS_MMAN_H' > tcc-0.9.27/fake_inc/sys/mman.h
	@echo '#define _SYS_MMAN_H' >> tcc-0.9.27/fake_inc/sys/mman.h
	@echo 'typedef unsigned int size_t;' >> tcc-0.9.27/fake_inc/sys/mman.h
	@echo '#define PROT_READ 1' >> tcc-0.9.27/fake_inc/sys/mman.h
	@echo '#define PROT_WRITE 2' >> tcc-0.9.27/fake_inc/sys/mman.h
	@echo '#define PROT_EXEC 4' >> tcc-0.9.27/fake_inc/sys/mman.h
	@echo '#define MAP_PRIVATE 2' >> tcc-0.9.27/fake_inc/sys/mman.h
	@echo '#define MAP_ANONYMOUS 0x20' >> tcc-0.9.27/fake_inc/sys/mman.h
	@echo 'void* mmap(void*,size_t,int,int,int,int);' >> tcc-0.9.27/fake_inc/sys/mman.h
	@echo 'int mprotect(void *addr, size_t len, int prot);' >> tcc-0.9.27/fake_inc/sys/mman.h
	@echo '#endif' >> tcc-0.9.27/fake_inc/sys/mman.h



	# Empty stubs
	@touch tcc-0.9.27/fake_inc/assert.h tcc-0.9.27/fake_inc/ctype.h tcc-0.9.27/fake_inc/limits.h

# 3. Compile Libtcc
libtcc.o: headers
	@echo "[COMPILE] Building libtcc.o..."
	$(CC) $(TCC_FLAGS) -c $(TCC_SRC) -o libtcc.o

# 4. Archive
$(LIBTCC): libtcc.o
	@echo "[ARCHIVE] Creating $(LIBTCC)..."
	ar rcs $(LIBTCC) libtcc.o

# 5. Build Kernel
kernel.o: kernel.c headers
	@echo "[COMPILE] Building kernel.o..."
	$(CC) $(CFLAGS) -I tcc-0.9.27/fake_inc -I tcc-0.9.27/include -c kernel.c -o kernel.o

# 6. Link

kernel.bin: kernel.o $(LIBTCC)
	@echo "[LINK] Linking kernel.bin..."
	$(LD) $(LDFLAGS) kernel.o $(LIBTCC) -o kernel.bin
	@echo "✅ Success! Run 'make run'"
	
	
# 7. Create Bootable ISO
iso: kernel.bin
	@echo "[ISO] Creating bootable ISO image..."
	@mkdir -p isodir/boot/grub
	@cp kernel.bin isodir/boot/kernel.bin
	@echo 'menuentry "TCC Bare Metal OS" {' > isodir/boot/grub/grub.cfg
	@echo '    multiboot /boot/kernel.bin' >> isodir/boot/grub/grub.cfg
	@echo '    boot' >> isodir/boot/grub/grub.cfg
	@echo '}' >> isodir/boot/grub/grub.cfg
	@grub-mkrescue -o os.iso isodir 2>/dev/null || echo "⚠️  grub-mkrescue not found. Install grub-pc-bin or xorriso."
	@echo "✅ ISO created: os.iso"
run: kernel.bin
	$(QEMU) -kernel kernel.bin -m 64 -no-reboot

clean:
	rm -f *.o *.bin *.a
	rm -rf tcc-0.9.27 tcc-0.9.27.tar.bz2

.PHONY: all run clean headers

CXX = i686-elf-g++
CC  = i686-elf-gcc
CFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -I./tcc

OBJS = kernel.o tcc_wrapper.o crt_stubs.o
LIBS = ./tcc/libtcc.a

kernel.elf: $(OBJS)
	$(CXX) -T linker.ld -o $@ $(OBJS) $(LIBS) -nostdlib

tcc_wrapper.o: tcc_wrapper.c
	$(CC) $(CFLAGS) -c tcc_wrapper.c -o tcc_wrapper.o

crt_stubs.o: crt_stubs.c
	$(CC) $(CFLAGS) -c crt_stubs.c -o crt_stubs.o
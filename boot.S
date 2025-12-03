# boot.S - Multiboot 2 Entry Point

# Constants for Multiboot 2
.set MB2_MAGIC, 0xE85250D6
.set MB2_ARCH, 0            # 0 = i386 protected mode
.set MB2_LEN, (header_end - header_start)
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH + MB2_LEN)

.section .multiboot
.align 8
header_start:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_LEN
    .long MB2_CHECKSUM

    # Tag: Information Request (Explicitly ask for Framebuffer info)
    .align 8
    .short 1      # Type: Information Request
    .short 0      # Flags
    .long  12     # Size
    .long  8      # Request: Framebuffer Tag
    
    # Tag: Framebuffer Request
    .align 8
    .short 5      # Type: Framebuffer
    .short 1      # Flags
    .long  20     # Size
    .long  1024   # Width
    .long  768    # Height
    .long  32     # Depth

    # Tag: End
    .align 8
    .short 0
    .short 0
    .long  8
header_end:

# Kernel Entry Point
.section .bootstrap_stack, "aw", @nobits
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    cli
    movl $stack_top, %esp

    # Multiboot 2 passes:
    # EAX = Magic (0x36d76289)
    # EBX = Address of Multiboot Information Structure
    
    # Push arguments for kernel_main(magic, addr)
    pushl %ebx
    pushl %eax
    
    call kernel_main

.Lhang:
    hlt
    jmp .Lhang

.size _start, . - _start

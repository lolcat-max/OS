/* boot.S - The Entry Point */
/* Constants for the Multiboot Header */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set FLAGS,    ALIGN | MEMINFO  /* this is the Multiboot 'flag' field */
.set MAGIC,    0x1BADB002       /* 'magic' number lets bootloader find the header */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum of above, to prove we are multiboot */

/* The Multiboot Header */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* Allocate a Stack (16KB) */
.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

/* The Entry Point */
.section .text
.global _start
.type _start, @function
_start:
    /* 1. Set up the stack pointer (Critical!) */
    mov $stack_top, %esp

    /* 2. Call the C kernel */
    call kernel_main

    /* 3. Halt if kernel_main returns */
    cli
1:  hlt
    jmp 1b

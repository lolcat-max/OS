/* linker.ld - simple script for i386 multiboot kernel */
OUTPUT_FORMAT(elf32-i386)
ENTRY(kernel_main)

SECTIONS
{
    /* Put multiboot header at very start (must be within first 8KiB) */
    . = 0x00100000; /* virtual load address - adjust if you want 1MB */

    .multiboot ALIGN(4) : {
        KEEP(*(.multiboot))
    }

    .text ALIGN(4) : {
        *(.text.entry)
        *(.text.start)
        *(.text*)
        *(.init)
    }

    .rodata ALIGN(4): { *(.rodata*) }

    .data ALIGN(4) : {
        *(.data*)
    }

    .bss ALIGN(4) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : { *(.comment) *(.note.GNU-stack) }
}

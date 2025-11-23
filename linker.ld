ENTRY(kernel_main)

SECTIONS {
    /* The kernel starts at 1MB */
    . = 1M;

    /* Force Multiboot header to be the VERY first thing */
    .text : {
        /* Ensure 4-byte alignment for the header */
        . = ALIGN(4);
        KEEP(*(.multiboot))
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}

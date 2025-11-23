/* kernel.c */
#include <stdint.h>

/* Video memory address */
volatile uint16_t* vga_buffer = (volatile uint16_t*)0xB8000;

/* Entry point called from boot.S */
void kernel_main(void) {
    const char* str = "Success! Kernel booted.";
    
    /* Clear screen (White on Blue) */
    for(int i = 0; i < 80*25; i++) {
        vga_buffer[i] = 0x1F20; 
    }

    /* Print Message */
    int i = 0;
    while(str[i]) {
        vga_buffer[i] = (uint16_t)str[i] | 0x1F00;
        i++;
    }
	/* STOP THE CPU HERE */
    while(1) {
        __asm__ volatile("cli");  // Disable interrupts
        __asm__ volatile("hlt");  // Halt the CPU (saves power)
    }
}

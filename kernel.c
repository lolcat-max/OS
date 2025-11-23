/* kernel.c - Pure C Kernel for TCC */
#include <stdarg.h>
#include <stdint.h>

/* Multiboot Header */
#define MULTIBOOT_MAGIC 0x1BADB002
#define MULTIBOOT_FLAGS 0x00000003
#define MULTIBOOT_CHECKSUM -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

/* Force header into correct section */
__attribute__((section(".multiboot")))
struct multiboot_header {
    uint32_t magic;
    uint32_t flags;
    uint32_t checksum;
} mb_header = { MULTIBOOT_MAGIC, MULTIBOOT_FLAGS, MULTIBOOT_CHECKSUM };

#define VGA_MEM ((volatile uint16_t*)0xB8000)

void kernel_main(void* mb_info, uint32_t magic) {
    const char* str = "Hello from TCC Bare Metal!";
    volatile uint16_t* vga = VGA_MEM;
    
    /* Clear screen (optional) */
    for(int i=0; i<80*25; i++) vga[i] = 0x0F20;

    /* Print string */
    int i = 0;
    while(str[i]) {
        vga[i] = (uint16_t)str[i] | 0x0F00; // White text
        i++;
    }

    while(1) __asm__ volatile("hlt");
}

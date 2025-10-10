#include <cstdarg>

// --- Type Definitions & Low-Level Functions ---
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

// Dummy print function. In a real kernel, this would write to the screen.
void kprint(const char*);

// =============================================================================
// SECTION 1: SMP (SYMMETRIC MULTI-PROCESSING) CORE LOGIC
// =============================================================================

// Base address of the Local APIC, memory-mapped for direct access.
#define LAPIC_BASE 0xFEE00000

// Key LAPIC register offsets from the base address.
#define LAPIC_ID_REG      0x0020  // Stores the unique ID of the core's LAPIC.
#define LAPIC_EOI_REG     0x00B0  // End Of Interrupt Register.
#define LAPIC_ICR_LOW     0x0300  // Interrupt Command Register [31:0].
#define LAPIC_ICR_HI      0x0310  // Interrupt Command Register [63:32].

// Volatile pointers to the LAPIC registers to prevent compiler optimizations.
volatile uint32_t* lapic_regs = (uint32_t*)LAPIC_BASE;

// A simple spinlock to ensure only one core prints at a time.
volatile int print_lock = 0;

void lock(volatile int* l) {
    // __sync_bool_compare_and_swap is an atomic instruction.
    // It tries to set the lock to 1 only if it's currently 0.
    while (!__sync_bool_compare_and_swap(l, 0, 1)) {
        asm volatile("pause"); // Hint to the CPU that we are in a spin-wait loop.
    }
}

void unlock(volatile int* l) {
    *l = 0;
}

// C++ entry point for the Application Processors (APs) after they are woken.
// This is the function the other cores will run.
extern "C" void ap_entry_cpp() {
    // Get this core's unique APIC ID. The ID is stored in bits 24-31.
    uint32_t my_id = lapic_regs[LAPIC_ID_REG / 4] >> 24;

    lock(&print_lock);
    kprint("Application Processor (AP) with APIC ID: ");
    // (In a real kernel, you would have an int-to-string function here)
    kprint("... has started!\n");
    unlock(&print_lock);

    // In a real OS, this core would now initialize its own timers
    // and enter the task scheduler. For this minimal example, we just halt.
    for (;;) {
        asm volatile("cli; hlt");
    }
}

// --- The 16-bit Trampoline Code (Assembly) ---
// This code is executed by APs when they first wake up in 16-bit real mode.
// Its job is to switch to 32-bit protected mode and jump to our C++ code.
uint8_t trampoline_code[] = {
    // --- 16-bit Stage: Set up segments and enter protected mode ---
    0xFA,                         // cli
    0x0F, 0x01, 0x16, 0x00, 0x90, // lgdt [0x9000] ; Load the GDT
    0x0F, 0x20, 0xC0,             // mov eax, cr0
    0x0C, 0x01,                   // or al, 1      ; Set protected mode enable bit
    0x0F, 0x22, 0xC0,             // mov cr0, eax
    0x66, 0xEA, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00 // ljmp $0x08, flush_cs
                                  // The offset will be patched at runtime.
    // --- 32-bit Stage (at flush_cs): Load data segments and jump to C++ ---
    // This part starts right after the long jump lands.
    // mov ax, 0x10 ; Load data segment selector
    // mov ds, ax
    // mov es, ax
    // mov ss, ax
    // jmp [ap_entry_cpp_ptr] ; Jump to the C++ entry point address
};
#define TRAMPOLINE_START_ADDR 0x8000
#define AP_GDT_ADDR           0x9000
#define AP_CPP_ENTRY_PTR_ADDR 0x900A

// Global Descriptor Table (GDT) for the APs.
uint64_t ap_gdt[] = {
    0,                      // Null Descriptor
    0x00CF9A000000FFFF,     // 32-bit Code Segment (Ring 0)
    0x00CF92000000FFFF,     // 32-bit Data Segment (Ring 0)
};
struct { uint16_t limit; uint32_t base; } __attribute__((packed)) ap_gdtr = { sizeof(ap_gdt) - 1, AP_GDT_ADDR };


// Function to send a command (IPI) to another core via its LAPIC.
void send_ipi(uint32_t lapic_id, uint32_t ipi_command) {
    // Write the destination LAPIC ID to the high bits of the ICR.
    lapic_regs[LAPIC_ICR_HI / 4] = (lapic_id << 24);
    // Write the command to the low bits of the ICR. This sends the IPI.
    lapic_regs[LAPIC_ICR_LOW / 4] = ipi_command;

    // Wait for the 'delivery status' bit to clear, confirming IPI was sent.
    while (lapic_regs[LAPIC_ICR_LOW / 4] & (1 << 12)) {}
}

// The main function to wake up all other cores.
void wake_all_aps() {
    // 1. Copy the trampoline code and GDT to a known low-memory address.
    memcpy((void*)TRAMPOLINE_START_ADDR, trampoline_code, sizeof(trampoline_code));
    memcpy((void*)AP_GDT_ADDR, &ap_gdtr, sizeof(ap_gdtr));
    memcpy((void*)(AP_GDT_ADDR + sizeof(ap_gdtr)), ap_gdt, sizeof(ap_gdt));

    // 2. Patch the trampoline with the address of our 32-bit C++ entry function.
    *(volatile uint32_t*)AP_CPP_ENTRY_PTR_ADDR = (uint32_t)ap_entry_cpp;

    // In a real OS, we would parse the ACPI MADT table to find all core APIC IDs.
    // For this minimal example, we'll assume there are 4 cores (IDs 0, 1, 2, 3).
    int core_count = 4;
    uint32_t bsp_id = lapic_regs[LAPIC_ID_REG / 4] >> 24;

    lock(&print_lock);
    kprint("BSP is waking other cores...\n");
    unlock(&print_lock);

    for (int i = 0; i < core_count; ++i) {
        if (i == bsp_id) continue; // Don't try to wake ourselves.

        // --- The INIT-SIPI-SIPI Sequence ---
        // Assert INIT IPI
        send_ipi(i, 0x00004500);
        // De-assert INIT IPI
        send_ipi(i, 0x00004000);

        // A small delay is needed here in a real system.

        // Send first Startup IPI (SIPI)
        // Command: 0x4600, Vector: TRAMPOLINE_START_ADDR / 4KB page
        send_ipi(i, 0x00004600 | (TRAMPOLINE_START_ADDR >> 12));

        // Another small delay.

        // Send second Startup IPI (SIPI)
        send_ipi(i, 0x00004600 | (TRAMPOLINE_START_ADDR >> 12));
    }
}

// =============================================================================
// SECTION 2: KERNEL MAIN (EXAMPLE USAGE)
// =============================================================================

// This would be your kernel's main entry point.
extern "C" void kernel_main() {
    // Initialize screen, etc.
    // kprint("Bootstrap Processor (BSP) started.\n");

    // After basic initialization, wake up the other cores.
    // wake_all_aps();

    // The BSP would then continue its own work or enter the scheduler.
    // kprint("BSP finished waking cores and continues execution.\n");

    // for (;;) {
    //     asm volatile("hlt");
    // }
}

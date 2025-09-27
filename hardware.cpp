//=============================================================================
// HARDWARE.CPP - Hardware Access and MMIO Operations Library
// Drop-in library for kernel hardware functions
//=============================================================================

// Hardware registry structures and globals
struct HardwareDevice {
    uint32_t vendorid;
    uint32_t deviceid;
    uint64_t baseaddress;
    uint64_t size;
    uint32_t devicetype;
    char description[64];
};

// Hardware registry - populated by discovery functions
static const int MAX_HARDWARE_DEVICES = 32;
static HardwareDevice hardware_registry[MAX_HARDWARE_DEVICES];
static int hardware_count = 0;

// Memory map data for self-hosted compiler
static const char** memory_map_data = nullptr;
static int memory_map_device_count = 0;

//=============================================================================
// PCI CONFIGURATION ACCESS
//=============================================================================

// PCI configuration space access
static uint32_t pci_config_read_dword(uint16_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)device << 11) | ((uint32_t)function << 8) | (offset & 0xFC);
    
    // Write address to CONFIG_ADDRESS (0xCF8) - Fixed constraint
    asm volatile ("outl %0, %w1" : : "a" (address), "Nd" (0xCF8) : "memory");
    
    // Read data from CONFIG_DATA (0xCFC) - Fixed constraint  
    uint32_t result;
    asm volatile ("inl %w1, %0" : "=a" (result) : "Nd" (0xCFC) : "memory");
    return result;
}

//=============================================================================
// HARDWARE DISCOVERY
//=============================================================================

void discover_pci_devices() {
    hardware_count = 0; // Reset count
    
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                uint32_t vendor_device = pci_config_read_dword(bus, device, function, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue; // No device
                if (hardware_count >= MAX_HARDWARE_DEVICES) return;

                HardwareDevice* dev = &hardware_registry[hardware_count];
                dev->vendorid = vendor_device & 0xFFFF;
                dev->deviceid = (vendor_device >> 16) & 0xFFFF;

                // Read BAR0 for base address
                uint32_t bar0 = pci_config_read_dword(bus, device, function, 0x10);
                if (bar0 & 0x1) { // I/O port
                    dev->baseaddress = bar0 & 0xFFFFFFFC;
                    dev->size = 0x100; // Assume 256 bytes for I/O ports
                } else { // Memory mapped
                    dev->baseaddress = bar0 & 0xFFFFFFF0;
                    if ((bar0 & 0x6) == 0x4) { // 64-bit BAR
                        uint32_t bar1 = pci_config_read_dword(bus, device, function, 0x14);
                        dev->baseaddress |= ((uint64_t)bar1 << 32);
                    }
                    dev->size = 0x1000; // Assume 4KB for memory mapped
                }

                // Determine device type based on class code
                uint32_t class_code = pci_config_read_dword(bus, device, function, 0x08);
                uint8_t base_class = (class_code >> 24) & 0xFF;
                switch (base_class) {
                    case 0x01: dev->devicetype = 1; strcpy(dev->description, "Storage Controller"); break;
                    case 0x02: dev->devicetype = 2; strcpy(dev->description, "Network Controller"); break;
                    case 0x03: dev->devicetype = 3; strcpy(dev->description, "Display Controller"); break;
                    case 0x04: dev->devicetype = 4; strcpy(dev->description, "Multimedia Controller"); break;
                    case 0x0C:
                        if (((class_code >> 16) & 0xFF) == 0x03) {
                            dev->devicetype = 5; strcpy(dev->description, "USB Controller");
                        } else {
                            dev->devicetype = 0; strcpy(dev->description, "Serial Bus Controller");
                        }
                        break;
                    default: dev->devicetype = 0; strcpy(dev->description, "Unknown Device"); break;
                }
                hardware_count++;

                if (function == 0 && !(pci_config_read_dword(bus, device, function, 0x0C) & 0x800000)) {
                    break; // Single function device
                }
            }
        }
    }
}

void discover_memory_regions() {
    // Add known memory regions
    if (hardware_count < MAX_HARDWARE_DEVICES) {
        HardwareDevice* dev = &hardware_registry[hardware_count];
        dev->vendorid = 0x0000;
        dev->deviceid = 0x0001;
        dev->baseaddress = 0xB8000; // VGA text mode buffer
        dev->size = 0x8000;
        dev->devicetype = 3;
        strcpy(dev->description, "VGA Text Buffer");
        hardware_count++;
    }

    if (hardware_count < MAX_HARDWARE_DEVICES) {
        HardwareDevice* dev = &hardware_registry[hardware_count];
        dev->vendorid = 0x0000;
        dev->deviceid = 0x0002;
        dev->baseaddress = 0xA0000; // VGA graphics buffer
        dev->size = 0x20000;
        dev->devicetype = 3;
        strcpy(dev->description, "VGA Graphics Buffer");
        hardware_count++;
    }
}

//=============================================================================
// MEMORY MAP DATA GENERATION
//=============================================================================

void populate_memory_map_data() {
    if (hardware_count == 0) {
        discover_pci_devices();
        discover_memory_regions();
    }
    
    memory_map_device_count = hardware_count;
    memory_map_data = (const char**)malloc(memory_map_device_count * sizeof(char*));
    
    if (!memory_map_data) {
        memory_map_device_count = 0;
        return;
    }
    
    for (int i = 0; i < hardware_count; ++i) {
        const HardwareDevice* dev = &hardware_registry[i];
        
        // Allocate 256 chars per device entry
        char* entry_str = (char*)malloc(256);
        if (entry_str) {
            uint64_t base_end = dev->baseaddress + dev->size - 1;
            snprintf(entry_str, 256,
                     "Device %d: %s Base 0x%llX - 0x%llX Size 0x%llX Vendor 0x%X Device 0x%X Type %u",
                     i,
                     dev->description,
                     dev->baseaddress,
                     base_end,
                     dev->size,
                     dev->vendorid,
                     dev->deviceid,
                     dev->devicetype);
            memory_map_data[i] = entry_str;
        } else {
            memory_map_data[i] = "Error: Memory allocation failed for device string.";
        }
    }
}

//=============================================================================
// MMIO SAFETY CHECKING
//=============================================================================

bool is_safe_mmio_address(uint64_t addr, uint64_t size) {
    // Check if address falls within any known device range
    for (int i = 0; i < hardware_count; i++) {
        const HardwareDevice* dev = &hardware_registry[i];
        if (addr >= dev->baseaddress && addr + size <= dev->baseaddress + dev->size) {
            return true;
        }
    }
    
    // Allow access to standard VGA and system areas even if not enumerated
    if (addr >= 0xA0000 && addr < 0x100000) return true; // VGA/BIOS area
    if (addr >= 0xB8000 && addr < 0xC0000) return true;  // VGA text buffer
    if (addr >= 0x3C0 && addr < 0x3E0) return true;      // VGA registers
    if (addr >= 0x60 && addr < 0x70) return true;        // Keyboard controller
    
    return false;
}

//=============================================================================
// MMIO READ/WRITE OPERATIONS
//=============================================================================

uint8_t mmio_read8(uint64_t addr) {
    if (!is_safe_mmio_address(addr, 1)) {
        cout << "MMIO: Unsafe read8 at 0x";
        char hex[17]; 
        uint64_to_hex_string(addr, hex); 
        cout << hex << "\n";
        return 0xFF;
    }
    return *(volatile uint8_t*)addr;
}

uint16_t mmio_read16(uint64_t addr) {
    if (!is_safe_mmio_address(addr, 2)) {
        cout << "MMIO: Unsafe read16 at 0x";
        char hex[17]; 
        uint64_to_hex_string(addr, hex); 
        cout << hex << "\n";
        return 0xFFFF;
    }
    return *(volatile uint16_t*)addr;
}

uint32_t mmio_read32(uint64_t addr) {
    if (!is_safe_mmio_address(addr, 4)) {
        cout << "MMIO: Unsafe read32 at 0x";
        char hex[17]; 
        uint64_to_hex_string(addr, hex); 
        cout << hex << "\n";
        return 0xFFFFFFFF;
    }
    return *(volatile uint32_t*)addr;
}

uint64_t mmio_read64(uint64_t addr) {
    if (!is_safe_mmio_address(addr, 8)) {
        cout << "MMIO: Unsafe read64 at 0x";
        char hex[17]; 
        uint64_to_hex_string(addr, hex); 
        cout << hex << "\n";
        return 0xFFFFFFFFFFFFFFFFULL;
    }
    return *(volatile uint64_t*)addr;
}

bool mmio_write8(uint64_t addr, uint8_t value) {
    if (!is_safe_mmio_address(addr, 1)) {
        cout << "MMIO: Unsafe write8 at 0x";
        char hex[17]; 
        uint64_to_hex_string(addr, hex); 
        cout << hex << "\n";
        return false;
    }
    *(volatile uint8_t*)addr = value;
    return true;
}

bool mmio_write16(uint64_t addr, uint16_t value) {
    if (!is_safe_mmio_address(addr, 2)) {
        cout << "MMIO: Unsafe write16 at 0x";
        char hex[17]; 
        uint64_to_hex_string(addr, hex); 
        cout << hex << "\n";
        return false;
    }
    *(volatile uint16_t*)addr = value;
    return true;
}

bool mmio_write32(uint64_t addr, uint32_t value) {
    if (!is_safe_mmio_address(addr, 4)) {
        cout << "MMIO: Unsafe write32 at 0x";
        char hex[17]; 
        uint64_to_hex_string(addr, hex); 
        cout << hex << "\n";
        return false;
    }
    *(volatile uint32_t*)addr = value;
    return true;
}

bool mmio_write64(uint64_t addr, uint64_t value) {
    if (!is_safe_mmio_address(addr, 8)) {
        cout << "MMIO: Unsafe write64 at 0x";
        char hex[17]; 
        uint64_to_hex_string(addr, hex); 
        cout << hex << "\n";
        return false;
    }
    *(volatile uint64_t*)addr = value;
    return true;
}

//=============================================================================
// TERMINAL FUNCTIONS
//=============================================================================

void terminal_clear() {
    terminal_clear_screen();  // Use existing kernel function
}

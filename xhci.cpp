#include "xhci.h"
#include "pci.h"
#include "terminal_io.h"
#include "iostream_wrapper.h"
#include "dma_memory.h"
#include "stdlib_hooks.h"
#include "terminal_hooks.h"

// --- Global Pointers to xHCI resources ---
volatile xhci_cap_regs_t* xhci_cap_regs;
volatile xhci_op_regs_t* xhci_op_regs;
volatile uint32_t* xhci_db_regs;
volatile xhci_port_regs_t* xhci_port_regs;
volatile uint64_t* xhci_runtime_regs;

// Memory structures for xHCI
uint64_t* dcbaa;
xhci_trb_t* cmd_ring;
xhci_trb_t* event_ring;
usb_device_context_t** device_contexts;

// USB keyboard state
bool usb_keyboard_active = false;
usb_keyboard_report_t* keyboard_buffer = nullptr;
usb_keyboard_report_t last_usb_report = {0};
uint8_t usb_keyboard_slot_id = 0;

// Caps Lock and modifier state
bool usb_caps_lock_on = false;
static bool usb_num_lock_on = false;
static bool usb_scroll_lock_on = false;

// Transfer rings for keyboard
static xhci_trb_t* keyboard_in_ring = nullptr;
static uint32_t keyboard_in_cycle = 1;
static uint32_t keyboard_in_enqueue = 0;

// Command and event ring management
static uint32_t cmd_ring_cycle = 1;
static uint32_t cmd_ring_enqueue = 0;
static uint32_t event_ring_cycle = 1;
static uint32_t event_ring_dequeue = 0;

// External DMA manager
extern DMAManager dma_manager;

// HID constants
#define HID_KEY_CAPS_LOCK 0x39
#define HID_KEY_NUM_LOCK  0x53  
#define HID_KEY_SCROLL_LOCK 0x47
#define HID_KEY_F5 0x3E

// Modifier key bits
#define USB_MOD_LEFT_CTRL   0x01
#define USB_MOD_LEFT_SHIFT  0x02
#define USB_MOD_LEFT_ALT    0x04
#define USB_MOD_LEFT_GUI    0x08
#define USB_MOD_RIGHT_CTRL  0x10
#define USB_MOD_RIGHT_SHIFT 0x20
#define USB_MOD_RIGHT_ALT   0x40
#define USB_MOD_RIGHT_GUI   0x80

// TRB Types
#define TRB_TYPE_NORMAL              1
#define TRB_TYPE_SETUP_STAGE         2
#define TRB_TYPE_DATA_STAGE          3
#define TRB_TYPE_STATUS_STAGE        4
#define TRB_TYPE_ISOCH               5
#define TRB_TYPE_LINK                6
#define TRB_TYPE_EVENT_DATA          7
#define TRB_TYPE_NO_OP               8
#define TRB_TYPE_ENABLE_SLOT         9
#define TRB_TYPE_DISABLE_SLOT       10
#define TRB_TYPE_ADDRESS_DEVICE     11
#define TRB_TYPE_CONFIGURE_ENDPOINT 12
#define TRB_TYPE_TRANSFER_EVENT     32
#define TRB_TYPE_COMMAND_COMPLETION 33
#define TRB_TYPE_PORT_STATUS_CHANGE 34

// Helper functions
static void* align_pointer(void* ptr, size_t alignment) {
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t offset = (alignment - (addr % alignment)) % alignment;
    return (void*)(addr + offset);
}

static void* simple_memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

static void* simple_memcpy(void* dst, const void* src, size_t n) {
    char* d = (char*)dst;
    const char* s = (const char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

// USB HID to ASCII conversion with Caps Lock support
char usb_hid_to_ascii(uint8_t hid_code, bool shift, bool caps_lock) {
    // Letters A-Z (HID codes 4-29)
    if (hid_code >= 4 && hid_code <= 29) {
        char base_letter = 'a' + (hid_code - 4);
        // Caps Lock XOR Shift determines case for letters
        if (caps_lock != shift) {
            return base_letter - 'a' + 'A';
        } else {
            return base_letter;
        }
    }
    
    // Numbers 1-9, 0 (HID codes 30-39)
    if (hid_code >= 30 && hid_code <= 39) {
        if (shift) {
            const char symbols[] = "!@#$%^&*()";
            return symbols[hid_code - 30];
        } else {
            if (hid_code == 39) return '0';
            return '1' + (hid_code - 30);
        }
    }
    
    // Special keys
    switch (hid_code) {
        case 40: return '\n';  // Enter
        case 41: return '\x1B'; // Escape
        case 42: return '\b';  // Backspace
        case 43: return '\t';  // Tab
        case 44: return ' ';   // Space
        case 45: return shift ? '_' : '-';
        case 46: return shift ? '+' : '=';
        case 47: return shift ? '{' : '[';
        case 48: return shift ? '}' : ']';
        case 49: return shift ? '|' : '\\';
        case 51: return shift ? ':' : ';';
        case 52: return shift ? '"' : '\'';
        case 53: return shift ? '~' : '`';
        case 54: return shift ? '<' : ',';
        case 55: return shift ? '>' : '.';
        case 56: return shift ? '?' : '/';
        default: return 0;
    }
}

// Send LED state to USB keyboard
void send_usb_keyboard_led_command(uint8_t led_state) {
    if (!usb_keyboard_active || usb_keyboard_slot_id == 0) return;
    
    cout << "Setting USB keyboard LEDs: 0x" << (int)led_state << "\n";
    
    // In a real implementation, this would send a SET_REPORT control transfer
    // to the keyboard's HID interface to update the LED state
    // For now, we just print the intended state
}

// Update USB keyboard LEDs
void update_usb_keyboard_leds(bool caps_lock, bool num_lock, bool scroll_lock) {
    uint8_t led_report = 0;
    if (num_lock) led_report |= 0x01;
    if (caps_lock) led_report |= 0x02;
    if (scroll_lock) led_report |= 0x04;
    
    send_usb_keyboard_led_command(led_report);
    
    cout << "USB Keyboard LEDs: ";
    cout << "Caps=" << (caps_lock ? "ON" : "OFF") << " ";
    cout << "Num=" << (num_lock ? "ON" : "OFF") << " ";
    cout << "Scroll=" << (scroll_lock ? "ON" : "OFF") << "\n";
}

// Process USB keyboard data
void process_keyboard_data(usb_keyboard_report_t* report) {
    if (!report) return;
    
    // Check for modifier keys
    bool shift_pressed = (report->modifier_keys & (USB_MOD_LEFT_SHIFT | USB_MOD_RIGHT_SHIFT)) != 0;
    
    // Process each keycode in the report
    for (int i = 0; i < 6; i++) {
        uint8_t keycode = report->keycodes[i];
        if (keycode == 0) continue;
        
        // Check if this is a new key press
        bool is_new_key = true;
        for (int j = 0; j < 6; j++) {
            if (last_usb_report.keycodes[j] == keycode) {
                is_new_key = false;
                break;
            }
        }
        
        if (!is_new_key) continue;
        
        // Handle toggle keys
        if (keycode == HID_KEY_CAPS_LOCK) {
            usb_caps_lock_on = !usb_caps_lock_on;
            update_usb_keyboard_leds(usb_caps_lock_on, usb_num_lock_on, usb_scroll_lock_on);
            continue;
        }
        
        if (keycode == HID_KEY_NUM_LOCK) {
            usb_num_lock_on = !usb_num_lock_on;
            update_usb_keyboard_leds(usb_caps_lock_on, usb_num_lock_on, usb_scroll_lock_on);
            continue;
        }
        
        if (keycode == HID_KEY_SCROLL_LOCK) {
            usb_scroll_lock_on = !usb_scroll_lock_on;
            update_usb_keyboard_leds(usb_caps_lock_on, usb_num_lock_on, usb_scroll_lock_on);
            continue;
        }
        
        // Handle F5 key for Pong
        if (keycode == HID_KEY_F5) {
            extern void start_pong_game();
            extern bool is_notepad_running();
            if (!is_notepad_running()) {
                start_pong_game();
            }
            continue;
        }
        
        // Convert HID code to ASCII
        char ascii_char = usb_hid_to_ascii(keycode, shift_pressed, usb_caps_lock_on);
        
        if (ascii_char != 0) {
            // Send to appropriate handler
            extern void notepad_handle_input(char c);
            extern void pong_handle_input(char c);
            extern bool is_notepad_running();
            extern bool is_pong_running();
            char input_buffer[256];
            extern int input_length;
            extern bool input_ready;
            
            if (is_notepad_running()) {
                notepad_handle_input(ascii_char);
            } else if (is_pong_running()) {
                pong_handle_input(ascii_char);
            } else {
                // Normal terminal input
                if (ascii_char == '\n') {
                    terminal_putchar(ascii_char);
                    input_buffer[input_length] = '\0';
                    input_ready = true;
                    input_length = 0;
                } else if (ascii_char == '\b') {
                    if (input_length > 0) {
                        terminal_putchar(ascii_char);
                        input_length--;
                    }
                } else if (input_length < 255) {
                    input_buffer[input_length++] = ascii_char;
                    terminal_putchar(ascii_char);
                }
            }
        }
    }
    
    // Save current report for next comparison
    simple_memcpy(&last_usb_report, report, sizeof(usb_keyboard_report_t));
}

// Ring doorbell for a specific slot and endpoint
void ring_doorbell(uint8_t slot_id, uint8_t endpoint_id) {
    if (xhci_db_regs && slot_id > 0) {
        xhci_db_regs[slot_id] = endpoint_id;
    }
}

// Submit a command TRB
bool submit_command(uint32_t param_low, uint32_t param_high, uint32_t status, uint32_t control) {
    if (!cmd_ring) return false;
    
    // Fill in the TRB
    cmd_ring[cmd_ring_enqueue].parameter = ((uint64_t)param_high << 32) | param_low;
    cmd_ring[cmd_ring_enqueue].status = status;
    cmd_ring[cmd_ring_enqueue].control = control | (cmd_ring_cycle ? 1 : 0);
    
    // Advance enqueue pointer
    cmd_ring_enqueue++;
    if (cmd_ring_enqueue >= 255) {  // Leave room for link TRB
        cmd_ring_enqueue = 0;
        cmd_ring_cycle = !cmd_ring_cycle;
    }
    
    // Ring command doorbell
    ring_doorbell(0, 0);
    
    return true;
}

// Process event ring
void process_events() {
    if (!event_ring) return;
    
    while ((event_ring[event_ring_dequeue].control & 1) == event_ring_cycle) {
        xhci_trb_t* event = &event_ring[event_ring_dequeue];
        uint32_t trb_type = (event->control >> 10) & 0x3F;
        
        switch (trb_type) {
            case TRB_TYPE_TRANSFER_EVENT:
                // Handle transfer completion
                if (keyboard_buffer && usb_keyboard_active) {
                    process_keyboard_data(keyboard_buffer);
                }
                break;
                
            case TRB_TYPE_COMMAND_COMPLETION:
                // Handle command completion
                break;
                
            case TRB_TYPE_PORT_STATUS_CHANGE:
                // Handle port status change
                cout << "USB port status change detected\n";
                enumerate_usb_devices();
                break;
        }
        
        // Advance dequeue pointer
        event_ring_dequeue++;
        if (event_ring_dequeue >= 256) {
            event_ring_dequeue = 0;
            event_ring_cycle = !event_ring_cycle;
        }
    }
}

// Setup USB keyboard device (simplified)
bool setup_usb_keyboard_device(uint8_t slot_id) {
    cout << "Setting up USB keyboard on slot " << (int)slot_id << "\n";
    
    // Allocate device context
    size_t context_size = sizeof(usb_device_context_t);
    void* context_raw = dma_manager.allocate_dma_buffer(context_size + 64);
    if (!context_raw) {
        cout << "Failed to allocate device context\n";
        return false;
    }
    
    usb_device_context_t* device_context = (usb_device_context_t*)align_pointer(context_raw, 64);
    simple_memset(device_context, 0, context_size);
    
    // Store device context in DCBAA
    if (dcbaa && slot_id > 0) {
        dcbaa[slot_id] = (uint64_t)device_context;
    }
    
    // Allocate keyboard transfer ring
    size_t ring_size = sizeof(xhci_trb_t) * 256;
    void* ring_raw = dma_manager.allocate_dma_buffer(ring_size + 64);
    if (!ring_raw) {
        cout << "Failed to allocate keyboard transfer ring\n";
        return false;
    }
    
    keyboard_in_ring = (xhci_trb_t*)align_pointer(ring_raw, 64);
    simple_memset(keyboard_in_ring, 0, ring_size);
    
    // Allocate keyboard buffer
    keyboard_buffer = (usb_keyboard_report_t*)dma_manager.allocate_dma_buffer(sizeof(usb_keyboard_report_t));
    if (!keyboard_buffer) {
        cout << "Failed to allocate keyboard buffer\n";
        return false;
    }
    
    simple_memset(keyboard_buffer, 0, sizeof(usb_keyboard_report_t));
    
    // Configure endpoint context for interrupt IN endpoint
    device_context->endpoint_contexts[1].ep_info = (1 << 16) | (3 << 3); // EP type = Interrupt IN
    device_context->endpoint_contexts[1].ep_info2 = (8 << 16) | 255;     // Max packet size = 8, interval = 255
    device_context->endpoint_contexts[1].dequeue_ptr = (uint64_t)keyboard_in_ring | 1;
    
    usb_keyboard_slot_id = slot_id;
    usb_keyboard_active = true;
    
    cout << "USB keyboard setup complete\n";
    return true;
}

// Enumerate USB devices (simplified)
bool enumerate_usb_devices() {
    cout << "Enumerating USB devices...\n";
    
    // Check each port for connected devices
    uint8_t num_ports = (xhci_cap_regs->hcs_params1 >> 24) & 0xFF;
    
    for (uint8_t port = 0; port < num_ports; port++) {
        volatile xhci_port_regs_t* port_regs = &xhci_port_regs[port];
        
        if (port_regs->portsc & 0x1) { // Device connected
            cout << "Device connected on port " << (int)(port + 1) << "\n";
            
            // For simplicity, assume first connected device is a keyboard
            if (!usb_keyboard_active) {
                // Enable slot command
                uint32_t slot_id = 1; // Simplified slot assignment
                if (setup_usb_keyboard_device(slot_id)) {
                    cout << "USB keyboard enumerated successfully\n";
                    return true;
                }
            }
        }
    }
    
    return false;
}

// Poll USB keyboard for input
bool poll_usb_keyboard() {
    if (!usb_keyboard_active) return false;
    
    // Process any pending events
    process_events();
    
    // Submit transfer request for keyboard input (simplified)
    static uint32_t poll_counter = 0;
    if (++poll_counter % 10000 == 0) { // Poll every 10000 iterations
        if (keyboard_in_ring && keyboard_buffer) {
            // Submit normal TRB for keyboard input
            keyboard_in_ring[keyboard_in_enqueue].parameter = (uint64_t)keyboard_buffer;
            keyboard_in_ring[keyboard_in_enqueue].status = (8 << 17); // 8 bytes expected
            keyboard_in_ring[keyboard_in_enqueue].control = (TRB_TYPE_NORMAL << 10) | (keyboard_in_cycle ? 1 : 0) | (1 << 5); // IOC bit
            
            keyboard_in_enqueue++;
            if (keyboard_in_enqueue >= 255) {
                keyboard_in_enqueue = 0;
                keyboard_in_cycle = !keyboard_in_cycle;
            }
            
            // Ring doorbell for keyboard endpoint
            ring_doorbell(usb_keyboard_slot_id, 2); // Endpoint 1 IN
        }
    }
    
    return true;
}

// Setup USB keyboard polling
void setup_usb_keyboard_polling() {
    cout << "Setting up USB keyboard polling...\n";
    
    // Initialize keyboard state
    simple_memset(&last_usb_report, 0, sizeof(usb_keyboard_report_t));
    usb_caps_lock_on = false;
    usb_num_lock_on = false;
    usb_scroll_lock_on = false;
    
    cout << "USB keyboard polling setup complete\n";
}

// Main xHCI initialization
bool xhci_init() {
    cout << "Initializing xHCI USB 3.0 driver...\n";
    
    // Find xHCI controller
    pci_device_info pci_device = scan_for_xhci();
    if (!pci_device.found) {
        cout << "Error: xHCI controller not found.\n";
        return false;
    }
    
    cout << "xHCI controller found at Bus " << (int)pci_device.bus << ", Dev " << (int)pci_device.device << "\n";
    
    // Map MMIO registers
    uint32_t base_addr = pci_device.bar0;
    cout << "Using base address: 0x" << base_addr << "\n";
    
    xhci_cap_regs = (xhci_cap_regs_t*)base_addr;
    
    // Validate controller
    uint16_t hci_version = xhci_cap_regs->hci_version;
    if (hci_version == 0x0000 || hci_version == 0xFFFF) {
        cout << "Error: Invalid xHCI version\n";
        return false;
    }
    
    cout << "xHCI Version: 0x" << hci_version << "\n";
    
    uint8_t cap_length = xhci_cap_regs->cap_length;
    xhci_op_regs = (xhci_op_regs_t*)(base_addr + cap_length);
    xhci_db_regs = (uint32_t*)(base_addr + xhci_cap_regs->dboff);
    xhci_runtime_regs = (uint64_t*)(base_addr + xhci_cap_regs->rtsoff);
    
    // Halt controller
    cout << "Halting controller...\n";
    xhci_op_regs->usb_cmd &= ~0x1;
    
    // Wait for halted
    int timeout = 1000000;
    while (!(xhci_op_regs->usb_sts & 0x1) && timeout > 0) {
        timeout--;
    }
    
    if (timeout == 0) {
        cout << "Controller failed to halt\n";
        return false;
    }
    
    // Reset controller
    cout << "Resetting controller...\n";
    xhci_op_regs->usb_cmd |= 0x2;
    
    timeout = 10000000;
    while ((xhci_op_regs->usb_cmd & 0x2) && timeout > 0) {
        timeout--;
    }
    
    if (timeout == 0) {
        cout << "Controller reset timeout\n";
        return false;
    }
    
    // Allocate DCBAA
    uint32_t max_slots = xhci_cap_regs->hcs_params1 & 0xFF;
    cout << "Max device slots: " << max_slots << "\n";
    
    size_t dcbaa_size = sizeof(uint64_t) * (max_slots + 1);
    void* dcbaa_raw = dma_manager.allocate_dma_buffer(dcbaa_size + 64);
    if (!dcbaa_raw) {
        cout << "Failed to allocate DCBAA\n";
        return false;
    }
    
    dcbaa = (uint64_t*)align_pointer(dcbaa_raw, 64);
    simple_memset(dcbaa, 0, dcbaa_size);
    xhci_op_regs->dcbaap = (uint64_t)dcbaa;
    
    // Allocate command ring
    size_t cmd_ring_size = sizeof(xhci_trb_t) * 256;
    void* cmd_ring_raw = dma_manager.allocate_dma_buffer(cmd_ring_size + 64);
    if (!cmd_ring_raw) {
        cout << "Failed to allocate command ring\n";
        return false;
    }
    
    cmd_ring = (xhci_trb_t*)align_pointer(cmd_ring_raw, 64);
    simple_memset(cmd_ring, 0, cmd_ring_size);
    xhci_op_regs->crcr = (uint64_t)cmd_ring | 0x1;
    
    // Allocate event ring
    size_t event_ring_size = sizeof(xhci_trb_t) * 256;
    void* event_ring_raw = dma_manager.allocate_dma_buffer(event_ring_size + 64);
    if (!event_ring_raw) {
        cout << "Failed to allocate event ring\n";
        return false;
    }
    
    event_ring = (xhci_trb_t*)align_pointer(event_ring_raw, 64);
    simple_memset(event_ring, 0, event_ring_size);
    
    // Set runtime registers (simplified)
    if (xhci_runtime_regs) {
        xhci_runtime_regs[4] = (uint64_t)event_ring; // Event ring base address
        xhci_runtime_regs[5] = 256; // Event ring size
    }
    
    // Configure and start controller
    xhci_op_regs->config = max_slots;
    xhci_op_regs->usb_cmd |= 0x1; // Run
    
    // Wait for controller to start
    while (xhci_op_regs->usb_sts & 0x1);
    
    cout << "xHCI controller started\n";
    
    // Setup port registers
    uint8_t num_ports = (xhci_cap_regs->hcs_params1 >> 24) & 0xFF;
    cout << "Number of USB ports: " << (int)num_ports << "\n";
    
    xhci_port_regs = (xhci_port_regs_t*)((uintptr_t)xhci_op_regs + 0x400);
    
    // Setup USB keyboard
    setup_usb_keyboard_polling();
    
    // Enumerate devices
    enumerate_usb_devices();
    
    cout << "xHCI driver initialized successfully\n";
    return true;
}

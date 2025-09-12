#ifndef XHCI_H
#define XHCI_H

#include "types.h"

// --- xHCI Register Structures ---
typedef struct {
    uint8_t cap_length;
    uint8_t reserved;
    uint16_t hci_version;
    uint32_t hcs_params1;
    uint32_t hcs_params2;
    uint32_t hcs_params3;
    uint32_t hcc_params1;
    uint32_t dboff;
    uint32_t rtsoff;
} __attribute__((packed)) xhci_cap_regs_t;

typedef struct {
    uint32_t usb_cmd;
    uint32_t usb_sts;
    uint32_t page_size;
    uint64_t dnctrl;
    uint64_t crcr;
    uint64_t dcbaap;
    uint32_t config;
} __attribute__((packed)) xhci_op_regs_t;

typedef struct {
    uint32_t portsc;
    uint32_t portpmsc;
    uint32_t portli;
    uint32_t porthlpmc;
} __attribute__((packed)) xhci_port_regs_t;

typedef struct {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} __attribute__((packed)) xhci_trb_t;

// --- USB HID Keyboard Support ---
typedef struct {
    uint8_t modifier_keys;    // Modifier key states (Ctrl, Shift, Alt, etc.)
    uint8_t reserved;         // Reserved byte (always 0)
    uint8_t keycodes[6];      // Up to 6 simultaneous key presses
} __attribute__((packed)) usb_keyboard_report_t;

typedef struct {
    uint32_t dev_info;
    uint32_t port_info;
    uint32_t tt_info;
    uint32_t dev_state;
    uint32_t reserved[4];
} __attribute__((packed)) usb_slot_context_t;

typedef struct {
    uint32_t ep_info;
    uint32_t ep_info2;
    uint64_t dequeue_ptr;
    uint32_t transfer_info;
    uint32_t reserved[3];
} __attribute__((packed)) usb_endpoint_context_t;

typedef struct {
    usb_slot_context_t slot_context;
    usb_endpoint_context_t endpoint_contexts[31];
} __attribute__((packed)) usb_device_context_t;

// USB Setup Request Structure
typedef struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_packet_t;

// Function declarations
bool xhci_init();
void setup_usb_keyboard_polling();
bool poll_usb_keyboard();
char usb_hid_to_ascii(uint8_t hid_code, bool shift, bool caps_lock);
void process_keyboard_data(usb_keyboard_report_t* report);
void update_usb_keyboard_leds(bool caps_lock, bool num_lock, bool scroll_lock);
bool enumerate_usb_devices();
bool setup_usb_keyboard_device(uint8_t slot_id);
void send_usb_keyboard_led_command(uint8_t led_state);

// Global USB keyboard state
extern bool usb_keyboard_active;
extern usb_keyboard_report_t* keyboard_buffer;
extern usb_keyboard_report_t last_usb_report;
extern bool usb_caps_lock_on;
extern uint8_t usb_keyboard_slot_id;

#endif // XHCI_H

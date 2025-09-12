#include "terminal_io.h"
#include "terminal_hooks.h"
#include "stdlib_hooks.h"
#include "xhci.h"

// Global terminal I/O object
TerminalIO kout;

// Global formatting state
bool use_hex = false;

// USB input mode
bool TerminalIO::usb_input_mode = false;

// USB integration methods
void TerminalIO::enable_usb_input() {
    extern bool usb_keyboard_active;
    if (usb_keyboard_active) {
        usb_input_mode = true;
        //cout << "USB keyboard input enabled\n";
    }
}

void TerminalIO::disable_usb_input() {
    usb_input_mode = false;
    ///cout << "USB keyboard input disabled\n";
}

bool TerminalIO::is_usb_input_enabled() {
    return usb_input_mode;
}

// Modified keyboard input function
TerminalIO& TerminalIO::operator>>(char* str) {
    // Check if USB keyboard should be used
    extern bool usb_keyboard_active;
    
    if (usb_input_mode && usb_keyboard_active) {
        // Use USB keyboard input
        extern bool input_ready;
        char input_buffer[256];
        
        input_ready = false;
        
        // Poll USB keyboard until input is ready
        while (!input_ready) {
            poll_usb_keyboard();
            // Small delay to prevent excessive CPU usage
            for (volatile int i = 0; i < 1000; i++);
        }
        
        strcpy(str, input_buffer);
    } else {
        // Use PS/2 keyboard input (existing implementation)
        extern bool input_ready;
        char input_buffer[256];
        
        input_ready = false;
        
        while (!input_ready) {
            asm volatile ("hlt");
        }
        
        strcpy(str, input_buffer);
    }
    
    return *this;
}

// Integer input function
TerminalIO& TerminalIO::operator>>(int& num) {
    char buffer[32];
    *this >> buffer;
    
    // Simple string to integer conversion
    num = 0;
    bool negative = false;
    int i = 0;
    
    if (buffer[0] == '-') {
        negative = true;
        i = 1;
    }
    
    while (buffer[i] >= '0' && buffer[i] <= '9') {
        num = num * 10 + (buffer[i] - '0');
        i++;
    }
    
    if (negative) {
        num = -num;
    }
    
    return *this;
}

// Unsigned integer input function
TerminalIO& TerminalIO::operator>>(unsigned int& num) {
    char buffer[32];
    *this >> buffer;
    
    num = 0;
    int i = 0;
    
    while (buffer[i] >= '0' && buffer[i] <= '9') {
        num = num * 10 + (buffer[i] - '0');
        i++;
    }
    
    return *this;
}

// Stream manipulators
TerminalIO& endl(TerminalIO& stream) {
    stream << '\n';
    return stream;
}

TerminalIO& hex(TerminalIO& stream) {
    use_hex = true;
    return stream;
}

TerminalIO& dec(TerminalIO& stream) {
    use_hex = false;
    return stream;
}

// Output operators (keep existing implementations)
TerminalIO& TerminalIO::operator<<(const char* str) {
    while (*str) {
        terminal_putchar(*str++);
    }
    return *this;
}

TerminalIO& TerminalIO::operator<<(char c) {
    terminal_putchar(c);
    return *this;
}

TerminalIO& TerminalIO::operator<<(int num) {
    if (use_hex) {
        char buffer[12];
        int i = 0;
        
        if (num == 0) {
            terminal_putchar('0');
            return *this;
        }
        
        if (num < 0) {
            terminal_putchar('-');
            num = -num;
        }
        
        while (num > 0) {
            int digit = num % 16;
            buffer[i++] = digit < 10 ? '0' + digit : 'a' + (digit - 10);
            num /= 16;
        }
        
        while (--i >= 0) {
            terminal_putchar(buffer[i]);
        }
    } else {
        char buffer[12];
        int i = 0;
        
        if (num == 0) {
            terminal_putchar('0');
            return *this;
        }
        
        if (num < 0) {
            terminal_putchar('-');
            num = -num;
        }
        
        while (num > 0) {
            buffer[i++] = '0' + (num % 10);
            num /= 10;
        }
        
        while (--i >= 0) {
            terminal_putchar(buffer[i]);
        }
    }
    return *this;
}

TerminalIO& TerminalIO::operator<<(unsigned int num) {
    if (use_hex) {
        char buffer[12];
        int i = 0;
        
        if (num == 0) {
            terminal_putchar('0');
            return *this;
        }
        
        while (num > 0) {
            int digit = num % 16;
            buffer[i++] = digit < 10 ? '0' + digit : 'a' + (digit - 10);
            num /= 16;
        }
        
        while (--i >= 0) {
            terminal_putchar(buffer[i]);
        }
    } else {
        char buffer[12];
        int i = 0;
        
        if (num == 0) {
            terminal_putchar('0');
            return *this;
        }
        
        while (num > 0) {
            buffer[i++] = '0' + (num % 10);
            num /= 10;
        }
        
        while (--i >= 0) {
            terminal_putchar(buffer[i]);
        }
    }
    return *this;
}

TerminalIO& TerminalIO::operator<<(long num) {
    return operator<<(static_cast<int>(num));
}

TerminalIO& TerminalIO::operator<<(unsigned long num) {
    return operator<<(static_cast<unsigned int>(num));
}

TerminalIO& TerminalIO::operator<<(void* ptr) {
    if (ptr == nullptr) {
        *this << "(null)";
        return *this;
    }
    
    *this << "0x";
    bool old_hex = use_hex;
    use_hex = true;
    *this << reinterpret_cast<uintptr_t>(ptr);
    use_hex = old_hex;
    return *this;
}

TerminalIO& TerminalIO::operator<<(ManipulatorFunc func) {
    return func(*this);
}

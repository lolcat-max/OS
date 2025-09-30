#include <cstdarg>

// --- Type Definitions ---
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned int uintptr_t;
typedef unsigned int size_t;

static uint32_t backbuffer[1024 * 768]; // adjust size to max screen width*height

// --- Low-level I/O functions ---
static inline void outb(uint16_t port, uint8_t val) { asm volatile ("outb %0, %1" : : "a"(val), "d"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t ret; asm volatile ("inb %1, %0" : "=a"(ret) : "d"(port)); return ret; }

// --- Standard Library Implementations ---
int strncmp(const char* str1, const char* str2, size_t n) {
    if (n == 0) return 0;
    if (str1 == str2) return 0;
    for (size_t i = 0; i < n; i++) {
        if (str1[i] == '\0' || str2[i] == '\0') {
            return (unsigned char)str1[i] - (unsigned char)str2[i];
        }
        if (str1[i] != str2[i]) {
            return (unsigned char)str1[i] - (unsigned char)str2[i];
        }
    }
    return 0;
}

// --- Minimal snprintf ---
int snprintf(char* buffer, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* buf = buffer;
    char* end = buffer + size - 1; // leave space for '\0'

    while (*fmt && buf < end) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                int val = va_arg(args, int);
                char tmp[32];
                char* t = tmp + 31; *t = '\0';
                bool neg = false;
                if (val < 0) { neg = true; val = -val; }
                do { *--t = '0' + val % 10; val /= 10; } while (val);
                if (neg) *--t = '-';
                while (*t && buf < end) *buf++ = *t++;
            }
            else if (*fmt == 'u') {
                unsigned int val = va_arg(args, unsigned int);
                char tmp[32];
                char* t = tmp + 31; *t = '\0';
                do { *--t = '0' + val % 10; val /= 10; } while (val);
                while (*t && buf < end) *buf++ = *t++;
            }
            else if (*fmt == 'l' && fmt[1] == 'l') { // %llu
                fmt += 2;
                unsigned long long val = va_arg(args, unsigned long long);
                char tmp[32];
                char* t = tmp + 31; *t = '\0';
                do { *--t = '0' + val % 10; val /= 10; } while (val);
                while (*t && buf < end) *buf++ = *t++;
            }
            else if (*fmt == 'x') {
                unsigned int val = va_arg(args, unsigned int);
                char tmp[32];
                char* t = tmp + 31; *t = '\0';
                const char* hex = "0123456789ABCDEF";
                do { *--t = hex[val & 0xF]; val >>= 4; } while (val);
                while (*t && buf < end) *buf++ = *t++;
            }
            else if (*fmt == 's') {
                const char* s = va_arg(args, const char*);
                while (*s && buf < end) *buf++ = *s++;
            }
            else if (*fmt == 'c') {
                char c = (char)va_arg(args, int);
                if (buf < end) *buf++ = c;
            }
            else {
                if (buf < end) *buf++ = *fmt;
            }
            fmt++;
        } else {
            *buf++ = *fmt++;
        }
    }

    *buf = '\0';
    va_end(args);
    return buf - buffer;
}


unsigned int strlen(const char* str) { unsigned int len = 0; while (str[len]) len++; return len; }
void* memset(void* ptr, int value, size_t num) { uint8_t* p = (uint8_t*)ptr; for (size_t i = 0; i < num; i++) { p[i] = (uint8_t)value; } return ptr; }
void* memcpy(void* dest, const void* src, size_t n) { uint8_t* d = (uint8_t*)dest; const uint8_t* s = (const uint8_t*)src; for (size_t i = 0; i < n; i++) { d[i] = s[i]; } return dest; }
int strcmp(const char* s1, const char* s2) { while(*s1 && (*s1 == *s2)) { s1++; s2++; } return *(const unsigned char*)s1 - *(const unsigned char*)s2; }

// --- Basic Memory Allocator (Bump Allocator) ---
static uint8_t kernel_heap[1024 * 1024]; // 1MB heap
static uintptr_t heap_ptr = (uintptr_t)kernel_heap;
void* operator new(size_t size) {
    uintptr_t end_of_heap = (uintptr_t)kernel_heap + sizeof(kernel_heap);
    if (heap_ptr + size > end_of_heap) {
        return nullptr;
    }
    uintptr_t addr = heap_ptr;
    heap_ptr += size;
    return (void*)addr;
}
void* operator new[](size_t size) { return operator new(size); }
void operator delete(void* ptr) noexcept { /* No-op */ }
void operator delete[](void* ptr) noexcept { /* No-op */ }
void operator delete(void* ptr, size_t size) noexcept { 
    (void)ptr; (void)size; /* No-op */ 
}
void operator delete[](void* ptr, size_t size) noexcept { 
    (void)ptr; (void)size; /* No-op */ 
}
// --- CXX ABI Stubs ---
extern "C" { int __cxa_guard_acquire(long long *g) {return !*(char *)(g);}; void __cxa_guard_release(long long *g) {*(char *)g = 1;}; void __cxa_pure_virtual() {}; }
// --- RTC Helpers ---
uint8_t rtc_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

uint8_t bcd_to_bin(uint8_t val) {
    return ((val / 16) * 10) + (val & 0x0F);
}

struct RTC_Time {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

RTC_Time read_rtc() {
    RTC_Time t;
    uint8_t century = 20;
    while (rtc_read(0x0A) & 0x80); 
    uint8_t regB = rtc_read(0x0B);
    bool is_bcd = !(regB & 0x04);
    t.second = rtc_read(0x00);
    t.minute = rtc_read(0x02);
    t.hour   = rtc_read(0x04);
    t.day    = rtc_read(0x07);
    t.month  = rtc_read(0x08);
    t.year   = rtc_read(0x09);
    if (is_bcd) {
        t.second = bcd_to_bin(t.second); t.minute = bcd_to_bin(t.minute);
        t.hour   = bcd_to_bin(t.hour);   t.day    = bcd_to_bin(t.day);
        t.month  = bcd_to_bin(t.month);  t.year   = bcd_to_bin(t.year);
    }
    t.year += century * 100;
    return t;
}

// --- Multiboot & Framebuffer ---
struct multiboot_info {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline, mods_count, mods_addr, syms[4], mmap_length, mmap_addr, drives_length, drives_addr, config_table, boot_loader_name, apm_table, vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
    uint8_t framebuffer_bpp, framebuffer_type, color_info[6];
} __attribute__((packed));
struct FramebufferInfo { uint32_t* ptr; uint32_t width, height, pitch; } fb_info;

// --- Basic Font (8x8) ---
#include "font.h" 

// --- Graphics Functions ---
void put_pixel_back(int x, int y, uint32_t color) {
    if (x >= 0 && x < (int)fb_info.width && y >= 0 && y < (int)fb_info.height) {
        backbuffer[y * fb_info.width + x] = color;
    }
}

void swap_buffers() {
    uint32_t* fb = fb_info.ptr;
    for (uint32_t y = 0; y < fb_info.height; y++) {
        for (uint32_t x = 0; x < fb_info.width; x++) {
            fb[y * (fb_info.pitch / 4) + x] = backbuffer[y * fb_info.width + x];
        }
    }
}
void draw_rect_filled(int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            put_pixel_back(i, j, color);
        }
    }
}
void draw_char(char c, int x, int y, uint32_t color) {
    if (c < 0) return;
    const uint8_t* glyph = font + (int)c * 8;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if ((glyph[i] & (0x80 >> j))) {
                put_pixel_back(x + j, y + i, color);
            }
        }
    }
}
void draw_string(const char* str, int x, int y, uint32_t color) {
    for (int i = 0; str[i]; i++) {
        draw_char(str[i], x + i * 8, y, color);
    }
}

// --- Windowing System ---
class Window {
public:
    int x, y, w, h;
    const char* title;
    bool has_focus;
    bool is_closed;

    Window(int x, int y, int w, int h, const char* title) 
        : x(x), y(y), w(w), h(h), title(title), has_focus(false), is_closed(false) {}
    virtual ~Window() {}

    virtual void draw() = 0;
    virtual void on_key_press(char c) = 0;
    virtual void on_mouse_event(int mx, int my, bool clicked) = 0;
    virtual void update() = 0;

    bool is_in_titlebar(int mx, int my) {
        return mx > x && mx < x + w && my > y && my < y + 25;
    }

    bool is_in_close_button(int mx, int my) {
        int btn_x = x + w - 22;
        int btn_y = y + 4;
        return mx >= btn_x && mx < btn_x + 18 && my >= btn_y && my < btn_y + 18;
    }

    void close() {
        is_closed = true;
    }
};

class TerminalWindow; 
void launch_new_terminal();

class TerminalWindow : public Window {
private:
    char buffer[25][81];
    int cursor_x, cursor_y;
    char current_line[81];
    int line_pos;

    void scroll() {
        memcpy(buffer[0], buffer[1], 24 * 81);
        memset(buffer[24], ' ', 80);
    }

    void handle_command() {
    if (strcmp(current_line, "help") == 0) {
        print_line("Commands: help, clear, echo, new_term, time, version");
    } 
    else if (strcmp(current_line, "clear") == 0) {
        for(int i=0; i<25; ++i) memset(buffer[i], ' ', 80);
        cursor_x = 0;
        cursor_y = 0;
    } 
    else if (strncmp(current_line, "echo ", 5) == 0) {
        print_line(&current_line[5]);
    } 
    else if (strcmp(current_line, "new_term") == 0) {
        launch_new_terminal();
        print_line("Launched new terminal.");
    } 
    else if (strcmp(current_line, "time") == 0) {
		RTC_Time t = read_rtc();
		char buf[64];
		snprintf(buf, sizeof(buf), "Time: %d:%d:%d %d/%d/%d",
				 t.hour, t.minute, t.second, t.day, t.month, t.year);
		print_line(buf);
	}
    else if (strcmp(current_line, "version") == 0) {
        print_line("RTOS++ v0.1 - Bare-metal OS");
    }
    else if (strlen(current_line) > 0) {
        print_line("Unknown command.");
    }
    print_prompt();
}

void print_prompt() {
    print_string("RTOS++> ");
}
    
public:
    TerminalWindow(int x, int y) : Window(x, y, 640, 425, "Terminal"), cursor_x(0), cursor_y(0), line_pos(0) {
        for (int i = 0; i < 25; ++i) {
            memset(buffer[i], ' ', 80);
            buffer[i][80] = '\0';
        }
        print_prompt();
    }

    void print_char(char c) {
        if (cursor_x >= 80) {
            cursor_x = 0;
            cursor_y++;
        }
        if (cursor_y >= 25) {
            scroll();
            cursor_y = 24;
        }
        buffer[cursor_y][cursor_x++] = c;
    }
    
    void print_string(const char* str) {
        for (int i = 0; str[i]; i++) print_char(str[i]);
    }
    
    void print_line(const char* str) {
        print_string(str);
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= 25) {
            scroll();
            cursor_y = 24;
        }
    }

    void draw() override {
        uint32_t border_color = has_focus ? 0xFFFFFF : 0x888888;
        uint32_t title_color = has_focus ? 0x0000AA : 0x555555;
        draw_rect_filled(x, y, w, h, border_color);
        draw_rect_filled(x + 2, y + 2, w - 4, h - 4, 0x000033);
        draw_rect_filled(x, y, w, 25, title_color);
        draw_string(title, x + 5, y + 8, 0xFFFFFF);
        
        int btn_x = x + w - 22;
        int btn_y = y + 4;
        draw_rect_filled(btn_x, btn_y, 18, 18, 0xFF0000); // Red square
        draw_char('X', btn_x + 5, btn_y + 5, 0xFFFFFF); // White 'X'

        for (int i = 0; i < 25; i++) {
            draw_string(buffer[i], x + 5, y + 30 + i * 15, 0xDDDDDD);
        }
    }
    
    void update() override {
        static int blink_counter = 0;
        if (has_focus && (blink_counter++ / 20) % 2 == 0) {
            draw_rect_filled(x + 5 + cursor_x * 8, y + 30 + cursor_y * 15, 8, 15, 0xFFFFFF);
        }
    }

    void on_key_press(char c) override {
        if (c == '\b') {
            if (line_pos > 0) {
                line_pos--;
                cursor_x--;
                buffer[cursor_y][cursor_x] = ' ';
            }
        } else if (c == '\n') {
            current_line[line_pos] = '\0';
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= 25) { scroll(); cursor_y = 24; }
            handle_command();
            line_pos = 0;
        } else {
            if (line_pos < 80) {
                current_line[line_pos++] = c;
                print_char(c);
            }
        }
    }
    
    void on_mouse_event(int mx, int my, bool clicked) override {}
};

void draw_cursor(int x, int y, uint32_t color) {
    for(int i = 0; i < 12; i++) put_pixel_back(x, y + i, color);
    for(int i = 0; i < 8; i++) put_pixel_back(x + i, y + i, color);
    for(int i = 0; i < 4; i++) put_pixel_back(x + i, y + (11 - i), color);
}

class WindowManager {
private:
    Window* windows[10];
    int num_windows;
    int focused_idx;
    int dragging_idx;
    int drag_offset_x, drag_offset_y;

public:
    WindowManager() : num_windows(0), focused_idx(-1), dragging_idx(-1) {}

    void add_window(Window* win) {
        if (num_windows < 10 && win != nullptr) {
            windows[num_windows] = win;
            set_focus(num_windows);
            num_windows++;
        }
    }

    void set_focus(int idx) {
        if (focused_idx != -1 && focused_idx < num_windows) {
            windows[focused_idx]->has_focus = false;
        }
        focused_idx = idx;
        if (idx == -1) return;

        windows[focused_idx]->has_focus = true;
        
        Window* focused = windows[idx];
        for (int i = idx; i < num_windows - 1; i++) {
            windows[i] = windows[i + 1];
        }
        windows[num_windows - 1] = focused;
        focused_idx = num_windows - 1;
    }
    
    // --- MODIFIED: Replaced with a safer implementation ---
    void cleanup_closed_windows() {
		int new_count = 0;
		for (int i = 0; i < num_windows; i++) {
			if (!windows[i]->is_closed) {
				windows[new_count++] = windows[i];
			}
		}
		num_windows = new_count;

		// Reset focus to top-most window
		if (num_windows > 0) {
			for (int i = 0; i < num_windows; i++) windows[i]->has_focus = false;
			focused_idx = num_windows - 1;
			windows[focused_idx]->has_focus = true;
		} else {
			focused_idx = -1;
		}
	}

    void draw_desktop() {
        draw_rect_filled(0, 0, fb_info.width, fb_info.height, 0x336699);
        draw_rect_filled(0, fb_info.height - 40, fb_info.width, 40, 0x808080);
        draw_rect_filled(5, fb_info.height - 35, 75, 30, 0xC0C0C0);
        draw_string("Terminal", 10, fb_info.height - 28, 0x000000);
    }
    
    void update_all() {
        draw_desktop();
        for (int i = 0; i < num_windows; i++) {
            windows[i]->draw();
            windows[i]->update();
        }
    }

    void handle_input(char key, int mx, int my, bool left_down, bool left_clicked) {
        if (dragging_idx != -1) {
            if (left_down) {
                windows[dragging_idx]->x = mx - drag_offset_x;
                windows[dragging_idx]->y = my - drag_offset_y;
            } else {
                dragging_idx = -1;
            }
        }
        
        if (left_clicked) {
            if (mx > 5 && mx < 80 && my > (int)fb_info.height - 35 && my < (int)fb_info.height - 5) {
                for (volatile int i = 0; i < 1000000; i++);
                launch_new_terminal();
                return; // ADDED: Prevents fall-through bug
            }
            
            for (int i = num_windows - 1; i >= 0; i--) {
                if (windows[i]->is_in_close_button(mx, my)) {
                    windows[i]->close();
                    return;
                }
                if (windows[i]->is_in_titlebar(mx, my)) {
                    set_focus(i);
                    dragging_idx = focused_idx;
                    drag_offset_x = mx - windows[dragging_idx]->x;
                    drag_offset_y = my - windows[dragging_idx]->y;
                    return;
                }
            }
        }
        
        if (key != 0 && focused_idx != -1) {
            windows[focused_idx]->on_key_press(key);
        }
    }
};

WindowManager wm;

void launch_new_terminal() {
    static int win_count = 0;
    TerminalWindow* term = new TerminalWindow(80 + (win_count * 30), 80 + (win_count * 30));
    if (term) {
        wm.add_window(term);
        win_count++;
    }
}

// --- PS/2 Driver ---
#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
const char sc_ascii_nomod[] = { 0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,0,0,' ',0 };
const char sc_ascii_shift[] = { 0,0,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,0,0,' ',0 };
bool is_shift_pressed = false, is_escape_code = false;
int mouse_x = 400, mouse_y = 300;
bool mouse_left_down = false, mouse_left_last = false;
char last_key_press = 0;

void poll_input() {
    last_key_press = 0; 
    while (inb(0x64) & 1) {
        uint8_t status = inb(0x64);
        uint8_t scancode = inb(0x60);

        if (status & 0x20) { // Mouse data
            static uint8_t mouse_cycle = 0;
            static int8_t mouse_packet[3];
            mouse_packet[mouse_cycle++] = scancode;
            if (mouse_cycle == 3) {
                mouse_cycle = 0;
                mouse_left_last = mouse_left_down;
                mouse_left_down = mouse_packet[0] & 0x01;
                mouse_x += mouse_packet[1];
                mouse_y -= mouse_packet[2];
                if (mouse_x < 0) mouse_x = 0; if (mouse_y < 0) mouse_y = 0;
                if (mouse_x >= (int)fb_info.width) mouse_x = fb_info.width - 1;
                if (mouse_y >= (int)fb_info.height) mouse_y = fb_info.height - 1;
            }
        } else { // Keyboard data
            if (scancode == 0xE0) { is_escape_code = true; continue; }
            bool is_press = !(scancode & 0x80);
            if (!is_press) scancode -= 0x80;
            
            if (scancode == 0x2A || scancode == 0x36) is_shift_pressed = is_press;

            if (is_press) {
                const char* map = is_shift_pressed ? sc_ascii_shift : sc_ascii_nomod;
                if (scancode < sizeof(sc_ascii_nomod) && map[scancode] != 0) {
                    last_key_press = map[scancode];
                }
            }
            is_escape_code = false;
        }
    }
}

extern "C" void kernel_main(uint32_t magic, uint32_t multiboot_addr) {
    multiboot_info* mbi = (multiboot_info*)multiboot_addr;
    
    if (!(mbi->flags & (1 << 12))) { return; }
    fb_info = { (uint32_t*)((uintptr_t)mbi->framebuffer_addr), mbi->framebuffer_width, mbi->framebuffer_height, mbi->framebuffer_pitch };

    outb(0x64, 0xA8); outb(0x64, 0x20);
    uint8_t status = (inb(0x60) | 2) & ~0x20;
    outb(0x64, 0x60); outb(0x60, status);
    outb(0x64, 0xD4); outb(0x60, 0xF6); inb(0x60);
    outb(0x64, 0xD4); outb(0x60, 0xF4); inb(0x60);

    launch_new_terminal();
    
    while (true) {
        poll_input();
		bool mouse_clicked_this_frame = mouse_left_down && !mouse_left_last;

		wm.handle_input(last_key_press, mouse_x, mouse_y, mouse_left_down, mouse_clicked_this_frame);
 
        wm.cleanup_closed_windows();

        wm.update_all();
        draw_cursor(mouse_x, mouse_y, 0xFFFFFF);

        swap_buffers();
    }
}
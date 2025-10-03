#include <cstdarg>

// =============================================================================
// SECTION 1: TYPE DEFS, STDLIB/CXX STUBS, AND LOW-LEVEL FUNCTIONS
// =============================================================================

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

// --- CXX ABI Stubs ---
namespace __cxxabiv1 {
    extern "C" int __cxa_guard_acquire(long long *g) { return !*(char *)(g); }
    extern "C" void __cxa_guard_release(long long *g) { *(char *)g = 1;}
    extern "C" void __cxa_pure_virtual() {}

    // Add this function
    extern "C" void __cxa_throw_bad_array_new_length() {
        // For a kernel, this is a fatal error. Halt the system.
        asm volatile("cli; hlt");
    }

    class __class_type_info { virtual void dummy(); };
    void __class_type_info::dummy() {}
    class __si_class_type_info { virtual void dummy(); };
    void __si_class_type_info::dummy() {}
}

// --- C++ Keywords and Forward Declarations ---
class Window;
class TerminalWindow;
extern "C" void kernel_main(uint32_t magic, uint32_t multiboot_addr);
void launch_new_terminal();
int fat32_write_file(const char* filename, const void* data, uint32_t size);
int fat32_remove_file(const char* filename);
char* fat32_read_file_as_string(const char* filename);
void fat32_list_files();
void interpret_c(const char* source);
bool fat32_init();


// --- Low-level I/O functions ---
static inline void outb(uint16_t port, uint8_t val) { asm volatile ("outb %0, %1" : : "a"(val), "d"(port)); }
static inline void outl(uint16_t port, uint32_t val) { asm volatile ("outl %0, %1" : : "a"(val), "d"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t ret; asm volatile ("inb %1, %0" : "=a"(ret) : "d"(port)); return ret; }
static inline uint32_t inl(uint16_t port) { uint32_t ret; asm volatile ("inl %1, %0" : "=a"(ret) : "d"(port)); return ret; }
static inline uint32_t pci_read_config_dword(uint16_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)device << 11) | ((uint32_t)function << 8) | (offset & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}

// --- Minimal Standard Library ---
size_t strlen(const char* str) { size_t len = 0; while (str[len]) len++; return len; }
void* memset(void* ptr, int value, size_t num) { uint8_t* p = (uint8_t*)ptr; for (size_t i = 0; i < num; i++) p[i] = (uint8_t)value; return ptr; }
void* memcpy(void* dest, const void* src, size_t n) { uint8_t* d = (uint8_t*)dest; const uint8_t* s = (const uint8_t*)src; for (size_t i = 0; i < n; i++) d[i] = s[i]; return dest; }
int memcmp(const void* ptr1, const void* ptr2, size_t n) { const uint8_t* p1 = (const uint8_t*)ptr1; const uint8_t* p2 = (const uint8_t*)ptr2; for(size_t i=0; i<n; ++i) if(p1[i] != p2[i]) return p1[i] - p2[i]; return 0; }
int strcmp(const char* s1, const char* s2) { while(*s1 && (*s1 == *s2)) { s1++; s2++; } return *(const unsigned char*)s1 - *(const unsigned char*)s2; }
int strncmp(const char* s1, const char* s2, size_t n) { if (n == 0) return 0; do { if (*s1 != *s2++) return *(unsigned const char*)s1 - *(unsigned const char*)--s2; if (*s1++ == 0) break; } while (--n != 0); return 0; }
char* strchr(const char* s, int c) { while (*s != (char)c) if (!*s++) return nullptr; return (char*)s; }
char* strcpy(char *dest, const char *src) { char *ret = dest; while ((*dest++ = *src++)); return ret; }
char* strncpy(char* dest, const char* src, size_t n) { size_t i; for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i]; for ( ; i < n; i++) dest[i] = '\0'; return dest; }
char* strncat(char *dest, const char *src, size_t n) {
    size_t dest_len = strlen(dest);
    size_t i;
    for (i = 0 ; i < n && src[i] != '\0' ; i++)
        dest[dest_len + i] = src[i];
    dest[dest_len + i] = '\0';
    return dest;
}
int simple_atoi(const char* str) { int res = 0; while(*str >= '0' && *str <= '9') { res = res * 10 + (*str - '0'); str++; } return res; }

int snprintf(char* buffer, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* buf = buffer;
    char* end = buffer + size - 1;
    while (*fmt && buf < end) {
        if (*fmt == '%') {
            fmt++; // Consume '%'
            if (*fmt == 'd') {
                int val = va_arg(args, int);
                char tmp[32];
                char* t = tmp + 31; *t = '\0';
                bool neg = val < 0;
                if(neg) val = -val;
                if(val == 0) *--t = '0';
                else while(val > 0) { *--t = '0' + val % 10; val /= 10; }
                if (neg) *--t = '-';
                while (*t && buf < end) *buf++ = *t++;
            } else if (*fmt == 's') {
                const char* s = va_arg(args, const char*);
                while (*s && buf < end) *buf++ = *s++;
            } else if (*fmt == 'c') {
                char c = (char)va_arg(args, int);
                if (buf < end) *buf++ = c;
            } else { // Handles %% and unknown specifiers by just printing the char
                 if (buf < end) *buf++ = *fmt;
            }
        } else {
            *buf++ = *fmt;
        }
        fmt++; // Move to the next character in the format string
    }
    *buf = '\0';
    va_end(args);
    return buf - buffer;
}

// --- Basic Memory Allocator ---
static uint8_t kernel_heap[1024 * 1024 * 8]; // 8MB heap
static size_t heap_ptr = 0;
void* operator new(size_t size) { if (heap_ptr + size > sizeof(kernel_heap)) return nullptr; void* addr = &kernel_heap[heap_ptr]; heap_ptr += size; return addr; }
void* operator new[](size_t size) { return operator new(size); }
void operator delete(void* ptr) noexcept {}
void operator delete[](void* ptr) noexcept {}
void operator delete(void* ptr, size_t size) noexcept { (void)ptr; (void)size; }
void operator delete[](void* ptr, size_t size) noexcept { (void)ptr; (void)size; }


void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i != 0; i--) {
            d[i-1] = s[i-1];
        }
    }
    return dest;
}

// =============================================================================
// SECTION 2: BOOTLOADER INFO, FONT, RTC
// =============================================================================
struct multiboot_info {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline, mods_count, mods_addr;
    uint32_t syms[4], mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr, config_table, boot_loader_name, apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch, framebuffer_width, framebuffer_height;
    uint8_t framebuffer_bpp, framebuffer_type, color_info[6];
} __attribute__((packed));

#include "font.h"

uint8_t rtc_read(uint8_t reg) { outb(0x70, reg); return inb(0x71); }
uint8_t bcd_to_bin(uint8_t val) { return ((val / 16) * 10) + (val & 0x0F); }
struct RTC_Time { uint8_t second, minute, hour, day, month; uint16_t year; };
RTC_Time read_rtc() {
    RTC_Time t;
    uint8_t century = 20;
    while (rtc_read(0x0A) & 0x80);
    uint8_t regB = rtc_read(0x0B);
    bool is_bcd = !(regB & 0x04);
    t.second = rtc_read(0x00); t.minute = rtc_read(0x02); t.hour = rtc_read(0x04);
    t.day = rtc_read(0x07); t.month = rtc_read(0x08); t.year = rtc_read(0x09);
    if (is_bcd) {
        t.second = bcd_to_bin(t.second); t.minute = bcd_to_bin(t.minute); t.hour = bcd_to_bin(t.hour);
        t.day = bcd_to_bin(t.day); t.month = bcd_to_bin(t.month); t.year = bcd_to_bin(t.year);
    }
    t.year += century * 100;
    return t;
}

// =============================================================================
// SECTION 3: GRAPHICS & WINDOWING SYSTEM
// =============================================================================

// --- Globals for Graphics ---
static uint32_t* backbuffer = nullptr;
struct FramebufferInfo { uint32_t* ptr; uint32_t width, height, pitch; } fb_info;

// --- Drawing Primitives ---
void put_pixel_back(int x, int y, uint32_t color) {
    if (backbuffer && x >= 0 && x < (int)fb_info.width && y >= 0 && y < (int)fb_info.height) {
        backbuffer[y * fb_info.width + x] = color;
    }
}

void swap_buffers() {
    if (!backbuffer) return;
    for (uint32_t y = 0; y < fb_info.height; y++) {
        memcpy(fb_info.ptr + y * (fb_info.pitch / 4), backbuffer + y * fb_info.width, fb_info.width * 4);
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
    if ((unsigned char)c > 127) return;
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

// --- Window Base Class ---
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
    virtual void update() = 0;
    virtual void console_print(const char* s) {}

    bool is_in_titlebar(int mx, int my) { return mx > x && mx < x + w && my > y && my < y + 25; }
    bool is_in_close_button(int mx, int my) { int btn_x = x + w - 22, btn_y = y + 4; return mx >= btn_x && mx < btn_x + 18 && my >= btn_y && my < btn_y + 18; }
    void close() { is_closed = true; }
};


// --- Window Manager ---
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
        if (num_windows < 10) {
            if (focused_idx != -1 && focused_idx < num_windows) windows[focused_idx]->has_focus = false;
            windows[num_windows] = win;
            focused_idx = num_windows;
            windows[num_windows]->has_focus = true;
            num_windows++;
        }
    }

    void set_focus(int idx) {
        if (idx < 0 || idx >= num_windows || idx == focused_idx) return;
        if (focused_idx != -1 && focused_idx < num_windows) windows[focused_idx]->has_focus = false;
        Window* focused = windows[idx];
        for (int i = idx; i < num_windows - 1; i++) windows[i] = windows[i+1];
        windows[num_windows - 1] = focused;
        focused_idx = num_windows - 1;
        windows[num_windows - 1]->has_focus = true;
    }

    void cleanup_closed_windows() {
        if (num_windows == 0) return;
        int current_idx = 0;
        while (current_idx < num_windows) {
            if (windows[current_idx]->is_closed) {
                delete windows[current_idx];
                for (int j = current_idx; j < num_windows - 1; j++) {
                    windows[j] = windows[j + 1];
                }
                num_windows--;
                // Do not increment current_idx as the next element has shifted into its place
            } else {
                current_idx++;
            }
        }
        
        if (num_windows > 0) {
            focused_idx = num_windows - 1;
            for(int i = 0; i < num_windows; i++) windows[i]->has_focus = false;
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
    
    void handle_input(char key, int mx, int my, bool left_down, bool left_clicked);
    void print_to_focused(const char* s);
};

WindowManager wm; // Global instance

// --- Terminal Window Class ---
#define KEY_UP     -1
#define KEY_DOWN   -2
#define KEY_LEFT   -3
#define KEY_RIGHT  -4
#define KEY_DELETE -5
#define KEY_HOME   -6
#define KEY_END    -7

class TerminalWindow : public Window {
private:
    char buffer[40][120];
    int line_count;
    char current_line[120];
    int line_pos;
    bool in_editor;
    char edit_filename[32];
    char** edit_lines;
    int edit_line_count;
    int edit_current_line;
    int edit_cursor_col;
    int edit_scroll_offset;
    
    void scroll() {
        for(int i=0; i<39; ++i) memcpy(buffer[i], buffer[i+1], 120);
        memset(buffer[39], 0, 120);
    }
    
    void push_line(const char* s) {
        if (line_count >= 40) scroll(); else line_count++;
        strncpy(buffer[line_count-1], s, 119);
    }
    
    void print_prompt() { push_line(">"); }
    void handle_command();

public:
    TerminalWindow(int x, int y) : Window(x, y, 640, 400, "Terminal"), line_count(0), line_pos(0), in_editor(false), 
        edit_lines(nullptr), edit_line_count(0), edit_current_line(0), edit_cursor_col(0), edit_scroll_offset(0) {
        memset(buffer, 0, sizeof(buffer));
        current_line[0] = '\0';
        print_prompt();
    }
    
    ~TerminalWindow() { 
        if(edit_lines) {
            for(int i = 0; i < edit_line_count; i++) delete[] edit_lines[i];
            delete[] edit_lines;
        }
    }

    void draw() override {
        uint32_t border_color = has_focus ? 0xFFFFFF : 0x888888;
        uint32_t title_color = has_focus ? 0x0000AA : 0x555555;
        draw_rect_filled(x, y, w, h, border_color);
        draw_rect_filled(x + 2, y + 2, w - 4, h - 4, 0x000033);
        draw_rect_filled(x, y, w, 25, title_color);
        draw_string(title, x + 5, y + 8, 0xFFFFFF);
        int btn_x = x + w - 22, btn_y = y + 4;
        draw_rect_filled(btn_x, btn_y, 18, 18, 0xFF0000);
        draw_char('X', btn_x + 5, btn_y + 5, 0xFFFFFF);

        if (in_editor) {
            int max_lines = (h - 60) / 10;
            draw_rect_filled(x, y + h - 25, w, 25, 0x004400);
            char status[120];
            snprintf(status, 120, "%s | Ln %d/%d Col %d | ^Q=Save | Move: Arrows, Home, End", 
                     edit_filename, edit_current_line + 1, edit_line_count, edit_cursor_col + 1);
            draw_string(status, x + 5, y + h - 18, 0xFFFFFF);
            
            for (int i = 0; i < max_lines && (edit_scroll_offset + i) < edit_line_count; i++) {
                uint32_t line_color = (edit_scroll_offset + i == edit_current_line) ? 0xFFFFFF : 0xDDDDDD;
                draw_string(edit_lines[edit_scroll_offset + i], x + 5, y + 30 + i * 10, line_color);
                
                if (edit_scroll_offset + i == edit_current_line) {
                    int cursor_x = x + 5 + edit_cursor_col * 8;
                    int cursor_y = y + 30 + i * 10;
                    draw_rect_filled(cursor_x, cursor_y, 2, 8, 0x00FF00);
                }
            }
        } else {
            int max_lines_shown = (h - 40) / 10;
            int start = (line_count > max_lines_shown) ? line_count - max_lines_shown : 0;
            for (int i = 0; i < max_lines_shown && (start + i) < line_count; ++i) {
                draw_string(buffer[start + i], x + 5, y + 30 + i * 10, 0xDDDDDD);
            }

            if (line_count > 0 && (line_count - 1 >= start)) {
                char temp_prompt[120];
                snprintf(temp_prompt, 120, "%s%s", buffer[line_count - 1], current_line);
                draw_string(temp_prompt, x + 5, y + 30 + (line_count - 1 - start) * 10, 0xFFFFFF);
                
                int cursor_x = x + 5 + (strlen(buffer[line_count - 1]) + line_pos) * 8;
                int cursor_y = y + 30 + (line_count - 1 - start) * 10;
                draw_rect_filled(cursor_x, cursor_y, 2, 8, 0x00FF00);
            }
        }
    }

    void update() override {}

    void on_key_press(char c) override {
        if (in_editor) {
            if (c == '\x11') { // Ctrl+Q: Save
                int total_size = 0;
                for (int i = 0; i < edit_line_count; i++) total_size += strlen(edit_lines[i]) + 1;
                
                char* file_content = new char[total_size + 1];
                char* ptr = file_content;
                for (int i = 0; i < edit_line_count; i++) {
                    int len = strlen(edit_lines[i]);
                    memcpy(ptr, edit_lines[i], len);
                    ptr += len;
                    *ptr++ = '\n';
                }
                *ptr = '\0';
                
                fat32_remove_file(edit_filename);
                if (fat32_write_file(edit_filename, file_content, total_size) == 0) {
                    console_print("File saved.\n");
                } else {
                    console_print("Error saving.\n");
                }
                delete[] file_content;
                
                for(int i = 0; i < edit_line_count; i++) delete[] edit_lines[i];
                delete[] edit_lines;
                edit_lines = nullptr;
                edit_line_count = 0;
                in_editor = false;
                print_prompt();
            } else if (c == KEY_HOME) { // Home key
                edit_cursor_col = 0;
            } else if (c == KEY_END) { // End key
                edit_cursor_col = strlen(edit_lines[edit_current_line]);
            } else if (c == KEY_LEFT) { // Left Arrow
                if (edit_cursor_col > 0) edit_cursor_col--;
            } else if (c == KEY_RIGHT) { // Right Arrow
                if (edit_cursor_col < (int)strlen(edit_lines[edit_current_line])) edit_cursor_col++;
            } else if (c == KEY_UP) { // Up Arrow
                if (edit_current_line > 0) {
                    edit_current_line--;
                    if (edit_cursor_col > (int)strlen(edit_lines[edit_current_line])) {
                        edit_cursor_col = strlen(edit_lines[edit_current_line]);
                    }
                    if (edit_current_line < edit_scroll_offset) edit_scroll_offset = edit_current_line;
                }
            } else if (c == KEY_DOWN) { // Down Arrow
                if (edit_current_line < edit_line_count - 1) {
                    edit_current_line++;
                    if (edit_cursor_col > (int)strlen(edit_lines[edit_current_line])) {
                        edit_cursor_col = strlen(edit_lines[edit_current_line]);
                    }
                    int max_lines = (h - 60) / 10;
                    if (edit_current_line >= edit_scroll_offset + max_lines) edit_scroll_offset = edit_current_line - max_lines + 1;
                }
            } else if (c == KEY_DELETE) { // Delete key
                char* line = edit_lines[edit_current_line];
                int len = strlen(line);
                if (edit_cursor_col < len) {
                    for (int i = edit_cursor_col; i < len; i++) line[i] = line[i + 1];
                }
            } else if (c == '\b') { // Backspace
                if (edit_cursor_col > 0) {
                    char* line = edit_lines[edit_current_line];
                    int len = strlen(line);
                    for (int i = edit_cursor_col - 1; i < len; i++) line[i] = line[i + 1];
                    edit_cursor_col--;
                } else if (edit_current_line > 0) {
                    int prev_len = strlen(edit_lines[edit_current_line - 1]);
                    int curr_len = strlen(edit_lines[edit_current_line]);
                    if (prev_len + curr_len < 119) {
                        strncat(edit_lines[edit_current_line - 1], edit_lines[edit_current_line], 119 - prev_len);
                        delete[] edit_lines[edit_current_line];
                        for (int i = edit_current_line; i < edit_line_count - 1; i++) {
                            edit_lines[i] = edit_lines[i + 1];
                        }
                        edit_line_count--;
                        edit_current_line--;
                        edit_cursor_col = prev_len;
                    }
                }
            } else if (c == '\n') { // Enter: Split line
                char* current = edit_lines[edit_current_line];
                char* new_line = new char[120];
                memset(new_line, 0, 120);
                strcpy(new_line, current + edit_cursor_col);
                current[edit_cursor_col] = '\0';
                
                char** new_lines = new char*[edit_line_count + 1];
                for (int i = 0; i <= edit_current_line; i++) new_lines[i] = edit_lines[i];
                new_lines[edit_current_line + 1] = new_line;
                for (int i = edit_current_line + 1; i < edit_line_count; i++) {
                    new_lines[i + 1] = edit_lines[i];
                }
                delete[] edit_lines;
                edit_lines = new_lines;
                edit_line_count++;
                edit_current_line++;
                edit_cursor_col = 0;
                
                int max_lines = (h - 60) / 10;
                if (edit_current_line >= edit_scroll_offset + max_lines) edit_scroll_offset = edit_current_line - max_lines + 1;
            } else if (c >= 32 && c < 127) { // Regular character
                char* line = edit_lines[edit_current_line];
                int len = strlen(line);
                if (len < 119) {
                    for (int i = len; i >= edit_cursor_col; i--) line[i + 1] = line[i];
                    line[edit_cursor_col] = c;
                    edit_cursor_col++;
                }
            }
        } else {
            if (c == '\b') { 
                if (line_pos > 0) current_line[--line_pos] = '\0'; 
            } else if (c == '\n') {
                if (line_count > 0) strncat(buffer[line_count-1], current_line, 119 - strlen(buffer[line_count-1]));
                handle_command();
                line_pos = 0;
                current_line[0] = '\0';
            } else if (line_pos < 119 && c >= 32 && c < 127) { 
                current_line[line_pos++] = c; 
                current_line[line_pos] = '\0'; 
            }
        }
    }
    
    void console_print(const char* s) override {
        const char* p = s;
        char line_buf[120];
        int lp = 0;
        while(*p) {
            if(*p == '\n') { line_buf[lp] = '\0'; push_line(line_buf); lp=0; }
            else if (lp < 119) line_buf[lp++] = *p;
            p++;
        }
        if(lp > 0) { line_buf[lp] = '\0'; push_line(line_buf); }
    }
};

// --- Implementations that depend on TerminalWindow class ---
void WindowManager::handle_input(char key, int mx, int my, bool left_down, bool left_clicked) {
    if (dragging_idx != -1) {
        if (left_down) { windows[dragging_idx]->x = mx - drag_offset_x; windows[dragging_idx]->y = my - drag_offset_y; }
        else dragging_idx = -1;
        return;
    }
    if (left_clicked) {
        for (int i = num_windows - 1; i >= 0; i--) { if (windows[i]->is_in_close_button(mx, my)) { windows[i]->close(); return; } }
        for (int i = num_windows - 1; i >= 0; i--) { if (windows[i]->is_in_titlebar(mx, my)) { set_focus(i); dragging_idx = focused_idx; drag_offset_x = mx - windows[dragging_idx]->x; drag_offset_y = my - windows[dragging_idx]->y; return; } }
        for (int i = num_windows - 1; i >= 0; i--) { if (mx >= windows[i]->x && mx < windows[i]->x + windows[i]->w && my >= windows[i]->y && my < windows[i]->y + windows[i]->h) { set_focus(i); return; } }
        if (mx >= 5 && mx <= 80 && my >= (int)fb_info.height - 35 && my <= (int)fb_info.height - 5) { launch_new_terminal(); return; }
    }
    if (key != 0 && focused_idx != -1 && focused_idx < num_windows) windows[focused_idx]->on_key_press(key);
}

void WindowManager::print_to_focused(const char* s) {
    if (focused_idx != -1 && focused_idx < num_windows) windows[focused_idx]->console_print(s);
}

void launch_new_terminal() {
    static int win_count = 0;
    wm.add_window(new TerminalWindow(100 + (win_count++ % 10) * 30, 50 + (win_count % 10) * 30));
}

// =============================================================================
// SECTION 4: KEYBOARD & MOUSE DRIVER
// =============================================================================
const char sc_ascii_nomod_map[]={0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,0,0,' ',0};
const char sc_ascii_shift_map[]={0,0,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,0,0,' ',0};
const char sc_ascii_ctrl_map[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,'\b','\t','\x11',0,0,0,0,0,0,0,0,'\x10',0,0,'\n',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,' ',0};
bool is_shift_pressed = false;
bool is_ctrl_pressed = false;
int mouse_x = 400, mouse_y = 300;
bool mouse_left_down = false;
bool mouse_left_last_frame = false;
char last_key_press = 0;

void poll_input() {
    last_key_press = 0;
    bool new_mouse_state = mouse_left_down;
    while (inb(0x64) & 1) {
        uint8_t status = inb(0x64);
        uint8_t scancode = inb(0x60);
        if (status & 0x20) {
            static uint8_t mouse_cycle = 0;
            static int8_t mouse_packet[3];
            mouse_packet[mouse_cycle++] = scancode;

            if (mouse_cycle == 3) {
                mouse_cycle = 0;
                new_mouse_state = mouse_packet[0] & 0x01;

                // Correctly calculate signed deltas
                int delta_x = mouse_packet[1];
                int delta_y = mouse_packet[2];

                if (mouse_packet[0] & 0x10) { // X sign bit is set
                    delta_x |= 0xFFFFFF00; // Sign extend to a negative 32-bit int
                }
                if (mouse_packet[0] & 0x20) { // Y sign bit is set
                    delta_y |= 0xFFFFFF00; // Sign extend to a negative 32-bit int
                }

                // Y movement is inverted in graphics coordinates
                mouse_x += delta_x;
                mouse_y -= delta_y;

                // Clamp coordinates to screen bounds
                if (mouse_x < 0) mouse_x = 0;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_x >= (int)fb_info.width) mouse_x = fb_info.width - 1;
                if (mouse_y >= (int)fb_info.height) mouse_y = fb_info.height - 1;
            }
        } else {
            bool is_press = !(scancode & 0x80);
            if (!is_press) scancode -= 0x80;
            if (scancode == 0x2A || scancode == 0x36) { is_shift_pressed = is_press; }
            if (scancode == 0x1D) { is_ctrl_pressed = is_press; }
            
            if (is_press) {
                if (scancode == 0x48) { last_key_press = KEY_UP; }
                else if (scancode == 0x50) { last_key_press = KEY_DOWN; }
                else if (scancode == 0x4B) { last_key_press = KEY_LEFT; }
                else if (scancode == 0x4D) { last_key_press = KEY_RIGHT; }
                else if (scancode == 0x53) { last_key_press = KEY_DELETE; }
                else if (scancode == 0x47) { last_key_press = KEY_HOME; }
                else if (scancode == 0x4F) { last_key_press = KEY_END; }
                else {
                    const char* map = is_ctrl_pressed ? sc_ascii_ctrl_map : (is_shift_pressed ? sc_ascii_shift_map : sc_ascii_nomod_map);
                    if (scancode < sizeof(sc_ascii_nomod_map) && map[scancode] != 0) last_key_press = map[scancode];
                }
            }
        }
    }
    mouse_left_last_frame = mouse_left_down;
    mouse_left_down = new_mouse_state;
}
void draw_cursor(int x, int y, uint32_t color) { for(int i=0;i<12;i++) put_pixel_back(x,y+i,color); for(int i=0;i<8;i++) put_pixel_back(x+i,y+i,color); for(int i=0;i<4;i++) put_pixel_back(x+i,y+(11-i),color); }

// =============================================================================
// SECTION 5: DISK DRIVER & FAT32 FILESYSTEM
// =============================================================================
#define SATA_SIG_ATA 0x00000101
#define PORT_CMD_ST 0x00000001
#define PORT_CMD_FRE 0x00000010
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define HBA_PORT_CMD_CR 0x00008000
#define TFD_STS_BSY 0x80
#define TFD_STS_DRQ 0x08
#define FIS_TYPE_REG_H2D 0x27
#define SECTOR_SIZE 512
#define DELETED_ENTRY 0xE5
#define ATTR_LONG_NAME 0x0F
#define ATTR_VOLUME_ID 0x08
#define ATTR_ARCHIVE 0x20
#define FAT_FREE_CLUSTER 0x00000000
#define FAT_END_OF_CHAIN 0x0FFFFFFF

typedef volatile struct {
    uint32_t clb;         // 0x00, command list base address, 1K-byte aligned
    uint32_t clbu;        // 0x04, command list base address upper 32 bits
    uint32_t fb;          // 0x08, FIS base address, 256-byte aligned
    uint32_t fbu;         // 0x0C, FIS base address upper 32 bits
    uint32_t is;          // 0x10, interrupt status
    uint32_t ie;          // 0x14, interrupt enable
    uint32_t cmd;         // 0x18, command and status
    uint32_t rsv0;        // 0x1C, Reserved
    uint32_t tfd;         // 0x20, task file data
    uint32_t sig;         // 0x24, signature
    uint32_t ssts;        // 0x28, SATA status (SCR0:SStatus)
    uint32_t sctl;        // 0x2C, SATA control (SCR2:SControl)
    uint32_t serr;        // 0x30, SATA error (SCR1:SError)
    uint32_t sact;        // 0x34, SATA active (SCR3:SActive)
    uint32_t ci;          // 0x38, command issue
    uint32_t sntf;        // 0x3C, SATA notification (SCR4:SNotification)
    uint32_t fbs;         // 0x40, FIS-based switching control
    uint32_t rsv1[11];    // 0x44 ~ 0x6F, Reserved
    uint32_t vendor[4];   // 0x70 ~ 0x7F, vendor specific
} HBA_PORT;

typedef volatile struct {
    uint32_t cap;         // 0x00, Host capability
    uint32_t ghc;         // 0x04, Global host control
    uint32_t is;          // 0x08, Interrupt status
    uint32_t pi;          // 0x0C, Port implemented
    uint32_t vs;          // 0x10, Version
    uint32_t ccc_ctl;     // 0x14, Command completion coalescing control
    uint32_t ccc_pts;     // 0x18, Command completion coalescing ports
    uint32_t em_loc;      // 0x1C, Enclosure management location
    uint32_t em_ctl;      // 0x20, Enclosure management control
    uint32_t cap2;        // 0x24, Host capabilities extended
    uint32_t bohc;        // 0x28, BIOS/OS handoff control and status
    uint8_t  rsv[0x60-0x2C];
    uint8_t  vendor[0x90-0x60]; // Vendor specific registers
    HBA_PORT ports[1];    // 0x90 ~ HBA memory mapped space, 1 ~ 32 ports
} HBA_MEM;


typedef struct { 
    uint8_t order; 
    uint16_t name1[5]; 
    uint8_t attr; 
    uint8_t type; 
    uint8_t checksum; 
    uint16_t name2[6]; 
    uint16_t fst_clus_lo; 
    uint16_t name3[2]; 
} __attribute__((packed)) fat_lfn_entry_t;

uint8_t lfn_checksum(const unsigned char *p_fname) {
    uint8_t sum = 0;
    for (int i = 11; i; i--) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *p_fname++;
    }
    return sum;
}
static int g_ahci_port = -1; // Will store the first active port number
typedef struct { uint8_t cfl:5, a:1, w:1, p:1, r:1, b:1, c:1, res0:1; uint16_t prdtl; volatile uint32_t prdbc; uint64_t ctba; uint32_t res1[4]; } __attribute__((packed)) HBA_CMD_HEADER;
typedef struct { uint64_t dba; uint32_t res0; uint32_t dbc:22, res1:9, i:1; } __attribute__((packed)) HBA_PRDT_ENTRY;
typedef struct { uint8_t fis_type, pmport:4, res0:3, c:1, command, featurel; uint8_t lba0, lba1, lba2, device; uint8_t lba3, lba4, lba5, featureh; uint8_t countl, counth, icc, control; uint8_t res1[4]; } __attribute__((packed)) FIS_REG_H2D;
typedef struct { uint8_t jmp[3]; char oem[8]; uint16_t bytes_per_sec; uint8_t sec_per_clus; uint16_t rsvd_sec_cnt; uint8_t num_fats; uint16_t root_ent_cnt; uint16_t tot_sec16; uint8_t media; uint16_t fat_sz16; uint16_t sec_per_trk; uint16_t num_heads; uint32_t hidd_sec; uint32_t tot_sec32; uint32_t fat_sz32; uint16_t ext_flags; uint16_t fs_ver; uint32_t root_clus; uint16_t fs_info; uint16_t bk_boot_sec; uint8_t res[12]; uint8_t drv_num; uint8_t res1; uint8_t boot_sig; uint32_t vol_id; char vol_lab[11]; char fil_sys_type[8]; } __attribute__((packed)) fat32_bpb_t;
typedef struct { char name[11]; uint8_t attr; uint8_t ntres; uint8_t crt_time_tenth; uint16_t crt_time, crt_date, lst_acc_date, fst_clus_hi; uint16_t wrt_time, wrt_date, fst_clus_lo; uint32_t file_size; } __attribute__((packed)) fat_dir_entry_t;

static uint64_t ahci_base = 0;
static HBA_CMD_HEADER* cmd_list;
static char* cmd_table_buffer;
static fat32_bpb_t bpb;
static uint32_t fat_start_sector, data_start_sector;
static uint32_t current_directory_cluster = 0;

// --- Aligned Memory Allocator ---
void* alloc_aligned(size_t size, size_t alignment) {
    size_t offset = alignment - 1 + sizeof(void*);
    void* p1 = operator new(size + offset);
    if (p1 == nullptr) return nullptr;
    void** p2 = (void**)(((uintptr_t)p1 + offset) & ~(alignment - 1));
    p2[-1] = p1;
    return p2;
}

void free_aligned(void* ptr) {
    if (ptr == nullptr) return;
    operator delete(((void**)ptr)[-1]);
}


int read_write_sectors(int port_num, uint64_t lba, uint16_t count, bool write, void* buffer) {
    if (port_num == -1 || !ahci_base) return -1;

    HBA_PORT* port = (HBA_PORT*)(ahci_base + 0x100 + (port_num * 0x80));
    
    port->is = 0xFFFFFFFF; // Clear any pending interrupt flags

    // Find a free command slot
    uint32_t slots = (port->sact | port->ci);
    int slot = -1;
    for (int i=0; i<32; i++) {
        if ((slots & (1 << i)) == 0) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;

    HBA_CMD_HEADER* cmd_header = &cmd_list[slot];
    cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd_header->w = write;
    cmd_header->prdtl = 1;

    uintptr_t cmd_table_addr = (uintptr_t)cmd_header->ctba;
    FIS_REG_H2D* cmd_fis = (FIS_REG_H2D*)(cmd_table_addr);
    HBA_PRDT_ENTRY* prdt = (HBA_PRDT_ENTRY*)(cmd_table_addr + 128);
    
    prdt->dba = (uint64_t)(uintptr_t)buffer;
    prdt->dbc = (count * SECTOR_SIZE) - 1;
    
    prdt->i = 0;

    // Configure the command FIS
    memset(cmd_fis, 0, sizeof(FIS_REG_H2D));
    cmd_fis->fis_type = FIS_TYPE_REG_H2D;
    cmd_fis->c = 1;
    cmd_fis->command = write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    
    cmd_fis->lba0 = (uint8_t)lba; cmd_fis->lba1 = (uint8_t)(lba >> 8); cmd_fis->lba2 = (uint8_t)(lba >> 16);
    cmd_fis->device = 1 << 6;
    cmd_fis->lba3 = (uint8_t)(lba >> 24); cmd_fis->lba4 = (uint8_t)(lba >> 32); cmd_fis->lba5 = (uint8_t)(lba >> 40);
    cmd_fis->countl = count & 0xFF; cmd_fis->counth = (count >> 8) & 0xFF;

    // Wait for the port to not be busy
    while((port->tfd & (TFD_STS_BSY | TFD_STS_DRQ)));

    // Issue the command
    port->ci = (1 << slot);

    int spin = 0;
    while (spin < 1000000) {
        if ((port->ci & (1 << slot)) == 0) {
             break; // Command completed successfully
        }
        spin++;
    }
    
    // Check if the loop timed out
    if (spin == 1000000) {
        return -1; // Timeout error
    }

    if (port->is & (1 << 30)) {
        return -1; // The drive reported a task file error
    }
    
    return 0; // Success
}
void stop_cmd(HBA_PORT *port) {
    port->cmd &= ~0x0001; // Clear ST (Start)
    port->cmd &= ~0x0010; // Clear FRE (FIS Receive Enable)

    // Wait until Command List Running (CR) and FIS Receive Running (FR) are cleared
    while(port->cmd & 0x8000 || port->cmd & 0x4000);
}

// Helper to start a port's command engine
void start_cmd(HBA_PORT *port) {
    // Wait until Command List Running (CR) is cleared
    while(port->cmd & 0x8000);

    port->cmd |= 0x0010; // Set FRE (FIS Receive Enable)
    port->cmd |= 0x0001; // Set ST (Start)
}
void disk_init() {
    // Find the AHCI controller's base address
    for (uint16_t bus = 0; bus < 256; bus++) for (uint8_t dev = 0; dev < 32; dev++) if ((pci_read_config_dword(bus, dev, 0, 0) & 0xFFFF) != 0xFFFF && (pci_read_config_dword(bus, dev, 0, 0x08) >> 16) == 0x0106) { ahci_base = pci_read_config_dword(bus, dev, 0, 0x24) & 0xFFFFFFF0; goto found; }
found:
    if (!ahci_base) return;

    cmd_list = (HBA_CMD_HEADER*)alloc_aligned(32 * sizeof(HBA_CMD_HEADER), 1024);
    cmd_table_buffer = (char*)alloc_aligned(32 * 256, 128);
    char* fis_buffer = (char*)alloc_aligned(256, 256);
    
    if (!cmd_list || !cmd_table_buffer || !fis_buffer) return;

    for(int k=0; k<32; ++k) {
        cmd_list[k].ctba = (uint64_t)(uintptr_t)(cmd_table_buffer + (k * 256));
    }

    uint32_t ports_implemented = *(volatile uint32_t*)(ahci_base + 0x0C);

    for (int i = 0; i < 32; i++) {
        if (ports_implemented & (1 << i)) {
            HBA_PORT* port = (HBA_PORT*)(ahci_base + 0x100 + (i * 0x80));

            uint8_t ipm = (port->ssts >> 8) & 0x0F;
            uint8_t det = port->ssts & 0x0F;
            if (det != 3 || ipm != 1) continue;

            stop_cmd(port);

            port->clb = (uint32_t)(uintptr_t)cmd_list;
            port->clbu = (uint32_t)(((uint64_t)(uintptr_t)cmd_list) >> 32);
            port->fb = (uint32_t)(uintptr_t)fis_buffer;
            port->fbu = (uint32_t)(((uint64_t)(uintptr_t)fis_buffer) >> 32);

            port->serr = 0xFFFFFFFF;

            start_cmd(port);

            g_ahci_port = i;
            return;
        }
    }
}
bool fat32_init() {
    if(!ahci_base) return false;
    char* buffer = new char[SECTOR_SIZE];
    if (read_write_sectors(0, 0, 1, false, buffer) != 0) { delete[] buffer; return false; }
    memcpy(&bpb, buffer, sizeof(bpb));
    delete[] buffer;
    if (strncmp(bpb.fil_sys_type, "FAT32", 5) != 0) { current_directory_cluster = 0; return false; }
    fat_start_sector = bpb.rsvd_sec_cnt;
    data_start_sector = fat_start_sector + (bpb.num_fats * bpb.fat_sz32);
    current_directory_cluster = bpb.root_clus;
    return true;
}
uint64_t cluster_to_lba(uint32_t cluster) { return data_start_sector + (cluster - 2) * bpb.sec_per_clus; }
void to_83_format(const char* filename, char* out) { memset(out, ' ', 11); int i = 0, j = 0; while (filename[i] && filename[i] != '.' && j < 8) { out[j++] = (filename[i] >= 'a' && filename[i] <= 'z') ? (filename[i]-32) : filename[i]; i++; } if(filename[i] == '.') i++; j=8; while(filename[i] && j<11) { out[j++] = (filename[i] >= 'a' && filename[i] <= 'z') ? (filename[i]-32) : filename[i]; i++; } }
void from_83_format(const char* fat_name, char* out) {
    int i, j = 0;
    // Process the name part (before the extension)
    for (i = 0; i < 8 && fat_name[i] != ' '; i++) {
        // Only convert uppercase letters to lowercase
        out[j++] = (fat_name[i] >= 'A' && fat_name[i] <= 'Z') ? fat_name[i] + 32 : fat_name[i];
    }
    
    // Process the extension part, if it exists
    if (fat_name[8] != ' ') {
        out[j++] = '.';
        for (i = 8; i < 11 && fat_name[i] != ' '; i++) {
            // Only convert uppercase letters to lowercase
            out[j++] = (fat_name[i] >= 'A' && fat_name[i] <= 'Z') ? fat_name[i] + 32 : fat_name[i];
        }
    }
    out[j] = '\0';
}
uint32_t read_fat_entry(uint32_t cluster) {
    static uint8_t fat_sector[SECTOR_SIZE]; // Use a single, reusable static buffer
    uint32_t fat_offset = cluster * 4;
    read_write_sectors(0, fat_start_sector + (fat_offset / SECTOR_SIZE), 1, false, fat_sector);
    uint32_t value = *(uint32_t*)(fat_sector + (fat_offset % SECTOR_SIZE)) & 0x0FFFFFFF;
    return value;
}

bool write_fat_entry(uint32_t cluster, uint32_t value) {
    uint8_t* fat_sector = new uint8_t[SECTOR_SIZE];
    uint32_t fat_offset = cluster * 4;
    uint32_t sector_num = fat_start_sector + (fat_offset / SECTOR_SIZE);
    read_write_sectors(0, sector_num, 1, false, fat_sector);
    *(uint32_t*)(fat_sector + (fat_offset % SECTOR_SIZE)) = (*(uint32_t*)(fat_sector + (fat_offset % SECTOR_SIZE)) & 0xF0000000) | (value & 0x0FFFFFFF);
    bool success = read_write_sectors(0, sector_num, 1, true, fat_sector) == 0;
    delete[] fat_sector;
    return success;
}

uint32_t find_free_cluster() {
    uint32_t max_clusters = (bpb.tot_sec32 - data_start_sector) / bpb.sec_per_clus + 2;
    for (uint32_t i = 2; i < max_clusters; i++) if (read_fat_entry(i) == FAT_FREE_CLUSTER) return i;
    return 0;
}

uint32_t allocate_cluster() {
    uint32_t free_cluster = find_free_cluster();
    if (free_cluster != 0) write_fat_entry(free_cluster, FAT_END_OF_CHAIN);
    return free_cluster;
}

void free_cluster_chain(uint32_t start_cluster) {
    uint32_t current = start_cluster;
    while(current < FAT_END_OF_CHAIN) { uint32_t next = read_fat_entry(current); write_fat_entry(current, FAT_FREE_CLUSTER); current = next; }
}

uint32_t allocate_cluster_chain(uint32_t num_clusters) {
    if(num_clusters == 0) return 0;
    uint32_t first = allocate_cluster();
    if(first == 0) return 0;
    uint32_t current = first;
    for(uint32_t i = 1; i < num_clusters; i++) {
        uint32_t next = allocate_cluster();
        if(next == 0) { free_cluster_chain(first); return 0; }
        write_fat_entry(current, next);
        current = next;
    }
    return first;
}

bool read_data_from_clusters(uint32_t start_cluster, void* data, uint32_t size) {
    if (size == 0) return true;
    uint8_t* data_ptr = (uint8_t*)data;
    uint32_t remaining = size;
    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size = bpb.sec_per_clus * SECTOR_SIZE;

    while (current_cluster >= 2 && current_cluster < FAT_END_OF_CHAIN && remaining > 0) {
        uint32_t to_read = (remaining > cluster_size) ? cluster_size : remaining;
        uint8_t* cluster_buf = new uint8_t[cluster_size];
        memset(cluster_buf, 0, cluster_size); // Clear buffer
        if(read_write_sectors(0, cluster_to_lba(current_cluster), bpb.sec_per_clus, false, cluster_buf) != 0) { 
            delete[] cluster_buf; 
            return false; 
        }
        memcpy(data_ptr, cluster_buf, to_read);
        delete[] cluster_buf;
        data_ptr += to_read;
        remaining -= to_read;
        if (remaining > 0) current_cluster = read_fat_entry(current_cluster);
        else break;
    }
    return true;
}

bool write_data_to_clusters(uint32_t start_cluster, const void* data, uint32_t size) {
    if (size == 0) return true;
    const uint8_t* data_ptr = (const uint8_t*)data;
    uint32_t remaining = size;
    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size = bpb.sec_per_clus * SECTOR_SIZE;
    uint8_t* cluster_buf = new uint8_t[cluster_size];

    while (current_cluster >= 2 && current_cluster < FAT_END_OF_CHAIN && remaining > 0) {
        uint32_t to_write = (remaining > cluster_size) ? cluster_size : remaining;
        memset(cluster_buf, 0, cluster_size);
        memcpy(cluster_buf, data_ptr, to_write);
        if (read_write_sectors(0, cluster_to_lba(current_cluster), bpb.sec_per_clus, true, cluster_buf) != 0) { 
            delete[] cluster_buf; 
            return false; 
        }
        data_ptr += to_write;
        remaining -= to_write;
        if (remaining > 0) current_cluster = read_fat_entry(current_cluster);
        else break;
    }
    delete[] cluster_buf;
    return true;
}

uint32_t clusters_needed(uint32_t size) {
    if (bpb.sec_per_clus == 0) return 0;
    uint32_t cluster_size = bpb.sec_per_clus * SECTOR_SIZE;
    return (size + cluster_size - 1) / cluster_size;
}

void fat32_list_files() {
    if (!ahci_base || !current_directory_cluster) {
        wm.print_to_focused("Filesystem not ready.\n");
        return;
    }
    uint8_t* buffer = new uint8_t[bpb.sec_per_clus * SECTOR_SIZE];
    if (read_write_sectors(0, cluster_to_lba(current_directory_cluster), bpb.sec_per_clus, false, buffer) != 0) {
        wm.print_to_focused("Read error\n");
        delete[] buffer;
        return;
    }

    wm.print_to_focused("Name                           Size\n");
    char lfn_buf[256] = {0};

    for (uint32_t i = 0; i < (bpb.sec_per_clus * SECTOR_SIZE); i += sizeof(fat_dir_entry_t)) {
        fat_dir_entry_t* entry = (fat_dir_entry_t*)(buffer + i);

        if (entry->name[0] == 0x00) break;
        if ((uint8_t)entry->name[0] == DELETED_ENTRY) {
            lfn_buf[0] = '\0';
            continue;
        }
        if (entry->name[0] == '.') continue;

        if (entry->attr == ATTR_LONG_NAME) {
            fat_lfn_entry_t* lfn = (fat_lfn_entry_t*)entry;
            if (lfn->order & 0x40) lfn_buf[0] = '\0';

            char name_part[14] = {0};
            int k = 0;
            auto extract = [&](uint16_t val) {
                if (k < 13 && val != 0x0000 && val != 0xFFFF) name_part[k++] = (char)val;
            };
            for(int j=0; j<5; j++) extract(lfn->name1[j]);
            for(int j=0; j<6; j++) extract(lfn->name2[j]);
            for(int j=0; j<2; j++) extract(lfn->name3[j]);

            memmove(lfn_buf + k, lfn_buf, strlen(lfn_buf) + 1);
            memcpy(lfn_buf, name_part, k);

        } else if (!(entry->attr & ATTR_VOLUME_ID)) {
            char line[120];
            char fname_83[13];
            const char* name_to_print;

            if (lfn_buf[0] != '\0') {
                name_to_print = lfn_buf;
            } else {
                from_83_format(entry->name, fname_83);
                name_to_print = fname_83;
            }

            // Manually copy and pad the filename to 30 characters
            int name_len = strlen(name_to_print);
            int copy_len = (name_len > 30) ? 30 : name_len;
            memcpy(line, name_to_print, copy_len);
            for (int k = copy_len; k < 30; ++k) {
                line[k] = ' ';
            }
            line[30] = '\0'; // Terminate after the padded name

            // Use a simple snprintf for just the size
            snprintf(line + 30, 90, " %d\n", entry->file_size);
            
            wm.print_to_focused(line);
            lfn_buf[0] = '\0'; // Reset for next entry
        }
    }
    delete[] buffer;
}
int fat32_write_file(const char* filename, const void* data, uint32_t size) {
    // First, safely remove the file if it already exists to handle overwrites correctly.
    fat32_remove_file(filename);

    char target_83[11];
    to_83_format(filename, target_83);
    uint32_t first_cluster = 0;

    if (size > 0) {
        uint32_t num_clusters = clusters_needed(size);
        if (num_clusters == 0) return -1;
        
        first_cluster = allocate_cluster_chain(num_clusters);
        if (first_cluster == 0) return -1; // Out of space
        if (!write_data_to_clusters(first_cluster, data, size)) {
            free_cluster_chain(first_cluster);
            return -1; // Write error
        }
    }

    uint8_t* dir_buf = new uint8_t[SECTOR_SIZE];
    for (uint8_t s = 0; s < bpb.sec_per_clus; s++) {
        uint64_t sector_lba = cluster_to_lba(current_directory_cluster) + s;
        if (read_write_sectors(0, sector_lba, 1, false, dir_buf) != 0) continue;

        for (uint16_t e = 0; e < SECTOR_SIZE / sizeof(fat_dir_entry_t); e++) {
            fat_dir_entry_t* entry = (fat_dir_entry_t*)(dir_buf + e * sizeof(fat_dir_entry_t));
            if (entry->name[0] == 0x00 || (uint8_t)entry->name[0] == DELETED_ENTRY) {
                // Found a free slot, create the entry.
                memset(entry, 0, sizeof(fat_dir_entry_t));
                memcpy(entry->name, target_83, 11);
                entry->attr = ATTR_ARCHIVE;
                entry->file_size = size;
                entry->fst_clus_lo = first_cluster & 0xFFFF;
                entry->fst_clus_hi = (first_cluster >> 16) & 0xFFFF;
                
                if (read_write_sectors(0, sector_lba, 1, true, dir_buf) == 0) {
                    delete[] dir_buf;
                    return 0; // Success
                } else {
                    delete[] dir_buf;
                    if(first_cluster > 0) free_cluster_chain(first_cluster);
                    return -1; // Directory write error
                }
            }
        }
    }

    delete[] dir_buf;
    if (first_cluster > 0) free_cluster_chain(first_cluster);
    return -1; // Directory is full
}

char* fat32_read_file_as_string(const char* filename) {
    char target[11]; to_83_format(filename, target);
    uint8_t* dir_buf = new uint8_t[SECTOR_SIZE];
    for (uint8_t s = 0; s < bpb.sec_per_clus; s++) {
        if (read_write_sectors(0, cluster_to_lba(current_directory_cluster) + s, 1, false, dir_buf) != 0) { delete[] dir_buf; return nullptr; }
        for (uint16_t e = 0; e < SECTOR_SIZE / sizeof(fat_dir_entry_t); e++) {
            fat_dir_entry_t* entry = (fat_dir_entry_t*)(dir_buf + e * sizeof(fat_dir_entry_t));
            if (entry->name[0] == 0x00) { delete[] dir_buf; return nullptr; }
            if (memcmp(entry->name, target, 11) == 0) {
                uint32_t size = entry->file_size;
                if(size == 0) { delete[] dir_buf; char* empty = new char[1]; empty[0] = '\0'; return empty; }
                char* data = new char[size + 1];
                if (read_data_from_clusters((entry->fst_clus_hi << 16) | entry->fst_clus_lo, data, size)) {
                    data[size] = '\0';
                    delete[] dir_buf;
                    return data;
                }
                delete[] data; delete[] dir_buf; return nullptr;
            }
        }
    }
    delete[] dir_buf; return nullptr;
}

int fat32_find_entry(const char* filename, fat_dir_entry_t* entry_out, uint32_t* sector_out, uint32_t* offset_out) {
    char lfn_buf[256] = {0};
    uint8_t current_checksum = 0;
    
    uint8_t* dir_buf = new uint8_t[SECTOR_SIZE];
    for(uint8_t s=0; s<bpb.sec_per_clus; ++s) {
        uint32_t current_sector = cluster_to_lba(current_directory_cluster) + s;
        if(read_write_sectors(0, current_sector, 1, false, dir_buf) != 0) { 
            delete[] dir_buf; 
            return -1; 
        }
        
        for(uint16_t e=0; e < SECTOR_SIZE / sizeof(fat_dir_entry_t); ++e) {
            fat_dir_entry_t* entry = (fat_dir_entry_t*)(dir_buf + e*sizeof(fat_dir_entry_t));
            if(entry->name[0] == 0x00) { delete[] dir_buf; return -1; }
            if((uint8_t)entry->name[0] == DELETED_ENTRY) { lfn_buf[0] = '\0'; continue; }

            if(entry->attr == ATTR_LONG_NAME) {
                fat_lfn_entry_t* lfn = (fat_lfn_entry_t*)entry;
                if (lfn->order & 0x40) { 
                    lfn_buf[0] = '\0'; 
                    current_checksum = lfn->checksum; 
                }
                
                char name_part[14] = {0};
                int k = 0;
                auto extract = [&](uint16_t val) {
                    if (k < 13 && val != 0x0000 && val != 0xFFFF) name_part[k++] = (char)val;
                };
                for(int j=0; j<5; j++) extract(lfn->name1[j]);
                for(int j=0; j<6; j++) extract(lfn->name2[j]);
                for(int j=0; j<2; j++) extract(lfn->name3[j]);

                memmove(lfn_buf + k, lfn_buf, strlen(lfn_buf) + 1);
                memcpy(lfn_buf, name_part, k);

            } else if (!(entry->attr & ATTR_VOLUME_ID)) {
                bool match = false;
                if(lfn_buf[0] != '\0' && lfn_checksum((unsigned char*)entry->name) == current_checksum) {
                    if(strcmp(lfn_buf, filename) == 0) match = true;
                } else {
                    char sfn_name[13]; 
                    from_83_format(entry->name, sfn_name);
                    if(strcmp(sfn_name, filename) == 0) match = true;
                }

                lfn_buf[0] = '\0';

                if(match) {
                    memcpy(entry_out, entry, sizeof(fat_dir_entry_t));
                    *sector_out = current_sector;
                    *offset_out = e * sizeof(fat_dir_entry_t);
                    delete[] dir_buf;
                    return 0;
                }
            }
        }
    }
    delete[] dir_buf;
    return -1;
}

int fat32_remove_file(const char* filename) {
    fat_dir_entry_t entry;
    uint32_t sector, offset;
    if(fat32_find_entry(filename, &entry, &sector, &offset) != 0) return -1;
    uint32_t start_cluster = (entry.fst_clus_hi << 16) | entry.fst_clus_lo;
    if(start_cluster != 0) free_cluster_chain(start_cluster);
    
    uint8_t* dir_buf = new uint8_t[SECTOR_SIZE];
    read_write_sectors(0, sector, 1, false, dir_buf);
    ((fat_dir_entry_t*)(dir_buf + offset))->name[0] = DELETED_ENTRY;
    read_write_sectors(0, sector, 1, true, dir_buf);
    delete[] dir_buf;
    return 0;
}

int fat32_rename_file(const char* old_name, const char* new_name) {
    fat_dir_entry_t entry;
    uint32_t sector, offset;
    fat_dir_entry_t dummy_entry;
    uint32_t dummy_sector, dummy_offset;

    // 1. Check if new_name already exists. If so, fail.
    if (fat32_find_entry(new_name, &dummy_entry, &dummy_sector, &dummy_offset) == 0) {
        return -1; // Destination file already exists
    }

    // 2. Find the old file. If it doesn't exist, fail.
    if (fat32_find_entry(old_name, &entry, &sector, &offset) != 0) {
        return -1; // Source file not found
    }
    
    // 3. Read, modify, and write back the directory sector.
    uint8_t* dir_buf = new uint8_t[SECTOR_SIZE];
    if (read_write_sectors(0, sector, 1, false, dir_buf) != 0) {
        delete[] dir_buf;
        return -1;
    }

    fat_dir_entry_t* target_entry = (fat_dir_entry_t*)(dir_buf + offset);
    to_83_format(new_name, target_entry->name);
    
    if (read_write_sectors(0, sector, 1, true, dir_buf) != 0) {
        delete[] dir_buf;
        return -1;
    }

    delete[] dir_buf;
    return 0; // Success
}

void fat32_format() {
    if(!ahci_base) {
        wm.print_to_focused("AHCI disk not found. Cannot format.\n");
        return;
    }
    wm.print_to_focused("WARNING: This is a destructive operation!\nFormatting disk...\n");

    fat32_bpb_t new_bpb;
    memset(&new_bpb, 0, sizeof(fat32_bpb_t));
    new_bpb.jmp[0] = 0xEB; new_bpb.jmp[1] = 0x58; new_bpb.jmp[2] = 0x90;
    memcpy(new_bpb.oem, "MYOS    ", 8);
    new_bpb.bytes_per_sec = 512;
    new_bpb.sec_per_clus = 8;
    new_bpb.rsvd_sec_cnt = 32;
    new_bpb.num_fats = 2;
    new_bpb.media = 0xF8;
    new_bpb.sec_per_trk = 32;
    new_bpb.num_heads = 64;
    uint32_t total_sectors = (128 * 1024 * 1024) / 512;
    new_bpb.tot_sec32 = total_sectors;
    new_bpb.fat_sz32 = (total_sectors * 2) / (new_bpb.sec_per_clus + 512) + 1;
    new_bpb.root_clus = 2;
    new_bpb.fs_info = 1;
    new_bpb.bk_boot_sec = 6;
    new_bpb.drv_num = 0x80;
    new_bpb.boot_sig = 0x29;
    new_bpb.vol_id = 0x12345678; // Example volume ID
    memcpy(new_bpb.vol_lab, "MYOS VOL   ", 11);
    memcpy(new_bpb.fil_sys_type, "FAT32   ", 8);
    
    wm.print_to_focused("Writing new boot sector...\n");
    char* boot_sector_buffer = new char[SECTOR_SIZE];
    memset(boot_sector_buffer, 0, SECTOR_SIZE);
    memcpy(boot_sector_buffer, &new_bpb, sizeof(fat32_bpb_t));
    boot_sector_buffer[510] = 0x55;
    boot_sector_buffer[511] = 0xAA;
	boot_sector_buffer[510] = 0x00;
    boot_sector_buffer[511] = 0x00;
    
    if (read_write_sectors(0, 0, 1, true, boot_sector_buffer) != 0) {
        wm.print_to_focused("Error: Failed to write new boot sector.\n");
        delete[] boot_sector_buffer;
        return;
    }
    delete[] boot_sector_buffer;

    memcpy(&bpb, &new_bpb, sizeof(fat32_bpb_t));
    fat_start_sector = bpb.rsvd_sec_cnt;
    data_start_sector = fat_start_sector + (bpb.num_fats * bpb.fat_sz32);

    uint8_t* zero_sector = new uint8_t[SECTOR_SIZE];
    memset(zero_sector, 0, SECTOR_SIZE);
    wm.print_to_focused("Clearing FATs...\n");
    for (uint32_t i = 0; i < bpb.fat_sz32; ++i) {
        read_write_sectors(0, fat_start_sector + i, 1, true, zero_sector); // FAT1
        read_write_sectors(0, fat_start_sector + bpb.fat_sz32 + i, 1, true, zero_sector); // FAT2
    }

    wm.print_to_focused("Clearing root directory...\n");
    for (uint8_t i = 0; i < bpb.sec_per_clus; ++i) {
        read_write_sectors(0, cluster_to_lba(bpb.root_clus) + i, 1, true, zero_sector);
    }
    delete[] zero_sector;

    wm.print_to_focused("Writing initial FAT entries...\n");
    write_fat_entry(0, 0x0FFFFFF8); // Media descriptor
    write_fat_entry(1, 0x0FFFFFFF); // Reserved, EOC
    write_fat_entry(bpb.root_clus, 0x0FFFFFFF); // Root directory cluster EOC

    wm.print_to_focused("Format complete. Re-initializing filesystem...\n");
    if (fat32_init()) {
        wm.print_to_focused("FAT32 FS re-initialized successfully.\n");
    } else {
        wm.print_to_focused("FAT32 FS re-initialization failed.\n");
    }
}

// =============================================================================
// SECTION 5.5: FILESYSTEM CHECK DISK UTILITY
// =============================================================================
#define ATTR_DIRECTORY 0x10
// --- CHKDSK Helper Function ---
// --- CHKDSK Helper Function ---
static void scan_directory_recursive(uint32_t current_cluster, uint8_t* status_map, uint32_t max_cluster, int* file_count, int* dir_count, uint32_t* cross_linked_errors, uint8_t* shared_dir_buf) {
    if (current_cluster == 0) return;

    // Define status values used within this check
    const uint8_t CLUSTER_STATUS_REFERENCED = 3;

    uint32_t cluster_chain_node = current_cluster;
    
    // Mark the directory's own cluster chain as referenced
    uint32_t temp_node = cluster_chain_node;
    while(temp_node < 0x0FFFFFF7 && temp_node >= 2) {
        if (status_map[temp_node] == CLUSTER_STATUS_REFERENCED) {
            (*cross_linked_errors)++;
            break; // Already processed this chain, it's cross-linked.
        }
        status_map[temp_node] = CLUSTER_STATUS_REFERENCED;
        temp_node = read_fat_entry(temp_node);
    }
    // Mark the final cluster in the chain if it's valid
    if (temp_node >= 2 && temp_node <= max_cluster && status_map[temp_node] != CLUSTER_STATUS_REFERENCED) {
         status_map[temp_node] = CLUSTER_STATUS_REFERENCED;
    }

    while (cluster_chain_node < 0x0FFFFFF7 && cluster_chain_node >= 2) {
        uint32_t cluster_size_bytes = bpb.sec_per_clus * SECTOR_SIZE;
        // Use the shared buffer instead of allocating a new one
        if (read_write_sectors(0, cluster_to_lba(cluster_chain_node), bpb.sec_per_clus, false, shared_dir_buf) != 0) {
            break; // Stop if read fails
        }

        for (uint32_t i = 0; i < cluster_size_bytes; i += sizeof(fat_dir_entry_t)) {
            fat_dir_entry_t* entry = (fat_dir_entry_t*)(shared_dir_buf + i);
            if (entry->name[0] == 0x00) { // End of directory marker
                cluster_chain_node = 0; // Force exit from outer while loop
                break; // Exit inner for loop
            }
            if ((uint8_t)entry->name[0] == DELETED_ENTRY) continue;
            if (entry->attr == ATTR_LONG_NAME || (entry->attr & ATTR_VOLUME_ID)) continue;
            if (entry->name[0] == '.') continue; // Skip '.' and '..'

            uint32_t start_cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
            
            if (entry->attr & ATTR_DIRECTORY) {
                (*dir_count)++;
                scan_directory_recursive(start_cluster, status_map, max_cluster, file_count, dir_count, cross_linked_errors, shared_dir_buf);
            } else {
                (*file_count)++;
                if (start_cluster >= 2) {
                    uint32_t file_cluster = start_cluster;
                    while (file_cluster < 0x0FFFFFF7 && file_cluster >= 2) {
                        if (file_cluster > max_cluster) break;
                        if (status_map[file_cluster] == CLUSTER_STATUS_REFERENCED) {
                            (*cross_linked_errors)++;
                            break;
                        }
                         status_map[file_cluster] = CLUSTER_STATUS_REFERENCED;
                        file_cluster = read_fat_entry(file_cluster);
                    }
                }
            }
        }
        if (cluster_chain_node != 0) {
             cluster_chain_node = read_fat_entry(cluster_chain_node);
        }
    }
}


// --- Main CHKDSK Function ---
void fat32_check_disk() {
    if (!current_directory_cluster) {
        wm.print_to_focused("Filesystem not initialized. Cannot run chkdsk.\n");
        return;
    }

    wm.print_to_focused("Starting Check Disk...\n\n");
    char buf[128];
    uint32_t total_clusters = (bpb.tot_sec32 - data_start_sector) / bpb.sec_per_clus;
    uint32_t cluster_size_kb = (bpb.sec_per_clus * SECTOR_SIZE) / 1024;
    
    snprintf(buf, 128, "Volume size: %d MB\nTotal clusters: %d (%d KB each)\n\n", (bpb.tot_sec32 * SECTOR_SIZE) / (1024 * 1024), total_clusters, cluster_size_kb);
    wm.print_to_focused(buf);

    // --- Phase 1: Verify FAT ---
    wm.print_to_focused("Phase 1: Verifying File Allocation Table...\n");
    uint8_t* cluster_status = new uint8_t[total_clusters + 2];
    memset(cluster_status, 0, total_clusters + 2);

    const uint8_t CLUSTER_STATUS_FREE = 1;
    const uint8_t CLUSTER_STATUS_IN_USE = 2;
    const uint8_t CLUSTER_STATUS_BAD = 4;

    uint32_t used_clusters = 0, free_clusters = 0, bad_clusters = 0;
    for (uint32_t i = 2; i <= total_clusters; i++) {
        uint32_t entry = read_fat_entry(i);
        if (entry == FAT_FREE_CLUSTER) {
            cluster_status[i] = CLUSTER_STATUS_FREE;
            free_clusters++;
        } else if (entry >= 0x0FFFFFF7) {
            cluster_status[i] = (entry == 0x0FFFFFF7) ? CLUSTER_STATUS_BAD : CLUSTER_STATUS_IN_USE;
            if(entry == 0x0FFFFFF7) bad_clusters++; else used_clusters++;
        } else {
            cluster_status[i] = CLUSTER_STATUS_IN_USE;
            used_clusters++;
        }
    }
    snprintf(buf, 128, "  %d clusters marked as used.\n  %d clusters marked as free.\n  %d clusters marked as bad.\n\n", used_clusters, free_clusters, bad_clusters);
    wm.print_to_focused(buf);

    // --- Phase 2: Verify Directory Structure ---
    wm.print_to_focused("Phase 2: Verifying directory structure...\n");
    int file_count = 0, dir_count = 0;
    uint32_t cross_linked_errors = 0;
    // Allocate the shared directory buffer ONCE
    uint8_t* shared_dir_buf = new uint8_t[bpb.sec_per_clus * SECTOR_SIZE];
    scan_directory_recursive(bpb.root_clus, cluster_status, total_clusters, &file_count, &dir_count, &cross_linked_errors, shared_dir_buf);
    delete[] shared_dir_buf; // Free the shared buffer

    snprintf(buf, 128, "  Found %d files and %d directories.\n  Detected %d cross-linked chains.\n\n", file_count, dir_count, cross_linked_errors);
    wm.print_to_focused(buf);

    // --- Phase 3: Find Lost Clusters ---
    wm.print_to_focused("Phase 3: Looking for lost clusters...\n");
    uint32_t lost_clusters = 0;
    for (uint32_t i = 2; i <= total_clusters; i++) {
        if (cluster_status[i] == CLUSTER_STATUS_IN_USE) {
            lost_clusters++;
        }
    }
    snprintf(buf, 128, "  Found %d lost clusters.\n\n", lost_clusters);
    wm.print_to_focused(buf);

    // --- Summary ---
    wm.print_to_focused("Check Disk Summary:\n");
    uint32_t errors_found = cross_linked_errors + lost_clusters + bad_clusters;
    snprintf(buf, 128, "  %u KB total disk space.\n  %u KB in %d files.\n  %u KB in lost clusters.\n  %u KB available on disk.\n\n  %u total errors found.\n",
        (total_clusters + 2) * cluster_size_kb,
        (used_clusters - lost_clusters) * cluster_size_kb, file_count,
        lost_clusters * cluster_size_kb,
        free_clusters * cluster_size_kb,
        errors_found);
    wm.print_to_focused(buf);

    delete[] cluster_status;
    wm.print_to_focused("CHKDSK finished.\n");
}
// =============================================================================
// SECTION 6: SELF-HOSTED C COMPILER (ELABORATED)
// =============================================================================

// --- Forward Declarations ---
class C_Interpreter;
static void parse_statement(C_Interpreter* interp);

// --- Interpreter Data Types ---
enum ValueType { VAL_NIL, VAL_INT, VAL_STRING, VAL_ARRAY };

struct Value; // Forward declare

struct StringObject {
    char* chars;
    int length;
    StringObject* next; // For memory management
};

struct ArrayObject {
    Value* items;
    int count;
    ArrayObject* next; // For memory management
};

struct Value {
    ValueType type;
    union {
        int integer;
        StringObject* string;
        ArrayObject* array;
    } as;
};

// --- Tokenizer (Lexer) ---
enum TokenType {
    TOKEN_EOF, TOKEN_ERROR,
    // Keywords
    TOKEN_INT, TOKEN_STRING, TOKEN_PRINT, TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE,
    // Literals & Identifiers
    TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_STRING_LITERAL,
    // Single-char
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_SEMICOLON, TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
    TOKEN_ASSIGN, TOKEN_LESS, TOKEN_GREATER,
    // Two-char
    TOKEN_EQ, TOKEN_NEQ, TOKEN_LEQ, TOKEN_GEQ
};

struct Token {
    TokenType type;
    const char* start;
    int length;
    Value value;
};

// --- Interpreter State ---
#define MAX_SYMBOLS_PER_SCOPE 50
#define MAX_SCOPES 20

struct Symbol {
    Token name;
    Value value;
    int array_size; // 0 for non-arrays
};

struct Scope {
    Symbol symbols[MAX_SYMBOLS_PER_SCOPE];
    int symbol_count;
};

class C_Interpreter {
public:
    const char* source_start;
    const char* current_char;
    Token current_token;
    Token prev_token;

    Scope scope_stack[MAX_SCOPES];
    int current_scope_level;

    StringObject* strings_head = nullptr;
    ArrayObject* arrays_head = nullptr;
    
    TerminalWindow* terminal;

    C_Interpreter(const char* source, TerminalWindow* term) : 
        source_start(source), current_char(source), current_scope_level(-1), terminal(term) {}

    ~C_Interpreter() {
        StringObject* s = strings_head;
        while (s) {
            StringObject* next = s->next;
            delete[] s->chars;
            delete s;
            s = next;
        }
        ArrayObject* a = arrays_head;
        while (a) {
            ArrayObject* next = a->next;
            delete[] a->items;
            delete a;
            a = next;
        }
    }
};

static bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_at_end(C_Interpreter* interp) { return *interp->current_char == '\0'; }

// --- Memory Management for Interpreter ---
StringObject* new_string(C_Interpreter* interp, const char* chars, int length) {
    StringObject* str = new StringObject;
    str->chars = new char[length + 1];
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    str->length = length;
    str->next = interp->strings_head;
    interp->strings_head = str;
    return str;
}

ArrayObject* new_array(C_Interpreter* interp, int count) {
    ArrayObject* arr = new ArrayObject;
    arr->items = new Value[count];
    for(int i=0; i<count; ++i) arr->items[i].type = VAL_NIL;
    arr->count = count;
    arr->next = interp->arrays_head;
    interp->arrays_head = arr;
    return arr;
}

// --- Lexer Implementation ---
static void skip_whitespace(C_Interpreter* interp) {
    for (;;) {
        char c = *interp->current_char;
        if (c == ' ' || c == '\r' || c == '\t' || c == '\n') interp->current_char++;
        else return;
    }
}

static Token make_token(C_Interpreter* interp, TokenType type) {
    Token token;
    token.type = type;
    token.start = interp->source_start;
    token.length = (int)(interp->current_char - interp->source_start);
    token.value.type = VAL_NIL;
    return token;
}

static TokenType check_keyword(C_Interpreter* interp, int start, int length, const char* rest, TokenType type) {
    if (interp->current_char - interp->source_start == start + length && memcmp(interp->source_start + start, rest, length) == 0) return type;
    return TOKEN_IDENTIFIER;
}

static TokenType identifier_type(C_Interpreter* interp) {
    switch (interp->source_start[0]) {
        case 'e': return check_keyword(interp, 1, 3, "lse", TOKEN_ELSE);
        case 'i':
            if (interp->current_char - interp->source_start > 1) {
                switch (interp->source_start[1]) {
                    case 'f': return TOKEN_IF;
                    case 'n': return check_keyword(interp, 2, 1, "t", TOKEN_INT);
                }
            }
            break;
        case 'p': return check_keyword(interp, 1, 4, "rint", TOKEN_PRINT);
        case 's': return check_keyword(interp, 1, 5, "tring", TOKEN_STRING);
        case 'w': return check_keyword(interp, 1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

static Token string_literal(C_Interpreter* interp) {
    while (*interp->current_char != '"' && !is_at_end(interp)) interp->current_char++;
    if (is_at_end(interp)) return make_token(interp, TOKEN_ERROR);
    interp->current_char++; // Consume closing quote
    Token token = make_token(interp, TOKEN_STRING_LITERAL);
    token.value.type = VAL_STRING;
    token.value.as.string = new_string(interp, token.start + 1, token.length - 2);
    return token;
}

static Token scan_token(C_Interpreter* interp) {
    skip_whitespace(interp);
    interp->source_start = interp->current_char;
    if (is_at_end(interp)) return make_token(interp, TOKEN_EOF);

    char c = *interp->current_char++;
    if (is_alpha(c)) {
        while (is_alpha(*interp->current_char) || is_digit(*interp->current_char)) interp->current_char++;
        return make_token(interp, identifier_type(interp));
    }
    if (is_digit(c)) {
        while (is_digit(*interp->current_char)) interp->current_char++;
        Token t = make_token(interp, TOKEN_NUMBER);
        t.value.type = VAL_INT;
        t.value.as.integer = simple_atoi(t.start);
        return t;
    }
    switch (c) {
        case '(': return make_token(interp, TOKEN_LPAREN);
        case ')': return make_token(interp, TOKEN_RPAREN);
        case '{': return make_token(interp, TOKEN_LBRACE);
        case '}': return make_token(interp, TOKEN_RBRACE);
        case '[': return make_token(interp, TOKEN_LBRACKET);
        case ']': return make_token(interp, TOKEN_RBRACKET);
        case ';': return make_token(interp, TOKEN_SEMICOLON);
        case '+': return make_token(interp, TOKEN_PLUS);
        case '-': return make_token(interp, TOKEN_MINUS);
        case '*': return make_token(interp, TOKEN_STAR);
        case '/': return make_token(interp, TOKEN_SLASH);
        case '"': return string_literal(interp);
        case '=': return make_token(interp, *interp->current_char == '=' ? (interp->current_char++, TOKEN_EQ) : TOKEN_ASSIGN);
        case '!': return make_token(interp, *interp->current_char == '=' ? (interp->current_char++, TOKEN_NEQ) : TOKEN_ERROR);
        case '<': return make_token(interp, *interp->current_char == '=' ? (interp->current_char++, TOKEN_LEQ) : TOKEN_LESS);
        case '>': return make_token(interp, *interp->current_char == '=' ? (interp->current_char++, TOKEN_GEQ) : TOKEN_GREATER);
    }
    return make_token(interp, TOKEN_ERROR);
}

// --- Parser & Interpreter ---
static void advance(C_Interpreter* interp) {
    interp->prev_token = interp->current_token;
    interp->current_token = scan_token(interp);
}

static bool match(C_Interpreter* interp, TokenType type) {
    if (interp->current_token.type == type) {
        advance(interp);
        return true;
    }
    return false;
}

static void consume(C_Interpreter* interp, TokenType type, const char* msg) {
    if (interp->current_token.type == type) {
        advance(interp);
        return;
    }
    // In a real compiler, you'd report an error. Here we just advance.
    advance(interp);
}

// --- Scoping ---
static void enter_scope(C_Interpreter* interp) {
    if (interp->current_scope_level < MAX_SCOPES - 1) {
        interp->current_scope_level++;
        interp->scope_stack[interp->current_scope_level].symbol_count = 0;
    }
}

static void leave_scope(C_Interpreter* interp) {
    if (interp->current_scope_level >= 0) interp->current_scope_level--;
}

static bool identifiers_equal(Token* a, Token* b) {
    return (a->length == b->length && memcmp(a->start, b->start, a->length) == 0);
}

static Symbol* find_symbol(C_Interpreter* interp, Token name) {
    for (int i = interp->current_scope_level; i >= 0; i--) {
        Scope* scope = &interp->scope_stack[i];
        for (int j = 0; j < scope->symbol_count; j++) {
            if (identifiers_equal(&name, &scope->symbols[j].name)) {
                return &scope->symbols[j];
            }
        }
    }
    return nullptr;
}

static Symbol* add_symbol(C_Interpreter* interp, Token name) {
    Scope* scope = &interp->scope_stack[interp->current_scope_level];
    if (scope->symbol_count < MAX_SYMBOLS_PER_SCOPE) {
        Symbol* symbol = &scope->symbols[scope->symbol_count++];
        symbol->name = name;
        symbol->value.type = VAL_NIL;
        symbol->array_size = 0;
        return symbol;
    }
    return nullptr;
}

// --- Expression Parsing ---
static Value parse_expression(C_Interpreter* interp); // Forward declaration

static Value call_native_function(C_Interpreter* interp, Token name) {
    Value result; result.type = VAL_NIL;

    // Create named tokens to compare against
    Token get_mouse_x_tok = {.start="get_mouse_x", .length=11};
    Token get_mouse_y_tok = {.start="get_mouse_y", .length=11};
    Token get_time_str_tok = {.start="get_time_str", .length=12};
    Token string_input_tok = {.start="string_input", .length=12};
    Token int_to_hex_tok = {.start="int_to_hex", .length=10};

    if (identifiers_equal(&name, &get_mouse_x_tok)){
        consume(interp, TOKEN_LPAREN, "Expect '('."); consume(interp, TOKEN_RPAREN, "Expect ')'.");
        result.type = VAL_INT; result.as.integer = mouse_x;
    } else if (identifiers_equal(&name, &get_mouse_y_tok)){
        consume(interp, TOKEN_LPAREN, "Expect '('."); consume(interp, TOKEN_RPAREN, "Expect ')'.");
        result.type = VAL_INT; result.as.integer = mouse_y;
    } else if (identifiers_equal(&name, &get_time_str_tok)){
        consume(interp, TOKEN_LPAREN, "Expect '('."); consume(interp, TOKEN_RPAREN, "Expect ')'.");
        RTC_Time t = read_rtc(); char buf[64];
        snprintf(buf, 64, "%d:%d:%d %d/%d/%d", t.hour, t.minute, t.second, t.day, t.month, t.year);
        result.type = VAL_STRING; result.as.string = new_string(interp, buf, strlen(buf));
    } else if (identifiers_equal(&name, &string_input_tok)){
        consume(interp, TOKEN_LPAREN, "Expect '('."); consume(interp, TOKEN_RPAREN, "Expect ')'.");
        char input_buf[120] = {0}; int pos = 0;
        interp->terminal->console_print("\r> "); // Initial prompt
        while(true) {
            poll_input();
            if(last_key_press) {
                if (last_key_press == '\n') break;
                else if (last_key_press == '\b') { if(pos > 0) input_buf[--pos] = '\0'; }
                else if (pos < 118 && last_key_press >= 32) input_buf[pos++] = last_key_press;
                
                // Redraw the input line
                char full_prompt[128];
                snprintf(full_prompt, 128, "\r> %s ", input_buf); // Add space to overwrite old chars
                interp->terminal->console_print(full_prompt);
            }
        }
        interp->terminal->console_print("\n");
        result.type = VAL_STRING; result.as.string = new_string(interp, input_buf, pos);
    } else if (identifiers_equal(&name, &int_to_hex_tok)){
        consume(interp, TOKEN_LPAREN, "Expect '('.");
        Value arg = parse_expression(interp);
        consume(interp, TOKEN_RPAREN, "Expect ')'.");
        if (arg.type == VAL_INT) {
            char hex_buf[12];
            // Custom simple hex conversion to avoid complex snprintf formatters
            uint32_t n = arg.as.integer;
            char* p = hex_buf + 11;
            *p-- = '\0';
            if (n == 0) {
                *p-- = '0';
            } else {
                while(n > 0) {
                    uint8_t rem = n % 16;
                    *p-- = (rem < 10) ? (rem + '0') : (rem - 10 + 'A');
                    n /= 16;
                }
            }
            *p-- = 'x';
            *p = '0';
            result.type = VAL_STRING; result.as.string = new_string(interp, p, strlen(p));
        }
    } else { advance(interp); /* Unknown function */ }
    return result;
}

static Value parse_primary(C_Interpreter* interp) {
    if (match(interp, TOKEN_NUMBER)) return interp->prev_token.value;
    if (match(interp, TOKEN_STRING_LITERAL)) return interp->prev_token.value;
    
    if (match(interp, TOKEN_IDENTIFIER)) {
        Token id_token = interp->prev_token;
        if (interp->current_token.type == TOKEN_LPAREN) return call_native_function(interp, id_token);

        Symbol* sym = find_symbol(interp, id_token);
        if (!sym) return (Value){.type=VAL_NIL};

        if (match(interp, TOKEN_LBRACKET)) { // Array access
            Value index_val = parse_expression(interp);
            consume(interp, TOKEN_RBRACKET, "Expect ']' after index.");
            if (sym->value.type == VAL_ARRAY && index_val.type == VAL_INT) {
                if (index_val.as.integer >= 0 && index_val.as.integer < sym->value.as.array->count) {
                    return sym->value.as.array->items[index_val.as.integer];
                }
            }
            return (Value){.type=VAL_NIL}; // Index out of bounds or not an array
        }
        return sym->value;
    }

    if (match(interp, TOKEN_LPAREN)) {
        Value value = parse_expression(interp);
        consume(interp, TOKEN_RPAREN, "Expect ')' after expression.");
        return value;
    }
    advance(interp);
    return (Value){.type=VAL_NIL};
}

static Value parse_unary(C_Interpreter* interp) {
    if (match(interp, TOKEN_MINUS)) {
        Value val = parse_unary(interp);
        if (val.type == VAL_INT) val.as.integer = -val.as.integer;
        return val;
    }
    return parse_primary(interp);
}

static Value parse_multiplication(C_Interpreter* interp) {
    Value value = parse_unary(interp);
    while (match(interp, TOKEN_STAR) || match(interp, TOKEN_SLASH)) {
        TokenType op = interp->prev_token.type;
        Value right = parse_unary(interp);
        if (value.type == VAL_INT && right.type == VAL_INT) {
            if (op == TOKEN_STAR) value.as.integer *= right.as.integer;
            else if (op == TOKEN_SLASH) {
                if (right.as.integer != 0) value.as.integer /= right.as.integer; else value.as.integer = 0;
            }
        }
    }
    return value;
}

static Value parse_addition(C_Interpreter* interp) {
    Value value = parse_multiplication(interp);
    while (match(interp, TOKEN_PLUS) || match(interp, TOKEN_MINUS)) {
        TokenType op = interp->prev_token.type;
        Value right = parse_multiplication(interp);
        if (op == TOKEN_PLUS) {
            if (value.type == VAL_INT && right.type == VAL_INT) value.as.integer += right.as.integer;
            else if (value.type == VAL_STRING && right.type == VAL_STRING) {
                int new_len = value.as.string->length + right.as.string->length;
                char* new_chars = new char[new_len + 1];
                memcpy(new_chars, value.as.string->chars, value.as.string->length);
                memcpy(new_chars + value.as.string->length, right.as.string->chars, right.as.string->length);
                new_chars[new_len] = '\0';
                value.as.string = new_string(interp, new_chars, new_len);
                delete[] new_chars;
            }
        } else if (op == TOKEN_MINUS) {
            if (value.type == VAL_INT && right.type == VAL_INT) value.as.integer -= right.as.integer;
        }
    }
    return value;
}

static Value parse_comparison(C_Interpreter* interp) {
    Value value = parse_addition(interp);
    while (match(interp, TOKEN_EQ) || match(interp, TOKEN_NEQ) || match(interp, TOKEN_LESS) || match(interp, TOKEN_LEQ) || match(interp, TOKEN_GREATER) || match(interp, TOKEN_GEQ)) {
        TokenType op = interp->prev_token.type;
        Value right = parse_addition(interp);
        Value result; result.type = VAL_INT;
        if (value.type == VAL_INT && right.type == VAL_INT) {
            switch (op) {
                case TOKEN_EQ: result.as.integer = (value.as.integer == right.as.integer); break;
                case TOKEN_NEQ: result.as.integer = (value.as.integer != right.as.integer); break;
                case TOKEN_LESS: result.as.integer = (value.as.integer < right.as.integer); break;
                case TOKEN_LEQ: result.as.integer = (value.as.integer <= right.as.integer); break;
                case TOKEN_GREATER: result.as.integer = (value.as.integer > right.as.integer); break;
                case TOKEN_GEQ: result.as.integer = (value.as.integer >= right.as.integer); break;
                default: result.as.integer = 0; break;
            }
        } else { result.as.integer = 0; }
        value = result;
    }
    return value;
}

static Value parse_expression(C_Interpreter* interp) { return parse_comparison(interp); }

// --- Statement Parsing ---
static void skip_statement(C_Interpreter* interp) {
    if (match(interp, TOKEN_LBRACE)) {
        int brace_level = 1;
        while (brace_level > 0 && interp->current_token.type != TOKEN_EOF) {
            if (match(interp, TOKEN_LBRACE)) brace_level++;
            else if (match(interp, TOKEN_RBRACE)) brace_level--;
            else advance(interp);
        }
    } else {
        while (interp->current_token.type != TOKEN_SEMICOLON && interp->current_token.type != TOKEN_EOF) {
            advance(interp);
        }
        match(interp, TOKEN_SEMICOLON);
    }
}

static void parse_print_statement(C_Interpreter* interp) {
    consume(interp, TOKEN_LPAREN, "Expect '(' after print.");
    Value value = parse_expression(interp);
    consume(interp, TOKEN_RPAREN, "Expect ')' after value.");
    consume(interp, TOKEN_SEMICOLON, "Expect ';' after statement.");
    
    char buf[128];
    if (value.type == VAL_INT) snprintf(buf, 128, "%d\n", value.as.integer);
    else if (value.type == VAL_STRING) snprintf(buf, 128, "%s\n", value.as.string->chars);
    else snprintf(buf, 128, "(nil)\n");
    interp->terminal->console_print(buf);
}

static void parse_declaration(C_Interpreter* interp, TokenType type) {
    consume(interp, TOKEN_IDENTIFIER, "Expect variable name.");
    Token var_name = interp->prev_token;
    Symbol* sym = add_symbol(interp, var_name);

    if (match(interp, TOKEN_LBRACKET)) { // Array declaration
        consume(interp, TOKEN_NUMBER, "Expect array size.");
        int size = interp->prev_token.value.as.integer;
        consume(interp, TOKEN_RBRACKET, "Expect ']' after size.");
        if (sym && size > 0) {
            sym->value.type = VAL_ARRAY;
            sym->value.as.array = new_array(interp, size);
            sym->array_size = size;
        }
    } else if (match(interp, TOKEN_ASSIGN)) { // Regular var with initializer
        Value value = parse_expression(interp);
        if (sym) sym->value = value;
    }
    consume(interp, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
}

static void parse_assignment(C_Interpreter* interp, Token var_name) {
    Symbol* sym = find_symbol(interp, var_name);
    Value index_val;
    bool is_array_assign = false;

    if (match(interp, TOKEN_LBRACKET)) {
        is_array_assign = true;
        index_val = parse_expression(interp);
        consume(interp, TOKEN_RBRACKET, "Expect ']' after index.");
    }

    consume(interp, TOKEN_ASSIGN, "Expect '=' for assignment.");
    Value value = parse_expression(interp);
    consume(interp, TOKEN_SEMICOLON, "Expect ';' after value.");

    if (sym) {
        if (is_array_assign) {
            if (sym->value.type == VAL_ARRAY && index_val.type == VAL_INT) {
                int idx = index_val.as.integer;
                if (idx >= 0 && idx < sym->value.as.array->count) {
                    sym->value.as.array->items[idx] = value;
                }
            }
        } else {
            sym->value = value;
        }
    }
}

static void parse_block(C_Interpreter* interp) {
    enter_scope(interp);
    while (interp->current_token.type != TOKEN_RBRACE && interp->current_token.type != TOKEN_EOF) {
        parse_statement(interp);
    }
    consume(interp, TOKEN_RBRACE, "Expect '}' after block.");
    leave_scope(interp);
}

static void parse_if_statement(C_Interpreter* interp) {
    consume(interp, TOKEN_LPAREN, "Expect '(' after 'if'.");
    Value condition = parse_expression(interp);
    consume(interp, TOKEN_RPAREN, "Expect ')' after condition.");

    bool is_true = (condition.type == VAL_INT && condition.as.integer != 0);
    
    if (is_true) {
        parse_statement(interp);
        if (match(interp, TOKEN_ELSE)) {
            skip_statement(interp);
        }
    } else {
        skip_statement(interp);
        if (match(interp, TOKEN_ELSE)) {
            parse_statement(interp);
        }
    }
}

static void parse_while_statement(C_Interpreter* interp) {
    const char* loop_start = interp->current_char;
    consume(interp, TOKEN_LPAREN, "Expect '('.");
    
    while(true) {
        interp->current_char = loop_start;
        advance(interp); // Re-scan the condition part
        Value condition = parse_expression(interp);
        consume(interp, TOKEN_RPAREN, "Expect ')'.");

        bool is_true = (condition.type == VAL_INT && condition.as.integer != 0);
        
        if (is_true) {
            parse_statement(interp);
        } else {
            skip_statement(interp);
            break;
        }
    }
}

static void parse_statement(C_Interpreter* interp) {
    if (match(interp, TOKEN_PRINT)) { parse_print_statement(interp); }
    else if (match(interp, TOKEN_INT)) { parse_declaration(interp, TOKEN_INT); }
    else if (match(interp, TOKEN_STRING)) { parse_declaration(interp, TOKEN_STRING); }
    else if (match(interp, TOKEN_IF)) { parse_if_statement(interp); }
    else if (match(interp, TOKEN_WHILE)) { parse_while_statement(interp); }
    else if (match(interp, TOKEN_LBRACE)) { parse_block(interp); }
    else if (match(interp, TOKEN_IDENTIFIER)) { parse_assignment(interp, interp->prev_token); } 
    else { advance(interp); }
}

void interpret_c(const char* source, TerminalWindow* term) {
    C_Interpreter interp(source, term);
    
    enter_scope(&interp); // Global scope
    advance(&interp);
    while (!match(&interp, TOKEN_EOF)) {
        parse_statement(&interp);
    }
    leave_scope(&interp);
}

// --- Command parsing helper ---
char* get_arg(char* args, int n) {
    char* p = args;
    char* arg_start = nullptr;
    
    for (int i = 0; i <= n; i++) {
        while (*p && (*p == ' ' || *p == '\t')) p++;
        if (*p == '\0') return nullptr;

        arg_start = p;
        if (*p == '"') {
            arg_start++; 
            p++;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0'; 
        } else {
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    return arg_start;
}

// --- Terminal command handler ---
void TerminalWindow::handle_command() {
    char cmd_line[120];
    strncpy(cmd_line, current_line, 119);
    cmd_line[119] = '\0';

    char* command = cmd_line;
    while (*command && *command == ' ') command++;
    if (*command == '\0') {
        if (!in_editor) print_prompt();
        return;
    }
    char* args = command;
    while (*args && *args != ' ') args++;
    if (*args) {
        *args = '\0';
        args++;
        while (*args && *args == ' ') args++;
    }
    if (strcmp(command, "help") == 0) { console_print("Commands: help, clear, ls, edit, run, rm, cp, mv, formatfs,chkdsk, time, version\n"); }
    else if (strcmp(command, "clear") == 0) { line_count = 0; memset(buffer, 0, sizeof(buffer)); }
    else if (strcmp(command, "ls") == 0) { fat32_list_files(); }
    else if (strcmp(command, "chkdsk") == 0) { fat32_check_disk(); } // NEW
	else if (strcmp(command, "edit") == 0) {
        char* filename = get_arg(args, 0);
        if(filename) {
            strncpy(edit_filename, filename, 31);
            edit_filename[31] = '\0';
            in_editor = true;
            edit_current_line = 0;
            edit_cursor_col = 0;
            edit_scroll_offset = 0;
            char* content = fat32_read_file_as_string(filename);
            if (content) {
                int line_count_temp = 1;
                for (char* p = content; *p; p++) if (*p == '\n') line_count_temp++;
                
                edit_lines = new char*[line_count_temp];
                edit_line_count = 0;
                
                char* line_start = content;
                for (char* p = content; *p; p++) {
                    if (*p == '\n') {
                        *p = '\0';
                        edit_lines[edit_line_count] = new char[120];
                        memset(edit_lines[edit_line_count], 0, 120);
                        strncpy(edit_lines[edit_line_count], line_start, 119);
                        edit_line_count++;
                        line_start = p + 1;
                    }
                }
                if (*line_start) {
                    edit_lines[edit_line_count] = new char[120];
                    memset(edit_lines[edit_line_count], 0, 120);
                    strncpy(edit_lines[edit_line_count], line_start, 119);
                    edit_line_count++;
                }
                delete[] content;
            } else {
                edit_lines = new char*[1];
                edit_lines[0] = new char[120];
                memset(edit_lines[0], 0, 120);
                edit_line_count = 1;
            }
        } else {
            console_print("Usage: edit \"<filename>\"\n");
        }
    }
    else if (strcmp(command, "run") == 0) { 
        char* filename = get_arg(args, 0); 
        if(filename) { 
            char* s = fat32_read_file_as_string(filename); 
            if(s) { 
                interpret_c(s, this); // Pass 'this' to the interpreter
                delete[] s; 
            } else { 
                console_print("File not found.\n"); 
            }
        } else { 
            console_print("Usage: run \"<filename>\"\n");
        }
    }
    else if (strcmp(command, "rm") == 0) { 
        char* filename = get_arg(args, 0); 
        if(filename) { 
            if(fat32_remove_file(filename) == 0) 
                console_print("File removed.\n"); 
            else 
                console_print("Failed to remove file.\n");
        } else { 
            console_print("Usage: rm \"<filename>\"\n");
        }
    }
    else if (strcmp(command, "cp") == 0) {
        char args_copy1[120]; strncpy(args_copy1, args, 119);
        char args_copy2[120]; strncpy(args_copy2, args, 119);
        char* src = get_arg(args_copy1, 0);
        char* dest = get_arg(args_copy2, 1);
        
        if(!src || !dest) { 
            console_print("Usage: cp \"<source>\" \"<dest>\"\n"); 
        } else {
            fat_dir_entry_t entry;
            uint32_t sector, offset;
            if (fat32_find_entry(src, &entry, &sector, &offset) == 0) {
                char* content = new char[entry.file_size];
                if (content && read_data_from_clusters((entry.fst_clus_hi << 16) | entry.fst_clus_lo, content, entry.file_size)) {
                    if(fat32_write_file(dest, content, entry.file_size) == 0) {
                        console_print("Copied.\n");
                    } else {
                        console_print("Write failed.\n");
                    }
                } else {
                    console_print("Read failed.\n");
                }
                if (content) delete[] content;
            } else {
                console_print("Source not found.\n");
            }
        }
    }
    else if (strcmp(command, "mv") == 0) {
        char args_copy1[120]; strncpy(args_copy1, args, 119);
        char args_copy2[120]; strncpy(args_copy2, args, 119);
        char* src = get_arg(args_copy1, 0);
        char* dest = get_arg(args_copy2, 1);

        if(!src || !dest) { 
            console_print("Usage: mv \"<source>\" \"<dest>\"\n"); 
        } else {
            if(fat32_rename_file(src, dest) == 0) {
                console_print("Moved.\n");
            } else {
                console_print("Failed. (Source not found or destination exists).\n");
            }
        }
    }
    else if (strcmp(command, "formatfs") == 0) { fat32_format(); }
    else if (strcmp(command, "time") == 0) { 
        RTC_Time t = read_rtc(); 
        char buf[64]; 
        snprintf(buf, 64, "%d:%d:%d %d/%d/%d\n", t.hour, t.minute, t.second, t.day, t.month, t.year); 
        console_print(buf); 
    }
    else if (strcmp(command, "version") == 0) { console_print("RTOS++ v1.2 - Advanced C Interpreter\n"); }
    else if (strlen(command) > 0) { 
        console_print("Unknown command.\n"); 
    }
    
    if(!in_editor) print_prompt();
}
// =============================================================================
// SECTION 7: KERNEL MAIN
// =============================================================================
extern "C" void kernel_main(uint32_t magic, uint32_t multiboot_addr) {
    multiboot_info* mbi = (multiboot_info*)multiboot_addr;
    if (!(mbi->flags & (1 << 12))) return;

    fb_info = { (uint32_t*)(uint64_t)mbi->framebuffer_addr, mbi->framebuffer_width, mbi->framebuffer_height, mbi->framebuffer_pitch };
    backbuffer = new uint32_t[fb_info.width * fb_info.height];
    
    outb(0x64, 0xA8); outb(0x64, 0x20);
    uint8_t status = (inb(0x60) | 2) & ~0x20;
    outb(0x64, 0x60); outb(0x60, status);
    outb(0x64, 0xD4); outb(0x60, 0xF6); inb(0x60);
    outb(0x64, 0xD4); outb(0x60, 0xF4); inb(0x60);

    disk_init();
    if(ahci_base) fat32_init();
    
    launch_new_terminal();

    if(ahci_base) wm.print_to_focused("AHCI disk found.\n"); else wm.print_to_focused("AHCI disk NOT found.\n");
    if(current_directory_cluster) wm.print_to_focused("FAT32 FS initialized.\n"); else wm.print_to_focused("FAT32 init failed. Use 'formatfs' to create filesystem.\n");

    while (true) {
        wm.cleanup_closed_windows();
        poll_input();
        bool mouse_clicked_this_frame = mouse_left_down && !mouse_left_last_frame;
        wm.handle_input(last_key_press, mouse_x, mouse_y, mouse_left_down, mouse_clicked_this_frame);
        draw_rect_filled(0, 0, fb_info.width, fb_info.height, 0x000000); // Clear backbuffer
        wm.update_all();
        draw_cursor(mouse_x, mouse_y, 0xFFFFFF);
        swap_buffers();
    }
}


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


    // Public getter methods for private members
    int get_num_windows() const { return num_windows; }
    int get_focused_idx() const { return focused_idx; }
    Window* get_window(int idx) { 
        if (idx >= 0 && idx < num_windows) return windows[idx];
        return nullptr;
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
    char buffer[38][120];
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
    uint32_t bordercolor = has_focus ? 0xFFFFFF : 0x888888;
    uint32_t titlecolor = has_focus ? 0x0000AA : 0x555555;
    
    // Draw window frame and title bar
    draw_rect_filled(x, y, w, h, bordercolor);
    draw_rect_filled(x + 2, y + 2, w - 4, h - 4, 0x000033);
    draw_rect_filled(x, y, w, 25, titlecolor);
    draw_string(title, x + 5, y + 8, 0xFFFFFF);
    
    int btnx = x + w - 22, btny = y + 4;
    draw_rect_filled(btnx, btny, 18, 18, 0xFF0000);
    draw_char('X', btnx + 5, btny + 5, 0xFFFFFF);
    
    if (in_editor) {
        // Editor mode - existing code with corrected variable names
        int maxlines = (h - 50) / 10;
        int maxcharsperline = (w - 10) / 8;
        
        draw_rect_filled(x, y + h - 25, w, 25, 0x004400);
        char status[120];
        snprintf(status, 120, "%s | Ln %d/%d | Col %d | ^Q=Save | Move: Arrows, Home, End", 
                 edit_filename, edit_current_line + 1, edit_line_count, edit_cursor_col + 1);
        draw_string(status, x + 5, y + h - 18, 0xFFFFFF);
        
        // Existing editor rendering code...
        int visualline = 0;
        for (int i = 0; i < edit_line_count && visualline < maxlines; i++) {
            if (i < edit_scroll_offset) continue;
            
            uint32_t linecolor = (i == edit_current_line) ? 0xFFFFFF : 0xDDDDDD;
            const char* line = edit_lines[i];
            int linelen = strlen(line);
            
            if (linelen == 0) {
                draw_string("", x + 5, y + 30 + visualline * 10, linecolor);
                if (i == edit_current_line && edit_cursor_col == 0) {
                    int cursorx = x + 5;
                    int cursory = y + 30 + visualline * 10;
                    draw_rect_filled(cursorx, cursory, 2, 8, 0x00FF00);
                }
                visualline++;
                continue;
            }
            
            // Word-wrap editor lines
            int numwrappedlines = (linelen + maxcharsperline - 1) / maxcharsperline;
            for (int wrapidx = 0; wrapidx < numwrappedlines; wrapidx++) {
                if (visualline >= maxlines) break;
                
                int offset = wrapidx * maxcharsperline;
                int charstodraw = linelen - offset;
                if (charstodraw > maxcharsperline) charstodraw = maxcharsperline;
                if (charstodraw < 0) charstodraw = 0;
                
                char wrappedline[120];
                strncpy(wrappedline, line + offset, charstodraw);
                wrappedline[charstodraw] = 0;
                
                draw_string(wrappedline, x + 5, y + 30 + visualline * 10, linecolor);
                
                // Draw cursor if on this wrapped segment
                if (i == edit_current_line) {
                    int cursoroffsetinline = edit_cursor_col - offset;
                    if (cursoroffsetinline >= 0 && cursoroffsetinline <= charstodraw) {
                        int cursorx = x + 5 + (cursoroffsetinline * 8);
                        int cursory = y + 30 + visualline * 10;
                        draw_rect_filled(cursorx, cursory, 2, 8, 0x00FF00);
                    }
                }
                visualline++;
            }
        }
    } else {
    // Console mode with improved word wrapping
    int maxlinesshown = (h - 40) / 10;
    int maxcharsperline = (w - 10) / 8;
    int start = (line_count > maxlinesshown) ? (line_count - maxlinesshown) : 0;
    
    int visualline = 0;
    
    // Render buffered output lines with word wrapping
    for (int i = start; i < line_count - 1 && visualline < maxlinesshown; i++) {
        const char* line = buffer[i];
        int linelen = strlen(line);
        
        if (linelen == 0) {
            // Empty line takes one visual line
            visualline++;
            continue;
        }
        
        // Word-wrap this line across multiple visual lines if needed
        int offset = 0;
        while (offset < linelen && visualline < maxlinesshown) {
            int charstodraw = linelen - offset;
            if (charstodraw > maxcharsperline) {
                charstodraw = maxcharsperline;
            }
            
            char wrappedline[120];
            strncpy(wrappedline, line + offset, charstodraw);
            wrappedline[charstodraw] = 0;
            
            draw_string(wrappedline, x + 5, y + 30 + visualline * 10, 0xDDDDDD);
            
            offset += charstodraw;
            visualline++;
        }
    }
    
    // Render current input line with prompt (only if there's space)
    if (line_count > 0 && visualline < maxlinesshown) {
        const int WRAPWIDTH = maxcharsperline;
        
        // Combine prompt and current typed line
        char fullinputline[240];
        snprintf(fullinputline, 240, "%s%s", buffer[line_count - 1], current_line);
        
        int promptlen = strlen(buffer[line_count - 1]);
        int totallen = strlen(fullinputline);
        
        // Draw the wrapped input line
        int inputstartline = visualline;
        
        for (int i = 0; i < totallen && visualline < maxlinesshown; i += WRAPWIDTH) {
            char segment[WRAPWIDTH + 1];
            int lentocopy = (totallen - i > WRAPWIDTH) ? WRAPWIDTH : (totallen - i);
            strncpy(segment, fullinputline + i, lentocopy);
            segment[lentocopy] = 0;
            
            int currentliney = y + 30 + visualline * 10;
            draw_string(segment, x + 5, currentliney, 0xFFFFFF);
            visualline++;
        }
        
        // Calculate and draw cursor position
        int cursorabspos = promptlen + line_pos;
        int cursorlineoffset = cursorabspos / WRAPWIDTH;
        int cursorcoloffset = cursorabspos % WRAPWIDTH;
        
        int cursorx = x + 5 + (cursorcoloffset * 8);
        int cursory = y + 30 + (inputstartline + cursorlineoffset) * 10;
        
        // Only draw cursor if it's within visible area
        if (inputstartline + cursorlineoffset < maxlinesshown) {
            draw_rect_filled(cursorx, cursory, 2, 8, 0x00FF00);
        }
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
// =============================================================================
// IMPROVED PS/2 MOUSE INITIALIZATION AND HANDLING
// =============================================================================

// Add these constants near the other keyboard/mouse definitions
#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

// PS/2 Controller Commands
#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_DISABLE_PORT2   0xA7
#define PS2_CMD_ENABLE_PORT2    0xA8
#define PS2_CMD_TEST_PORT2      0xA9
#define PS2_CMD_WRITE_PORT2     0xD4

// PS/2 Mouse Commands
#define MOUSE_CMD_RESET         0xFF
#define MOUSE_CMD_ENABLE_DATA   0xF4
#define MOUSE_CMD_SET_DEFAULTS  0xF6
#define MOUSE_CMD_SET_SAMPLE    0xF3

// Status register bits
#define PS2_STATUS_OUTPUT_FULL  0x01
#define PS2_STATUS_INPUT_FULL   0x02
#define PS2_STATUS_TIMEOUT      0x40

// Improved PS/2 wait functions with timeout
static bool ps2_wait_input(int timeout = 100000) {
    while (timeout--) {
        if (!(inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL)) {
            return true;
        }
    }
    return false;
}

static bool ps2_wait_output(int timeout = 100000) {
    while (timeout--) {
        if (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
            return true;
        }
    }
    return false;
}

// Send command to PS/2 controller
static void ps2_write_command(uint8_t cmd) {
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, cmd);
}

// Write data to PS/2 controller
static void ps2_write_data(uint8_t data) {
    ps2_wait_input();
    outb(PS2_DATA_PORT, data);
}

// Read data from PS/2 controller
static uint8_t ps2_read_data() {
    ps2_wait_output();
    return inb(PS2_DATA_PORT);
}

// Send command to mouse (port 2)
static bool ps2_mouse_write(uint8_t cmd) {
    ps2_write_command(PS2_CMD_WRITE_PORT2);
    ps2_write_data(cmd);
    
    // Wait for ACK (0xFA)
    if (ps2_wait_output(100000)) {
        uint8_t response = ps2_read_data();
        return response == 0xFA;
    }
    return false;
}

// Improved PS/2 mouse initialization
static bool init_ps2_mouse() {
    // 1. Disable both PS/2 ports
    ps2_write_command(0xAD); // Disable keyboard
    ps2_write_command(PS2_CMD_DISABLE_PORT2); // Disable mouse
    
    // 2. Flush output buffer
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
        inb(PS2_DATA_PORT);
    }
    
    // 3. Read and modify controller configuration
    ps2_write_command(PS2_CMD_READ_CONFIG);
    if (!ps2_wait_output()) return false;
    uint8_t config = ps2_read_data();
    
    // Enable interrupts and clock for both ports
    config |= 0x03;  // Enable port 1 and port 2 interrupts
    config &= ~0x30; // Enable both clocks
    
    ps2_write_command(PS2_CMD_WRITE_CONFIG);
    ps2_write_data(config);
    
    // 4. Enable second PS/2 port (mouse)
    ps2_write_command(PS2_CMD_ENABLE_PORT2);
    
    // 5. Test second port
    ps2_write_command(PS2_CMD_TEST_PORT2);
    if (!ps2_wait_output()) return false;
    uint8_t test_result = ps2_read_data();
    if (test_result != 0x00) {
        return false; // Port test failed
    }
    
    // 6. Reset mouse
    if (!ps2_mouse_write(MOUSE_CMD_RESET)) {
        return false;
    }
    
    // Wait for self-test result (0xAA = pass)
    if (ps2_wait_output(500000)) {
        uint8_t result = ps2_read_data();
        if (result != 0xAA) return false;
        
        // Read device ID (should be 0x00)
        if (ps2_wait_output()) {
            ps2_read_data();
        }
    }
    
    // 7. Set mouse defaults
    if (!ps2_mouse_write(MOUSE_CMD_SET_DEFAULTS)) {
        return false;
    }
    
    // 8. Set sample rate (optional, helps with some USB-PS/2 adapters)
    ps2_mouse_write(MOUSE_CMD_SET_SAMPLE);
    ps2_mouse_write(100); // 100 samples/sec
    
    // 9. Enable data reporting
    if (!ps2_mouse_write(MOUSE_CMD_ENABLE_DATA)) {
        return false;
    }
    
    // 10. Re-enable keyboard
    ps2_write_command(0xAE);
    
    return true;
}

// Improved mouse packet handling with better synchronization
struct MouseState {
    uint8_t cycle;
    int8_t packet[3];
    bool synchronized;
    int x;
    int y;
    bool left_button;
    bool right_button;
    bool middle_button;
};

static MouseState mouse_state = {0, {0, 0, 0}, false, 400, 300, false, false, false};

static void handle_mouse_packet() {
    if (!ps2_wait_output(1000)) return;
    
    uint8_t data = ps2_read_data();
    
    // If not synchronized, look for valid first byte
    if (!mouse_state.synchronized) {
        // First byte always has bit 3 set
        if (data & 0x08) {
            mouse_state.packet[0] = data;
            mouse_state.cycle = 1;
            mouse_state.synchronized = true;
        }
        return;
    }
    
    mouse_state.packet[mouse_state.cycle] = data;
    mouse_state.cycle++;
    
    if (mouse_state.cycle == 3) {
        mouse_state.cycle = 0;
        
        // Verify packet validity (bit 3 should be set in first byte)
        if (!(mouse_state.packet[0] & 0x08)) {
            mouse_state.synchronized = false;
            return;
        }
        
        // Extract button states
        mouse_state.left_button = mouse_state.packet[0] & 0x01;
        mouse_state.right_button = mouse_state.packet[0] & 0x02;
        mouse_state.middle_button = mouse_state.packet[0] & 0x04;
        
        // Extract movement with proper sign extension
        int dx = mouse_state.packet[1];
        int dy = mouse_state.packet[2];
        
        // Check for overflow flags
        if (mouse_state.packet[0] & 0x40) dx = 0; // X overflow
        if (mouse_state.packet[0] & 0x80) dy = 0; // Y overflow
        
        // Apply sign extension if negative movement flags are set
        if (mouse_state.packet[0] & 0x10) dx |= 0xFFFFFF00; // X sign
        if (mouse_state.packet[0] & 0x20) dy |= 0xFFFFFF00; // Y sign
        
        // Update position
        mouse_state.x += dx;
        mouse_state.y -= dy; // Y is inverted
        
        // Clamp to screen bounds
        if (mouse_state.x < 0) mouse_state.x = 0;
        if (mouse_state.y < 0) mouse_state.y = 0;
        if (mouse_state.x >= (int)fb_info.width) mouse_state.x = fb_info.width - 1;
        if (mouse_state.y >= (int)fb_info.height) mouse_state.y = fb_info.height - 1;
    }
}

// =============================================================================
// IMPROVED MOUSE PACKET HANDLING - FIXED VERSION
// =============================================================================

// Simpler, more reliable mouse packet handling
static void process_mouse_data(uint8_t data) {
    static uint8_t mouse_cycle = 0;
    static int8_t mouse_packet[3];
    
    mouse_packet[mouse_cycle] = data;
    mouse_cycle++;
    
    if (mouse_cycle == 3) {
        mouse_cycle = 0;
        
        // Parse the packet
        uint8_t flags = mouse_packet[0];
        
        // Validate: bit 3 must be set in first byte
        if (!(flags & 0x08)) {
            return; // Invalid packet, skip
        }
        
        // Extract button states
        mouse_state.left_button = flags & 0x01;
        mouse_state.right_button = flags & 0x02;
        mouse_state.middle_button = flags & 0x04;
        
        // Extract movement deltas
        int dx = mouse_packet[1];
        int dy = mouse_packet[2];
        
        // Handle sign extension for 9-bit values
        if (flags & 0x10) dx |= 0xFFFFFF00; // X is negative
        if (flags & 0x20) dy |= 0xFFFFFF00; // Y is negative
        
        // Check for overflow and ignore if present
        if (flags & 0x40) dx = 0; // X overflow
        if (flags & 0x80) dy = 0; // Y overflow
        
        // Update mouse position
        mouse_state.x += dx;
        mouse_state.y -= dy; // Y axis is inverted
        
        // Clamp to screen bounds
        if (mouse_state.x < 0) mouse_state.x = 0;
        if (mouse_state.y < 0) mouse_state.y = 0;
        if (mouse_state.x >= (int)fb_info.width) mouse_state.x = fb_info.width - 1;
        if (mouse_state.y >= (int)fb_info.height) mouse_state.y = fb_info.height - 1;
    }
}

// =============================================================================
// REPLACEMENT FOR poll_input() IN KERNEL MAIN
// =============================================================================

void poll_input() {
    last_key_press = 0;
    bool new_mouse_state = mouse_left_down;

    while (inb(0x64) & 1) {
        uint8_t status = inb(0x64);
        uint8_t data = inb(0x60);
        
        if (status & 0x20) {
            // Mouse data (bit 5 of status = 1)
            process_mouse_data(data);
            new_mouse_state = mouse_state.left_button;
        } else {
            // Keyboard data (bit 5 of status = 0)
            bool is_press = !(data & 0x80);
            if (!is_press) data -= 0x80;
            
            if (data == 0x2A || data == 0x36) { 
                is_shift_pressed = is_press; 
            } else if (data == 0x1D) { 
                is_ctrl_pressed = is_press; 
            } else if (is_press) {
                if (data == 0x48) last_key_press = KEY_UP;
                else if (data == 0x50) last_key_press = KEY_DOWN;
                else if (data == 0x4B) last_key_press = KEY_LEFT;
                else if (data == 0x4D) last_key_press = KEY_RIGHT;
                else if (data == 0x53) last_key_press = KEY_DELETE;
                else if (data == 0x47) last_key_press = KEY_HOME;
                else if (data == 0x4F) last_key_press = KEY_END;
                else {
                    const char* map = is_ctrl_pressed ? sc_ascii_ctrl_map : 
                                     (is_shift_pressed ? sc_ascii_shift_map : sc_ascii_nomod_map);
                    if (data < sizeof(sc_ascii_nomod_map) && map[data] != 0) {
                        last_key_press = map[data];
                    }
                }
            }
        }
    }
    
    // Update global mouse state
    mouse_x = mouse_state.x;
    mouse_y = mouse_state.y;
    mouse_left_last_frame = mouse_left_down;
    mouse_left_down = new_mouse_state;
}
// =============================================================================
// COMPLETE INTEGRATION GUIDE
// =============================================================================

/*
STEP 1: Add MouseState structure to your globals (in SECTION 4):

struct MouseState {
    int x;
    int y;
    bool left_button;
    bool right_button;
    bool middle_button;
};

static MouseState mouse_state = {400, 300, false, false, false};

STEP 2: In kernel_main(), replace your existing mouse initialization:

OLD CODE:
    outb(0x64, 0xA8); outb(0x64, 0x20);
    uint8_t status = (inb(0x60) | 2) & ~0x20;
    outb(0x64, 0x60); outb(0x60, status);
    outb(0x64, 0xD4); outb(0x60, 0xF6); inb(0x60);
    outb(0x64, 0xD4); outb(0x60, 0xF4); inb(0x60);

NEW CODE:
    // Initialize PS/2 mouse
    if (init_ps2_mouse()) {
        wm.print_to_focused("PS/2 mouse initialized.\n");
    } else {
        wm.print_to_focused("Using fallback mouse init.\n");
        // Fallback to your original code
        outb(0x64, 0xA8); outb(0x64, 0x20);
        uint8_t status = (inb(0x60) | 2) & ~0x20;
        outb(0x64, 0x60); outb(0x60, status);
        outb(0x64, 0xD4); outb(0x60, 0xF6); inb(0x60);
        outb(0x64, 0xD4); outb(0x60, 0xF4); inb(0x60);
    }

STEP 3: Replace your poll_input() function:

OPTION A - Use poll_input_simple() (recommended first try)
OPTION B - Use poll_input_improved() (if simple doesn't work)
OPTION C - Use poll_input_debug() (to see what's happening)

STEP 4: In kernel_main() main loop, replace:
    poll_input();
WITH:
    poll_input_simple();  // or poll_input_improved() or poll_input_debug()

TROUBLESHOOTING:
If mouse still doesn't work:

1. Try poll_input_debug() and watch the console output
2. Check if mouse_x and mouse_y values are changing
3. Verify status byte bit 5 is set for mouse packets
4. Some systems need extra delays in initialization

COMMON ISSUES:
- Mouse moves but erratically: Sign extension problem (fixed in simple version)
- Mouse doesn't move at all: Check if status bit 5 is working
- Mouse moves in wrong direction: Y inversion issue (fixed with -= instead of +=)
- Mouse jumps around: Overflow not handled (fixed in all versions)
*/

// =============================================================================
// FINAL RECOMMENDATION
// =============================================================================

// Start with this version - it's the most straightforward:
static inline void io_delay() {
    for (volatile int i = 0; i < 500; i++);  // small pause to avoid bus lock
}
void poll_input_simple() {
    last_key_press = 0;

    for (int tries = 0; tries < 8; tries++) { // don’t spin forever
        if (!(inb(0x64) & 1)) {
            io_delay();
            continue;
        }
        uint8_t status = inb(0x64);
        uint8_t data = inb(0x60);
        
        if (status & 0x20) {
            // Mouse data detected
            static uint8_t cycle = 0;
            static uint8_t bytes[3];
            
            bytes[cycle++] = data;
            
            if (cycle >= 3) {
                cycle = 0;
                
                // Validate packet (bit 3 must be set)
                if (bytes[0] & 0x08) {
                    // Update button
                    mouse_left_down = bytes[0] & 0x01;
                    
                    // Get deltas
                    int dx = bytes[1];
                    int dy = bytes[2];
                    
                    // Handle negative values (9-bit two's complement)
                    if (bytes[0] & 0x10) dx -= 256;
                    if (bytes[0] & 0x20) dy -= 256;
                    
                    // Ignore overflow
                    if (bytes[0] & 0x40) dx = 0;
                    if (bytes[0] & 0x80) dy = 0;
                    
                    // Update position
                    mouse_x += dx;
                    mouse_y -= dy; // Y is inverted
                    
                    // Clamp to screen
                    if (mouse_x < 0) mouse_x = 0;
                    if (mouse_y < 0) mouse_y = 0;
                    if (mouse_x >= (int)fb_info.width) mouse_x = fb_info.width - 1;
                    if (mouse_y >= (int)fb_info.height) mouse_y = fb_info.height - 1;
                }
            }
        } else {
            // Keyboard data
            bool is_press = !(data & 0x80);
            if (!is_press) data &= 0x7F;
            
            if (data == 0x2A || data == 0x36) is_shift_pressed = is_press;
            else if (data == 0x1D) is_ctrl_pressed = is_press;
            else if (is_press) {
                switch(data) {
                    case 0x48: last_key_press = KEY_UP; break;
                    case 0x50: last_key_press = KEY_DOWN; break;
                    case 0x4B: last_key_press = KEY_LEFT; break;
                    case 0x4D: last_key_press = KEY_RIGHT; break;
                    case 0x53: last_key_press = KEY_DELETE; break;
                    case 0x47: last_key_press = KEY_HOME; break;
                    case 0x4F: last_key_press = KEY_END; break;
                    default: {
                        const char* map = is_ctrl_pressed ? sc_ascii_ctrl_map : 
                                         (is_shift_pressed ? sc_ascii_shift_map : sc_ascii_nomod_map);
                        if (data < sizeof(sc_ascii_nomod_map) && map[data] != 0) {
                            last_key_press = map[data];
                        }
                    }
                }
            }
        }
    }
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
// Add these definitions near the other AHCI/FAT32 structs
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
    uint8_t* fat_sector = new uint8_t[SECTOR_SIZE];
    uint32_t fat_offset = cluster * 4;
    read_write_sectors(0, fat_start_sector + (fat_offset / SECTOR_SIZE), 1, false, fat_sector);
    uint32_t value = *(uint32_t*)(fat_sector + (fat_offset % SECTOR_SIZE)) & 0x0FFFFFFF;
    delete[] fat_sector;
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
	
	boot_sector_buffer[510] = 0x00; //dummy boot for testing
    boot_sector_buffer[511] = 0x00; //dummy boot for testing
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
// SECTION 6: SELF-HOSTED C COMPILER
// =============================================================================

// Forward declarations consumed by the command shell
extern "C" void cmd_compile(uint64_t ahci_base, int port, const char* filename);
extern "C" void cmd_run(uint64_t ahci_base, int port, const char* filename);
extern "C" void cmd_exec(const char* code_text);

// Provided by the command shell tokenizer
char* parts[32];
int     part_count;

// ---- tiny helpers ----
static inline int tcc_is_digit(char c){ return c>='0' && c<='9'; }
static inline int tcc_is_alpha(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
static inline int tcc_is_alnum(char c){ return tcc_is_alpha(c)||tcc_is_digit(c); }
static inline int tcc_strlen(const char* s){ int n=0; while(s && s[n]) ++n; return n; }

// ============================================================
// Console and Terminal I/O Functions
// ============================================================

// VGA Text Mode Buffer (typically at 0xB8000)
static volatile char* const VGA_BUFFER = (volatile char* const)0xB8000;
static int vga_row = 0;
static int vga_col = 0;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;
void vga_print_char(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            vga_row = VGA_HEIGHT - 1;
            // Scroll VGA buffer up
            for (int row = 0; row < VGA_HEIGHT - 1; row++) {
                for (int col = 0; col < VGA_WIDTH; col++) {
                    int src_idx = ((row + 1) * VGA_WIDTH + col) * 2;
                    int dst_idx = (row * VGA_WIDTH + col) * 2;
                    VGA_BUFFER[dst_idx] = VGA_BUFFER[src_idx];
                    VGA_BUFFER[dst_idx + 1] = VGA_BUFFER[src_idx + 1];
                }
            }
            // Clear last line
            for (int col = 0; col < VGA_WIDTH; col++) {
                int idx = ((VGA_HEIGHT - 1) * VGA_WIDTH + col) * 2;
                VGA_BUFFER[idx] = ' ';
                VGA_BUFFER[idx + 1] = 0x07;
            }
        }
    } else if (c >= 32 && c < 127) {
        int index = (vga_row * VGA_WIDTH + vga_col) * 2;
        VGA_BUFFER[index] = c;
        VGA_BUFFER[index + 1] = 0x07;
        vga_col++;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
            if (vga_row >= VGA_HEIGHT) {
                vga_row = VGA_HEIGHT - 1;
                // Scroll VGA buffer up
                for (int row = 0; row < VGA_HEIGHT - 1; row++) {
                    for (int col = 0; col < VGA_WIDTH; col++) {
                        int src_idx = ((row + 1) * VGA_WIDTH + col) * 2;
                        int dst_idx = (row * VGA_WIDTH + col) * 2;
                        VGA_BUFFER[dst_idx] = VGA_BUFFER[src_idx];
                        VGA_BUFFER[dst_idx + 1] = VGA_BUFFER[src_idx + 1];
                    }
                }
                // Clear last line
                for (int col = 0; col < VGA_WIDTH; col++) {
                    int idx = ((VGA_HEIGHT - 1) * VGA_WIDTH + col) * 2;
                    VGA_BUFFER[idx] = ' ';
                    VGA_BUFFER[idx + 1] = 0x07;
                }
            }
        }
    }
}

void vga_print(const char* str) {
    if (!str) return;
    while (*str) {
        vga_print_char(*str);
        str++;
    }
}

// Route to window if available, otherwise VGA
void console_print_char(char c) {
    int num_wins = wm.get_num_windows();
    int focused = wm.get_focused_idx();
    if (num_wins > 0 && focused >= 0 && focused < num_wins) {
        Window* win = wm.get_window(focused);
        if (win) {
            char buf[2] = {c, 0};
            win->console_print(buf);
        }
    } else {
        vga_print_char(c);
    }
}

void console_print(const char* str) {
    if (!str) return;
    int num_wins = wm.get_num_windows();
    int focused = wm.get_focused_idx();
    if (num_wins > 0 && focused >= 0 && focused < num_wins) {
        Window* win = wm.get_window(focused);
        if (win) {
            win->console_print(str);
        }
    } else {
        vga_print(str);
    }
}

// CORRECTED: Non-blocking get_char with fallback
static char pending_char = 0;

char get_char() {
    // Check if we have a pending character from previous call
    if (pending_char != 0) {
        char c = pending_char;
        pending_char = 0;
        return c;
    }

    // Non-blocking read from keyboard
    while (1) {
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Data available
            uint8_t scancode = inb(0x60);

            // Simple scancode to ASCII conversion (US keyboard layout)
            static const char scancode_map[] = {
                0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
                'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
                'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
                'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
            };

            if (scancode < sizeof(scancode_map)) {
                char c = scancode_map[scancode];
                if (c != 0) {
                    vga_print_char(c);
                    return c;
                }
            }
        } else {
            // No data available - return a null character
            // The caller should handle this and retry if needed
            return 0;
        }
    }
}

// CORRECTED: read_line with timeout and proper handling
static void read_line(char* buffer, int max_len) {
    int i = 0;
    int timeout_count = 0;
    const int TIMEOUT_THRESHOLD = 100000; // Prevent infinite wait

    while (i < max_len - 1 && timeout_count < TIMEOUT_THRESHOLD) {
        char c = get_char();

        if (c == 0) {
            // No character available, continue waiting
            timeout_count++;
            continue;
        }

        timeout_count = 0; // Reset timeout on character received

        if (c == '\n' || c == '\r') {
            break;
        }
        if (c == '\b') {
            if (i > 0) {
                i--;
            }
        } else if (c >= 32 && c <= 126) {
            buffer[i++] = c;
        }
    }

    buffer[i] = 0;
    vga_print_char('\n');
}

// ============================================================
// Integer Conversion Functions
// ============================================================

void int_to_string(int value, char* buffer) {
    if (!buffer) return;
    
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = 0;
        return;
    }
    
    int negative = value < 0;
    if (negative) value = -value;
    
    int i = 0;
    char temp[16];
    
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    int j = 0;
    if (negative) buffer[j++] = '-';
    
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    
    buffer[j] = 0;
}


// ============================================================
// File I/O Functions (FAT32 Support)
// ============================================================

// Simplified file buffer for storage
static char file_buffer[65536]; // 64KB file buffer



// ============================================================
// PCI Device Configuration Functions
// ============================================================

uint32_t pci_read_dword(uint16_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    // Create configuration address
    uint32_t address = 0x80000000 | ((uint32_t)bus << 16) | 
                       ((uint32_t)device << 11) | ((uint32_t)function << 8) | 
                       (offset & 0xFC);
    
    // Write to CONFIG_ADDRESS port (0xCF8)
    asm volatile("outl %0, %w1" : : "a"(address), "Nd"(0xCF8) : "memory");
    
    // Read from CONFIG_DATA port (0xCFC)
    uint32_t result;
    asm volatile("inl %w1, %0" : "=a"(result) : "Nd"(0xCFC) : "memory");
    
    return result;
}

uint32_t pci_write_dword(uint16_t bus, uint8_t device, uint8_t function, 
                         uint8_t offset, uint32_t value) {
    // Create configuration address
    uint32_t address = 0x80000000 | ((uint32_t)bus << 16) | 
                       ((uint32_t)device << 11) | ((uint32_t)function << 8) | 
                       (offset & 0xFC);
    
    // Write to CONFIG_ADDRESS port (0xCF8)
    asm volatile("outl %0, %w1" : : "a"(address), "Nd"(0xCF8) : "memory");
    
    // Write to CONFIG_DATA port (0xCFC)
    asm volatile("outl %0, %w1" : : "a"(value), "Nd"(0xCFC) : "memory");
    
    return value;
}

// ============================================================
// Memory Management (new/delete operators)
// ============================================================

// Simple heap allocator
static unsigned char heap[1048576]; // 1MB heap
static int heap_offset = 0;


void simple_strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

int simple_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void* simple_memcpy(void* dest, const void* src, int n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}
// Basic printf implementation
void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    char buffer[256]; // A buffer to hold consecutive characters
    int buffer_index = 0;

    while (*format != '\0') {
        if (*format == '%') {
            // If there's anything in the buffer, print it first
            if (buffer_index > 0) {
                buffer[buffer_index] = '\0';
                console_print(buffer);
                buffer_index = 0; // Reset buffer
            }

            format++; // Move past the '%'
            if (*format == 'd') {
                int i = va_arg(args, int);
                char num_buf[12];
                int_to_string(i, num_buf);
                console_print(num_buf);
            } else if (*format == 's') {
                char* s = va_arg(args, char*);
                console_print(s);
            } else if (*format == 'c') {
                char c = (char)va_arg(args, int);
                char str[2] = {c, 0};
                console_print(str);
            } else { // Handles %% and unknown specifiers
                console_print_char('%');
                console_print_char(*format);
            }
        } else {
            // Add the character to our buffer
            if (buffer_index < 255) {
                buffer[buffer_index++] = *format;
            }
        }
        format++;
    }

    // Print any remaining characters in the buffer at the end
    if (buffer_index > 0) {
        buffer[buffer_index] = '\0';
        console_print(buffer);
    }

    va_end(args);
}


// Helper functions for hex conversion and PCI access
static void uint32_to_hex_string(uint32_t value, char* buffer) {
    const char hex_chars[] = "0123456789ABCDEF";
    for(int i = 7; i >= 0; i--) {
        buffer[7-i] = hex_chars[(value >> (i*4)) & 0xF];
    }
    buffer[8] = 0;
}

static void uint64_to_hex_string(uint64_t value, char* buffer) {
    const char hex_chars[] = "0123456789ABCDEF";
    for(int i = 15; i >= 0; i--) {
        buffer[15-i] = hex_chars[(value >> (i*4)) & 0xF];
    }
    buffer[16] = 0;
}

// Simple PCI configuration space access
static uint32_t pci_config_read_dword(uint16_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = 0x80000000 | ((uint32_t)bus << 16) | ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) | (offset & 0xFC);

    // Write address to CONFIG_ADDRESS (0xCF8)
    asm volatile("outl %0, %w1" : : "a"(address), "Nd"(0xCF8) : "memory");

    // Read data from CONFIG_DATA (0xCFC)
    uint32_t result;
    asm volatile("inl %w1, %0" : "=a"(result) : "Nd"(0xCFC) : "memory");

    return result;
}

// ============================================================
// Hardware Interface Discovery Structures
// ============================================================
struct HardwareDevice {
    uint32_t vendor_id;
    uint32_t device_id;
    uint64_t base_address;
    uint64_t size;
    uint32_t device_type;  // 0=Unknown, 1=Storage, 2=Network, 3=Graphics, 4=Audio, 5=USB
    char description[64];
};

// Global hardware registry
static const int MAX_HARDWARE_DEVICES = 32;
static HardwareDevice hardware_registry[MAX_HARDWARE_DEVICES];
static int hardware_count = 0;

// More comprehensive PCI class codes
static const char* get_pci_class_name(uint8_t base_class, uint8_t sub_class) {
    switch (base_class) {
        case 0x00: return "Unclassified";
        case 0x01:
            switch (sub_class) {
                case 0x00: return "SCSI Controller";
                case 0x01: return "IDE Controller";
                case 0x02: return "Floppy Controller";
                case 0x03: return "IPI Controller";
                case 0x04: return "RAID Controller";
                case 0x05: return "ATA Controller";
                case 0x06: return "SATA Controller";
                case 0x07: return "SAS Controller";
                case 0x08: return "NVMe Controller";
                default: return "Storage Controller";
            }
        case 0x02: return "Network Controller";
        case 0x03:
            switch (sub_class) {
                case 0x00: return "VGA Controller";
                case 0x01: return "XGA Controller";
                case 0x02: return "3D Controller";
                default: return "Display Controller";
            }
        case 0x04: return "Multimedia Controller";
        case 0x05: return "Memory Controller";
        case 0x06: return "Bridge Device";
        case 0x07: return "Communication Controller";
        case 0x08: return "System Peripheral";
        case 0x09: return "Input Device";
        case 0x0A: return "Docking Station";
        case 0x0B: return "Processor";
        case 0x0C:
            switch (sub_class) {
                case 0x00: return "FireWire Controller";
                case 0x01: return "ACCESS Bus";
                case 0x02: return "SSA";
                case 0x03: return "USB Controller";
                case 0x04: return "Fibre Channel";
                case 0x05: return "SMBus";
                default: return "Serial Bus Controller";
            }
        case 0x0D: return "Wireless Controller";
        case 0x0E: return "Intelligent Controller";
        case 0x0F: return "Satellite Controller";
        case 0x10: return "Encryption Controller";
        case 0x11: return "Signal Processing Controller";
        default: return "Unknown Device";
    }
}

// Improved PCI device discovery
static void discover_pci_devices() {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            for (uint8_t function = 0; function < 8; function++) {
                uint32_t vendor_device = pci_config_read_dword(bus, device, function, 0);
                if ((vendor_device & 0xFFFF) == 0xFFFF) continue;

                if (hardware_count >= MAX_HARDWARE_DEVICES) return;

                HardwareDevice& dev = hardware_registry[hardware_count];
                dev.vendor_id = vendor_device & 0xFFFF;
                dev.device_id = (vendor_device >> 16) & 0xFFFF;

                // Read class code
                uint32_t class_code = pci_config_read_dword(bus, device, function, 0x08);
                uint8_t base_class = (class_code >> 24) & 0xFF;
                uint8_t sub_class = (class_code >> 16) & 0xFF;

                // Map to device type
                switch (base_class) {
                    case 0x01: dev.device_type = 1; break; // Storage
                    case 0x02: dev.device_type = 2; break; // Network
                    case 0x03: dev.device_type = 3; break; // Graphics
                    case 0x04: dev.device_type = 4; break; // Audio
                    case 0x0C:
                        dev.device_type = (sub_class == 0x03) ? 5 : 0; // USB or other
                        break;
                    default: dev.device_type = 0; break;
                }

                // Get description
                const char* desc = get_pci_class_name(base_class, sub_class);
                strncpy(dev.description, desc, 63);
                dev.description[63] = '\0';

                // Read BAR0 for base address (handle both 32-bit and 64-bit BARs)
                uint32_t bar0 = pci_config_read_dword(bus, device, function, 0x10);
                if (bar0 & 0x1) {
                    // I/O port
                    dev.base_address = bar0 & 0xFFFFFFFC;
                    dev.size = 0x100;
                } else {
                    // Memory mapped
                    dev.base_address = bar0 & 0xFFFFFFF0;
                    
                    // Check if 64-bit BAR
                    if ((bar0 & 0x6) == 0x4) {
                        uint32_t bar1 = pci_config_read_dword(bus, device, function, 0x14);
                        dev.base_address |= ((uint64_t)bar1 << 32);
                    }
                    
                    // Try to determine size by writing all 1s and reading back
                    pci_config_read_dword(bus, device, function, 0x04); // Save command reg
                    uint32_t orig_bar = bar0;
                    
                    outl(0xCF8, 0x80000000 | ((uint32_t)bus << 16) | 
                         ((uint32_t)device << 11) | ((uint32_t)function << 8) | 0x10);
                    outl(0xCFC, 0xFFFFFFFF);
                    uint32_t size_bar = inl(0xCFC);
                    
                    // Restore original BAR
                    outl(0xCF8, 0x80000000 | ((uint32_t)bus << 16) | 
                         ((uint32_t)device << 11) | ((uint32_t)function << 8) | 0x10);
                    outl(0xCFC, orig_bar);
                    
                    if (size_bar != 0 && size_bar != 0xFFFFFFFF) {
                        size_bar &= 0xFFFFFFF0;
                        dev.size = ~size_bar + 1;
                    } else {
                        dev.size = 0x1000; // Default to 4KB
                    }
                }

                hardware_count++;

                if (function == 0) {
                    uint8_t header_type = (class_code >> 16) & 0xFF;
                    if (!(header_type & 0x80)) {
                        break; // Single function device
                    }
                }
            }
        }
    }
}


static void discover_memory_regions() {
    // Add known memory regions
    if (hardware_count < MAX_HARDWARE_DEVICES) {
        HardwareDevice& dev = hardware_registry[hardware_count];
        dev.vendor_id = 0x0000;
        dev.device_id = 0x0001;
        dev.base_address = 0xB8000; // VGA text mode buffer
        dev.size = 0x8000;
        dev.device_type = 3;
        simple_strcpy(dev.description, "VGA Text Buffer");
        hardware_count++;
    }

    if (hardware_count < MAX_HARDWARE_DEVICES) {
        HardwareDevice& dev = hardware_registry[hardware_count];
        dev.vendor_id = 0x0000;
        dev.device_id = 0x0002;
        dev.base_address = 0xA0000; // VGA graphics buffer
        dev.size = 0x20000;
        dev.device_type = 3;
        simple_strcpy(dev.description, "VGA Graphics Buffer");
        hardware_count++;
    }
}

static int scan_hardware() {
    hardware_count = 0;
    discover_pci_devices();
    discover_memory_regions();
    return hardware_count;
}

// Safety check for MMIO access
static bool is_safe_mmio_address(uint64_t addr, uint64_t size) {
    // Check if address falls within any known device range
    for (int i = 0; i < hardware_count; i++) {
        const HardwareDevice& dev = hardware_registry[i];
        if (addr >= dev.base_address &&
            addr + size <= dev.base_address + dev.size) {
            return true;
        }
    }

    // Allow access to standard VGA and system areas even if not enumerated
    if (addr >= 0xA0000 && addr < 0x100000) return true; // VGA/BIOS area
    if (addr >= 0xB8000 && addr < 0xC0000) return true; // VGA text buffer
    if (addr >= 0x3C0 && addr < 0x3E0) return true;     // VGA registers
    if (addr >= 0x60 && addr < 0x70) return true;       // Keyboard controller

    return false;
}

// ============================================================
// Enhanced Bytecode ISA with Hardware Discovery and MMIO
// ============================================================
enum TOp : unsigned char {
    // stack/data
    T_NOP=0, T_PUSH_IMM, T_PUSH_STR, T_LOAD_LOCAL, T_STORE_LOCAL, T_POP,

    // arithmetic / unary
    T_ADD, T_SUB, T_MUL, T_DIV, T_NEG,

    // comparisons
    T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE,

    // control flow
    T_JMP, T_JZ, T_JNZ, T_RET,

    // I/O and args
    T_PRINT_INT, T_PRINT_CHAR, T_PRINT_STR, T_PRINT_ENDL, T_PRINT_INT_ARRAY, T_PRINT_STRING_ARRAY,
    T_READ_INT, T_READ_CHAR, T_READ_STR,
    T_PUSH_ARGC, T_PUSH_ARGV_PTR,

    // File I/O operations
    T_READ_FILE, T_WRITE_FILE, T_APPEND_FILE,

    // Array operations
    T_ALLOC_ARRAY, T_LOAD_ARRAY, T_STORE_ARRAY, T_ARRAY_SIZE, T_ARRAY_RESIZE,

    // String operations
    T_STR_CONCAT, T_STR_LENGTH, T_STR_SUBSTR, T_INT_TO_STR, T_STR_COMPARE,
    T_STR_FIND_CHAR, T_STR_FIND_STR, T_STR_FIND_LAST_CHAR, T_STR_CONTAINS,
    T_STR_STARTS_WITH, T_STR_ENDS_WITH, T_STR_COUNT_CHAR, T_STR_REPLACE_CHAR,

    // NEW: Hardware Discovery and Memory-Mapped I/O
    T_SCAN_HARDWARE,     // () -> device_count
    T_GET_DEVICE_INFO,   // (device_index) -> device_array_handle
    T_MMIO_READ8,        // (address) -> uint8_value
    T_MMIO_READ16,       // (address) -> uint16_value
    T_MMIO_READ32,       // (address) -> uint32_value
    T_MMIO_READ64,       // (address) -> uint64_value (split into two 32-bit values)
    T_MMIO_WRITE8,       // (address, value) -> success
    T_MMIO_WRITE16,      // (address, value) -> success
    T_MMIO_WRITE32,      // (address, value) -> success
    T_MMIO_WRITE64,      // (address, low32, high32) -> success
    T_GET_HARDWARE_ARRAY, // () -> hardware_device_array_handle
    T_DISPLAY_MEMORY_MAP // () -> displays formatted memory map
};

// ============================================================
// Enhanced Program buffers with hardware support
// ============================================================
struct TProgram {
    static const int CODE_MAX = 8192;
    unsigned char code[CODE_MAX];
    int pc = 0;

    static const int LIT_MAX = 4096;
    char lit[LIT_MAX];
    int lit_top = 0;

    static const int LOC_MAX = 32;
    char  loc_name[LOC_MAX][32];
    unsigned char loc_type[LOC_MAX]; // 0=int,1=char,2=string,3=int_array,4=string_array,5=device_array
    int   loc_array_size[LOC_MAX];
    int   loc_count = 0;

    int add_local(const char* name, unsigned char t, int array_size = 0){
        for(int i=0;i<loc_count;i++){ if(simple_strcmp(loc_name[i], name)==0) return i; }
        if(loc_count>=LOC_MAX) return -1;
        simple_strcpy(loc_name[loc_count], name);
        loc_type[loc_count]=t;
        loc_array_size[loc_count] = array_size;
        return loc_count++;
    }
    int get_local(const char* name){
        for(int i=0;i<loc_count;i++){ if(simple_strcmp(loc_name[i], name)==0) return i; }
        return -1;
    }
    int get_local_type(int idx){ return (idx>=0 && idx<loc_count)? loc_type[idx] : 0; }
    int get_array_size(int idx){ return (idx>=0 && idx<loc_count)? loc_array_size[idx] : 0; }

    void emit1(unsigned char op){ if(pc<CODE_MAX) code[pc++]=op; }
    void emit4(int v){ if(pc+4<=CODE_MAX){ code[pc++]=v&0xff; code[pc++]=(v>>8)&0xff; code[pc++]=(v>>16)&0xff; code[pc++]=(v>>24)&0xff; } }
    int  mark(){ return pc; }
    void patch4(int at, int v){ if(at+4<=CODE_MAX){ code[at+0]=v&0xff; code[at+1]=(v>>8)&0xff; code[at+2]=(v>>16)&0xff; code[at+3]=(v>>24)&0xff; } }

    const char* add_lit(const char* s){
        int n = tcc_strlen(s)+1;
        if(lit_top+n > LIT_MAX) return "";
        char* p = &lit[lit_top];
        simple_memcpy(p, s, n);
        lit_top += n;
        return p;
    }
};

// ============================================================
// Enhanced Tokenizer with hardware and MMIO keywords
// ============================================================
enum TTokType { TT_EOF, TT_ID, TT_NUM, TT_STR, TT_CH, TT_KW, TT_OP, TT_PUNC };
struct TTok { TTokType t; char v[256]; int ival; };

struct TLex {
    const char* src; int pos; int line;
    void init(const char* s){ src=s; pos=0; line=1; }

    void skipws(){
        for(;;){
            char c=src[pos];
            if(c==' '||c=='\t'||c=='\r'||c=='\n'){ if(c=='\n') line++; pos++; continue; }
            if(c=='/' && src[pos+1]=='/'){ pos+=2; while(src[pos] && src[pos]!='\n') pos++; continue; }
            if(c=='/' && src[pos+1]=='*'){ pos+=2; while(src[pos] && !(src[pos]=='*'&&src[pos+1]=='/')) pos++; if(src[pos]) pos+=2; continue; }
            break;
        }
    }

    TTok number(){
        TTok t; t.t=TT_NUM; t.ival=0; int i=0;
        // Support hex numbers (0x prefix)
        if(src[pos] == '0' && (src[pos+1] == 'x' || src[pos+1] == 'X')) {
            pos += 2;
            t.v[i++] = '0'; t.v[i++] = 'x';
            while(i < 63 && ((src[pos] >= '0' && src[pos] <= '9') ||
                              (src[pos] >= 'a' && src[pos] <= 'f') ||
                              (src[pos] >= 'A' && src[pos] <= 'F'))) {
                char c = src[pos];
                t.v[i++] = c;
                if(c >= '0' && c <= '9') t.ival = t.ival * 16 + (c - '0');
                else if(c >= 'a' && c <= 'f') t.ival = t.ival * 16 + (c - 'a' + 10);
                else if(c >= 'A' && c <= 'F') t.ival = t.ival * 16 + (c - 'A' + 10);
                pos++;
            }
        } else {
            while(tcc_is_digit(src[pos])){ t.v[i++]=src[pos]; t.ival = t.ival*10 + (src[pos]-'0'); pos++; if(i>=63) break; }
        }
        t.v[i]=0; return t;
    }

    TTok ident(){
        TTok t; t.t=TT_ID; int i=0;
        while(tcc_is_alnum(src[pos])){ t.v[i++]=src[pos++]; if(i>=63) break; } t.v[i]=0;
        // Enhanced keywords with hardware and MMIO functions
        const char* kw[]={"int","char","string","return","if","else","while","break","continue",
                         "cin","cout","endl","argc","argv","read_file","write_file","append_file",
                         "array_size","array_resize","str_length","str_substr","int_to_str","str_compare",
                         "str_find_char","str_find_str","str_find_last_char","str_contains",
                         "str_starts_with","str_ends_with","str_count_char","str_replace_char",
                         "scan_hardware","get_device_info","get_hardware_array","display_memory_map",
                         "mmio_read8","mmio_read16","mmio_read32","mmio_read64",
                         "mmio_write8","mmio_write16","mmio_write32","mmio_write64",0};
        for(int k=0; kw[k]; ++k){ if(simple_strcmp(t.v,kw[k])==0){ t.t=TT_KW; break; } }
        return t;
    }

    TTok string(){
        TTok t; t.t=TT_STR; int i=0; pos++;
        while(src[pos] && src[pos]!='"'){ if(i<256) t.v[i++]=src[pos]; pos++; }
        t.v[i]=0; if(src[pos]=='"') pos++; return t;
    }

    TTok chlit(){
        TTok t; t.t=TT_CH; t.v[0]=0; int v=0; pos++; // skip '
        if(src[pos] && src[pos+1]=='\''){ v = (unsigned char)src[pos]; pos+=2; }
        t.ival = v; return t;
    }

    TTok op_or_punc(){
        TTok t; t.t=TT_OP; t.v[0]=src[pos]; t.v[1]=0; char c=src[pos];
        if(c=='<' && src[pos+1]=='<'){ t.v[0]='<'; t.v[1]='<'; t.v[2]=0; pos+=2; return t; }
        if(c=='>' && src[pos+1]=='>'){ t.v[0]='>'; t.v[1]='>'; t.v[2]=0; pos+=2; return t; }
        if((c=='='||c=='!'||c=='<'||c=='>') && src[pos+1]=='='){ t.v[0]=c; t.v[1]='='; t.v[2]=0; pos+=2; return t; }
        pos++; if(c=='('||c==')'||c=='{'||c=='}'||c==';'||c==','||c=='['||c==']') t.t=TT_PUNC; return t;
    }

    TTok next(){
        skipws();
        if(src[pos]==0){ TTok t; t.t=TT_EOF; t.v[0]=0; return t; }
        if(src[pos]=='"') return string();
        if(src[pos]=='\'') return chlit();
        if(tcc_is_digit(src[pos]) || (src[pos]=='0' && (src[pos+1]=='x'||src[pos+1]=='X'))) return number();
        if(tcc_is_alpha(src[pos])) return ident();
        return op_or_punc();
    }
};

// ============================================================
// Enhanced Parser / Compiler with Hardware and MMIO support
// ============================================================
struct TCompiler {
    TLex lx; TTok tk; TProgram pr;

    int brk_pos[32]; int brk_cnt=0;
    int cont_pos[32]; int cont_cnt=0;

    void adv(){ tk = lx.next(); }
    int  accept(const char* s){ if(simple_strcmp(tk.v,s)==0){ adv(); return 1; } return 0; }
    void expect(const char* s){ if(!accept(s)) { printf("Parse error near: %s\n", tk.v); } }

    void parse_primary(){
        if(tk.t==TT_NUM){ pr.emit1(T_PUSH_IMM); pr.emit4(tk.ival); adv(); return; }
        if(tk.t==TT_CH){ pr.emit1(T_PUSH_IMM); pr.emit4(tk.ival); adv(); return; }
        if(tk.t==TT_STR){ const char* p=pr.add_lit(tk.v); pr.emit1(T_PUSH_STR); pr.emit4((int)p); adv(); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"argc")==0){ pr.emit1(T_PUSH_ARGC); adv(); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"argv")==0){ adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_PUSH_ARGV_PTR); return; }

        // File I/O built-ins
        if(tk.t==TT_KW && simple_strcmp(tk.v,"read_file")==0){
            adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_READ_FILE); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"write_file")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_WRITE_FILE); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"append_file")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_APPEND_FILE); return;
        }

        // Array built-ins
        if(tk.t==TT_KW && simple_strcmp(tk.v,"array_size")==0){
            adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_ARRAY_SIZE); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"array_resize")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_ARRAY_RESIZE); return;
        }

        // String built-ins
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_length")==0){
            adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_STR_LENGTH); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_substr")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(",");
            parse_expression(); expect(")"); pr.emit1(T_STR_SUBSTR); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"int_to_str")==0){
            adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_INT_TO_STR); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_compare")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_STR_COMPARE); return;
        }

        // String search functions
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_find_char")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_STR_FIND_CHAR); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_find_str")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_STR_FIND_STR); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_find_last_char")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_STR_FIND_LAST_CHAR); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_contains")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_STR_CONTAINS); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_starts_with")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_STR_STARTS_WITH); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_ends_with")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_STR_ENDS_WITH); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_count_char")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_STR_COUNT_CHAR); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"str_replace_char")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(",");
            parse_expression(); expect(")"); pr.emit1(T_STR_REPLACE_CHAR); return;
        }

        // NEW: Hardware Discovery Functions
        if(tk.t==TT_KW && simple_strcmp(tk.v,"scan_hardware")==0){
            adv(); expect("("); expect(")"); pr.emit1(T_SCAN_HARDWARE); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"get_device_info")==0){
            adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_GET_DEVICE_INFO); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"get_hardware_array")==0){
            adv(); expect("("); expect(")"); pr.emit1(T_GET_HARDWARE_ARRAY); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"display_memory_map")==0){
            adv(); expect("("); expect(")"); pr.emit1(T_DISPLAY_MEMORY_MAP); return;
        }

        // NEW: Memory-Mapped I/O Functions
        if(tk.t==TT_KW && simple_strcmp(tk.v,"mmio_read8")==0){
            adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_MMIO_READ8); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"mmio_read16")==0){
            adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_MMIO_READ16); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"mmio_read32")==0){
            adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_MMIO_READ32); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"mmio_read64")==0){
            adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_MMIO_READ64); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"mmio_write8")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_MMIO_WRITE8); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"mmio_write16")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_MMIO_WRITE16); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"mmio_write32")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(")"); pr.emit1(T_MMIO_WRITE32); return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"mmio_write64")==0){
            adv(); expect("("); parse_expression(); expect(","); parse_expression(); expect(",");
            parse_expression(); expect(")"); pr.emit1(T_MMIO_WRITE64); return;
        }

        if(tk.t==TT_PUNC && tk.v[0]=='('){ adv(); parse_expression(); expect(")"); return; }

        if(tk.t==TT_ID){
            int idx = pr.get_local(tk.v);
            if(idx<0){ printf("Unknown var %s\n", tk.v); }
            char var_name[32]; simple_strcpy(var_name, tk.v);
            adv();

            // Array indexing
            if(tk.t==TT_PUNC && tk.v[0]=='['){
                pr.emit1(T_LOAD_LOCAL); pr.emit4(idx); // push handle
                adv(); // past '['
                parse_expression(); // push index
                expect("]");
                pr.emit1(T_LOAD_ARRAY);
                return;
            }

            pr.emit1(T_LOAD_LOCAL); pr.emit4(idx);
            return;
        }
    }

    void parse_unary(){
        if(accept("-")){ parse_unary(); pr.emit1(T_NEG); return; }
        parse_primary();
    }

    void parse_term(){
        parse_unary();
        while(tk.v[0]=='*' || tk.v[0]=='/'){
            char op=tk.v[0]; adv(); parse_unary();
            pr.emit1(op=='*'?T_MUL:T_DIV);
        }
    }

    void parse_arith(){
        parse_term();
        while(tk.v[0]=='+' || tk.v[0]=='-'){
            char op=tk.v[0]; adv(); parse_term();
            if(op=='+') {
                pr.emit1(T_ADD); // This will be overridden for strings in VM
            } else {
                pr.emit1(T_SUB);
            }
        }
    }

    void parse_cmp(){
        parse_arith();
        while(tk.t==TT_OP && (simple_strcmp(tk.v,"==")==0 || simple_strcmp(tk.v,"!=")==0 ||
              simple_strcmp(tk.v,"<")==0 || simple_strcmp(tk.v,"<=")==0 ||
              simple_strcmp(tk.v,">")==0 || simple_strcmp(tk.v,">=")==0)){
            char opv[3]; simple_strcpy(opv, tk.v); adv(); parse_arith();
            if(simple_strcmp(opv,"==")==0) pr.emit1(T_EQ);
            else if(simple_strcmp(opv,"!=")==0) pr.emit1(T_NE);
            else if(simple_strcmp(opv,"<")==0)  pr.emit1(T_LT);
            else if(simple_strcmp(opv,"<=")==0) pr.emit1(T_LE);
            else if(simple_strcmp(opv,">")==0)  pr.emit1(T_GT);
            else pr.emit1(T_GE);
        }
    }

    void parse_expression(){ parse_cmp(); }

    void parse_decl(unsigned char tkind){
        adv(); // past type keyword
        if(tk.t!=TT_ID){ printf("Expected identifier\n"); return; }
        char nm[32]; simple_strcpy(nm, tk.v); adv();

        int array_size = 0;
        // Array declaration syntax: int arr[size] or string arr[size]
        if(tk.t==TT_PUNC && tk.v[0]=='['){
            adv();
            if(tk.t==TT_NUM){
                array_size = tk.ival;
                adv();
            } else {
                printf("Expected array size\n"); return;
            }
            expect("]");

            if (tkind == 0) tkind = 3; // int -> int_array
            else if (tkind == 2) tkind = 4; // string -> string_array
        }

        int idx = pr.add_local(nm, tkind, array_size);

        // If it's an array, allocate it now, before parsing initializer
        if (tkind == 3 || tkind == 4) {
            pr.emit1(T_PUSH_IMM); pr.emit4(array_size);
            pr.emit1(T_ALLOC_ARRAY);
            pr.emit1(T_STORE_LOCAL); pr.emit4(idx);
        }

        if(accept("=")){
            if(tkind==3 || tkind==4){ // Array initialization
                expect("{");
                int i = 0;
                do {
                    if (tk.t == TT_PUNC && tk.v[0] == '}') break; // empty list or trailing comma
                    if (i >= array_size) {
                        printf("Too many initializers for array\n");
                        while(!accept("}")) { if(tk.t==TT_EOF) break; adv(); }
                        goto end_init;
                    }

                    pr.emit1(T_LOAD_LOCAL); pr.emit4(idx);      // 1. Push handle
                    pr.emit1(T_PUSH_IMM); pr.emit4(i);          // 2. Push index
                    parse_expression();                         // 3. Push value
                    pr.emit1(T_STORE_ARRAY);                    // 4. Store
                    i++;
                } while(accept(","));
                expect("}");
                end_init:;
            } else if(tkind==2){ // string
                if(tk.t==TT_STR){ const char* p=pr.add_lit(tk.v); pr.emit1(T_PUSH_STR); pr.emit4((int)p); adv(); }
                else if(tk.t==TT_KW && simple_strcmp(tk.v,"argv")==0){ adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_PUSH_ARGV_PTR); }
                else if(tk.t==TT_ID){ int j=pr.get_local(tk.v); adv(); pr.emit1(T_LOAD_LOCAL); pr.emit4(j); }
                else { parse_expression(); }
                pr.emit1(T_STORE_LOCAL); pr.emit4(idx);
            } else {
                parse_expression();
                pr.emit1(T_STORE_LOCAL); pr.emit4(idx);
            }
        }
        expect(";");
    }

    void parse_assign_or_coutcin(){
        if(tk.t==TT_KW && simple_strcmp(tk.v,"cout")==0){ adv();
            for(;;){
                expect("<<");
                if(tk.t==TT_KW && simple_strcmp(tk.v,"endl")==0){ adv(); pr.emit1(T_PRINT_ENDL); }
                else if(tk.t==TT_STR){ const char* p=pr.add_lit(tk.v); pr.emit1(T_PUSH_STR); pr.emit4((int)p); adv(); pr.emit1(T_PRINT_STR); }
                else if(tk.t==TT_KW && simple_strcmp(tk.v,"argv")==0){ adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_PUSH_ARGV_PTR); pr.emit1(T_PRINT_STR); }
                else if(tk.t==TT_ID){
                    char var_name[32]; simple_strcpy(var_name, tk.v);
                    int idx = pr.get_local(tk.v); int ty = pr.get_local_type(idx); adv();

                    // Handle array element printing vs whole array printing
                    if(tk.t==TT_PUNC && tk.v[0]=='['){
                        pr.emit1(T_LOAD_LOCAL); pr.emit4(idx); // load array
                        adv(); // past '['
                        parse_expression(); // push index
                        expect("]");
                        pr.emit1(T_LOAD_ARRAY); // load element
                        if (ty == 3) pr.emit1(T_PRINT_INT);      // int array element
                        else if (ty == 4) pr.emit1(T_PRINT_STR);  // string array element
                        else if (ty == 5) pr.emit1(T_PRINT_INT);  // device array element
                    } else {
                        pr.emit1(T_LOAD_LOCAL); pr.emit4(idx);
                        if(ty==4) pr.emit1(T_PRINT_STRING_ARRAY); // Print whole string array
                        else if(ty==3) pr.emit1(T_PRINT_INT_ARRAY); // Print whole int array
                        else if(ty==2) pr.emit1(T_PRINT_STR);
                        else if(ty==1) pr.emit1(T_PRINT_CHAR);
                        else pr.emit1(T_PRINT_INT);
                    }
                } else { parse_expression(); pr.emit1(T_PRINT_INT); }
                if(tk.t==TT_PUNC && tk.v[0]==';'){ adv(); break; }
            }
            return;
        }
        if (tk.t==TT_KW && simple_strcmp(tk.v,"cin")==0) {
            adv();
            for (;;) {
                expect(">>");
                if (tk.t != TT_ID) {
                    printf("cin expects identifier\n");
                    return;
                }
                int idx = pr.get_local(tk.v);
                int ty  = pr.get_local_type(idx);
                adv(); // past identifier

                // Read into scalar variable based on its type
                if (ty == 2)      pr.emit1(T_READ_STR);   // string
                else if (ty == 1) pr.emit1(T_READ_CHAR);  // char
                else              pr.emit1(T_READ_INT);   // int (default)

                pr.emit1(T_STORE_LOCAL);
                pr.emit4(idx);

                // End the chain only at a semicolon; otherwise continue parsing >>
                if (tk.t == TT_PUNC && tk.v[0] == ';') {
                    adv();
                    break;
                }
            }
            return;
        }

        if(tk.t==TT_ID){
            int idx = pr.get_local(tk.v);
            if(idx<0){ printf("Unknown var %s\n", tk.v); }
            int ty = pr.get_local_type(idx);
            adv();

            // Array element assignment
            if(tk.t==TT_PUNC && tk.v[0]=='['){
                pr.emit1(T_LOAD_LOCAL); pr.emit4(idx);  // 1. Push handle
                adv(); // past '['
                parse_expression();                    // 2. Push index
                expect("]");
                expect("=");
                parse_expression();                    // 3. Push value
                pr.emit1(T_STORE_ARRAY);               // 4. Store
                expect(";");
                return;
            }

            expect("=");
            if(ty==2){
                if(tk.t==TT_STR){ const char* p=pr.add_lit(tk.v); pr.emit1(T_PUSH_STR); pr.emit4((int)p); adv(); }
                else if(tk.t==TT_KW && simple_strcmp(tk.v,"argv")==0){ adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_PUSH_ARGV_PTR); }
                else if(tk.t==TT_ID){ int j=pr.get_local(tk.v); adv(); pr.emit1(T_LOAD_LOCAL); pr.emit4(j); }
                else { parse_expression(); }
            } else {
                parse_expression();
            }
            pr.emit1(T_STORE_LOCAL); pr.emit4(idx);
            expect(";");
            return;
        }

        // Expression statement
        parse_expression();
        pr.emit1(T_POP); // Pop unused result
        expect(";");
    }

    void parse_if(){
        adv(); expect("("); parse_expression(); expect(")");
        pr.emit1(T_JZ); int jz_at = pr.mark(); pr.emit4(0);
        parse_block();
        int has_else = (tk.t==TT_KW && simple_strcmp(tk.v,"else")==0);
        if(has_else){
            pr.emit1(T_JMP); int j_at = pr.mark(); pr.emit4(0);
            int here = pr.pc; pr.patch4(jz_at, here);
            adv(); // else
            parse_block();
            int end = pr.pc; pr.patch4(j_at, end);
        } else {
            int here = pr.pc; pr.patch4(jz_at, here);
        }
    }

    void parse_while(){
        adv(); expect("("); int cond_ip = pr.pc; parse_expression(); expect(")");
        pr.emit1(T_JZ); int jz_at = pr.mark(); pr.emit4(0);
        int brk_base=brk_cnt, cont_base=cont_cnt;
        parse_block();
        for(int i=cont_base;i<cont_cnt;i++){ pr.patch4(cont_pos[i], cond_ip); }
        cont_cnt = cont_base;
        pr.emit1(T_JMP); pr.emit4(cond_ip);
        int end_ip = pr.pc; pr.patch4(jz_at, end_ip);
        for(int i=brk_base;i<brk_cnt;i++){ pr.patch4(brk_pos[i], end_ip); }
        brk_cnt = brk_base;
    }

    void parse_block(){
        if(accept("{")){
            while(!(tk.t==TT_PUNC && tk.v[0]=='}') && tk.t!=TT_EOF) parse_stmt();
            expect("}");
        } else {
            parse_stmt();
        }
    }

    void parse_stmt(){
        if(tk.t==TT_KW && simple_strcmp(tk.v,"int")==0){ parse_decl(0); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"char")==0){ parse_decl(1); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"string")==0){ parse_decl(2); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"return")==0){ adv(); parse_expression(); pr.emit1(T_RET); expect(";"); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"if")==0){ parse_if(); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"while")==0){ parse_while(); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"break")==0){ adv(); expect(";"); pr.emit1(T_JMP); int at=pr.mark(); pr.emit4(0); brk_pos[brk_cnt++]=at; return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"continue")==0){ adv(); expect(";"); pr.emit1(T_JMP); int at=pr.mark(); pr.emit4(0); cont_pos[cont_cnt++]=at; return; }
        parse_assign_or_coutcin();
    }

    int compile(const char* source){
        lx.init(source); adv();
        if(!(tk.t==TT_KW && simple_strcmp(tk.v,"int")==0)) { printf("Expected 'int' at start\n"); return -1; }
        adv();
        if(!(tk.t==TT_ID && simple_strcmp(tk.v,"main")==0)){ printf("Expected main\n"); return -1; }
        adv(); expect("("); expect(")"); parse_block();
        pr.emit1(T_PUSH_IMM); pr.emit4(0); pr.emit1(T_RET);
        return pr.pc;
    }
};

// ============================================================
// Enhanced VM with Hardware Discovery and Memory-Mapped I/O
// ============================================================
struct TinyVM {
    static const int STK_MAX = 1024;
    int   stk[STK_MAX]; int sp=0;
    int   locals[TProgram::LOC_MAX];
    int   argc; const char** argv;
    TProgram* P;
    char str_in[256];
    uint64_t ahci_base; int port; // for file I/O

    // String pool for dynamic string management
    static const int STRING_POOL_SIZE = 8192;
    char string_pool[STRING_POOL_SIZE];
    int string_pool_top = 0;

    // Simple array management
    struct Array {
        int* data;
        int size;
        int capacity;
    };
    static const int MAX_ARRAYS = 64;
    Array arrays[MAX_ARRAYS];
    int array_count = 0;

    // Special array handle for hardware devices
    int hardware_array_handle = 0;

    inline void push(int v){ if(sp<STK_MAX) stk[sp++]=v; }
    inline int  pop(){ return sp?stk[--sp]:0; }

    // Memory-Mapped I/O functions
    uint8_t mmio_read_8(uint64_t addr) {
        if (!is_safe_mmio_address(addr, 1)) {
            char hex[17]; uint64_to_hex_string(addr, hex);
            printf("MMIO: Unsafe read8 at 0x%s\n", hex);
            return 0xFF;
        }
        return *(volatile uint8_t*)addr;
    }

    uint16_t mmio_read_16(uint64_t addr) {
        if (!is_safe_mmio_address(addr, 2)) {
            char hex[17]; uint64_to_hex_string(addr, hex);
            printf("MMIO: Unsafe read16 at 0x%s\n", hex);
            return 0xFFFF;
        }
        return *(volatile uint16_t*)addr;
    }

    uint32_t mmio_read_32(uint64_t addr) {
        if (!is_safe_mmio_address(addr, 4)) {
            char hex[17]; uint64_to_hex_string(addr, hex);
            printf("MMIO: Unsafe read32 at 0x%s\n", hex);
            return 0xFFFFFFFF;
        }
        return *(volatile uint32_t*)addr;
    }

    uint64_t mmio_read_64(uint64_t addr) {
        if (!is_safe_mmio_address(addr, 8)) {
            char hex[17]; uint64_to_hex_string(addr, hex);
            printf("MMIO: Unsafe read64 at 0x%s\n", hex);
            return 0xFFFFFFFFFFFFFFFFULL;
        }
        return *(volatile uint64_t*)addr;
    }

    bool mmio_write_8(uint64_t addr, uint8_t value) {
        if (!is_safe_mmio_address(addr, 1)) {
            char hex[17]; uint64_to_hex_string(addr, hex);
            printf("MMIO: Unsafe write8 at 0x%s\n", hex);
            return false;
        }
        *(volatile uint8_t*)addr = value;
        return true;
    }

    bool mmio_write_16(uint64_t addr, uint16_t value) {
        if (!is_safe_mmio_address(addr, 2)) {
            char hex[17]; uint64_to_hex_string(addr, hex);
            printf("MMIO: Unsafe write16 at 0x%s\n", hex);
            return false;
        }
        *(volatile uint16_t*)addr = value;
        return true;
    }

    bool mmio_write_32(uint64_t addr, uint32_t value) {
        if (!is_safe_mmio_address(addr, 4)) {
            char hex[17]; uint64_to_hex_string(addr, hex);
            printf("MMIO: Unsafe write32 at 0x%s\n", hex);
            return false;
        }
        *(volatile uint32_t*)addr = value;
        return true;
    }

    bool mmio_write_64(uint64_t addr, uint64_t value) {
        if (!is_safe_mmio_address(addr, 8)) {
            char hex[17]; uint64_to_hex_string(addr, hex);
            printf("MMIO: Unsafe write64 at 0x%s\n", hex);
            return false;
        }
        *(volatile uint64_t*)addr = value;
        return true;
    }

    // String management functions
    const char* alloc_string(int len) {
        if(string_pool_top + len + 1 > STRING_POOL_SIZE) {
            string_pool_top = 0; // Simple reset
        }
        if(string_pool_top + len + 1 > STRING_POOL_SIZE) return nullptr;
        char* result = &string_pool[string_pool_top];
        string_pool_top += len + 1;
        return result;
    }

    const char* concat_strings(const char* a, const char* b) {
        if(!a) a = "";
        if(!b) b = "";
        int len_a = tcc_strlen(a);
        int len_b = tcc_strlen(b);
        const char* result = alloc_string(len_a + len_b);
        if(!result) return "";
        char* dest = (char*)result;
        simple_memcpy(dest, a, len_a);
        simple_memcpy(dest + len_a, b, len_b + 1);
        return result;
    }

    const char* int_to_string_vm(int value) {
        static char temp_buf[16];
        int_to_string(value, temp_buf);
        int len = tcc_strlen(temp_buf);
        const char* result = alloc_string(len);
        if(!result) return "";
        simple_memcpy((char*)result, temp_buf, len + 1);
        return result;
    }

    const char* substring(const char* str, int start, int len) {
        if(!str) return "";
        int str_len = tcc_strlen(str);
        if(start < 0 || start >= str_len || len <= 0) return "";
        if(start + len > str_len) len = str_len - start;

        const char* result = alloc_string(len);
        if(!result) return "";
        char* dest = (char*)result;
        simple_memcpy(dest, str + start, len);
        dest[len] = 0;
        return result;
    }

    int string_compare(const char* a, const char* b) {
        if(!a && !b) return 0;
        if(!a) return -1;
        if(!b) return 1;
        return simple_strcmp(a, b);
    }

    // String search and manipulation functions (abbreviated for space)
    int find_char(const char* str, char c) {
        if(!str) return -1;
        for(int i = 0; str[i]; i++) {
            if(str[i] == c) return i;
        }
        return -1;
    }

    int find_last_char(const char* str, char c) {
        if(!str) return -1;
        int last_pos = -1;
        for(int i = 0; str[i]; i++) {
            if(str[i] == c) last_pos = i;
        }
        return last_pos;
    }

    int find_string(const char* haystack, const char* needle) {
        if(!haystack || !needle) return -1;
        if(!needle[0]) return 0;

        int hay_len = tcc_strlen(haystack);
        int needle_len = tcc_strlen(needle);
        if(needle_len > hay_len) return -1;

        for(int i = 0; i <= hay_len - needle_len; i++) {
            int j;
            for(j = 0; j < needle_len; j++) {
                if(haystack[i + j] != needle[j]) break;
            }
            if(j == needle_len) return i;
        }
        return -1;
    }

    int string_contains(const char* str, const char* substr) {
        return find_string(str, substr) != -1 ? 1 : 0;
    }

    int string_starts_with(const char* str, const char* prefix) {
        if(!str || !prefix) return 0;
        if(!prefix[0]) return 1;

        int i = 0;
        while(prefix[i] && str[i]) {
            if(str[i] != prefix[i]) return 0;
            i++;
        }
        return prefix[i] == 0 ? 1 : 0;
    }

    int string_ends_with(const char* str, const char* suffix) {
        if(!str || !suffix) return 0;
        if(!suffix[0]) return 1;

        int str_len = tcc_strlen(str);
        int suffix_len = tcc_strlen(suffix);
        if(suffix_len > str_len) return 0;

        int start_pos = str_len - suffix_len;
        for(int i = 0; i < suffix_len; i++) {
            if(str[start_pos + i] != suffix[i]) return 0;
        }
        return 1;
    }

    int count_char(const char* str, char c) {
        if(!str) return 0;
        int count = 0;
        for(int i = 0; str[i]; i++) {
            if(str[i] == c) count++;
        }
        return count;
    }

    const char* replace_char(const char* str, char old_char, char new_char) {
        if(!str) return "";
        int len = tcc_strlen(str);
        const char* result = alloc_string(len);
        if(!result) return "";

        char* dest = (char*)result;
        for(int i = 0; i < len; i++) {
            dest[i] = (str[i] == old_char) ? new_char : str[i];
        }
        dest[len] = 0;
        return result;
    }

    // Check if values on stack are strings
    bool is_string_ptr(int val) {
        const char* ptr = (const char*)val;
        return (ptr >= P->lit && ptr < P->lit + P->lit_top) ||
               (ptr >= string_pool && ptr < string_pool + string_pool_top);
    }

    // Array management functions
    int alloc_array(int size) {
        if(array_count >= MAX_ARRAYS) return 0;
        Array& arr = arrays[array_count];
        arr.size = size;
        arr.capacity = size;
        static int array_pool[MAX_ARRAYS * 256];
        static int pool_offset = 0;
        if(pool_offset + size > MAX_ARRAYS * 256) return 0;
        arr.data = &array_pool[pool_offset];
        pool_offset += size;
        for(int i = 0; i < size; i++) arr.data[i] = 0;
        return ++array_count;
    }

    Array* get_array(int handle) {
        if(handle <= 0 || handle > array_count) return nullptr;
        return &arrays[handle - 1];
    }

    int resize_array(int handle, int new_size) {
        Array* arr = get_array(handle);
        if(!arr || new_size <= 0) return 0;

        int new_handle = alloc_array(new_size);
        Array* new_arr = get_array(new_handle);
        if(!new_arr) return 0;

        int copy_size = (arr->size < new_size) ? arr->size : new_size;
        for(int i = 0; i < copy_size; i++) {
            new_arr->data[i] = arr->data[i];
        }
        return new_handle;
    }

    // NEW: Create hardware device info array
    int create_device_info_array(int device_index) {
        if(device_index < 0 || device_index >= hardware_count) return 0;

        const HardwareDevice& dev = hardware_registry[device_index];

        // Create array with device info: [vendor_id, device_id, base_addr_low, base_addr_high, size_low, size_high, device_type]
        int handle = alloc_array(7);
        Array* arr = get_array(handle);
        if(!arr) return 0;

        arr->data[0] = dev.vendor_id;
        arr->data[1] = dev.device_id;
        arr->data[2] = (uint32_t)(dev.base_address & 0xFFFFFFFF);      // low 32 bits
        arr->data[3] = (uint32_t)((dev.base_address >> 32) & 0xFFFFFFFF); // high 32 bits
        arr->data[4] = (uint32_t)(dev.size & 0xFFFFFFFF);              // size low 32 bits
        arr->data[5] = (uint32_t)((dev.size >> 32) & 0xFFFFFFFF);      // size high 32 bits
        arr->data[6] = dev.device_type;

        return handle;
    }

    // NEW: Create array containing all hardware devices
    int create_hardware_array() {
        if(hardware_array_handle > 0) return hardware_array_handle; // Return existing handle

        hardware_array_handle = alloc_array(hardware_count * 7); // 7 fields per device
        Array* arr = get_array(hardware_array_handle);
        if(!arr) return 0;

        for(int i = 0; i < hardware_count; i++) {
            const HardwareDevice& dev = hardware_registry[i];
            int base = i * 7;
            arr->data[base + 0] = dev.vendor_id;
            arr->data[base + 1] = dev.device_id;
            arr->data[base + 2] = (uint32_t)(dev.base_address & 0xFFFFFFFF);
            arr->data[base + 3] = (uint32_t)((dev.base_address >> 32) & 0xFFFFFFFF);
            arr->data[base + 4] = (uint32_t)(dev.size & 0xFFFFFFFF);
            arr->data[base + 5] = (uint32_t)((dev.size >> 32) & 0xFFFFFFFF);
            arr->data[base + 6] = dev.device_type;
        }

        return hardware_array_handle;
    }

    int run(TProgram& prog, int ac, const char** av, uint64_t base, int p){
        P=&prog; argc=ac; argv=av; sp=0; ahci_base=base; port=p;
        for (int i=0;i<TProgram::LOC_MAX;i++) locals[i]=0;
        array_count = 0;
        hardware_array_handle = 0;
        string_pool_top = 0;
        int ip=0;

        // Initialize arrays declared in locals
        for(int i = 0; i < P->loc_count; i++) {
            if(P->loc_type[i] == 3 || P->loc_type[i] == 4) {
                int arr_handle = alloc_array(P->loc_array_size[i]);
                locals[i] = arr_handle;
            }
        }

        while(ip < P->pc){
            TOp op = (TOp)P->code[ip++];
            switch(op){
                case T_NOP: break;
                case T_PUSH_IMM: { int v= *(int*)&P->code[ip]; ip+=4; push(v); } break;
                case T_PUSH_STR: { int p= *(int*)&P->code[ip]; ip+=4; push(p); } break;
                case T_LOAD_LOCAL:{ int i=*(int*)&P->code[ip]; ip+=4; push(locals[i]); } break;
                case T_STORE_LOCAL:{ int i=*(int*)&P->code[ip]; ip+=4; locals[i]=pop(); } break;
                case T_POP: { if(sp) --sp; } break;

                // Enhanced ADD operation - handles both integers and string concatenation
                case T_ADD: {
                    int b=pop(), a=pop();
                    if(is_string_ptr(a) || is_string_ptr(b)) {
                        const char* result = concat_strings((const char*)a, (const char*)b);
                        push((int)result);
                    } else {
                        push(a+b);
                    }
                } break;
                case T_SUB: { int b=pop(), a=pop(); push(a-b);} break;
                case T_MUL: { int b=pop(), a=pop(); push(a*b);} break;
                case T_DIV: { int b=pop(), a=pop(); push(b? a/b:0);} break;
                case T_NEG: { int a=pop(); push(-a);} break;

                // Enhanced comparison operations - handle string comparisons
                case T_EQ: {
                    int b=pop(), a=pop();
                    if(is_string_ptr(a) || is_string_ptr(b)) {
                        push(string_compare((const char*)a, (const char*)b) == 0 ? 1 : 0);
                    } else {
                        push(a==b);
                    }
                } break;
                case T_NE: {
                    int b=pop(), a=pop();
                    if(is_string_ptr(a) || is_string_ptr(b)) {
                        push(string_compare((const char*)a, (const char*)b) != 0 ? 1 : 0);
                    } else {
                        push(a!=b);
                    }
                } break;
                case T_LT: {
                    int b=pop(), a=pop();
                    if(is_string_ptr(a) || is_string_ptr(b)) {
                        push(string_compare((const char*)a, (const char*)b) < 0 ? 1 : 0);
                    } else {
                        push(a<b);
                    }
                } break;
                case T_LE: {
                    int b=pop(), a=pop();
                    if(is_string_ptr(a) || is_string_ptr(b)) {
                        push(string_compare((const char*)a, (const char*)b) <= 0 ? 1 : 0);
                    } else {
                        push(a<=b);
                    }
                } break;
                case T_GT: {
                    int b=pop(), a=pop();
                    if(is_string_ptr(a) || is_string_ptr(b)) {
                        push(string_compare((const char*)a, (const char*)b) > 0 ? 1 : 0);
                    } else {
                        push(a>b);
                    }
                } break;
                case T_GE: {
                    int b=pop(), a=pop();
                    if(is_string_ptr(a) || is_string_ptr(b)) {
                        push(string_compare((const char*)a, (const char*)b) >= 0 ? 1 : 0);
                    } else {
                        push(a>=b);
                    }
                } break;

                case T_JMP: { int t=*(int*)&P->code[ip]; ip=t; } break;
                case T_JZ:  { int t=*(int*)&P->code[ip]; ip+=4; int v=pop(); if(v==0) ip=t; } break;
                case T_JNZ: { int t=*(int*)&P->code[ip]; ip+=4; int v=pop(); if(v!=0) ip=t; } break;

                case T_PRINT_INT: { int v=pop(); char b[16]; int_to_string(v,b); printf("%s", b); } break;
                case T_PRINT_CHAR:{ int v=pop(); char b[2]; b[0]=(char)(v&0xff); b[1]=0; printf("%s", b); } break;
                case T_PRINT_STR: { const char* p=(const char*)pop(); if(p) printf("%s", p); } break;
                case T_PRINT_ENDL:{ printf("\n"); } break;

                case T_PRINT_INT_ARRAY: {
                    int handle = pop();
                    Array* arr = get_array(handle);
                    if (arr) {
                        printf("[");
                        for (int i = 0; i < arr->size; i++) {
                            char b[16];
                            int_to_string(arr->data[i], b);
                            printf("%s", b);
                            if (i < arr->size - 1) printf(", ");
                        }
                        printf("]");
                    } else {
                        printf("(null array)");
                    }
                } break;

                case T_PRINT_STRING_ARRAY: {
                    int handle = pop();
                    Array* arr = get_array(handle);
                    if (arr) {
                        printf("[");
                        for (int i = 0; i < arr->size; i++) {
                            const char* p = (const char*)arr->data[i];
                            printf("\"");
                            if (p) printf("%s", p);
                            printf("\"");
                            if (i < arr->size - 1) printf(", ");
                        }
                        printf("]");
                    } else {
                        printf("(null array)");
                    }
                } break;

                case T_READ_INT: {
                    char t[32];
                    read_line(t, 32);
                    push(simple_atoi(t));
                } break;
                case T_READ_CHAR:{
                    char t[4];
                    read_line(t, 4);
                    push((unsigned char)t[0]);
                } break;
                case T_READ_STR: {
                    read_line(str_in, 256);
                    push((int)str_in);
                } break;

                case T_PUSH_ARGC: { push(argc); } break;
                case T_PUSH_ARGV_PTR: { int idx=pop(); const char* p=(idx>=0 && idx<argc && argv)? argv[idx]:""; push((int)p); } break;

                // File I/O operations
                case T_READ_FILE: {
                    const char* filename = (const char*)pop();
                    char* file_buffer = fat32_read_file_as_string(filename);
                    if(file_buffer) {
                        push((int)file_buffer);
                    } else {
                        push((int)"");
                    }
                } break;

                case T_WRITE_FILE: {
                    const char* content = (const char*)pop();
                    const char* filename = (const char*)pop();
                    int len = tcc_strlen(content);
                    int result = fat32_write_file(filename, (const unsigned char*)content, len);
                    push(result >= 0 ? 1 : 0);
                } break;

                case T_APPEND_FILE: {
                    const char* content = (const char*)pop();
                    const char* filename = (const char*)pop();
                    char* existing_buffer = fat32_read_file_as_string(filename);
                    int n = 0;
                    if(existing_buffer) {
                        n = tcc_strlen(existing_buffer);
                    }

                    int content_len = tcc_strlen(content);
                    char* new_buffer = new char[n + content_len + 1];
                    if (existing_buffer) {
                        simple_memcpy(new_buffer, existing_buffer, n);
                        delete[] existing_buffer;
                    }
                    simple_memcpy(new_buffer + n, content, content_len + 1);

                    int result = fat32_write_file(filename, (const unsigned char*)new_buffer, n + content_len);
                    push(result >= 0 ? 1 : 0);
                    delete[] new_buffer;
                } break;

                // Array operations
                case T_ALLOC_ARRAY: {
                    int size = pop();
                    int handle = alloc_array(size);
                    push(handle);
                } break;

                case T_LOAD_ARRAY: {
                    int index = pop();
                    int handle = pop();
                    Array* arr = get_array(handle);
                    if(arr && index >= 0 && index < arr->size) {
                        push(arr->data[index]);
                    } else {
                        push(0);
                    }
                } break;

                case T_STORE_ARRAY: {
                    int value = pop();
                    int index = pop();
                    int handle = pop();
                    Array* arr = get_array(handle);
                    if(arr && index >= 0 && index < arr->size) {
                        arr->data[index] = value;
                    }
                } break;

                case T_ARRAY_SIZE: {
                    int handle = pop();
                    Array* arr = get_array(handle);
                    push(arr ? arr->size : 0);
                } break;

                case T_ARRAY_RESIZE: {
                    int new_size = pop();
                    int handle = pop();
                    int new_handle = resize_array(handle, new_size);
                    push(new_handle);
                } break;

                // String operations
                case T_STR_CONCAT: {
                    const char* b = (const char*)pop();
                    const char* a = (const char*)pop();
                    const char* result = concat_strings(a, b);
                    push((int)result);
                } break;

                case T_STR_LENGTH: {
                    const char* str = (const char*)pop();
                    push(str ? tcc_strlen(str) : 0);
                } break;

                case T_STR_SUBSTR: {
                    int len = pop();
                    int start = pop();
                    const char* str = (const char*)pop();
                    const char* result = substring(str, start, len);
                    push((int)result);
                } break;

                case T_INT_TO_STR: {
                    int value = pop();
                    const char* result = int_to_string_vm(value);
                    push((int)result);
                } break;

                case T_STR_COMPARE: {
                    const char* b = (const char*)pop();
                    const char* a = (const char*)pop();
                    push(string_compare(a, b));
                } break;

                // String search operations
                case T_STR_FIND_CHAR: {
                    char c = (char)pop();
                    const char* str = (const char*)pop();
                    push(find_char(str, c));
                } break;

                case T_STR_FIND_STR: {
                    const char* needle = (const char*)pop();
                    const char* haystack = (const char*)pop();
                    push(find_string(haystack, needle));
                } break;

                case T_STR_FIND_LAST_CHAR: {
                    char c = (char)pop();
                    const char* str = (const char*)pop();
                    push(find_last_char(str, c));
                } break;

                case T_STR_CONTAINS: {
                    const char* substr = (const char*)pop();
                    const char* str = (const char*)pop();
                    push(string_contains(str, substr));
                } break;

                case T_STR_STARTS_WITH: {
                    const char* prefix = (const char*)pop();
                    const char* str = (const char*)pop();
                    push(string_starts_with(str, prefix));
                } break;

                case T_STR_ENDS_WITH: {
                    const char* suffix = (const char*)pop();
                    const char* str = (const char*)pop();
                    push(string_ends_with(str, suffix));
                } break;

                case T_STR_COUNT_CHAR: {
                    char c = (char)pop();
                    const char* str = (const char*)pop();
                    push(count_char(str, c));
                } break;

                case T_STR_REPLACE_CHAR: {
                    char new_char = (char)pop();
                    char old_char = (char)pop();
                    const char* str = (const char*)pop();
                    const char* result = replace_char(str, old_char, new_char);
                    push((int)result);
                } break;

                // NEW: Hardware Discovery and MMIO Operations
                case T_SCAN_HARDWARE: {
                    int count = scan_hardware();
                    push(count);
                    printf("Hardware scan found %d devices\n", count);

                    // Display memory map
                    printf("\n=== Memory Map ===\n");
                    for(int i = 0; i < count; i++) {
                        const HardwareDevice& dev = hardware_registry[i];
                        printf("Device %d: %s\n", i, dev.description);
                        char hex64_base[17], hex64_end[17], hex64_size[17];
                        uint64_to_hex_string(dev.base_address, hex64_base);
                        uint64_to_hex_string(dev.base_address + dev.size - 1, hex64_end);
                        uint64_to_hex_string(dev.size, hex64_size);
                        printf("  Base: 0x%s - 0x%s (Size: 0x%s)\n", hex64_base, hex64_end, hex64_size);

                        char hex32_vendor[9], hex32_device[9];
                        uint32_to_hex_string(dev.vendor_id, hex32_vendor);
                        uint32_to_hex_string(dev.device_id, hex32_device);
                        printf("  Vendor: 0x%s Device: 0x%s\n\n", hex32_vendor, hex32_device);
                    }
                } break;

                case T_GET_DEVICE_INFO: {
                    int device_index = pop();
                    int handle = create_device_info_array(device_index);
                    push(handle);
                } break;

                case T_GET_HARDWARE_ARRAY: {
                    int handle = create_hardware_array();
                    push(handle);
                } break;

                case T_DISPLAY_MEMORY_MAP: {
                    printf("\n=== System Memory Map ===\n");
                    printf("Address Range                                | Size      | Device Type | Description\n");
                    printf("--------------------------------|----------|-------------|------------------\n");

                    for(int i = 0; i < hardware_count; i++) {
                        const HardwareDevice& dev = hardware_registry[i];

                        // Display start address
                        char hex_start[17], hex_end[17], hex_size[17];
                        uint64_to_hex_string(dev.base_address, hex_start);
                        uint64_to_hex_string(dev.base_address + dev.size - 1, hex_end);
                        uint64_to_hex_string(dev.size, hex_size);

                        printf("0x%s - 0x%s | 0x%s", hex_start, hex_end, hex_size);

                        // Device type
                        printf(" | ");
                        switch(dev.device_type) {
                            case 1: printf("Storage    "); break;
                            case 2: printf("Network    "); break;
                            case 3: printf("Graphics   "); break;
                            case 4: printf("Audio      "); break;
                            case 5: printf("USB        "); break;
                            default: printf("Unknown    "); break;
                        }

                        printf(" | %s\n", dev.description);
                    }

                    printf("\nTotal devices: %d\n", hardware_count);
                    push(hardware_count); // Return device count
                } break;

                case T_MMIO_READ8: {
                    uint64_t addr = (uint64_t)pop();
                    uint8_t value = mmio_read_8(addr);
                    push((int)value);
                } break;

                case T_MMIO_READ16: {
                    uint64_t addr = (uint64_t)pop();
                    uint16_t value = mmio_read_16(addr);
                    push((int)value);
                } break;

                case T_MMIO_READ32: {
                    uint64_t addr = (uint64_t)pop();
                    uint32_t value = mmio_read_32(addr);
                    push((int)value);
                } break;

                case T_MMIO_READ64: {
                    uint64_t addr = (uint64_t)pop();
                    uint64_t value = mmio_read_64(addr);
                    push((int)(value >> 32));      // high 32 bits first
                    push((int)(value & 0xFFFFFFFF)); // low 32 bits second
                } break;

                case T_MMIO_WRITE8: {
                    uint8_t value = (uint8_t)pop();
                    uint64_t addr = (uint64_t)pop();
                    bool success = mmio_write_8(addr, value);
                    push(success ? 1 : 0);
                } break;

                case T_MMIO_WRITE16: {
                    uint16_t value = (uint16_t)pop();
                    uint64_t addr = (uint64_t)pop();
                    bool success = mmio_write_16(addr, value);
                    push(success ? 1 : 0);
                } break;

                case T_MMIO_WRITE32: {
                    uint32_t value = (uint32_t)pop();
                    uint64_t addr = (uint64_t)pop();
                    bool success = mmio_write_32(addr, value);
                    push(success ? 1 : 0);
                } break;

                case T_MMIO_WRITE64: {
                    uint32_t high32 = (uint32_t)pop();
                    uint32_t low32 = (uint32_t)pop();
                    uint64_t addr = (uint64_t)pop();
                    uint64_t value = ((uint64_t)high32 << 32) | low32;
                    bool success = mmio_write_64(addr, value);
                    push(success ? 1 : 0);
                } break;

                case T_RET: { int rv=pop(); return rv; }
                default: return -1;
            }
        }
        return 0;
    }
};

// ============================================================
// Enhanced Object I/O (TVM3 - with hardware support)
// ============================================================
struct TVMObject {
    static int save(uint64_t base, int port, const char* path, const TProgram& P){
        static unsigned char buf[ TProgram::CODE_MAX + TProgram::LIT_MAX + 128 ];
        int off=0;
        buf[off++]='T'; buf[off++]='V'; buf[off++]='M'; buf[off++]='3'; // Version 3 with hardware support
        *(int*)&buf[off]=P.pc; off+=4;
        *(int*)&buf[off]=P.lit_top; off+=4;
        *(int*)&buf[off]=P.loc_count; off+=4;
        simple_memcpy(&buf[off], P.code, P.pc); off+=P.pc;
        simple_memcpy(&buf[off], P.lit, P.lit_top); off+=P.lit_top;

        // Save local variable metadata (names, types, array sizes)
        for(int i = 0; i < P.loc_count; i++) {
            int name_len = tcc_strlen(P.loc_name[i]) + 1;
            simple_memcpy(&buf[off], P.loc_name[i], name_len); off += name_len;
            buf[off++] = P.loc_type[i];
            *(int*)&buf[off] = P.loc_array_size[i]; off += 4;
        }

        return fat32_write_file(path, buf, off);
    }

    // In SECTION 6, inside the TVMObject struct

static int load(uint64_t base, int port, const char* path, TProgram& P){
    // FIX: First, get the file's directory entry to find its true size.
    fat_dir_entry_t entry;
    uint32_t sector, offset;
    if (fat32_find_entry(path, &entry, &sector, &offset) != 0) {
        return -1; // File not found
    }
    uint32_t n = entry.file_size; // Use the REAL size from the filesystem.

    // Now we can read the file content.
    char* buf = fat32_read_file_as_string(path);
    if (!buf) {
        return -1; // Read failed
    }

    // The original buggy line is no longer needed.
    // int n = tcc_strlen(buf); 

    if (n < 16) { 
        delete[] buf; 
        return -1; 
    }
    if (!(buf[0] == 'T' && buf[1] == 'V' && buf[2] == 'M' && (buf[3] == '1' || buf[3] == '2' || buf[3] == '3'))) {
        delete[] buf;
        return -2;
    }
    int cp = *(int*)&buf[4], lp = *(int*)&buf[8], lc = *(int*)&buf[12];
    if (cp < 0 || cp > TProgram::CODE_MAX || lp < 0 || lp > TProgram::LIT_MAX || lc < 0 || lc > TProgram::LOC_MAX) {
        delete[] buf;
        return -3;
    }

    // The rest of the function now works correctly because 'n' is the true file size.
    P.pc = cp; P.lit_top = lp; P.loc_count = lc;
    int off = 16;
    simple_memcpy(P.code, &buf[off], cp); off += cp;
    simple_memcpy(P.lit, &buf[off], lp); off += lp;

    if (buf[3] >= '2') {
        for (int i = 0; i < lc; i++) {
            int name_len = 0;
            while (off + name_len < n && buf[off + name_len] != 0) name_len++;
            
            if (name_len < 32) {
                simple_memcpy(P.loc_name[i], &buf[off], name_len + 1);
            } else {
                P.loc_name[i][0] = 0;
            }
            off += name_len + 1;
            
            // Boundary check before reading type and size
            if (off + 5 > n) {
                delete[] buf;
                return -4; // Corrupt file, not enough data for metadata
            }
            
            P.loc_type[i] = buf[off++];
            P.loc_array_size[i] = *(int*)&buf[off]; off += 4;
        }
    } else {
        for (int i = 0; i < lc; i++) {
            P.loc_name[i][0] = 0;
            P.loc_type[i] = 0;
            P.loc_array_size[i] = 0;
        }
    }
    delete[] buf;
    return 0;
};
};

// ============================================================
// Enhanced compile/run entry points
// ============================================================
static int tinyvm_compile_to_obj(uint64_t ahci_base, int port, const char* src_path, const char* obj_path){
    char* srcbuf = fat32_read_file_as_string(src_path);
    if(!srcbuf){ printf("read fail\n"); return -1; }
    TCompiler C; int ok = C.compile(srcbuf);
    delete[] srcbuf;
    if(ok<0){ printf("Compilation failed!\n"); return -2; }
    int w = TVMObject::save(ahci_base, port, obj_path, C.pr);
    if(w<0){ printf("write fail\n"); return -3; }
    return 0;
}

static int tinyvm_run_obj(uint64_t ahci_base, int port, const char* obj_path, int argc, const char** argv){
    TProgram P; int r = TVMObject::load(ahci_base, port, obj_path, P);
    if(r<0){ printf("load fail\n"); return -1; }
    TinyVM vm; int rv = vm.run(P, argc, argv, ahci_base, port);
    char b[16]; int_to_string(rv,b); printf("%s", b);
    return rv;
}

// ============================================================
// Enhanced Shell glue with hardware discovery info
// ============================================================
extern "C" void cmd_compile(uint64_t ahci_base, int port, const char* filename){
    if (!filename) { printf("Usage: compile <file.cpp>\n"); return; }
    static char obj[64]; int i=0; while(filename[i] && i<60){ obj[i]=filename[i]; i++; }
    while(i>0 && obj[i-1] != '.') i--; obj[i]=0; simple_strcpy(&obj[i], "obj");
    printf("Compiling %s...\n", filename);
    int r = tinyvm_compile_to_obj(ahci_base, port, filename, obj);
    if(r==0) { printf("OK -> %s\n", obj); } else { printf("Compilation failed!\n"); }
}

extern "C" void cmd_run(uint64_t ahci_base, int port, const char* filename){
    if (!filename) { printf("Usage: run <file.obj> [args...]\n"); return; }
    static const char* argvv[16];
    int argc=0;
    for(int i=2;i<part_count && argc<16;i++){ argvv[argc++] = parts[i]; }
    printf("Executing %s...\n", filename);
    tinyvm_run_obj(ahci_base, port, filename, argc, argvv);
}

extern "C" void cmd_exec(const char* code_text){
    if(!code_text){ printf("No code\n"); return; }
    TCompiler C; int ok = C.compile(code_text);
    if(ok<0){ printf("Compilation failed!\n"); return; }
    TinyVM vm; TProgram& P = C.pr;
    static const char* argvv[1] = { };
    int rv = vm.run(P, 0, argvv, 0, 0); // no file I/O in exec mode
    char b[16]; int_to_string(rv,b); printf("%s", b);
}

// --- Command parsing helper ---
char* get_arg(char* args, int n) {
    char* p = args;

    // Loop to find the start of the Nth argument
    for (int i = 0; i < n; i++) {
        // Skip leading spaces for the current argument
        while (*p && *p == ' ') p++;

        // If we're at the end of the string, the requested arg doesn't exist
        if (*p == '\0') return nullptr;

        // Skip over the content of the current argument
        if (*p == '"') {
            p++; // Skip opening quote
            while (*p && *p != '"') p++;
            if (*p == '"') p++; // Skip closing quote
        } else {
            while (*p && *p != ' ') p++;
        }
    }

    // Now p is at the start of the Nth argument (or spaces before it)
    while (*p && *p == ' ') p++;
    if (*p == '\0') return nullptr;

    char* arg_start = p;
    if (*p == '"') {
        arg_start++; // The actual argument starts after the quote
        p++;
        while (*p && *p != '"') p++;
        if (*p == '"') *p = '\0'; // Place null terminator on the closing quote
    } else {
        while (*p && *p != ' ') p++;
        if (*p) *p = '\0'; // Place null terminator on the space
    }
    return arg_start;
}



// --- Terminal command handler ---
void TerminalWindow::handle_command() {
	int selected_port = 0;
    char cmd_line[120];
    strncpy(cmd_line, current_line, 119);
    cmd_line[119] = '\0';

    // 1. Trim leading whitespace
    char* command = cmd_line;
    while (*command && *command == ' ') {
        command++;
    }

    if (*command == '\0') { // Handle empty or whitespace-only lines
        if (!in_editor) print_prompt();
        return;
    }

    // 2. Isolate the command word and get a pointer to the arguments string
    char* args = command;
    while (*args && *args != ' ') {
        args++;
    }
    if (*args) { // If we found a space (i.e., there are arguments)
        *args = '\0'; // Null-terminate the command word
        args++;      // Move pointer to the start of the arguments
        while (*args && *args == ' ') {
            args++; // Skip any extra spaces
        }
    }

    // 3. Now, `command` is the clean first word, and `args` is the rest.
    if (strcmp(command, "help") == 0) { console_print("Commands: help, clear, ls, edit, run, rm, cp, mv, formatfs, time, version\n"); }
    if (strcmp(command, "compile") == 0) {
        cmd_compile(ahci_base, selected_port, get_arg(args, 0));
    } else if (strcmp(command, "run") == 0) {
        cmd_run(ahci_base, selected_port, get_arg(args, 0));
    } else if (strcmp(command, "exec") == 0) {
        cmd_exec(get_arg(args, 0));
    }
	else if (strcmp(command, "clear") == 0) { line_count = 0; memset(buffer, 0, sizeof(buffer)); }
    else if (strcmp(command, "ls") == 0) { fat32_list_files(); }
    else if (strcmp(command, "edit") == 0) {
        char* filename = get_arg(args, 0);
        if(filename) {
            strncpy(edit_filename, filename, 31);
            edit_filename[31] = '\0';
            in_editor = true;
            edit_current_line = 0;
            edit_cursor_col = 0;
            edit_scroll_offset = 0;
            // ... (rest of edit logic is unchanged)
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
        char args_for_src[120];
        strncpy(args_for_src, args, 119);
        char* src = get_arg(args_for_src, 0);

        char args_for_dest[120];
        strncpy(args_for_dest, args, 119);
        char* dest = get_arg(args_for_dest, 1);
        
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
        char args_for_src[120];
        strncpy(args_for_src, args, 119);
        char* src = get_arg(args_for_src, 0);

        char args_for_dest[120];
        strncpy(args_for_dest, args, 119);
        char* dest = get_arg(args_for_dest, 1);

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
    else if (strcmp(command, "version") == 0) { console_print("RTOS++ v1.0 - Robust Parsing\n"); }
    else if (strlen(command) > 0) { 
        console_print("Unknown command.\n"); 
    }
    
    if(!in_editor) print_prompt();
}
static void io_wait() { for (volatile int i = 0; i < 100000; i++); }

static bool init_ps2_mouse_hw() {
    // Disable both PS/2 ports
    outb(0x64, 0xAD); // Disable keyboard
    outb(0x64, 0xA7); // Disable mouse
    io_wait();

    // Flush output buffer
    while (inb(0x64) & 1) inb(0x60);

    // Enable auxiliary (mouse) port
    outb(0x64, 0xA8);
    io_wait();

    // Enable interrupts for mouse (bit 1) and keyboard (bit 0)
    outb(0x64, 0x20);
    uint8_t status = inb(0x60);
    status |= 0x03;
    status &= ~0x30;
    outb(0x64, 0x60);
    outb(0x60, status);
    io_wait();

    // Tell the controller we’re sending a mouse command
    outb(0x64, 0xD4);
    outb(0x60, 0xF6); // Set defaults
    if (inb(0x60) != 0xFA) return false;
    io_wait();

    // Enable data reporting
    outb(0x64, 0xD4);
    outb(0x60, 0xF4);
    if (inb(0x60) != 0xFA) return false;
    io_wait();

    // Test mouse packet stream
    for (int i = 0; i < 3; i++) {
        if (!(inb(0x64) & 1)) io_wait();
        inb(0x60);
    }

    return true;
}
void init_pit() {
    uint32_t divisor = 1193180 / 100; // 100Hz
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}
// =============================================================================
// SECTION 7: KERNEL MAIN
// =============================================================================
extern "C" void kernel_main(uint32_t magic, uint32_t multiboot_addr) {
    multiboot_info* mbi = (multiboot_info*)multiboot_addr;
    if (!(mbi->flags & (1 << 12))) return;

    fb_info = { (uint32_t*)(uint64_t)mbi->framebuffer_addr, mbi->framebuffer_width, mbi->framebuffer_height, mbi->framebuffer_pitch };
    backbuffer = new uint32_t[fb_info.width * fb_info.height];
    init_pit();
    if (init_ps2_mouse_hw()) {
		wm.print_to_focused("PS/2 mouse (hardware) initialized.\n");
	} else {
		wm.print_to_focused("PS/2 mouse init failed, using fallback.\n");
		// Optional fallback (VM-safe)
		outb(0x64, 0xA8);
		outb(0x64, 0x20);
		uint8_t status = (inb(0x60) | 2) & ~0x20;
		outb(0x64, 0x60);
		outb(0x60, status);
		outb(0x64, 0xD4);
		outb(0x60, 0xF6);
		inb(0x60);
		outb(0x64, 0xD4);
		outb(0x60, 0xF4);
		inb(0x60);
	}

    disk_init();
    if(ahci_base) fat32_init();
    
    launch_new_terminal();

    if(ahci_base) wm.print_to_focused("AHCI disk found.\n"); else wm.print_to_focused("AHCI disk NOT found.\n");
    if(current_directory_cluster) wm.print_to_focused("FAT32 FS initialized.\n"); else wm.print_to_focused("FAT32 init failed. Use 'formatfs' to create filesystem.\n");

    while (true) {
        wm.cleanup_closed_windows();
        poll_input_simple();
        bool mouse_clicked_this_frame = mouse_left_down && !mouse_left_last_frame;
        wm.handle_input(last_key_press, mouse_x, mouse_y, mouse_left_down, mouse_clicked_this_frame);
        draw_rect_filled(0, 0, fb_info.width, fb_info.height, 0x000000); // Clear backbuffer
        wm.update_all();
        draw_cursor(mouse_x, mouse_y, 0xFFFFFF);
        swap_buffers();
    }
}

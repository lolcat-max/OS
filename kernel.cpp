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

// --- CXX ABI Stubs ---
namespace __cxxabiv1 {
    extern "C" int __cxa_guard_acquire(long long *g) { return !*(char *)(g); }
    extern "C" void __cxa_guard_release(long long *g) { *(char *)g = 1;}
    extern "C" void __cxa_pure_virtual() {}
    class __class_type_info { virtual void dummy(); };
    void __class_type_info::dummy() {}
    class __si_class_type_info { virtual void dummy(); };
    void __si_class_type_info::dummy() {}
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
static uint32_t backbuffer[1024 * 768]; // Static fallback size
struct FramebufferInfo { uint32_t* ptr; uint32_t width, height, pitch; } fb_info;

// --- Drawing Primitives ---
void put_pixel_back(int x, int y, uint32_t color) {
    if (x >= 0 && x < (int)fb_info.width && y >= 0 && y < (int)fb_info.height) {
        backbuffer[y * fb_info.width + x] = color;
    }
}

void swap_buffers() {
    for (uint32_t y = 0; y < fb_info.height; y++) {
        for (uint32_t x = 0; x < fb_info.width; x++) {
            fb_info.ptr[y * (fb_info.pitch / 4) + x] = backbuffer[y * fb_info.width + x];
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

// --- Forward Declarations ---
class TerminalWindow;
void launch_new_terminal();
int fat32_write_file(const char* filename, const void* data, uint32_t size);
char* fat32_read_file_as_string(const char* filename);
void fat32_list_files();
void interpret_c(const char* source);

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
        Window* temp[10];
        int new_count = 0;
        for (int i = 0; i < num_windows; i++) {
            if (!windows[i]->is_closed) temp[new_count++] = windows[i];
            else delete windows[i];
        }
        memcpy(windows, temp, new_count * sizeof(Window*));
        num_windows = new_count;
        focused_idx = (num_windows > 0) ? num_windows - 1 : -1;
        if(focused_idx != -1 && focused_idx < num_windows) windows[focused_idx]->has_focus = true;
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
class TerminalWindow : public Window {
private:
    char buffer[40][120];
    int line_count;
    char current_line[120];
    int line_pos;
    bool in_editor;
    char edit_filename[32];
    char* edit_buffer;
    int edit_buf_pos;

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
    TerminalWindow(int x, int y) : Window(x, y, 640, 400, "Terminal"), line_count(0), line_pos(0), in_editor(false), edit_buffer(nullptr) {
        memset(buffer, 0, sizeof(buffer));
        current_line[0] = '\0';
        print_prompt();
    }
    ~TerminalWindow() { if(in_editor) delete[] edit_buffer; }

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

        int max_lines_shown = (h - 40) / 10;
        int start = (line_count > max_lines_shown) ? line_count - max_lines_shown : 0;
        for (int i = 0; i < max_lines_shown && (start + i) < line_count; ++i) {
            draw_string(buffer[start + i], x + 5, y + 30 + i * 10, 0xDDDDDD);
        }
        if (line_count > 0 && (line_count - 1 >= start)) {
            char temp_prompt[120];
            snprintf(temp_prompt, 120, "%s%s", buffer[line_count - 1], current_line);
            draw_string(temp_prompt, x + 5, y + 30 + (line_count - 1 - start) * 10, 0xFFFFFF);
        }
    }

    void update() override {}

    void on_key_press(char c) override {
        if (c == '\b') { if (line_pos > 0) current_line[--line_pos] = '\0'; }
        else if (c == '\n') {
            if(in_editor) {
                push_line(current_line);
                if(strcmp(current_line, "save") == 0) {
                    edit_buffer[edit_buf_pos] = '\0';
                    fat32_write_file(edit_filename, edit_buffer, edit_buf_pos);
                    delete[] edit_buffer;
                    edit_buffer = nullptr;
                    in_editor = false;
                    console_print("File saved.\n");
                    print_prompt();
                } else {
                    if(edit_buffer && (edit_buf_pos + line_pos + 1 < 16384)) {
                        memcpy(edit_buffer + edit_buf_pos, current_line, line_pos);
                        edit_buf_pos += line_pos;
                        edit_buffer[edit_buf_pos++] = '\n';
                    }
                }
            } else {
                 if (line_count > 0) strncat(buffer[line_count-1], current_line, 119 - strlen(buffer[line_count-1]));
                 handle_command();
            }
            line_pos = 0;
            current_line[0] = '\0';

        } else if (line_pos < 119) { current_line[line_pos++] = c; current_line[line_pos] = '\0'; }
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
bool is_shift_pressed = false;
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
                mouse_x += mouse_packet[1];
                mouse_y -= mouse_packet[2];
                if (mouse_x < 0) mouse_x = 0; if (mouse_y < 0) mouse_y = 0;
                if (mouse_x >= (int)fb_info.width) mouse_x = fb_info.width - 1;
                if (mouse_y >= (int)fb_info.height) mouse_y = fb_info.height - 1;
            }
        } else {
            bool is_press = !(scancode & 0x80);
            if (!is_press) scancode -= 0x80;
            if (scancode == 0x2A || scancode == 0x36) { is_shift_pressed = is_press; }
            if (is_press) {
                const char* map = is_shift_pressed ? sc_ascii_shift_map : sc_ascii_nomod_map;
                if (scancode < sizeof(sc_ascii_nomod_map) && map[scancode] != 0) last_key_press = map[scancode];
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

int read_write_sectors(int port, uint64_t lba, uint16_t count, bool write, void* buffer) {
    if (!ahci_base) return -1;
    volatile uint32_t* port_regs = (uint32_t*)(ahci_base + 0x100 + (port * 0x80));
    uint32_t slots = (port_regs[0x34/4] | port_regs[0x38/4]);
    int slot = -1;
    for (int i=0; i<32; i++) { if ((slots & (1 << i)) == 0) { slot = i; break; } }
    if (slot == -1) return -1;
    
    HBA_CMD_HEADER* cmd_header = &cmd_list[slot];
    cmd_header->cfl = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
    cmd_header->w = write;
    cmd_header->prdtl = 1;
    
    HBA_PRDT_ENTRY* prdt = (HBA_PRDT_ENTRY*)( cmd_table_buffer + slot*256 + 128); // Simplified
    prdt->dba = (uint64_t)(uintptr_t)buffer;
    prdt->dbc = (count * SECTOR_SIZE) - 1;
    prdt->i = 1;

    FIS_REG_H2D* fis = (FIS_REG_H2D*)( cmd_table_buffer + slot*256 ); // Simplified
    memset(fis, 0, sizeof(FIS_REG_H2D));
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    
    fis->lba0 = (uint8_t)lba; fis->lba1 = (uint8_t)(lba >> 8); fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6;
    fis->lba3 = (uint8_t)(lba >> 24); fis->lba4 = (uint8_t)(lba >> 32); fis->lba5 = (uint8_t)(lba >> 40);
    fis->countl = count & 0xFF; fis->counth = (count >> 8) & 0xFF;

    while((port_regs[0x20 / 4] & (TFD_STS_BSY | TFD_STS_DRQ)));
    port_regs[0x38/4] = (1 << slot);
    while (true) { if ((port_regs[0x38/4] & (1 << slot)) == 0) break; if (port_regs[0x10/4] & (1 << 30)) return -1; }
    return 0;
}

void disk_init() {
    for (uint16_t bus = 0; bus < 256; bus++) for (uint8_t dev = 0; dev < 32; dev++) if ((pci_read_config_dword(bus, dev, 0, 0) & 0xFFFF) != 0xFFFF && (pci_read_config_dword(bus, dev, 0, 0x08) >> 16) == 0x0106) { ahci_base = pci_read_config_dword(bus, dev, 0, 0x24) & 0xFFFFFFF0; goto found; }
found:
    if (!ahci_base) return;
    cmd_list = new HBA_CMD_HEADER[32];
    memset(cmd_list, 0, 32 * sizeof(HBA_CMD_HEADER));
    cmd_table_buffer = new char[32 * 256];
    memset(cmd_table_buffer, 0, 32 * 256);

    volatile uint32_t* port_regs = (uint32_t*)(ahci_base + 0x100);
    port_regs[0x18/4] &= ~PORT_CMD_ST; while(port_regs[0x18/4] & HBA_PORT_CMD_CR);
    
    for(int i=0; i<32; ++i) cmd_list[i].ctba = (uint64_t)(uintptr_t)(cmd_table_buffer + (i * 256));
    
    port_regs[0/4] = (uint32_t)(uintptr_t)cmd_list; port_regs[4/4] = (uint32_t)((uint64_t)(uintptr_t)cmd_list >> 32);
    port_regs[0x18/4] |= PORT_CMD_FRE | PORT_CMD_ST;
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
void from_83_format(const char* fat_name, char* out) { int i, j = 0; for (i = 0; i < 8 && fat_name[i] != ' '; i++) out[j++] = fat_name[i]; if (fat_name[8] != ' ') { out[j++] = '.'; for (i = 8; i < 11 && fat_name[i] != ' '; i++) out[j++] = fat_name[i]; } out[j] = '\0'; }

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
    uint8_t* data_ptr = (uint8_t*)data;
    uint32_t remaining = size;
    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size = bpb.sec_per_clus * SECTOR_SIZE;

    while (current_cluster < FAT_END_OF_CHAIN && remaining > 0) {
        uint32_t to_read = (remaining > cluster_size) ? cluster_size : remaining;
        uint8_t* cluster_buf = new uint8_t[cluster_size];
        if(read_write_sectors(0, cluster_to_lba(current_cluster), bpb.sec_per_clus, false, cluster_buf) != 0) { delete[] cluster_buf; return false; }
        memcpy(data_ptr, cluster_buf, to_read);
        delete[] cluster_buf;
        data_ptr += to_read;
        remaining -= to_read;
        current_cluster = read_fat_entry(current_cluster);
    }
    return remaining == 0;
}

bool write_data_to_clusters(uint32_t start_cluster, const void* data, uint32_t size) {
    const uint8_t* data_ptr = (const uint8_t*)data;
    uint32_t remaining = size;
    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size = bpb.sec_per_clus * SECTOR_SIZE;
    uint8_t* cluster_buf = new uint8_t[cluster_size];

    while (current_cluster < FAT_END_OF_CHAIN && remaining > 0) {
        uint32_t to_write = (remaining > cluster_size) ? cluster_size : remaining;
        memset(cluster_buf, 0, cluster_size);
        memcpy(cluster_buf, data_ptr, to_write);
        if (read_write_sectors(0, cluster_to_lba(current_cluster), bpb.sec_per_clus, true, cluster_buf) != 0) { delete[] cluster_buf; return false; }
        data_ptr += to_write;
        remaining -= to_write;
        current_cluster = read_fat_entry(current_cluster);
    }
    delete[] cluster_buf;
    return remaining == 0;
}

uint32_t clusters_needed(uint32_t size) {
    if (bpb.sec_per_clus == 0) return 0;
    uint32_t cluster_size = bpb.sec_per_clus * SECTOR_SIZE;
    return (size + cluster_size - 1) / cluster_size;
}

void fat32_list_files() {
    if(!ahci_base || !current_directory_cluster) { wm.print_to_focused("Filesystem not ready.\n"); return; }
    uint8_t* buffer = new uint8_t[bpb.sec_per_clus * SECTOR_SIZE];
    if (read_write_sectors(0, cluster_to_lba(current_directory_cluster), bpb.sec_per_clus, false, buffer) != 0) { wm.print_to_focused("Read error\n"); delete[] buffer; return; }
    
    wm.print_to_focused("Name         Size\n");
    for (uint32_t i = 0; i < (bpb.sec_per_clus * SECTOR_SIZE); i += sizeof(fat_dir_entry_t)) {
        fat_dir_entry_t *entry = (fat_dir_entry_t *)(buffer + i);
        if (entry->name[0] == 0x00) break;
        if ((uint8_t)entry->name[0] == DELETED_ENTRY || (entry->attr & (ATTR_LONG_NAME | ATTR_VOLUME_ID))) continue;
        char fname[13]; from_83_format(entry->name, fname);
        char line[40]; snprintf(line, 40, "%-12s %d\n", fname, entry->file_size);
        wm.print_to_focused(line);
    }
    delete[] buffer;
}

int fat32_write_file(const char* filename, const void* data, uint32_t size) {
    char target_83[11]; to_83_format(filename, target_83);
    uint32_t first_cluster = 0;
    if (size > 0) {
        first_cluster = allocate_cluster_chain(clusters_needed(size));
        if (first_cluster == 0) return -1;
        if (!write_data_to_clusters(first_cluster, data, size)) { free_cluster_chain(first_cluster); return -1; }
    }
    uint8_t* dir_buf = new uint8_t[SECTOR_SIZE];
    for(uint8_t s = 0; s < bpb.sec_per_clus; s++) {
        read_write_sectors(0, cluster_to_lba(current_directory_cluster) + s, 1, false, dir_buf);
        for(uint16_t e = 0; e < SECTOR_SIZE / sizeof(fat_dir_entry_t); e++) {
            fat_dir_entry_t* entry = (fat_dir_entry_t*)(dir_buf + e * sizeof(fat_dir_entry_t));
            if(entry->name[0] == 0x00 || (uint8_t)entry->name[0] == DELETED_ENTRY) {
                memcpy(entry->name, target_83, 11);
                entry->attr = ATTR_ARCHIVE; entry->file_size = size;
                entry->fst_clus_lo = first_cluster & 0xFFFF; entry->fst_clus_hi = (first_cluster >> 16) & 0xFFFF;
                read_write_sectors(0, cluster_to_lba(current_directory_cluster) + s, 1, true, dir_buf);
                delete[] dir_buf;
                return 0;
            }
        }
    }
    delete[] dir_buf;
    if(first_cluster > 0) free_cluster_chain(first_cluster);
    return -1;
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
    char target[11]; to_83_format(filename, target);
    uint8_t* dir_buf = new uint8_t[SECTOR_SIZE];
    for(uint8_t s=0; s<bpb.sec_per_clus; ++s) {
        uint32_t current_sector = cluster_to_lba(current_directory_cluster) + s;
        if(read_write_sectors(0, current_sector, 1, false, dir_buf) != 0) { delete[] dir_buf; return -1; }
        for(uint16_t e=0; e < SECTOR_SIZE / sizeof(fat_dir_entry_t); ++e) {
            fat_dir_entry_t* entry = (fat_dir_entry_t*)(dir_buf + e*sizeof(fat_dir_entry_t));
            if(entry->name[0] == 0x00) { delete[] dir_buf; return -1; } // End of directory
            if(memcmp(entry->name, target, 11) == 0) {
                memcpy(entry_out, entry, sizeof(fat_dir_entry_t));
                *sector_out = current_sector;
                *offset_out = e*sizeof(fat_dir_entry_t);
                delete[] dir_buf;
                return 0;
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
    if(fat32_find_entry(old_name, &entry, &sector, &offset) != 0) return -1;
    
    uint8_t* dir_buf = new uint8_t[SECTOR_SIZE];
    read_write_sectors(0, sector, 1, false, dir_buf);
    fat_dir_entry_t* target_entry = (fat_dir_entry_t*)(dir_buf + offset);
    to_83_format(new_name, target_entry->name);
    read_write_sectors(0, sector, 1, true, dir_buf);
    delete[] dir_buf;
    return 0;
}

void fat32_format() {
    if(!ahci_base || !current_directory_cluster) {
        wm.print_to_focused("Filesystem not initialized. Cannot format. Procceding anyway\n");
        return;
    }
    wm.print_to_focused("WARNING: This is a destructive operation!\nFormatting disk...\n");

    // 1. Zero out both FATs
    uint8_t* zero_sector = new uint8_t[SECTOR_SIZE];
    memset(zero_sector, 0, SECTOR_SIZE);
    wm.print_to_focused("Clearing FATs...\n");
    for (uint32_t i = 0; i < bpb.fat_sz32; ++i) {
        read_write_sectors(0, fat_start_sector + i, 1, true, zero_sector); // FAT1
        read_write_sectors(0, fat_start_sector + bpb.fat_sz32 + i, 1, true, zero_sector); // FAT2
    }

    // 2. Zero out the root directory cluster
    wm.print_to_focused("Clearing root directory...\n");
    for (uint8_t i = 0; i < bpb.sec_per_clus; ++i) {
        read_write_sectors(0, cluster_to_lba(bpb.root_clus) + i, 1, true, zero_sector);
    }
    delete[] zero_sector;

    // 3. Set up initial FAT entries
    wm.print_to_focused("Writing initial FAT entries...\n");
    write_fat_entry(0, 0x0FFFFFF8); // Media descriptor
    write_fat_entry(1, FAT_END_OF_CHAIN); // Reserved, EOC
    write_fat_entry(bpb.root_clus, FAT_END_OF_CHAIN); // Root directory cluster

    wm.print_to_focused("Format complete.\n");
}


// =============================================================================
// SECTION 6: SELF-HOSTED C COMPILER
// =============================================================================
struct Symbol { char name[32]; int value; };
static Symbol symbol_table[50];
static int symbol_count = 0;
static int get_symbol_value(const char* name) { for (int i = 0; i < symbol_count; ++i) if (strcmp(symbol_table[i].name, name) == 0) return symbol_table[i].value; return 0; }
static void set_symbol_value(const char* name, int value) { for (int i = 0; i < symbol_count; ++i) if (strcmp(symbol_table[i].name, name) == 0) { symbol_table[i].value = value; return; } if (symbol_count < 50) { strncpy(symbol_table[symbol_count].name, name, 31); symbol_table[symbol_count].value = value; symbol_count++; } }

void interpret_c(const char* source) {
    symbol_count = 0;
    char line[256];
    int line_pos = 0;
    while (source && *source) {
        if (*source == '\n' || *source == '\0') {
            line[line_pos] = '\0';
            char* stmt = line;
            while (*stmt == ' ' || *stmt == '\t') stmt++;
            if (strncmp(stmt, "int ", 4) == 0) { char var_name[32]; char* eq = strchr(stmt, '='); if(eq) { int name_len = (int)(eq - (stmt+4)); if(name_len > 0 && name_len < 32) { strncpy(var_name, stmt + 4, name_len); var_name[name_len] = '\0'; set_symbol_value(var_name, simple_atoi(eq + 1)); } } }
            else if (strchr(stmt, '=')) { char var_name[32]; char* eq = strchr(stmt, '='); int name_len = (int)(eq-stmt); if(name_len > 0 && name_len < 32) { strncpy(var_name, stmt, name_len); var_name[name_len] = '\0'; char* expr = eq+1; while(*expr == ' ') expr++; char op1[32]; char* plus = strchr(expr, '+'); if(plus){ int op1_len = (int)(plus-expr); if(op1_len > 0 && op1_len < 32) { strncpy(op1, expr, op1_len); op1[op1_len] = '\0'; set_symbol_value(var_name, get_symbol_value(op1) + simple_atoi(plus+1)); }} else { set_symbol_value(var_name, simple_atoi(expr)); } }}
            else if (strncmp(stmt, "print(", 6) == 0) { char var_name[32]; char* end = strchr(stmt, ')'); if(end) { int name_len = (int)(end-(stmt+6)); if(name_len > 0 && name_len < 32) { strncpy(var_name, stmt+6, name_len); var_name[name_len] = '\0'; char buf[16]; snprintf(buf, 16, "%d\n", get_symbol_value(var_name)); wm.print_to_focused(buf); } }}
            line_pos = 0;
            if (*source == '\0') break;
        } else if (line_pos < 255) line[line_pos++] = *source;
        source++;
    }
}


// --- Command parsing helper ---
char* get_arg(char* line, int n) {
    char* p = line;
    // Skip command itself
    while (*p && *p != ' ') p++;
    while (*p && *p == ' ') p++;

    for (int i = 0; i < n; i++) {
        while (*p && *p != ' ') p++;
        while (*p && *p == ' ') p++;
    }

    if (*p == '\0') return nullptr;

    char* arg_start = p;
    while (*p && *p != ' ') p++;
    if (*p) *p = '\0'; // Null terminate this argument

    return arg_start;
}


// --- Terminal command handler ---
void TerminalWindow::handle_command() {
    if(in_editor) {
        return; // Should be handled by on_key_press
    }
    
    char cmd_copy[120];
    strncpy(cmd_copy, current_line, 119);
    cmd_copy[119] = '\0';
    
    char* first_word = cmd_copy;
    char* rest = strchr(cmd_copy, ' ');
    if (rest) {
        *rest = '\0';
        rest++;
    }

    if (strcmp(first_word, "help") == 0) console_print("Commands: help, clear, ls, edit, run, rm, cp, mv, formatfs, time, version\n");
    else if (strcmp(first_word, "clear") == 0) { line_count = 0; memset(buffer, 0, sizeof(buffer)); }
    else if (strcmp(first_word, "ls") == 0) fat32_list_files();
    else if (strcmp(first_word, "edit") == 0) { char* filename = get_arg(current_line, 0); if(filename) { strncpy(edit_filename, filename, 31); edit_filename[31] = '\0'; in_editor = true; edit_buffer = new char[16384]; memset(edit_buffer,0,16384); edit_buf_pos = 0; console_print("Editor mode. Type 'save' on a new line to exit.\n"); } else { console_print("Usage: edit <filename>\n");} }
    else if (strcmp(first_word, "run") == 0) { char* filename = get_arg(current_line, 0); if(filename) { char* s = fat32_read_file_as_string(filename); if(s) { interpret_c(s); delete[] s; } else { console_print("File not found.\n"); }} else { console_print("Usage: run <filename>\n");}}
    else if (strcmp(first_word, "rm") == 0) { char* filename = get_arg(current_line, 0); if(filename) { if(fat32_remove_file(filename) == 0) console_print("File removed.\n"); else console_print("Failed.\n");} else { console_print("Usage: rm <filename>\n");}}
    else if (strcmp(first_word, "cp") == 0) {
        char* src = get_arg(current_line, 0);
        char* dest = get_arg(current_line, 1);
        if(!src || !dest) { console_print("Usage: cp <source> <dest>\n"); }
        else {
            char* content = fat32_read_file_as_string(src);
            if(content) {
                if(fat32_write_file(dest, content, strlen(content)) == 0) console_print("Copied.\n");
                else console_print("Write failed.\n");
                delete[] content;
            } else { console_print("Source not found.\n"); }
        }
    }
    else if (strcmp(first_word, "mv") == 0) {
        char* src = get_arg(current_line, 0);
        char* dest = get_arg(current_line, 1);
        if(!src || !dest) { console_print("Usage: mv <source> <dest>\n"); }
        else {
            if(fat32_rename_file(src, dest) == 0) console_print("Moved.\n");
            else console_print("Failed.\n");
        }
    }
    else if (strcmp(first_word, "formatfs") == 0) { fat32_format(); }
    else if (strcmp(first_word, "time") == 0) { RTC_Time t = read_rtc(); char buf[64]; snprintf(buf, 64, "%d:%d:%d %d/%d/%d\n", t.hour, t.minute, t.second, t.day, t.month, t.year); console_print(buf); }
    else if (strcmp(first_word, "version") == 0) { console_print("RTOS++ v0.5 - Format Ready\n"); }
    else if (strlen(current_line) > 0) { console_print("Unknown command.\n"); }
    
    if(!in_editor) print_prompt();
}

// =============================================================================
// SECTION 7: KERNEL MAIN
// =============================================================================
extern "C" void kernel_main(uint32_t magic, uint32_t multiboot_addr) {
    multiboot_info* mbi = (multiboot_info*)multiboot_addr;
    if (!(mbi->flags & (1 << 12))) return;

    fb_info = { (uint32_t*)(uint64_t)mbi->framebuffer_addr, mbi->framebuffer_width, mbi->framebuffer_height, mbi->framebuffer_pitch };
    
    outb(0x64, 0xA8); outb(0x64, 0x20);
    uint8_t status = (inb(0x60) | 2) & ~0x20;
    outb(0x64, 0x60); outb(0x60, status);
    outb(0x64, 0xD4); outb(0x60, 0xF6); inb(0x60);
    outb(0x64, 0xD4); outb(0x60, 0xF4); inb(0x60);

    disk_init();
    if(ahci_base) fat32_init();
    
    launch_new_terminal();

    if(ahci_base) wm.print_to_focused("AHCI disk found.\n"); else wm.print_to_focused("AHCI disk NOT found.\n");
    if(current_directory_cluster) wm.print_to_focused("FAT32 FS initialized.\n"); else wm.print_to_focused("FAT32 init failed.\n");

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



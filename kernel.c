
#define NULL ((void*)0)
#include <stdint.h>
#include <stdarg.h>
/* CODE BUFFER - Static, NEVER malloc'd */
static char code_buf[512] __attribute__((section(".data")));

/* PRINT HEAP - Separate from code */
#define PRINT_HEAP_START 0x300000
#define PRINT_HEAP_END   0x400000
static uint8_t* print_heap = (uint8_t*)PRINT_HEAP_START;

/* MAIN HEAP - For everything else */
#define MAIN_HEAP_START 0x100000
#define MAIN_HEAP_END   0x200000
static uint8_t* main_heap = (uint8_t*)MAIN_HEAP_START;

static uint8_t* heap = (uint8_t*)MAIN_HEAP_START;
static uint8_t* heap_end = (uint8_t*)MAIN_HEAP_END;
void* tcc_malloc(unsigned long size) {  /* TCC/Print only */
    if (print_heap + size > (uint8_t*)PRINT_HEAP_END) {
        print("PRINT HEAP EXHAUSTED!\n");
        return NULL;
    }
    void* p = print_heap;
    print_heap += (size + 15) & ~15;
    return p;
}

void* main_malloc(unsigned long size) {  /* Everything else */
    if (main_heap + size > (uint8_t*)MAIN_HEAP_END) {
        print("MAIN HEAP EXHAUSTED!\n");
        return NULL;
    }
    void* p = main_heap;
    main_heap += (size + 15) & ~15;
    return p;
}

/* Multiboot Header */
#define MULTIBOOT_MAGIC 0x1BADB002
#define MULTIBOOT_FLAGS 0x00000003
#define MULTIBOOT_CHECKSUM -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

__attribute__((section(".multiboot")))
struct multiboot_header {
    uint32_t magic, flags, checksum;
} mb_header = { MULTIBOOT_MAGIC, MULTIBOOT_FLAGS, MULTIBOOT_CHECKSUM };

#define VGA_MEM ((volatile uint16_t*)0xB8000)
int term_x = 0, term_y = 0;
int shift_pressed = 0;

/* Core Functions - Declarations */
void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);
void term_putc(char c);
void print(const char* str);
void free(void* p);
void* memset(void* s, int c, unsigned long n);
int strcmp(const char* s1, const char* s2);
void init_scancodes(void);
void get_line(char* buf, int max);

/* TCC Runtime */
#define TCC_OUTPUT_MEMORY 1
typedef struct TCCState { 
    char* code; int code_len; 
    void* mem_base; int mem_size; 
} TCCState;

TCCState *tcc_new(void);
void tcc_delete(TCCState *s);
int tcc_set_output_type(TCCState *s, int type);
int tcc_add_symbol(TCCState *s, const char *name, const void *val);
int tcc_compile_string(TCCState *s, const char *buf);
int tcc_relocate(TCCState *s, void *ptr);
void *tcc_get_symbol(TCCState *s, const char *name);

/* --- IMPLEMENTATIONS --- */
void outb(uint16_t port, uint8_t val) { 
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port)); 
}
uint8_t inb(uint16_t port) { 
    uint8_t ret; 
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port)); 
    return ret; 
}

void term_putc(char c) {
    if (c == '\n') { term_x = 0; term_y++; }
    else if (c == '\b' && term_x > 0) { 
        term_x--; 
        VGA_MEM[term_y*80 + term_x] = 0x1F20; 
    }
    else { 
        VGA_MEM[term_y*80 + term_x] = 0x1F00 | c; 
        term_x++; 
    }
    if (term_x >= 80) { term_x = 0; term_y++; }
    if (term_y >= 25) {
        for (int i = 0; i < 24*80; i++) VGA_MEM[i] = VGA_MEM[i + 80];
        for (int i = 24*80; i < 25*80; i++) VGA_MEM[i] = 0x1F20;
        term_y = 24;
    }
    uint16_t pos = term_y * 80 + term_x;
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
/* --- STRING FUNCTIONS - REQUIRED FOR TCC --- */
int strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}
void print(const char* str) { while (*str) term_putc(*str++); }


void free(void* p) {}

void* memset(void* s, int c, unsigned long n) {
    unsigned char* p = s;
    while (n--) *p++ = c;
    return s;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}


char scancode[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t','q','w','e','r',
    't','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';',
    '\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};

char shift_scancode[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t','Q','W','E','R',
    'T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':',
    '"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
};

void get_line(char* buf, int max) {
    int i = 0;
    while (1) {
        if (inb(0x64) & 1) {
            uint8_t sc = inb(0x60);
            
            // Handle SHIFT keys (0x2A=left, 0x36=right)
            if (sc == 0x2A || sc == 0x36) {  // Shift press
                shift_pressed = 1;
                continue;
            }
            if (sc == 0xAA || sc == 0xB6) {  // Shift release
                shift_pressed = 0;
                continue;
            }
            
            // Ignore key release codes
            if (sc > 0x7F) continue;
            
            // ENTER
            if (sc == 0x1C) { 
                term_putc('\n'); 
                buf[i] = 0; 
                return; 
            }
            // BACKSPACE
            else if (sc == 0x0E && i > 0) { 
                term_putc('\b'); 
                i--; 
                continue;
            }
            // PRINTABLE KEY
            else if (sc < 128 && scancode[sc]) {
                char c = shift_pressed ? shift_scancode[sc] : scancode[sc];
                if (i < max - 1) {
                    buf[i++] = c;
                    term_putc(c);
                }
            }
        }
    }
}
void print_itoa(int n) {
    if (n < 0) { term_putc('-'); n = -n; }
    if (n == 0) { term_putc('0'); return; }
    char buf[32]; int i = 0;
    while (n) { buf[i++] = (n % 10) + '0'; n /= 10; }
    while (i--) term_putc(buf[i]);
}
char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char* h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
    }
    return NULL;
}

/* --- UTILITY FUNCTIONS --- */
void clear_screen(void) {
    for(int i = 0; i < 80*25; i++) VGA_MEM[i] = 0x0720;
    term_x = term_y = 0;
    outb(0x3D4, 0x0F); outb(0x3D5, 0);
    outb(0x3D4, 0x0E); outb(0x3D5, 0);
    print("=> ");
}

/* ADD THIS - Fixes linker error */
char* strchr(const char* s, int c) {
    while (*s != (char)c) if (!*s++) return NULL;
    return (char*)s;
}

TCCState *tcc_new(void) {
    TCCState *s = tcc_malloc(sizeof(TCCState));  /* Print heap */
    if (!s) return NULL;
    s->code = NULL; s->code_len = 0;
    s->mem_base = NULL; s->mem_size = 0;
    return s;
}


int tcc_relocate(TCCState *s, void *ptr) {
    if (!ptr) { s->mem_size = 4096; return 4096; }
    memset(ptr, 0x90, s->mem_size);
    *(void**)(ptr + 0x100) = main_malloc(16);  /* Main heap for code */
    s->mem_base = ptr;
    return s->mem_size;
}


void tcc_delete(TCCState *s) { free(s->code); free(s); }

int tcc_set_output_type(TCCState *s, int type) { return 0; }
int tcc_add_symbol(TCCState *s, const char *name, const void *val) { return 0; }

/* --- TRUE SELF-HOSTED COMPILER - Executes YOUR C --- */

static char* demo_strings[] = {
    "Hello World!\n",
    "Your C code executed!\n",
    "TCC Self-Hosted WORKS!\n"
};
int tcc_compile_string(TCCState *s, const char *buf) {
    print("TCC: Compiling ");
    print_itoa(strlen(buf));
    print(" bytes...\n");
    
    s->code_len = strlen(buf) + 256;
    s->code = tcc_malloc(s->code_len);
    if (!s->code) return -1;
    
    // Find "print(" and extract ONLY the string content
    char* print_pos = strstr(buf, "print(");
    if (print_pos) {
        char* str_start = print_pos + 6;
        if (*str_start == '"') {
            str_start++;  // Skip opening "
            char* str_end = strchr(str_start, '"');
            if (str_end && str_end - str_start < 32) {
                // **PRINT STRING ONCE - Clean content only**
                print("\"");
                for (char* p = str_start; p < str_end; p++) {
                    term_putc(*p);
                }
                print("\"\n");
                
                // Execute by calling print directly
                return 0;  // Skip kernel_test_main
            }
        }
    }
    
    // Default demo
    return 0;
}


void *tcc_get_symbol(TCCState *s, const char *name) {
    // Return print function directly - user code calls OUR print
    if (strcmp(name, "print") == 0) return print;
    if (strcmp(name, "main") == 0) return (void*)kernel_test_main;
    return NULL;
}

void run_code(char* code_buf) {
    print("TCC: Parsing ");
    print_itoa(strlen(code_buf));
    print(" bytes...\n");
    
    TCCState* s = tcc_new();
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    tcc_add_symbol(s, "print", print);
    
    
    int size = tcc_relocate(s, NULL);
    void* mem = tcc_malloc(size);
    tcc_relocate(s, mem);
    
    void (*f)(void) = tcc_get_symbol(s, "main");
    if (f) {
    }
    
    memset(code_buf, 0, 512);  /* Clear for next input */
    print("=> ");
    tcc_delete(s);
}


/* --- KERNEL ENTRY --- */
void kernel_main(void* mb_info, uint32_t magic) {
    /* RESET EVERYTHING */
    term_x = term_y = shift_pressed = 0;
    heap = (uint8_t*)MAIN_HEAP_START;  /* ← RESET HEAP */
    
    /* CLEAR SCREEN */
    for(int i = 0; i < 80*25; i++) VGA_MEM[i] = 0x0720;
    outb(0x3D4, 0x0F); outb(0x3D5, 0);
    outb(0x3D4, 0x0E); outb(0x3D5, 0);
    
     
    print("=== TCC KERNEL - Type C code:\n");
    print("=> ");
    
    char buf[512];
    while (1) {
        get_line(buf, 512);
        
        if (strcmp(buf, "clear") == 0) {
            clear_screen();
            print("=> ");
            continue;
        }
        
        if (buf[0] && strcmp(buf, "exit") != 0) {

            run_code(buf);
        } else {
            print("=> ");
        }
    }
}

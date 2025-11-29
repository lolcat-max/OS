/* TCC Bare Metal Kernel - FIXED VERSION */
#include <stdint.h>
#include <stddef.h>

// --- MISSING LIBC STUBS (keeping your existing ones) ---
int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *p1 = (const unsigned char *)a, *p2 = (const unsigned char *)b;
    for (size_t i=0; i<n; i++) if (p1[i]!=p2[i]) return p1[i]-p2[i];
    return 0;
}
float strtof(const char* s, char** e) {
    if(e) *e=(char*)s;
    return 0.0f;
}
unsigned long long strtoull(const char* s, char** e, int base) { return 0; }
long long strtoll(const char* s, char** e, int base) { return 0; }
long long atoll(const char* s) { return 0; }
int atoi(const char* s) { return 0; }
unsigned long long __udivdi3(unsigned long long n, unsigned long long d) { return n/d; }
unsigned long long __umoddi3(unsigned long long n, unsigned long long d) { return n%d; }
void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) { }
char* strerror(int e) { return "stub"; }
char* strncpy(char* d, const char* s, size_t n) { 
    size_t i=0; 
    for(; i<n && s[i]; i++) d[i]=s[i]; 
    for(; i<n; i++) d[i]=0; 
    return d;
}
int sscanf(const char* s, const char* fmt, ...) { return 0; }
int fseek(void* stream, long offset, int whence) { return 0; }
int ftell(void* stream) { return 0; }
int fgets(char* s, int size, void* stream) { return 0; }
int chmod(const char* path, int mode) { return 0; }
int dup(int fd) { return 0; }
int strcasecmp(const char* a, const char* b) { 
    while(*a&&*b&&(*a|32)==(*b|32)) {a++;b++;} 
    return (*(unsigned char*)a|32)-(*(unsigned char*)b|32); 
}
int strncasecmp(const char* a, const char* b, size_t n) { 
    while(n && *a&&*b&&(*a|32)==(*b|32)){a++;b++;n--;} 
    if(n==0)return 0; 
    return (*(unsigned char*)a|32)-(*(unsigned char*)b|32); 
}
int vsnprintf(char* buf, size_t size, const char* fmt, ...) { return 0; }

// --- VGA DISPLAY ---
#define VGA_MEM ((volatile uint16_t*)0xB8000)
int term_x=0, term_y=0;

void outb(uint16_t p,uint8_t v){__asm__("outb %0,%1"::"a"(v),"Nd"(p));}
uint8_t inb(uint16_t p){uint8_t r;__asm__("inb %1,%0":"=a"(r):"Nd"(p));return r;}

void term_putc(char c){
    if(c=='\n'){term_x=0;term_y++;}
    else if(c=='\b'){if(term_x>0)term_x--;VGA_MEM[term_y*80+term_x]=0x0720;}
    else{VGA_MEM[term_y*80+term_x]=0x0700|c;term_x++;}
    if(term_x>=80){term_x=0;term_y++;}
    if(term_y>=25){
        // Scroll screen
        for(int i=0; i<80*24; i++) VGA_MEM[i] = VGA_MEM[i+80];
        for(int i=80*24; i<80*25; i++) VGA_MEM[i] = 0x0720;
        term_y=24;
    }
    uint16_t pos=term_y*80+term_x;
    outb(0x3D4,14);outb(0x3D5,pos>>8);outb(0x3D4,15);outb(0x3D5,pos);
}

void print(const char*s){while(*s)term_putc(*s++);} 
void println(const char*s){print(s);term_putc('\n');}

void printi(int n){
    if(n<0){term_putc('-');n=-n;}
    if(!n){term_putc('0');return;}
    char d[12];int i=0;
    while(n){d[i++]='0'+n%10;n/=10;}
    while(i--)term_putc(d[i]);
}

void print_hex(unsigned int n) {
    print("0x");
    for(int i = 28; i >= 0; i -= 4) {
        int digit = (n >> i) & 0xF;
        term_putc(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
}

void tcc_print(const char* s) { print(s); }
void tcc_printi(int n) { printi(n); }
void tcc_println(const char* s) { println(s); }

void clear_screen(){
    for(int i=0;i<80*25;i++)VGA_MEM[i]=0x0720;
    term_x=0;term_y=0;
}

// --- HEAP with boundary checking ---
static char heap[160 * 1024 * 1024]; 
static char* hp = heap;

void* malloc(size_t size) {
    if (size == 0) return NULL;
    
    // Align to 16 bytes
    size = (size + 15) & ~15;
    
    // Check bounds
    if (hp + size > heap + sizeof(heap)) {
        println("MALLOC: Out of memory!");
        print("Requested: "); printi(size); println(" bytes");
        print("Available: "); printi((heap + sizeof(heap)) - hp); println(" bytes");
        return NULL;
    }
    
    void* p = hp;
    hp += size;
    
    // Zero out the memory
    char* ptr = (char*)p;
    for(size_t i = 0; i < size; i++) {
        ptr[i] = 0;
    }
    
    return p;
}

void free(void* p) {}
void* realloc(void* p, size_t s) { 
    if(!p) return malloc(s);
    void* new_p = malloc(s);
    if(!new_p) return NULL;
    // Copy old data (simplified - no size tracking)
    return new_p;
}

void* calloc(size_t n, size_t s) { 
    return malloc(n * s);
}

// --- MINIMAL LIBC STUBS ---
size_t strlen(const char* s){int i=0;while(s[i])i++;return i;}
int strcmp(const char* s1, const char* s2){
    while(*s1&&*s1==*s2){s1++;s2++;}
    return*(unsigned char*)s1-*(unsigned char*)s2;
}
int strncmp(const char* s1, const char* s2, size_t n){
    while(n--&&*s1&&*s1==*s2){s1++;s2++;}
    return(n==0?0:*(unsigned char*)s1-*(unsigned char*)s2);
}
char* strcpy(char* d, const char* s){char* p=d;while((*d++=*s++));return p;}
char* strchr(const char* s, int c){while(*s!=(char)c)if(!*s++)return 0;return(char*)s;}
char* strrchr(const char* s, int c){
    char* ret=0;
    do{if(*s==(char)c)ret=(char*)s;}while(*s++);
    return ret;
}
void* memset(void* s, int c, size_t n){
    unsigned char* p=s;
    while(n--)*p++=c;
    return s;
}
void* memcpy(void* d, const void* s, size_t n){
    char* p=d;
    const char* q=s;
    while(n--)*p++=*q++;
    return d;
}
void* memmove(void* d, const void* s, size_t n){
    char* pd=d;
    const char* ps=s;
    if(ps<pd&&ps+n>pd){
        pd+=n;ps+=n;
        while(n--)*--pd=*--ps;
    }else{
        while(n--)*pd++=*ps++;
    }
    return d;
}
char* strcat(char* d, const char* s){strcpy(d+strlen(d),s);return d;}
long strtol(const char* n,char**e,int b){
    long r=0;
    int neg=0;
    if(*n=='-'){neg=1;n++;}
    while(*n>='0'&&*n<='9'){r=r*10+(*n++-'0');}
    if(e)*e=(char*)n;
    return neg?-r:r;
}
double strtod(const char* n, char** e){return(double)strtol(n,e,10);}
unsigned long strtoul(const char*n,char**e,int b){return(unsigned long)strtol(n,e,b);}
void* memchr(const void* s, int c, size_t n) {
    const unsigned char* p = s;
    for(size_t i = 0; i < n; i++) {
        if(p[i] == (unsigned char)c) return (void*)(p + i);
    }
    return NULL;
}

// Fake stdio
typedef int FILE;
FILE* stdout=(FILE*)1; 
FILE* stderr=(FILE*)2; 
int errno;

int fprintf(FILE* s,const char* f,...){return 0;}
int printf(const char* f,...){return 0;}
int sprintf(char* b,const char* f,...){strcpy(b,f);return strlen(b);}
int snprintf(char*s,size_t n,const char*f,...){return 0;}
int vfprintf(FILE*s,const char*f,char*a){return 0;}
int fputc(int c, FILE*s){term_putc(c);return c;}
int fputs(const char* s, FILE* st){print(s);return 0;}
int fflush(FILE*s){return 0;}
int fclose(FILE*s){return 0;}
FILE* fopen(const char*p,const char*m){return NULL;}
FILE* fdopen(int fd, const char* mode){return NULL;}
int open(const char*p,int f,...){return-1;}
int close(int fd){return 0;}
int lseek(int f,int o,int w){return 0;}
int read(int f,void*b,size_t c){return 0;}
int fwrite(const void*p,size_t s,size_t n,FILE*st){return s*n;}
void exit(int s){println("exit()");while(1)__asm__("hlt");}

typedef int jmp_buf[16]; 
int setjmp(jmp_buf e){return 0;} 
void longjmp(jmp_buf e,int v){}

char* getcwd(char*b,size_t s){return NULL;}
int unlink(const char* p){return 0;}
char* getenv(const char* n){return NULL;}
double ldexp(double x, int exp){return x;}
int stat(const char*p, void*b){return -1;} 
int fstat(int f, void*b){return -1;}

typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler) { return (sighandler_t)0; }
int sigaction(int signum, const void *act, void *oldact) { return 0; }
int mprotect(void *addr, size_t len, int prot) { return 0; }
void* mmap(void* addr, size_t length, int prot, int flags, int fd, int offset) { 
    return malloc(length);
}
int sigemptyset(void* set) {
    unsigned long* s = (unsigned long*)set;
    for(int i=0; i<32; i++) s[i] = 0;
    return 0;
}

long double strtold(const char* nptr, char** endptr) {
    if(endptr) *endptr = (char*)nptr;
    return 0.0L;
}

long time(long* t) {
    long dummy_time = 1234567890;
    if(t) *t = dummy_time;
    return dummy_time;
}

struct tm {
    int tm_sec; int tm_min; int tm_hour;
    int tm_mday; int tm_mon; int tm_year;
};

struct tm* localtime(const long* timep) {
    static struct tm dummy_tm = {0, 0, 12, 1, 0, 124};
    return &dummy_tm;
}

// Critical missing function that TCC might use
char* strdup(const char* s) {
    if(!s) return NULL;
    size_t len = strlen(s) + 1;
    char* dup = (char*)malloc(len);
    if(!dup) return NULL;
    memcpy(dup, s, len);
    return dup;
}

// Another potential issue - TCC might call this
int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int toupper(int c) {
    if(c >= 'a' && c <= 'z') return c - 32;
    return c;
}

int tolower(int c) {
    if(c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

// --- REAL TCC ---
#include "tcc-0.9.27/libtcc.h"

// Global crash handler flag
volatile int in_user_code = 0;

void run_tcc_code(char* source){
    println("");
    println("=== Starting TCC Compilation ===");
    
    // Save heap pointer to check allocations
    char* heap_before = hp;
    
    println("Step 1: Creating TCC state...");
    TCCState* s = tcc_new();
    if(!s) { 
        println("ERROR: TCC State creation failed"); 
        return; 
    }
    print("  OK - TCC State at "); print_hex((unsigned int)s); println("");
    
    println("Step 2: Setting output type...");
    int ret = tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    print("  Return value: "); printi(ret); println("");
    if(ret < 0) {
        println("ERROR: Failed to set output type");
        tcc_delete(s);
        return;
    }
    println("  OK - Output type set");
    
    println("Step 3: Adding symbols...");
    println("  3a: Adding 'print'...");
    print("    Function at: "); print_hex((unsigned int)tcc_print); println("");
    tcc_add_symbol(s, "print", (void*)tcc_print);
    println("    OK");
    
    println("  3b: Adding 'printi'...");
    print("    Function at: "); print_hex((unsigned int)tcc_printi); println("");
    tcc_add_symbol(s, "printi", (void*)tcc_printi);
    println("    OK");
    
    println("  3c: Adding 'println'...");
    print("    Function at: "); print_hex((unsigned int)tcc_println); println("");
    tcc_add_symbol(s, "println", (void*)tcc_println);
    println("    OK");
    
    println("  OK - All symbols added");
    
    // Show source code
    println("Step 4: Compiling source:");
    println("---");
    println(source);
    println("---");
    
    if(tcc_compile_string(s, source) == -1) {
        println("ERROR: Compilation failed");
        println("Check your syntax!");
        tcc_delete(s); 
        return;
    }
    println("  OK - Compilation successful");
    
    println("Step 5: Getting relocation size...");
    int size = tcc_relocate(s, NULL);
    if(size < 0) {
        println("ERROR: Failed to get relocation size");
        tcc_delete(s);
        return;
    }
    print("  OK - Need "); printi(size); println(" bytes");
    
    println("Step 6: Allocating memory...");
    void* mem = malloc(size + 32); // Extra padding
    if(!mem) {
        println("ERROR: Out of memory");
        tcc_delete(s);
        return;
    }
    
    // Align to 16-byte boundary
    void* aligned_mem = (void*)(((uintptr_t)mem + 15) & ~15);
    print("  OK - Allocated at "); print_hex((unsigned int)aligned_mem); println("");
    
    // Zero out memory
    println("Step 7: Zeroing memory...");
    for(int i = 0; i < size; i++) {
        ((char*)aligned_mem)[i] = 0;
    }
    println("  OK - Memory cleared");
    
    println("Step 8: Relocating code...");
    if(tcc_relocate(s, aligned_mem) < 0) {
        println("ERROR: Relocation failed");
        tcc_delete(s);
        return;
    }
    println("  OK - Code relocated");
    
    println("Step 9: Looking for main()...");
    void (*func)(void) = (void(*)(void))tcc_get_symbol(s, "main");
    
    if(!func) {
        println("ERROR: main() not found");
        println("Your code must have: int main() { ... }");
        tcc_delete(s);
        return;
    }
    print("  OK - main() at "); print_hex((unsigned int)func); println("");
    
    // Verify function pointer is reasonable
    if((unsigned int)func < 0x100000 || (unsigned int)func > 0x10000000) {
        println("ERROR: Function pointer looks invalid!");
        print("Address: "); print_hex((unsigned int)func); println("");
        tcc_delete(s);
        return;
    }
    
    println("Step 10: Preparing to execute...");
    println("");
    println("=== EXECUTING USER CODE ===");
    println("");
    
    // Set flag before execution
    in_user_code = 1;
    
    // Try to call with maximum safety
    __asm__ volatile (
        "pushl %%ebp\n"           // Save base pointer
        "movl %%esp, %%ebp\n"     // Set up frame
        "andl $-16, %%esp\n"      // Align stack to 16 bytes
        "subl $16, %%esp\n"       // Reserve some stack space
        "call *%0\n"              // Call the function
        "movl %%ebp, %%esp\n"     // Restore stack
        "popl %%ebp\n"            // Restore base pointer
        :
        : "r"(func)
        : "memory", "eax", "ecx", "edx"
    );
    
    in_user_code = 0;
    
    println("");
    println("=== EXECUTION COMPLETE ===");
    println("");
    
    // Cleanup
    println("Step 11: Cleaning up...");
    tcc_delete(s);
    println("  OK - Cleanup complete");
    
    // Show memory usage
    print("Heap used: "); 
    printi((int)(hp - heap_before)); 
    println(" bytes");
    println("");
}

// --- KERNEL MAIN ---
__attribute__((section(".multiboot")))
struct {uint32_t m,f,c;} mb = {0x1BADB002, 3, -(0x1BADB002+3)};

void kernel_main() {
    clear_screen();
    println("=== TCC Bare Metal OS (Debug Mode) ===");
    println("Commands:");
    println("  test     - Run simple test without TCC");
    println("  compile  - Enter code to compile");
    println("");
    print("=> ");

    char buf[2048];
    int idx=0;
    int shift_pressed = 0;
    
    while(1) {
        if(inb(0x64) & 1) {
            uint8_t sc = inb(0x60);
            
            // Handle shift key press/release
            if(sc == 0x2A || sc == 0x36) {
                shift_pressed = 1;
                continue;
            } else if(sc == 0xAA || sc == 0xB6) {
                shift_pressed = 0;
                continue;
            }
            
            if(sc < 0x80) { 
                if(sc == 0x1C) { // Enter
                    term_putc('\n');
                    buf[idx] = 0;
                    
                    if(idx > 0) {
                        // Check for test command
                        if(strcmp(buf, "test") == 0) {
                            println("Running basic test...");
                            println("Test 1: Print function");
                            tcc_print("Hello ");
                            tcc_println("World!");
                            
                            println("Test 2: Integer print");
                            tcc_printi(12345);
                            term_putc('\n');
                            
                            println("Test 3: Memory allocation");
                            void* p = malloc(100);
                            if(p) {
                                print("Allocated at: ");
                                print_hex((unsigned int)p);
                                println("");
                                println("All tests passed!");
                            } else {
                                println("Malloc failed!");
                            }
                        } else if(strcmp(buf, "compile") == 0) {
                            println("Enter your C code (single line):");
                            print("code> ");
                            idx = 0;
                            continue;
                        } else if(idx > 0) {
                            // Treat as code to compile
                            run_tcc_code(buf);
                        }
                    }
                    
                    idx=0;
                    print("=> ");
                } else if(sc == 0x0E) { // Backspace
                    if(idx>0) { 
                        idx--; 
                        term_putc('\b'); 
                    }
                } else {
                    if(idx < sizeof(buf)-1) {
                        char c = 0;
                        
                        // Map scancodes to characters with shift support
                        if(shift_pressed) {
                            // Uppercase letters and shifted symbols
                            if(sc >= 0x10 && sc <= 0x19) c = "QWERTYUIOP"[sc-0x10];
                            else if(sc >= 0x1E && sc <= 0x26) c = "ASDFGHJKL"[sc-0x1E];
                            else if(sc >= 0x2C && sc <= 0x32) c = "ZXCVBNM"[sc-0x2C];
                            else if(sc >= 0x02 && sc <= 0x0B) c = "!@#$%^&*()"[sc-0x02];
                            else if(sc == 0x39) c = ' ';
                            else if(sc == 0x27) c = ':';
                            else if(sc == 0x28) c = '"';
                            else if(sc == 0x1A) c = '{';
                            else if(sc == 0x1B) c = '}';
                            else if(sc == 0x33) c = '<';
                            else if(sc == 0x34) c = '>';
                            else if(sc == 0x2B) c = '|';
                            else if(sc == 0x35) c = '?';
                            else if(sc == 0x0C) c = '_';
                            else if(sc == 0x0D) c = '+';
                            else if(sc == 0x29) c = '~';
                        } else {
                            // Lowercase letters and normal symbols
                            if(sc >= 0x10 && sc <= 0x19) c = "qwertyuiop"[sc-0x10];
                            else if(sc >= 0x1E && sc <= 0x26) c = "asdfghjkl"[sc-0x1E];
                            else if(sc >= 0x2C && sc <= 0x32) c = "zxcvbnm"[sc-0x2C];
                            else if(sc >= 0x02 && sc <= 0x0B) c = "1234567890"[sc-0x02];
                            else if(sc == 0x39) c = ' ';
                            else if(sc == 0x27) c = ';';
                            else if(sc == 0x28) c = '\'';
                            else if(sc == 0x1A) c = '[';
                            else if(sc == 0x1B) c = ']';
                            else if(sc == 0x33) c = ',';
                            else if(sc == 0x34) c = '.';
                            else if(sc == 0x2B) c = '\\';
                            else if(sc == 0x35) c = '/';
                            else if(sc == 0x0C) c = '-';
                            else if(sc == 0x0D) c = '=';
                            else if(sc == 0x29) c = '`';
                        }
                        
                        if(c) {
                            buf[idx++] = c;
                            term_putc(c);
                        }
                    }
                }
            }
        }
    }
}
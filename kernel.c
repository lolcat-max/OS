/* TCC Bare Metal Kernel - No Checks */
// --- MISSING LIBC STUBS ---
#include <stdint.h>
#include <stddef.h>

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *p1 = (const unsigned char *)a, *p2 = (const unsigned char *)b;
    for (size_t i=0; i<n; i++) if (p1[i]!=p2[i]) return p1[i]-p2[i];
    return 0;
}
float strtof(const char* s, char** e) {
    if(e) *e=(char*)s; // Not implemented
    return 0.0f;
}
unsigned long long strtoull(const char* s, char** e, int base) { return 0; }
long long strtoll(const char* s, char** e, int base) { return 0; }
long long atoll(const char* s) { return 0; }
int atoi(const char* s) { return 0; }
unsigned long long __udivdi3(unsigned long long n, unsigned long long d) { return 0; }
unsigned long long __umoddi3(unsigned long long n, unsigned long long d) { return 0; }
void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) { }
char* strerror(int e) { return "stub"; }
char* strncpy(char* d, const char* s, size_t n) { size_t i=0; for(; i<n && s[i]; i++) d[i]=s[i]; for(; i<n; i++) d[i]=0; return d;}
int sscanf(const char* s, const char* fmt, ...) { return 0; }
int fseek(void* stream, long offset, int whence) { return 0; }
int ftell(void* stream) { return 0; }
int fgets(char* s, int size, void* stream) { return 0; }
int chmod(const char* path, int mode) { return 0; }
int dup(int fd) { return 0; }
int strcasecmp(const char* a, const char* b) { while(*a&&*b&&*a==*b) {a++;b++;} return *(unsigned char*)a-*(unsigned char*)b; }
int strncasecmp(const char* a, const char* b, size_t n) { while(n && *a&&*b&&*a==*b){a++;b++;n--;} if(n==0)return 0; return *(unsigned char*)a-*(unsigned char*)b; }
int vsnprintf(char* buf, size_t size, const char* fmt, ...) { return 0; }

// --- HEADERS & TYPES ---
#define NULL ((void*)0)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int FILE; 

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
    if(term_y>=25){term_y=0;} 
    uint16_t pos=term_y*80+term_x;
    outb(0x3D4,14);outb(0x3D5,pos>>8);outb(0x3D4,15);outb(0x3D5,pos);
}
void print(const char*s){while(*s)term_putc(*s++);} 
void println(const char*s){print(s);term_putc('\n');}
void printi(int n){
    if(n<0){term_putc('-');n=-n;}
    if(!n){term_putc('0');return;}
    char d[12];int i=0;while(n){d[i++]='0'+n%10;n/=10;}while(i--)term_putc(d[i]);
}
// Function specifically for TCC to call
void tcc_print(const char* s) {
    print(s);
}

void tcc_printi(int n) {
    printi(n);
}

void tcc_println(const char* s) {
    println(s);
}
void clear_screen(){
    for(int i=0;i<80*25;i++)VGA_MEM[i]=0x0720;
    term_x=0;term_y=0;
    print("=> ");
}

// --- HEAP ---
static char heap[160 * 1024 * 1024]; 
static char* hp = heap;


void* malloc(size_t size) {
    if (size == 0) return NULL;
    
    // Align to 16 bytes
    size = (size + 15) & ~15;
    
    // Check bounds
    if (hp + size > heap + sizeof(heap)) {
        println("MALLOC: Out of memory!");
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
void* realloc(void* p, size_t s) { return malloc(s); }
void* calloc(size_t n, size_t s) { 
    void* p=malloc(n*s); char* c=(char*)p; 
    for(int i=0;i<n*s;i++)c[i]=0; return p; 
}

// --- MINIMAL LIBC STUBS (No checks) ---
size_t strlen(const char* s){int i=0;while(s[i])i++;return i;}
int strcmp(const char* s1, const char* s2){while(*s1&&*s1==*s2){s1++;s2++;}return*(unsigned char*)s1-*(unsigned char*)s2;}
int strncmp(const char* s1, const char* s2, size_t n){while(n--&&*s1&&*s1==*s2){s1++;s2++;}return(n==0?0:*(unsigned char*)s1-*(unsigned char*)s2);}
char* strcpy(char* d, const char* s){char* p=d;while((*d++=*s++));return p;}
char* strchr(const char* s, int c){while(*s!=(char)c)if(!*s++)return 0;return(char*)s;}
char* strrchr(const char* s, int c){char* ret=0;do{if(*s==(char)c)ret=(char*)s;}while(*s++);return ret;}
void* memset(void* s, int c, size_t n){unsigned char* p=s;while(n--)*p++=c;return s;}
void* memcpy(void* d, const void* s, size_t n){char* p=d;const char* q=s;while(n--)*p++=*q++;return d;}
void* memmove(void* d, const void* s, size_t n){char* pd=d;const char* ps=s;if(ps<pd&&ps+n>pd){pd+=n;ps+=n;while(n--)*--pd=*--ps;}else{while(n--)*pd++=*ps++;}return d;}
char* strcat(char* d, const char* s){strcpy(d+strlen(d),s);return d;}
long strtol(const char* n,char**e,int b){long r=0;while(*n>='0'&&*n<='9'){r=r*10+(*n++-'0');}if(e)*e=(char*)n;return r;}
double strtod(const char* n, char** e){return(double)strtol(n,e,10);}
unsigned long strtoul(const char*n,char**e,int b){return(unsigned long)strtol(n,e,b);}
// Add to kernel.c stubs
void* memchr(const void* s, int c, size_t n) {
    const unsigned char* p = s;
    for(size_t i = 0; i < n; i++) {
        if(p[i] == (unsigned char)c) return (void*)(p + i);
    }
    return NULL;
}


// Fake stdio vars
FILE* stdout=(FILE*)1; FILE* stderr=(FILE*)2; int errno;

// Simplified stubs (return 0 or -1, no checking)
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
typedef int jmp_buf[16]; int setjmp(jmp_buf e){return 0;} void longjmp(jmp_buf e,int v){}
char* getcwd(char*b,size_t s){return NULL;}
int unlink(const char* p){return 0;}
char* getenv(const char* n){return NULL;}
double ldexp(double x, int exp){return x;}
int stat(const char*p, void*b){return -1;} int fstat(int f, void*b){return -1;}

// --- REAL TCC ---
#include "tcc-0.9.27/libtcc.h"
void run_tcc_code(char* source){
    println("");
    println("=== Starting TCC ===");
    
    TCCState* s = tcc_new();
    if(!s) { 
        println("TCC State Failed"); 
        return; 
    }
    
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    
    // Add symbols with explicit function pointers
    tcc_add_symbol(s, "print", (void*)tcc_print);
    tcc_add_symbol(s, "printi", (void*)tcc_printi); 
    tcc_add_symbol(s, "println", (void*)tcc_println);
    
    if(tcc_compile_string(s, source) == -1) {
        println("Compile Failed"); 
        tcc_delete(s); 
        return;
    }
    
    int size = tcc_relocate(s, NULL);
    if(size < 0) {
        println("Relocate size failed");
        tcc_delete(s);
        return;
    }
    
    void* mem = malloc(size);
    if(!mem) {
        println("Malloc failed for relocation");
        tcc_delete(s);
        return;
    }
    
    if(tcc_relocate(s, mem) < 0) {
        println("Relocate failed");
        tcc_delete(s);
        return;
    }
    
    void (*func)(void) = (void(*)(void))tcc_get_symbol(s, "main");
    
    if(func) {
        println("EXEC >>>"); 
        func(); 
        println("<<< DONE");
    } else {
        println("No main()");
    }
    
    tcc_delete(s);
}


// signal.h stub
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler) {
    return (sighandler_t)0; // Do nothing in bare metal
}
// signal & mmap stubs
int sigaction(int signum, const void *act, void *oldact) { return 0; }
int mprotect(void *addr, size_t len, int prot) { return 0; }
void* mmap(void* addr, size_t length, int prot, int flags, int fd, int offset) { return (void*)-1; }
// signal stub
int sigemptyset(void* set) {
    unsigned long* s = (unsigned long*)set;
    for(int i=0; i<32; i++) s[i] = 0;
    return 0;
}
// Final missing stubs
long double strtold(const char* nptr, char** endptr) {
    // Minimal stub - just return 0.0
    if(endptr) *endptr = (char*)nptr;
    return 0.0L;
}

long time(long* t) {
    // Return a dummy timestamp
    long dummy_time = 1234567890;
    if(t) *t = dummy_time;
    return dummy_time;
}

struct tm {
    int tm_sec; int tm_min; int tm_hour;
    int tm_mday; int tm_mon; int tm_year;
};

struct tm* localtime(const long* timep) {
    // Return a static dummy tm struct
    static struct tm dummy_tm = {0, 0, 12, 1, 0, 124}; // 2024-01-01 12:00:00
    return &dummy_tm;
}

// --- KERNEL MAIN ---
__attribute__((section(".multiboot")))
struct {uint32_t m,f,c;} mb = {0x1BADB002, 3, -(0x1BADB002+3)};

void kernel_main() {
    clear_screen();
    println("=== TCC Bare Metal (No Checks) ===");
    print("=> ");

    char buf[1024];
    int idx=0;
    while(1) {
        if(inb(0x64) & 1) {
            uint8_t sc = inb(0x60);
            if(sc < 0x80) { 
                if(sc == 0x1C) { // Enter
                    term_putc('\n');
                    buf[idx] = 0;
                    if(idx > 0)         				run_tcc_code("int main(){print(\"HELLO!\");return 0;}");
                    idx=0;
                    print("=> ");
                } else if(sc == 0x0E) { // Backspace
                    if(idx>0) { idx--; term_putc('\b'); }
                } else {
                    const char* map = "\0\e1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
                    if(idx < 1023) {
						char c = 0;
						
						// Only handle safe ASCII letters/numbers
						if(sc >= 0x10 && sc <= 0x19) c = "qwertyuiop"[sc-0x10];
						else if(sc >= 0x1E && sc <= 0x26) c = "asdfghjkl"[sc-0x1E];
						else if(sc >= 0x2C && sc <= 0x32) c = "zxcvbnm"[sc-0x2C];
						else if(sc >= 0x02 && sc <= 0x0B) c = "1234567890"[sc-0x02];
						else if(sc == 0x39) c = ' ';
						else if(sc == 0x27) c = ';';
						else if(sc == 0x28) c = '"'; // Use " directly
						else if(sc == 0x1A) c = '(';  // Map [ to (
						else if(sc == 0x1B) c = ')';  // Map ] to )
						else if(sc == 0x33) c = ',';
						else if(sc == 0x34) c = '.';
						else if(sc == 0x2B) c = '{';
						else if(sc == 0x35) c = '}';
						
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

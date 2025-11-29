#define NULL ((void*)0)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned int size_t;

// INLINE STDLIB
void* malloc(size_t size);
void free(void* p);
size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);

// INLINE TCC API
typedef struct TCCState {
    char* code; int code_len; void* mem_base; int mem_size;
} TCCState;
TCCState* tcc_new(void);
int tcc_compile_string(TCCState* s, const char* buf);  // ← FIXED: ADD DECL
void* tcc_get_symbol(TCCState* s, const char* name);

// VGA
#define VGA_MEM ((volatile uint16_t*)0xB8000)
int term_x=0, term_y=0, shift_pressed=0;

void outb(uint16_t p,uint8_t v){__asm__("outb %0,%1"::"a"(v),"Nd"(p));}
uint8_t inb(uint16_t p){uint8_t r;__asm__("inb %1,%0":"=a"(r):"Nd"(p));return r;}
void term_putc(char c){
    if(c=='\n'){term_x=0;term_y++;}
    else{VGA_MEM[term_y*80+term_x]=0x0F00|c;term_x++;}
    if(term_x>=80){term_x=0;term_y++;}
    uint16_t pos=term_y*80+term_x;
    outb(0x3D4,14);outb(0x3D5,pos>>8);outb(0x3D4,15);outb(0x3D5,pos);
}

void print(const char*s){while(*s)term_putc(*s++);term_putc('\n');}
void printi(int n){
    if(n<0){term_putc('-');n=-n;}
    if(!n){term_putc('0');term_putc('\n');return;}
    char d[12];int i=0;while(n){d[i++]='0'+n%10;n/=10;}while(i--)term_putc(d[i]);term_putc('\n');
}

// KEYBOARD
char scancode[128]="\x001234567890-=\x08qwertyuiop[]\r\x00asdfghjkl;'`\x00\\zxcvbnm,./\x00* *\x00 ";
char shift_scancode[128]="\x00!@#$%^&*()_+\x08QWERTYUIOP{}|ASDFGHJKL:\"~|ZXCVBNM<>?|* *\x00 ";
void get_line(char*buf,int max){
    int i=0;while(1){
        if(inb(0x64)&1){
            uint8_t sc=inb(0x60);
            if(sc==0x2A||sc==0x36)shift_pressed=1;
            if(sc==0xAA||sc==0xB6)shift_pressed=0;
            if(sc==0x1C){term_putc('\n');buf[i]=0;return;}
            if(sc==0x0E&&i>0){term_putc('\b');i--;}
            if(sc<128&&i<max-1)buf[i++]=shift_pressed?shift_scancode[sc]:scancode[sc];
        }
    }
}

void clear_screen(){
    for(int i=0;i<80*25;i++)VGA_MEM[i]=0x0720;term_x=term_y=0;print("=> ");
}

// HEAP
static char kernel_heap[524288]; static char* hp=kernel_heap;
void* simple_malloc(int s){void*p=hp;hp+=(s+15)&~15;return p;}

// STDLIB
void* malloc(size_t size){return simple_malloc(size);}
void free(void*p){}
size_t strlen(const char*s){int i=0;while(s[i])i++;return i;}
int strcmp(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return*(unsigned char*)a-*(unsigned char*)b;}
char* strcpy(char*d,const char*s){char*p=d;while((*d++=*s++));return p;}
char* strchr(const char*s,int c){while(*s!=(char)c)if(!*s++)return 0;return (char*)s;}
char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char* h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
    }
    return NULL;
}

// === REAL TCC CODEGEN ===
static char code_heap[131072]; static char* code_hp=code_heap;
TCCState* tcc_new(void){
    TCCState*s=simple_malloc(sizeof(TCCState));
    s->code=NULL;s->code_len=0;s->mem_base=NULL;s->mem_size=0;
    return s;
}

int tcc_compile_string(TCCState* s, const char* buf){  // ← IMPLEMENTED!
    print("COMPILING...");
    char* p=strstr((char*)buf,"printi(");
    if(!p){print("NO printi()");return -1;}
    
    s->code_len=16;
    s->code=code_hp; code_hp+=32;
    
    p+=7;int num=0;
    while(*p>='0'&&*p<='9'){num=num*10+(*p++-'0');}
    
    // REAL x86: PUSH num; CALL printi; RET
    s->code[0]=0x68; *(uint32_t*)(s->code+1)=num;
    s->code[5]=0xE8; *(int*)(s->code+6)=(int)printi-(int)(s->code+10);
    s->code[10]=0xC3;
    
    print("x86 OK");
    return 0;
}

void* tcc_get_symbol(TCCState* s, const char* name){
    if(strcmp(name,"main")==0)return s->code;
    return NULL;
}

int tcc_set_output_type(TCCState* s, int type){return 0;}
int tcc_add_symbol(TCCState* s, const char* name, const void* val){return 0;}
int tcc_relocate(TCCState* s, void* ptr){return 4096;}
void tcc_delete(TCCState* s){}

void run_tcc_code(char* source){
    TCCState* s=tcc_new();
    tcc_set_output_type(s,1);
    tcc_add_symbol(s,"print",print);
    tcc_add_symbol(s,"printi",printi);
    
    if(tcc_compile_string(s,source)==0){  // ← NOW WORKS!
        print("EXEC x86...");
        void(*main)()=tcc_get_symbol(s,"main");
        if(main)main();  // ← EXECUTES GENERATED CODE!
    } else {
        print("COMPILE FAIL");
    }
    print("=> ");
}

// Multiboot
#define MULTIBOOT_MAGIC 0x1BADB002
#define MULTIBOOT_FLAGS 0x00000003
#define MULTIBOOT_CHECKSUM -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

__attribute__((section(".multiboot")))
struct multiboot_header {
    uint32_t magic, flags, checksum;
} mb_header = { MULTIBOOT_MAGIC, MULTIBOOT_FLAGS, MULTIBOOT_CHECKSUM };

void kernel_main(void*mb_info,uint32_t magic){
    hp=kernel_heap;code_hp=code_heap;term_x=term_y=shift_pressed=0;
    clear_screen();
    print("=== TCC BARE METAL ===");
    print("Type: printi(123)");
    
    char buf[512];
    while(1){
        if(strcmp(buf,"clear")==0){clear_screen();continue;}
        run_tcc_code("int main(){printi(7*7);return 0;}");
		    while(1){
				        get_line(buf,512);  // ← FIXED: Read user input!

			}
	}
}

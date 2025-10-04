// tcc_wrapper.c
#include "tcc/libtcc.h"
#include <stdint.h>

static void tcc_error(void *opaque, const char *msg) {
    extern void kernel_print(const char*); // your kernel console print
    kernel_print(msg);
    kernel_print("\n");
}

extern void* malloc(size_t);
extern void free(void*);
extern int kernel_printf(const char*, ...);

int tcc_compile_and_run(const char* src) {
    TCCState* s = tcc_new();
    if (!s) return -1;

    tcc_set_error_func(s, NULL, tcc_error);
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);

    // Export kernel runtime functions to generated C code
    tcc_add_symbol(s, "printf", (void*)kernel_printf);
    tcc_add_symbol(s, "malloc", (void*)malloc);
    tcc_add_symbol(s, "free", (void*)free);

    if (tcc_compile_string(s, src) == -1) {
        tcc_delete(s);
        return -1;
    }

    if (tcc_relocate(s, TCC_RELOCATE_AUTO) < 0) {
        tcc_delete(s);
        return -1;
    }

    int (*entry)(void) = tcc_get_symbol(s, "main");
    int ret = entry ? entry() : tcc_run(s, 0, NULL);
    tcc_delete(s);
    return ret;
}

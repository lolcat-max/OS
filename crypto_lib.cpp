// Crypto library functions
#include "stdlib_hooks.h"
int simple_hash(const char* text) {
    int hash = 0;
    for (int i = 0; text[i]; i++) {
        hash = hash * 31 + text[i];
    }
    return hash;
}

void xor_encrypt(char* text, int key) {
    for (int i = 0; text[i]; i++) {
        text[i] = text[i] ^ (key & 0xFF);
    }
}

char* caesar_cipher(const char* text, int shift) {
    static char result[256];
    for (int i = 0; text[i] && i < 255; i++) {
        char c = text[i];
        if (c >= 'a' && c <= 'z') {
            result[i] = ((c - 'a' + shift) % 26) + 'a';
        } else if (c >= 'A' && c <= 'Z') {
            result[i] = ((c - 'A' + shift) % 26) + 'A';
        } else {
            result[i] = c;
        }
    }
    result[strlen(text)] = '\0';
    return result;
}

int checksum(const void* data, int length) {
    int sum = 0;
    const char* bytes = (const char*)data;
    for (int i = 0; i < length; i++) {
        sum += bytes[i];
    }
    return sum & 0xFFFF;
}

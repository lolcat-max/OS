#include "types.h"
//=============================================================================
// STRING.CPP - String Processing and Utilities Library
// Drop-in library for kernel string functions
//=============================================================================

//=============================================================================
// KERNEL-COMPATIBLE STRING FUNCTIONS
//=============================================================================
// Add these hex conversion functions to string.cpp

// Convert 32-bit integer to hex string
void uint32_to_hex_string(uint32_t value, char buffer[9]) {
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        buffer[7-i] = hex_chars[(value >> (i*4)) & 0xF];
    }
    buffer[8] = '\0';
}

// Convert 64-bit integer to hex string  
void uint64_to_hex_string(uint64_t value, char buffer[17]) {
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) {
        buffer[15-i] = hex_chars[(value >> (i*4)) & 0xF];
    }
    buffer[16] = '\0';
}

// Alternative versions that return static strings (easier to use)
char* uint32_to_hex(uint32_t value) {
    static char buffer[9];
    uint32_to_hex_string(value, buffer);
    return buffer;
}

char* uint64_to_hex(uint64_t value) {
    static char buffer[17];
    uint64_to_hex_string(value, buffer);
    return buffer;
}

// String search function (replacement for standard library strstr)
char* strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return nullptr;
    if (!*needle) return (char*)haystack;
    
    for (const char* h = haystack; *h; h++) {
        const char* h_temp = h;
        const char* n_temp = needle;
        
        while (*h_temp && *n_temp && *h_temp == *n_temp) {
            h_temp++;
            n_temp++;
        }
        
        if (!*n_temp) return (char*)h;
    }
    return nullptr;
}

// String length function (replacement for strlen)
int string_length(const char* str) {
    if (!str) return 0;
    int len = 0;
    while (str[len]) len++;
    return len;
}

// String copy function (replacement for strcpy)
char* string_copy(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* orig_dest = dest;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return orig_dest;
}

// String concatenate function (replacement for strcat)
char* string_concat(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* end = dest;
    while (*end) end++; // Find end of dest
    while (*src) {
        *end++ = *src++;
    }
    *end = '\0';
    return dest;
}

// String compare function (replacement for strcmp)
int string_compare(const char* str1, const char* str2) {
    if (!str1 && !str2) return 0;
    if (!str1) return -1;
    if (!str2) return 1;
    
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    
    return (unsigned char)*str1 - (unsigned char)*str2;
}

//=============================================================================
// EXTENDED STRING UTILITIES
//=============================================================================

// Reverse a string in place
char* string_reverse(char* str) {
    if (!str) return str;
    
    int len = string_length(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
    return str;
}

// Convert string to uppercase
char* string_upper(char* str) {
    if (!str) return str;
    
    for (int i = 0; str[i]; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] = str[i] - 'a' + 'A';
        }
    }
    return str;
}

// Convert string to lowercase
char* string_lower(char* str) {
    if (!str) return str;
    
    for (int i = 0; str[i]; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = str[i] - 'A' + 'a';
        }
    }
    return str;
}

// Trim whitespace from both ends
char* string_trim(char* str) {
    if (!str) return str;
    
    // Trim leading whitespace
    char* start = str;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }
    
    // If string was all whitespace
    if (*start == '\0') {
        *str = '\0';
        return str;
    }
    
    // Trim trailing whitespace
    char* end = start + string_length(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    *(end + 1) = '\0';
    
    // Move trimmed string to beginning if needed
    if (start != str) {
        string_copy(str, start);
    }
    
    return str;
}



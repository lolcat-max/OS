#include "terminal_hooks.h"
#include "terminal_io.h"
#include "iostream_wrapper.h"
#include "interrupts.h"
#include "hardware_specs.h"
#include "stdlib_hooks.h"
#include "pci.h"
#include "sata.h"
#include "test.h"
#include "test2.h"
#include "disk.h"
#include "dma_memory.h"
#include "identify.h"
#include "notepad.h"
#include "xhci.h"





void usb_keyboard_self_test();

int fat32_write_file(uint64_t ahci_base, int port, const char* filename, const void* data, uint32_t size);



// --- COMMAND IMPLEMENTATIONS ---


// --- Add to FORWARD DECLARATIONS ---
void cmd_cat(uint64_t ahci_base, int port, const char* filename);

// --- COMMAND IMPLEMENTATIONS ---
void cmd_help() {
    cout << "--- KERNEL COMMANDS ---\n"
         << "  help, clear, pong, ls, rm, chkdsk\n"
         << "  cat <file>, kbtest\n"
         << "  cp <src> <dest>, mv <old> <new>\n"
         << "  compile <file.cpp>, run <file.obj>\n"
         << "  exec <inline_code>\n"
         << "  formatfs, mount, unmount\n";
}

static void int_to_string(int value, char* buffer) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    bool negative = value < 0;
    if (negative) value = -value;
    
    char temp[16];
    int i = 0;
    
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    int j = 0;
    if (negative) buffer[j++] = '-';
    
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    
    buffer[j] = '\0';
}

//=============================================================================
// REAL-TIME COMPILER WITH NATIVE CPP INCLUDES AS LIBRARIES
// Include CPP files directly in kernel.cpp to make functions available
// to the self-hosted compiler automatically
//=============================================================================
#include "libs.h"
//=============================================================================
// AUTOMATIC FUNCTION REGISTRATION SYSTEM
//=============================================================================
//=============================================================================
// ENHANCED REAL-TIME COMPILER WITH DATA RETURNS AND PERSISTENT VARIABLES
//=============================================================================

// Result structure to hold function return data
struct FunctionResult {
    enum Type { VOID_RESULT, INT_RESULT, STRING_RESULT, ARRAY_RESULT } type;
    int int_value;
    char string_value[512];
    int* array_data;
    int array_size;
    bool success;
    char error_message[256];
    
    FunctionResult() : type(VOID_RESULT), int_value(0), array_data(nullptr), array_size(0), success(true) {
        string_value[0] = '\0';
        error_message[0] = '\0';
    }
};

// Enhanced library function structure with return type info
struct LibraryFunction {
    const char* name;
    void* function_ptr;
    const char* signature;
    const char* description;
    const char* library;
    const char* return_type; // "void", "int", "string", "array"
};

#define REGISTER_FUNCTION_EXT(func, sig, desc, lib, ret_type) \
    {#func, (void*)func, sig, desc, lib, ret_type}

// Updated function registry with return types
static const LibraryFunction library_functions[] = {
    // Core kernel functions
    REGISTER_FUNCTION_EXT(memory_map_data, "string[]()", "Get hardware device list", "kernel", "array"),
    REGISTER_FUNCTION_EXT(fat32_read_file, "int(char*,void*,int)", "Read file", "filesystem", "int"),
    REGISTER_FUNCTION_EXT(fat32_write_file, "int(char*,void*,int)", "Write file", "filesystem", "int"),
    REGISTER_FUNCTION_EXT(fat32_list_files, "string[]()", "List files", "filesystem", "array"),
    REGISTER_FUNCTION_EXT(mmio_read8, "int(long)", "Read 8-bit MMIO", "hardware", "int"),
    REGISTER_FUNCTION_EXT(mmio_read16, "int(long)", "Read 16-bit MMIO", "hardware", "int"),
    REGISTER_FUNCTION_EXT(mmio_read32, "int(long)", "Read 32-bit MMIO", "hardware", "int"),
    REGISTER_FUNCTION_EXT(mmio_write8, "int(long,int)", "Write 8-bit MMIO", "hardware", "int"),
    REGISTER_FUNCTION_EXT(mmio_write16, "int(long,int)", "Write 16-bit MMIO", "hardware", "int"),
    REGISTER_FUNCTION_EXT(mmio_write32, "int(long,int)", "Write 32-bit MMIO", "hardware", "int"),
    REGISTER_FUNCTION_EXT(print_hex, "void(char*,int)", "Print hex value", "display", "void"),
    REGISTER_FUNCTION_EXT(terminal_clear, "void()", "Clear screen", "display", "void"),
    
    // Math library functions
    REGISTER_FUNCTION_EXT(fibonacci, "int(int)", "Calculate Fibonacci number", "math", "int"),
    REGISTER_FUNCTION_EXT(factorial, "int(int)", "Calculate factorial", "math", "int"),
    REGISTER_FUNCTION_EXT(power, "int(int,int)", "Calculate power", "math", "int"),
    REGISTER_FUNCTION_EXT(gcd, "int(int,int)", "Greatest common divisor", "math", "int"),
    REGISTER_FUNCTION_EXT(sqrt_approx, "int(int)", "Approximate square root", "math", "int"),
    REGISTER_FUNCTION_EXT(is_prime, "int(int)", "Check if prime", "math", "int"),
    
    // Crypto library functions
    REGISTER_FUNCTION_EXT(simple_hash, "int(char*)", "Simple hash function", "crypto", "int"),
    REGISTER_FUNCTION_EXT(xor_encrypt, "void(char*,int)", "XOR encryption", "crypto", "void"),
    REGISTER_FUNCTION_EXT(caesar_cipher, "string(char*,int)", "Caesar cipher", "crypto", "string"),
    REGISTER_FUNCTION_EXT(checksum, "int(void*,int)", "Calculate checksum", "crypto", "int"),
    
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr} // Sentinel
};

class RTCompiler {
private:
    uint64_t ahci_base;
    int port;
    
    // Enhanced variable storage with persistent state across runs
    struct Variable {
        char name[64];
        enum Type { INT_VAR, STRING_VAR, ARRAY_VAR } type;
        int int_value;
        char string_value[512];
        int* array_data;
        int array_size;
        bool initialized;
        
        Variable() : type(INT_VAR), int_value(0), array_data(nullptr), array_size(0), initialized(false) {
            name[0] = '\0';
            string_value[0] = '\0';
        }
        
        ~Variable() {
            if (array_data) delete[] array_data;
        }
    };
    
    Variable variables[64];  // Increased capacity
    int variable_count = 0;
    
    // Execution context for multi-line programs
    struct ExecutionContext {
        int line_number;
        bool in_function;
        bool in_loop;
        char current_function[64];
        bool should_continue;
        
        ExecutionContext() : line_number(0), in_function(false), in_loop(false), should_continue(true) {
            current_function[0] = '\0';
        }
    } context;
    
public:
    RTCompiler(uint64_t ahci, int disk_port) : ahci_base(ahci), port(disk_port) {}
    
    // Main compilation and execution entry point
    FunctionResult compile_and_run_file(const char* program_file) {
        FunctionResult result;
        
        // Read program file
        static char program_source[8192];  // Increased buffer size
        int bytes_read = fat32_read_file(ahci_base, port, program_file,
                                       (unsigned char*)program_source,
                                       sizeof(program_source) - 1);
        
        if (bytes_read <= 0) {
            result.success = false;
            strcpy(result.error_message, "Could not read source file");
            return result;
        }
        
        program_source[bytes_read] = '\0';
        
        // Execute the multi-line program with persistent state
        return execute_multiline_program(program_source);
    }
    
    // Execute inline code (for REPL-style interaction)
    FunctionResult execute_inline(const char* code) {
        return execute_multiline_program(code);
    }
    
    // List available functions with their return types
    void list_available_functions() {
        cout << "=== Available Library Functions ===\n";
        
        const char* current_lib = nullptr;
        for (int i = 0; library_functions[i].name; i++) {
            const LibraryFunction& func = library_functions[i];
            
            if (!current_lib || strcmp(current_lib, func.library) != 0) {
                current_lib = func.library;
                cout << "\n" << func.library << " Library:\n";
            }
            
            cout << "  " << func.return_type << " " << func.name 
                 << func.signature << " - " << func.description << "\n";
        }
        cout << "\n";
    }
    
    // Show current variables and their values
    void show_variables() {
        cout << "=== Current Variables ===\n";
        if (variable_count == 0) {
            cout << "No variables defined.\n";
            return;
        }
        
        for (int i = 0; i < variable_count; i++) {
            Variable& var = variables[i];
            if (!var.initialized) continue;
            
            cout << var.name << " = ";
            
            switch (var.type) {
                case Variable::INT_VAR:
                    cout << var.int_value;
                    break;
                case Variable::STRING_VAR:
                    cout << "\"" << var.string_value << "\"";
                    break;
                case Variable::ARRAY_VAR:
                    cout << "[";
                    for (int j = 0; j < var.array_size; j++) {
                        cout << var.array_data[j];
                        if (j < var.array_size - 1) cout << ", ";
                    }
                    cout << "]";
                    break;
            }
            cout << "\n";
        }
        cout << "\n";
    }
    
    // Clear all variables (reset state)
    void clear_variables() {
        for (int i = 0; i < variable_count; i++) {
            if (variables[i].array_data) {
                delete[] variables[i].array_data;
                variables[i].array_data = nullptr;
            }
        }
        variable_count = 0;
        cout << "All variables cleared.\n";
    }
    
private:
    
// Also improve the line parsing to handle multiple statements better
FunctionResult execute_multiline_program(const char* source) {
    FunctionResult final_result;
    context = ExecutionContext(); // Reset context
    
    // Split source into lines and execute each one
    char line[512];
    const char* current_pos = source;
    
    while (*current_pos && context.should_continue) {
        // Extract next line or statement
        int line_len = 0;
        bool in_quotes = false;
        int brace_level = 0;
        bool found_semicolon = false;
        
        while (*current_pos && line_len < 511) {
            char c = *current_pos;
            
            if (c == '"' && (line_len == 0 || line[line_len-1] != '\\')) {
                in_quotes = !in_quotes;
            }
            
            if (!in_quotes) {
                if (c == '{') brace_level++;
                else if (c == '}') brace_level--;
                else if (c == ';' && brace_level == 0) {
                    found_semicolon = true;
                }
            }
            
            line[line_len++] = c;
            current_pos++;
            
            // End of statement conditions
            if (!in_quotes && brace_level == 0) {
                if (found_semicolon || c == '\n') {
                    break;
                }
            }
        }
        
        line[line_len] = '\0';
        
        // Clean up the line (remove whitespace, newlines, semicolons)
        char* clean_line = line;
        while (*clean_line == ' ' || *clean_line == '\t' || *clean_line == '\n') {
            clean_line++;
        }
        
        // Remove trailing whitespace and semicolon
        int len = strlen(clean_line);
        while (len > 0 && (clean_line[len-1] == ' ' || clean_line[len-1] == '\t' || 
                          clean_line[len-1] == '\n' || clean_line[len-1] == ';')) {
            clean_line[--len] = '\0';
        }
        
        // Skip empty lines and comments
        if (len == 0 || strncmp(clean_line, "//", 2) == 0 || 
            strncmp(clean_line, "/*", 2) == 0) {
            context.line_number++;
            continue;
        }
        
        // Execute the statement
        FunctionResult line_result = execute_statement(clean_line);
        
        // Update final result with the last meaningful result
        if (line_result.type != FunctionResult::VOID_RESULT || !line_result.success) {
            final_result = line_result;
        }
        
        if (!line_result.success) {
            final_result.success = false;
            cout << "Error on line " << context.line_number + 1 << ": " << line_result.error_message << "\n";
            break;
        }
        
        context.line_number++;
    }
    
    return final_result;
}

FunctionResult execute_statement(const char* stmt) {
    FunctionResult result;
    
    // Variable declarations
    if (strncmp(stmt, "int ", 4) == 0) {
        return execute_int_declaration(stmt + 4);
    }
    else if (strncmp(stmt, "string ", 7) == 0) {
        return execute_string_declaration(stmt + 7);
    }
    // Function calls
    else if (strchr(stmt, '(') && strchr(stmt, ')')) {
        return execute_function_call(stmt);
    }
    // Assignments
    else if (strchr(stmt, '=') && !strstr(stmt, "==")) {
        return execute_assignment(stmt);
    }
    // Handle cout statements - IMPORTANT FIX
    else if (strncmp(stmt, "cout << ", 8) == 0) {
        return execute_cout_statement(stmt + 8);
    }
    // Control flow statements
    else if (strncmp(stmt, "print ", 6) == 0) {
        return execute_print_statement(stmt + 6);
    }
    else if (strlen(stmt) > 0) {
        result.success = false;
        strcpy(result.error_message, "Unknown statement");
    }
    
    return result;
}

// New function to handle cout statements
FunctionResult execute_cout_statement(const char* expr) {
    FunctionResult result;
    result.type = FunctionResult::VOID_RESULT;
    
    while (*expr == ' ') expr++; // Skip whitespace
    
    if (*expr == '"') {
        // String literal: cout << "hello"
        expr++; // Skip opening quote
        char output[512];
        int i = 0;
        while (*expr && *expr != '"' && i < 511) {
            output[i++] = *expr++;
        }
        output[i] = '\0';
        cout << output;
        
        // Check for << endl or similar
        const char* remaining = expr;
        if (*remaining == '"') remaining++; // Skip closing quote
        while (*remaining == ' ') remaining++;
        if (strncmp(remaining, "<< endl", 7) == 0 || 
            strncmp(remaining, "<<endl", 6) == 0 || 
            *remaining == '\0') {
            cout << "\n";
        }
    }
    else if (strchr(expr, '(')) {
        // Function call: cout << fibonacci(10)
        FunctionResult func_result = execute_function_call(expr);
        if (func_result.success) {
            print_result_inline(func_result); // Don't add newline automatically
        }
        result = func_result;
    }
    else {
        // Variable name or expression: cout << test
        // Remove any << endl part first
        char var_expr[128];
        int i = 0;
        while (*expr && *expr != ' ' && i < 127) {
            if (strncmp(expr, "<<", 2) == 0) break;
            var_expr[i++] = *expr++;
        }
        var_expr[i] = '\0';
        
        Variable* var = find_variable(var_expr);
        if (var && var->initialized) {
            switch (var->type) {
                case Variable::INT_VAR:
                    cout << var->int_value;
                    break;
                case Variable::STRING_VAR:
                    cout << var->string_value;
                    break;
                case Variable::ARRAY_VAR:
                    cout << "[";
                    for (int j = 0; j < var->array_size; j++) {
                        cout << var->array_data[j];
                        if (j < var->array_size - 1) cout << ", ";
                    }
                    cout << "]";
                    break;
            }
        } else {
            cout << "Undefined: " << var_expr;
        }
        
        // Check for << endl
        while (*expr == ' ') expr++;
        if (strncmp(expr, "<< endl", 7) == 0 || strncmp(expr, "<<endl", 6) == 0) {
            cout << "\n";
        } else if (*expr == '\0') {
            // If no endl specified, still add newline for convenience
            cout << "\n";
        }
    }
    
    return result;
}

// Helper function to print results inline (without automatic newline)
void print_result_inline(const FunctionResult& result) {
    switch (result.type) {
        case FunctionResult::INT_RESULT:
            cout << result.int_value;
            break;
        case FunctionResult::STRING_RESULT:
            cout << result.string_value;
            break;
        case FunctionResult::ARRAY_RESULT:
            cout << "[";
            for (int i = 0; i < result.array_size; i++) {
                cout << result.array_data[i];
                if (i < result.array_size - 1) cout << ", ";
            }
            cout << "]";
            break;
        case FunctionResult::VOID_RESULT:
            // Don't print anything for void results
            break;
    }
}

    
    FunctionResult execute_function_call(const char* call) {
        FunctionResult result;
        
        char func_name[64];
        int i = 0;
        
        // Extract function name
        while (call[i] && call[i] != '(' && i < 63) {
            func_name[i] = call[i];
            i++;
        }
        func_name[i] = '\0';
        
        // Check if it's a variable assignment with function call
        const char* assignment = strchr(call, '=');
        char var_name[64] = {0};
        
        if (assignment && assignment < strchr(call, '(')) {
            // Extract variable name
            int j = 0;
            const char* p = call;
            while (p < assignment && *p != ' ' && j < 63) {
                var_name[j++] = *p++;
            }
            var_name[j] = '\0';
            
            // Move to function call part
            call = assignment + 1;
            while (*call == ' ') call++;
            
            // Re-extract function name
            i = 0;
            while (call[i] && call[i] != '(' && i < 63) {
                func_name[i] = call[i];
                i++;
            }
            func_name[i] = '\0';
        }
        
        // Find the function in our registry
        const LibraryFunction* func = find_function(func_name);
        if (!func) {
            result.success = false;
            strcpy(result.error_message, "Unknown function");
            return result;
        }
        
        // Execute the function and get result
        result = execute_library_function_call(func, call);
        
        // If there was a variable assignment, store the result
        if (var_name[0] != '\0' && result.success) {
            store_result_in_variable(var_name, result);
        }
        
        return result;
    }
    
   // FIXED HARDWARE INTEGRATION FOR RT COMPILER
// This fixes the issues preventing hardware read/write from working

// Add these missing function declarations that the compiler expects

// Fix the MMIO function calls in the compiler
FunctionResult execute_library_function_call(const LibraryFunction* func, const char* call) {
    FunctionResult result;
    
    // Parse arguments from inside the parentheses of the function call
    const char* args = strchr(call, '(');
    if (!args) {
        result.success = false;
        strcpy(result.error_message, "Invalid function call syntax");
        return result;
    }
    args++; // Skip '('
    
    // --- Math Library ---
    if (strcmp(func->name, "fibonacci") == 0) {
        int n = parse_int_argument(args);
        result.type = FunctionResult::INT_RESULT;
        result.int_value = fibonacci(n);
    }
    else if (strcmp(func->name, "factorial") == 0) {
        int n = parse_int_argument(args);
        result.type = FunctionResult::INT_RESULT;
        result.int_value = factorial(n);
    }
    else if (strcmp(func->name, "power") == 0) {
        int base, exp;
        parse_two_int_arguments(args, &base, &exp);
        result.type = FunctionResult::INT_RESULT;
        result.int_value = power(base, exp);
    }
    else if (strcmp(func->name, "gcd") == 0) {
        int a, b;
        parse_two_int_arguments(args, &a, &b);
        result.type = FunctionResult::INT_RESULT;
        result.int_value = gcd(a, b);
    }
    else if (strcmp(func->name, "sqrt_approx") == 0) {
        int n = parse_int_argument(args);
        result.type = FunctionResult::INT_RESULT;
        result.int_value = sqrt_approx(n);
    }
    else if (strcmp(func->name, "is_prime") == 0) {
        int n = parse_int_argument(args);
        result.type = FunctionResult::INT_RESULT;
        result.int_value = is_prime(n);
    }

    // --- Crypto Library ---
    else if (strcmp(func->name, "simple_hash") == 0) {
        char* str = parse_string_argument(args);
        if (str) {
            result.type = FunctionResult::INT_RESULT;
            result.int_value = simple_hash(str);
        } else {
            result.success = false;
            strcpy(result.error_message, "Invalid string argument for simple_hash");
        }
    }
    else if (strcmp(func->name, "caesar_cipher") == 0) {
        char* str = parse_string_argument(args);
        const char* comma = strchr(args, ',');
        if (str && comma) {
            int shift = parse_int_argument(comma + 1);
            char* encrypted = caesar_cipher(str, shift);
            result.type = FunctionResult::STRING_RESULT;
            strcpy(result.string_value, encrypted);
        } else {
            result.success = false;
            strcpy(result.error_message, "Invalid arguments for caesar_cipher");
        }
    }

    // --- Hardware Library ---
    else if (strcmp(func->name, "mmio_read8") == 0) {
        uint64_t addr = parse_hex_or_int_argument(args);
        result.type = FunctionResult::INT_RESULT;
        result.int_value = (int)mmio_read8(addr);
    }
    else if (strcmp(func->name, "mmio_read16") == 0) {
        uint64_t addr = parse_hex_or_int_argument(args);
        result.type = FunctionResult::INT_RESULT;
        result.int_value = (int)mmio_read16(addr);
    }
    else if (strcmp(func->name, "mmio_read32") == 0) {
        uint64_t addr = parse_hex_or_int_argument(args);
        result.type = FunctionResult::INT_RESULT;
        result.int_value = (int)mmio_read32(addr);
    }
    else if (strcmp(func->name, "mmio_write8") == 0) {
        uint64_t addr = parse_hex_or_int_argument(args);
        const char* comma = strchr(args, ',');
        if (comma) {
            int value = parse_int_argument(comma + 1);
            result.type = FunctionResult::INT_RESULT;
            result.int_value = mmio_write8(addr, (uint8_t)value) ? 1 : 0;
        } else {
            result.success = false;
            strcpy(result.error_message, "mmio_write8 requires address and value");
        }
    }
    else if (strcmp(func->name, "mmio_write16") == 0) {
        uint64_t addr = parse_hex_or_int_argument(args);
        const char* comma = strchr(args, ',');
        if (comma) {
            int value = parse_int_argument(comma + 1);
            result.type = FunctionResult::INT_RESULT;
            result.int_value = mmio_write16(addr, (uint16_t)value) ? 1 : 0;
        } else {
            result.success = false;
            strcpy(result.error_message, "mmio_write16 requires address and value");
        }
    }
    else if (strcmp(func->name, "mmio_write32") == 0) {
        uint64_t addr = parse_hex_or_int_argument(args);
        const char* comma = strchr(args, ',');
        if (comma) {
            int value = parse_int_argument(comma + 1);
            result.type = FunctionResult::INT_RESULT;
            result.int_value = mmio_write32(addr, (uint32_t)value) ? 1 : 0;
        } else {
            result.success = false;
            strcpy(result.error_message, "mmio_write32 requires address and value");
        }
    }

    // --- Filesystem Library ---
    else if (strcmp(func->name, "fat32_list_files") == 0) {
        fat32_list_files(ahci_base, port);
        result.type = FunctionResult::VOID_RESULT;
    }
    else if (strcmp(func->name, "fat32_read_file") == 0) {
        char* filename = parse_string_argument(args);
        if (filename) {
            static char file_buffer[4096]; // Static buffer for file content
            int bytes_read = fat32_read_file(ahci_base, port, filename, file_buffer, sizeof(file_buffer));
            if (bytes_read >= 0) {
                result.type = FunctionResult::STRING_RESULT;
                strcpy(result.string_value, file_buffer); // Return content as a string
            } else {
                result.success = false;
                strcpy(result.error_message, "Failed to read file");
            }
        } else {
            result.success = false;
            strcpy(result.error_message, "fat32_read_file requires a filename");
        }
    }
else if (strcmp(func->name, "fat32_write_file") == 0) {
    // Create separate, dedicated buffers for the filename and content
    char filename[256];
    char content[256];
    
    // A pointer to keep track of our position in the argument string
    const char* current_pos = args;

    // Parse the first argument into 'filename' and the second into 'content'
    if (parse_file_argument(&current_pos, filename, sizeof(filename)) &&
        parse_file_argument(&current_pos, content, sizeof(content))) {
        
        // Now 'filename' and 'content' are in separate memory and hold the correct values
        int write_result = fat32_write_file(ahci_base, port, filename, content, strlen(content));
        
        result.type = FunctionResult::INT_RESULT;
        result.int_value = write_result;

    } else {
        result.success = false;
        strcpy(result.error_message, "fat32_write_file requires two valid string arguments (e.g., \"file.txt\", \"data\")");
    }
}

    // --- Kernel & Display Library ---
    else if (strcmp(func->name, "memory_map_data") == 0) {
        populate_memory_map_data();
        result.type = FunctionResult::STRING_RESULT;
        char summary[1024] = "";
        for (int i = 0; i < memory_map_device_count && i < 10; i++) {
            strcat(summary, memory_map_data[i]);
            strcat(summary, "\n");
        }
        strcpy(result.string_value, summary);
    }
    else if (strcmp(func->name, "terminal_clear") == 0) {
        terminal_clear_screen();
        result.type = FunctionResult::VOID_RESULT;
    }
    else if (strcmp(func->name, "print_hex") == 0) {
        char* label;
        int value;
        parse_string_and_int_arguments(args, &label, &value);
        if(label){
            print_hex(label, value);
        }
        result.type = FunctionResult::VOID_RESULT;
    }

    // --- Fallback for Unimplemented Functions ---
    else {
        result.success = false;
        strcpy(result.error_message, "Function execution not implemented in RTCompiler");
    }
    
    return result;
}

    
    // Enhanced print statement that can handle variables
    FunctionResult execute_print_statement(const char* expr) {
        FunctionResult result;
        result.type = FunctionResult::VOID_RESULT;
        
        // Handle different print formats:
        // print "string literal"
        // print variable_name
        // print function_call()
        
        while (*expr == ' ') expr++; // Skip whitespace
        
        if (*expr == '"') {
            // String literal
            expr++; // Skip opening quote
            char output[512];
            int i = 0;
            while (*expr && *expr != '"' && i < 511) {
                output[i++] = *expr++;
            }
            output[i] = '\0';
            cout << output << "\n";
        }
        else if (strchr(expr, '(')) {
            // Function call - execute and print result
            FunctionResult func_result = execute_function_call(expr);
            if (func_result.success) {
                print_result(func_result);
            }
            result = func_result;
        }
        else {
            // Variable name
            Variable* var = find_variable(expr);
            if (var && var->initialized) {
                print_variable(*var);
            } else {
                cout << "Undefined variable: " << expr << "\n";
                result.success = false;
                strcpy(result.error_message, "Undefined variable");
            }
        }
        
        return result;
    }
    
    void print_result(const FunctionResult& result) {
        switch (result.type) {
            case FunctionResult::INT_RESULT:
                cout << result.int_value << "\n";
                break;
            case FunctionResult::STRING_RESULT:
                cout << result.string_value << "\n";
                break;
            case FunctionResult::ARRAY_RESULT:
                cout << "[";
                for (int i = 0; i < result.array_size; i++) {
                    cout << result.array_data[i];
                    if (i < result.array_size - 1) cout << ", ";
                }
                cout << "]\n";
                break;
            case FunctionResult::VOID_RESULT:
                // Don't print anything for void results
                break;
        }
    }
    
    void print_variable(const Variable& var) {
        switch (var.type) {
            case Variable::INT_VAR:
                cout << var.int_value << "\n";
                break;
            case Variable::STRING_VAR:
                cout << var.string_value << "\n";
                break;
            case Variable::ARRAY_VAR:
                cout << "[";
                for (int i = 0; i < var.array_size; i++) {
                    cout << var.array_data[i];
                    if (i < var.array_size - 1) cout << ", ";
                }
                cout << "]\n";
                break;
        }
    }
    
    void store_result_in_variable(const char* var_name, const FunctionResult& result) {
        Variable* var = find_variable(var_name);
        if (!var) {
            if (variable_count >= 256) return;
            var = &variables[variable_count++];
            strcpy(var->name, var_name);
        }
        
        var->initialized = true;
        
        switch (result.type) {
            case FunctionResult::INT_RESULT:
                var->type = Variable::INT_VAR;
                var->int_value = result.int_value;
                break;
            case FunctionResult::STRING_RESULT:
                var->type = Variable::STRING_VAR;
                strcpy(var->string_value, result.string_value);
                break;
            case FunctionResult::ARRAY_RESULT:
                var->type = Variable::ARRAY_VAR;
                if (var->array_data) delete[] var->array_data;
                var->array_size = result.array_size;
                var->array_data = new int[result.array_size];
                for (int i = 0; i < result.array_size; i++) {
                    var->array_data[i] = result.array_data[i];
                }
                break;
            default:
                break;
        }
    }
    
    // Variable declaration implementations
    FunctionResult execute_int_declaration(const char* decl) {
        FunctionResult result;
        result.type = FunctionResult::VOID_RESULT;
        
        char var_name[64];
        int i = 0;
        
        while (decl[i] && decl[i] != ' ' && decl[i] != '=' && i < 63) {
            var_name[i] = decl[i];
            i++;
        }
        var_name[i] = '\0';
        
        int value = 0;
        const char* equals = strchr(decl, '=');
        if (equals) {
            equals++;
            while (*equals == ' ') equals++;
            
            // Check if it's a function call or literal value
            if (strchr(equals, '(')) {
                FunctionResult func_result = execute_function_call(equals);
                if (func_result.success && func_result.type == FunctionResult::INT_RESULT) {
                    value = func_result.int_value;
                }
            } else {
                value = parse_number(equals);
            }
        }
        
        set_int_variable(var_name, value);
        return result;
    }
    
    FunctionResult execute_string_declaration(const char* decl) {
        FunctionResult result;
        result.type = FunctionResult::VOID_RESULT;
        
        char var_name[64];
        int i = 0;
        
        while (decl[i] && decl[i] != ' ' && decl[i] != '=' && i < 63) {
            var_name[i] = decl[i];
            i++;
        }
        var_name[i] = '\0';
        
        const char* value = "";
        const char* equals = strchr(decl, '=');
        if (equals) {
            equals++;
            while (*equals == ' ') equals++;
            
            if (*equals == '"') {
                equals++; // Skip opening quote
                static char string_buf[512];
                int j = 0;
                while (*equals && *equals != '"' && j < 511) {
                    string_buf[j++] = *equals++;
                }
                string_buf[j] = '\0';
                value = string_buf;
            } else if (strchr(equals, '(')) {
                // Function call returning string
                FunctionResult func_result = execute_function_call(equals);
                if (func_result.success && func_result.type == FunctionResult::STRING_RESULT) {
                    value = func_result.string_value;
                }
            }
        }
        
        set_string_variable(var_name, value);
        return result;
    }
    
    FunctionResult execute_assignment(const char* assign) {
        FunctionResult result;
        result.type = FunctionResult::VOID_RESULT;
        
        const char* equals = strchr(assign, '=');
        if (!equals) return result;
        
        char var_name[64];
        int i = 0;
        const char* p = assign;
        
        while (p < equals && *p != ' ' && i < 63) {
            var_name[i++] = *p++;
        }
        var_name[i] = '\0';
        
        equals++;
        while (*equals == ' ') equals++;
        
        // Check if assignment is from function call
        if (strchr(equals, '(')) {
            FunctionResult func_result = execute_function_call(equals);
            if (func_result.success) {
                store_result_in_variable(var_name, func_result);
            }
            result = func_result;
        } else {
            // Simple value assignment
            int value = parse_number(equals);
            set_int_variable(var_name, value);
        }
        
        return result;
    }
    
    // Helper functions (argument parsing, variable management, etc.)
    // ... [Include all the parsing and utility functions from the original code]
    
    const LibraryFunction* find_function(const char* name) {
        for (int i = 0; library_functions[i].name; i++) {
            if (strcmp(library_functions[i].name, name) == 0) {
                return &library_functions[i];
            }
        }
        return nullptr;
    }
    
    Variable* find_variable(const char* name) {
        for (int i = 0; i < variable_count; i++) {
            if (strcmp(variables[i].name, name) == 0) {
                return &variables[i];
            }
        }
        return nullptr;
    }
    
    void set_int_variable(const char* name, int value) {
        Variable* var = find_variable(name);
        if (!var) {
            if (variable_count >= 256) return;
            var = &variables[variable_count++];
            strcpy(var->name, name);
        }
        
        var->type = Variable::INT_VAR;
        var->int_value = value;
        var->initialized = true;
    }
    
    void set_string_variable(const char* name, const char* value) {
        Variable* var = find_variable(name);
        if (!var) {
            if (variable_count >= 256) return;
            var = &variables[variable_count++];
            strcpy(var->name, name);
        }
        
        var->type = Variable::STRING_VAR;
        strcpy(var->string_value, value);
        var->initialized = true;
    }
    
    // Include all parsing functions from original code
    int parse_int_argument(const char* args) {
        while (*args == ' ') args++;
        return parse_number(args);
    }
    
    uint64_t parse_hex_or_int_argument(const char* args) {
        while (*args == ' ') args++;
        return parse_number(args);
    }
    /**
 * Parses a double-quoted string from the input.
 * @param args_ptr A pointer to a char pointer, which will be advanced past the parsed string.
 * @param out_buffer The buffer to store the parsed string.
 * @param buffer_size The size of the output buffer.
 * @return True on success, false on parsing failure.
 */
	bool parse_file_argument(const char** args_ptr, char* out_buffer, int buffer_size) {
		const char* p = *args_ptr;
		
		// Skip leading whitespace and a single comma
		while (*p == ' ' || *p == '\t') p++;
		if (*p == ',') {
			p++;
			while (*p == ' ' || *p == '\t') p++;
		}

		// Ensure the string starts with a quote
		if (*p != '"') {
			return false;
		}
		p++; // Skip opening quote

		// Copy characters until the closing quote or buffer limit
		int i = 0;
		while (*p && *p != '"' && i < buffer_size - 1) {
			out_buffer[i++] = *p++;
		}
		out_buffer[i] = '\0'; // Null-terminate the string

		// Check for a closing quote and skip it
		if (*p != '"') {
			return false; // Unterminated string
		}
		p++; // Skip closing quote

		*args_ptr = p; // Update the original pointer for the next call
		return true;
	}
    char* parse_string_argument(const char* args) {
        while (*args == ' ') args++;
        if (*args == '"') {
            args++; // Skip opening quote
            static char string_buf[256];
            int i = 0;
            while (*args && *args != '"' && i < 255) {
                string_buf[i++] = *args++;
            }
            string_buf[i] = '\0';
            return string_buf;
        }
        return nullptr;
    }
    
    void parse_two_int_arguments(const char* args, int* a, int* b) {
        *a = parse_int_argument(args);
        const char* comma = strchr(args, ',');
        if (comma) {
            *b = parse_int_argument(comma + 1);
        }
    }
    
    void parse_string_and_int_arguments(const char* args, char** str, int* val) {
        *str = parse_string_argument(args);
        const char* comma = strchr(args, ',');
        if (comma) {
            *val = parse_int_argument(comma + 1);
        }
    }
    
    uint64_t parse_number(const char* str) {
        while (*str == ' ') str++; // Skip whitespace
        
        uint64_t result = 0;
        int base = 10;
        
        // Check for hex prefix
        if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
            base = 16;
            str += 2; // Skip "0x"
        }
        
        while (*str) {
            char c = *str;
            int digit_value = -1;
            
            if (c >= '0' && c <= '9') {
                digit_value = c - '0';
            } else if (base == 16) {
                if (c >= 'a' && c <= 'f') {
                    digit_value = c - 'a' + 10;
                } else if (c >= 'A' && c <= 'F') {
                    digit_value = c - 'A' + 10;
                }
            }
            
            if (digit_value == -1 || digit_value >= base) {
                break; // Invalid character or end of number
            }
            
            result = result * base + digit_value;
            str++;
        }
        
        return result;
    }
};

//=============================================================================
// ENHANCED KERNEL INTEGRATION WITH PERSISTENT STATE
//=============================================================================
// This function handles the logic for the 'savemap' command.
void cmd_save_memmap(const char* filename, uint64_t ahci_base, int port) {
    if (!filename || strlen(filename) == 0) {
        cout << "Usage: savemap <filename>\n";
        return;
    }

    // Use a large static buffer, which is safe for a kernel environment.
    static char map_buffer[4096];
    map_buffer[0] = '\0'; // Ensure the buffer is empty before use.

    // Make sure the memory map data is up-to-date.
    populate_memory_map_data();

    // Concatenate all discovered device strings into the single buffer.
    strcat(map_buffer, "--- Hardware Device Memory Map ---\n");
    for (int i = 0; i < memory_map_device_count; i++) {
        // Prevent a buffer overflow before appending the next line.
        if (strlen(map_buffer) + strlen(memory_map_data[i]) + 2 > sizeof(map_buffer)) {
            strcat(map_buffer, "... (Buffer full, output truncated) ...\n");
            break;
        }
        strcat(map_buffer, memory_map_data[i]);
        strcat(map_buffer, "\n");
    }

    // Call your existing FAT32 write function to save the buffer to disk.
    // This assumes 'ahcibase' and 'port' are globally accessible.
    int result = fat32_write_file(ahci_base, port, filename, map_buffer, strlen(map_buffer));

    if (result == 0) {
        cout << "Hardware memory map saved to file: " << filename << "\n";
    } else {
        cout << "Error: Failed to save memory map. Write operation failed.\n";
    }
}



// --- ENHANCED KERNEL ENTRY POINT WITH NOTEPAD AND FILE OPERATIONS ---
extern "C" void kernel_main() {
    terminal_initialize(); 
    init_terminal_io(); 
    init_keyboard();
    cout << "Kernel Initialized.\n";
    
    uint64_t dma_base = 0xFED00000;
    if (dma_manager.initialize(dma_base)) { 
        cout << "DMA Manager Initialized.\n"; 
    }
    
    cout << "FAT32 Filesystem Support Ready.\n\n";
    
    uint64_t ahci_base = disk_init();
    int port = 0;

    // Initialize filesystem
    bool fat32_initialized = false;
    if (fat32_init(ahci_base, port)) {
        fat32_initialized = true;
        cout << "FAT32 filesystem mounted.\n";
    }

    RTCompiler compiler(ahci_base, port);
    cout << "=== Real-Time CPP Compiler with Native Libraries ===\n";
    
    char cmd[256];
    while (true) {
        cout << "rtcpp> ";
        cin >> cmd;
        
        if (strcmp(cmd, "exit") == 0) {
            break;
        }
		if (strcmp(cmd, "discover") == 0) {
			cout << "Discovering hardware devices...\n";
			populate_memory_map_data();
			cout << "Found " << memory_map_device_count << " devices:\n";
			for (int i = 0; i < memory_map_device_count; i++) {
				cout << memory_map_data[i] << "\n";
			}
		}
		else if (strncmp(cmd, "savemap ", 8) == 0) {
			const char* filename = cmd + 8;
			while (*filename == ' ') { // Trim any leading spaces from the filename
				filename++;
			}
			cmd_save_memmap(filename, ahci_base, 0);
		}
		// For "test_mmio" command:
		else if (strcmp(cmd, "test_mmio") == 0) {
			cout << "Testing MMIO operations:\n";
			cout << "Reading VGA text buffer (0xB8000): ";
			uint16_t vga_char = mmio_read16(0xB8000);
			cout << "0x" << std::hex << vga_char << std::dec << "\n";
			
			cout << "Writing test pattern to VGA:\n";
			bool write_ok = mmio_write16(0xB8000, 0x0741); // 'A' with white on black
			cout << "Write result: " << (write_ok ? "SUCCESS" : "FAILED") << "\n";
		}
		else if (strcmp(cmd, "formatfs") == 0) {
			cmd_formatfs(ahci_base, 0);
        }
        else if (strcmp(cmd, "list") == 0) {
            compiler.list_available_functions();
        }
        else if (strncmp(cmd, "run ", 4) == 0) {
            compiler.compile_and_run_file(cmd + 4);
        }
        // NEW: Save code to file
        else if (strncmp(cmd, "save ", 5) == 0) {
            // Parse: save filename "code content"
            const char* args = cmd + 5;
            
            // Find filename (up to first space)
            char filename[64];
            int i = 0;
            while (args[i] && args[i] != ' ' && i < 63) {
                filename[i] = args[i];
                i++;
            }
            filename[i] = '\0';
            
            // Find quoted content
            const char* content_start = strchr(args, '"');
            if (content_start) {
                content_start++; // Skip opening quote
                const char* content_end = strrchr(content_start, '"');
                
                if (content_end && content_end > content_start) {
                    int content_length = content_end - content_start;
                    
                    // Save to file
                    int result = fat32_write_file(ahci_base, port, filename, 
                                                content_start, content_length);
                    
                    if (result == 0) {
                        cout << "File saved: " << filename << " (" << content_length << " bytes)\n";
                    } else {
                        cout << "Error saving file\n";
                    }
                } else {
                    cout << "Error: Missing closing quote\n";
                }
            } else {
                cout << "Usage: save filename \"code content\"\n";
            }
        }
        // NEW: List files in filesystem
        else if (strcmp(cmd, "ls") == 0) {
            if (fat32_initialized) {
                fat32_list_files(ahci_base, port);
            } else {
                cout << "Filesystem not mounted\n";
            }
        }
        // NEW: Display file content
        else if (strncmp(cmd, "cat ", 4) == 0) {
            const char* filename = cmd + 4;
            
            static char view_buffer[4096];
            int bytes_read = fat32_read_file(ahci_base, port, filename,
                                                     (unsigned char*)view_buffer,
                                                     sizeof(view_buffer) - 1);
            
            if (bytes_read > 0) {
                view_buffer[bytes_read] = '\0';
                cout << "=== " << filename << " ===\n";
                cout << view_buffer << "\n";
                cout << "=== End of " << filename << " ===\n";
            } else {
                cout << "Error: Could not read file " << filename << "\n";
            }
        }
        // NEW: Help command
        else if (strcmp(cmd, "help") == 0) {
            cout << "Available commands:\n";
            cout << "  list                    - Show available library functions\n";
            cout << "  run <file.cpp>          - Compile and run CPP file\n";
            cout << "  notepad <file.cpp>      - Edit file with notepad\n";
            cout << "  save <file> \"content\"   - Save content to file\n";
            cout << "  ls                      - List files in filesystem\n";
            cout << "  cat <file>              - Display file content\n";
            cout << "  help                    - Show this help\n";
            cout << "  exit                    - Exit compiler\n";
			cout << "  discover                - Discover hardware\n";
			cout << "  test_mmio               - Test memory map IO\n";
			cout << "  savemap <file>          - Saves the hardware memory map to a file\n";
            cout << "  formatfs                - format drive on sata port 0\n";

        }
        else if (strlen(cmd) > 0) {
            cout << "Unknown command. Type 'help' for available commands.\n";
        }
    }
}

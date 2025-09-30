#include "terminal_hooks.h"
#include "terminal_io.h"
#include "iostream_wrapper.h"
#include "interrupts.h"
#include "hardware_specs.h"
#include "stdlib_hooks.h"
#include "pci.h"
#include "sata.h"
#include "test2.h"
#include "disk.h"
#include "dma_memory.h"
#include "identify.h"
#include "notepad.h"
#include "xhci.h"

extern "C" {
    // A 64-bit integer is used for the guard variable.
    // The first byte is a flag, 0 for not yet initialized, 1 for initialized.
    int __cxa_guard_acquire(long long *guard) {
        // In a single-threaded kernel, we can simply check the flag.
        // The first byte of the guard variable acts as a flag.
        return !(*(char*)guard);
    }

    void __cxa_guard_release(long long *guard) {
        // Mark the guard as initialized.
        *(char*)guard = 1;
    }

    // This function is also part of the Itanium C++ ABI for pure virtual function calls.
    // It's good practice to provide it to avoid other potential linker errors.
    void __cxa_pure_virtual() {
        // You can halt, panic, or log an error here.
        // This function should ideally never be called.
    }
}



void usb_keyboard_self_test();




// --- COMMAND IMPLEMENTATIONS ---


// --- Add to FORWARD DECLARATIONS ---
void cmd_cat(uint64_t ahci_base, int port, const char* filename);


//=============================================================================
// REAL-TIME COMPILER WITH NATIVE CPP INCLUDES AS LIBRARIES
// Include CPP files directly in kernel.cpp to make functions available
// to the self-hosted compiler automatically
//=============================================================================
#include "libs.h"

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
    REGISTER_FUNCTION_EXT(xor_encrypt, "string(char*,int)", "XOR encryption", "crypto", "string"),
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
                cout << "Press enter to continue...";
                // BUG FIX: Reading into an uninitialized pointer causes a crash.
                // Use a small static buffer instead.
                char input_buffer[2];
                cin >> input_buffer;
                cout << "\n" << func.library << " Library:\n";
            }
            
            cout << "  " << func.return_type << " " << func.name << " " << 
            func.signature << " - " << func.description << "\n";
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
    // Handle cout statements
    else if (strncmp(stmt, "cout << ", 8) == 0) {
        return execute_cout_statement(stmt + 8);
    }
    // NEW: Handle cin statements
    else if (strncmp(stmt, "cin >> ", 7) == 0) {
        return execute_cin_statement(stmt + 7);
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

// NEW: Function to handle cin statements
FunctionResult execute_cin_statement(const char* expr) {
    FunctionResult result;
    result.type = FunctionResult::VOID_RESULT;

    // Skip leading whitespace
    while (*expr == ' ') expr++;

    // Extract the variable name
    char var_name[64];
    int i = 0;
    while (expr[i] && expr[i] != ' ' && expr[i] != ';' && i < 63) {
        var_name[i] = expr[i];
        i++;
    }
    var_name[i] = '\0';

    if (strlen(var_name) == 0) {
        result.success = false;
        strcpy(result.error_message, "cin requires a variable name.");
        return result;
    }

    // Read input from the actual terminal
    char input_buffer[256];
    cin >> input_buffer;

    // Store the result in the variable. This will create it if it doesn't exist.
    // Input is always read as a string.
    set_string_variable(var_name, input_buffer);

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
    
FunctionResult execute_library_function_call(const LibraryFunction* func, const char* call) {
    FunctionResult result;
    
    const char* args_str = strchr(call, '(');
    if (!args_str) {
        result.success = false;
        strcpy(result.error_message, "Invalid function call syntax");
        return result;
    }
    args_str++; // Skip '('
    
    // This is a simplified parser. It expects simple literals or variables.
    // A more robust solution would evaluate expressions.
    char arg1_str[256] = {0}, arg2_str[256] = {0};
    int arg1_int = 0, arg2_int = 0;

    // Basic argument parsing
    const char* comma = strchr(args_str, ',');
    if (comma) {
        strncpy(arg1_str, args_str, comma - args_str);
        const char* end_paren = strchr(comma + 1, ')');
        if(end_paren) strncpy(arg2_str, comma + 1, end_paren - (comma + 1));
    } else {
        const char* end_paren = strchr(args_str, ')');
        if(end_paren) strncpy(arg1_str, args_str, end_paren - args_str);
    }
    
    // --- Math Library ---
    if (strcmp(func->name, "fibonacci") == 0) {
        arg1_int = parse_number(arg1_str);
        result.type = FunctionResult::INT_RESULT;
        result.int_value = fibonacci(arg1_int);
    }
    // ... other math functions similarly ...

    // --- Crypto Library ---
    else if (strcmp(func->name, "xor_encrypt") == 0) {
        char* str_arg = parse_string_argument(arg1_str);
        int key_arg = parse_number(arg2_str);
        if (str_arg) {
            // The function modifies in-place, we need to copy for the result
            char temp_buf[512];
            strcpy(temp_buf, str_arg);
            xor_encrypt(temp_buf, key_arg); // This function is assumed to be void from libs.h
            result.type = FunctionResult::STRING_RESULT;
            strcpy(result.string_value, temp_buf);
        } else {
             result.success = false; strcpy(result.error_message, "Invalid string argument for xor_encrypt");
        }
    }
    // ... other crypto functions ...

    // --- String library ---
    else if (strcmp(func->name, "uint32_to_hex_string") == 0) {
        uint32_t val = parse_number(arg1_str);
        char buffer[9];
        uint32_to_hex_string(val, buffer); // Assumes declaration from string_lib.h
        result.type = FunctionResult::STRING_RESULT;
        strcpy(result.string_value, buffer);
    }
    else if (strcmp(func->name, "uint64_to_hex_string") == 0) {
        uint64_t val = parse_number(arg1_str);
        char buffer[17];
        uint64_to_hex_string(val, buffer); // Assumes declaration from string_lib.h
        result.type = FunctionResult::STRING_RESULT;
        strcpy(result.string_value, buffer);
    }

    // --- Fallback ---
    else {
        // ... existing fallback ...
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
// KERNEL UTILITY AND COMMAND FUNCTIONS
//=============================================================================
void cmd_save_memmap(const char* filename, uint64_t ahci_base, int port) {
    if (!filename || strlen(filename) == 0) {
        cout << "Usage: savemap <filename>\n";
        return;
    }

    static char map_buffer[4096];
    map_buffer[0] = '\0';
    populate_memory_map_data();

    strcat(map_buffer, "--- Hardware Device Memory Map ---\n");
    for (int i = 0; i < memory_map_device_count; i++) {
        if (strlen(map_buffer) + strlen(memory_map_data[i]) + 2 > sizeof(map_buffer)) {
            strcat(map_buffer, "... (Buffer full, output truncated) ...\n");
            break;
        }
        strcat(map_buffer, memory_map_data[i]);
        strcat(map_buffer, "\n");
    }

    int result = fat32_write_file(filename, map_buffer, strlen(map_buffer));

    if (result == 0) {
        cout << "Hardware memory map saved to file: " << filename << "\n";
    } else {
        cout << "Error: Failed to save memory map.\n";
    }
}

void int_to_string(int value, char* buffer) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    char temp[12];
    int i = 0;
    bool is_negative = false;

    if (value < 0) {
        is_negative = true;
        value = -value;
    }

    while (value > 0) {
        temp[i++] = (value % 10) + '0';
        value /= 10;
    }

    if (is_negative) {
        temp[i++] = '-';
    }

    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

//=============================================================================
// NOTEPAD IMPLEMENTATION
//=============================================================================

// --- NOTEPAD CONSTANTS ---
#define MAX_LINES 4000
#define MAX_VISIBLE_LINES 20
#define MAX_LINE_LENGTH 79
#define NOTEPAD_START_ROW 3
#define NOTEPAD_END_ROW (NOTEPAD_START_ROW + MAX_VISIBLE_LINES - 1)

// --- NOTEPAD STATE ---
static bool notepad_running = false;
static char notepad_filename[256];
static char notepad_buffer[MAX_LINES][MAX_LINE_LENGTH + 1];
static int cursor_row = 0;
static int cursor_col = 0;
static int current_line_count = 1;
static char current_filename[32] = "";
static int scroll_offset = 0;
static int visible_lines = MAX_VISIBLE_LINES;

extern bool extended_key;
extern int input_length;


// VGA text mode cursor and buffer functions
static volatile uint16_t* vga_buffer = (volatile uint16_t*)0xB8000;

static void notepad_set_cursor_position(int row, int col) {
    uint16_t pos = row * 80 + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void notepad_show_cursor() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 14);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

static void notepad_hide_cursor() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

static void notepad_write_char_at(int row, int col, char c, uint8_t color) {
    if (row >= 0 && row < 25 && col >= 0 && col < 80) {
        vga_buffer[row * 80 + col] = (uint16_t)c | ((uint16_t)color << 8);
    }
}

static void notepad_write_string_at(int row, int col, const char* str, uint8_t color) {
    int i = 0;
    while (str[i] != '\0' && col + i < 80) {
        notepad_write_char_at(row, col + i, str[i], color);
        i++;
    }
}

static void notepad_clear_line(int row, uint8_t color) {
    for (int i = 0; i < 80; i++) {
        notepad_write_char_at(row, i, ' ', color);
    }
}




void notepad_save_and_exit(const char* filename_arg) {
    char final_filename[256];
    if (filename_arg && filename_arg[0] != '\0') {
        strcpy(final_filename, filename_arg);
    } else if (current_filename[0] != '\0') {
        strcpy(final_filename, current_filename);
    } else {
        strcpy(final_filename, "untitled.txt");
    }

    char save_buffer[MAX_LINES * (MAX_LINE_LENGTH + 1)];
    save_buffer[0] = '\0';
    for (int i = 0; i < current_line_count; i++) {
        strcat(save_buffer, notepad_buffer[i]);
        if (i < current_line_count - 1) {
            strcat(save_buffer, "\n");
        }
    }

    int result = fat32_write_file(final_filename, save_buffer, strlen(save_buffer));
    notepad_write_string_at(24, 0, "                                                                                ", 0x07);
    if (result == 0) {
        notepad_write_string_at(24, 0, "File saved successfully!", 0x0A);
    } else {
        notepad_write_string_at(24, 0, "Error saving file!", 0x0C);
    }

    // Simple delay
    for (volatile int delay = 0; delay < 2000000; delay++);

    // ONLY set the flag - don't call any terminal functions
    notepad_running = false;
    
    // Clear screen manually using VGA buffer
    for (int i = 0; i < 25 * 80; i++) {
        vga_buffer[i] = 0x0720; // Space with gray on black
    }
    
    // Position cursor at start of line after prompt
    notepad_set_cursor_position(0, 5);
    notepad_show_cursor();
	
}
void notepad_load_file(const char* filename) {
    notepad_clear_buffer();
    static char view_buffer[4096];
    int bytes_read = fat32_read_file(ahci_base, 0, filename, (unsigned char*)view_buffer,sizeof(view_buffer) - 1);
    if (sizeof(view_buffer) >= 0) {
        view_buffer[sizeof(view_buffer)] = '\0';
        strcpy(current_filename, filename);
        int line = 0, col = 0;
        for (int i = 0; i < sizeof(view_buffer) && line < MAX_LINES; i++) {
            char c = view_buffer[i];
            if (c == '\n') {
                notepad_buffer[line][col] = '\0';
                line++;
                col = 0;
            } else if (c != '\r' && col < MAX_LINE_LENGTH) {
                notepad_buffer[line][col++] = c;
                notepad_buffer[line][col] = '\0';
            }
        }
        current_line_count = line + 1;
    } else {
        strcpy(current_filename, filename);
    }
}
void start_notepad(const char* filename) {
    // 1. Initial Setup
    notepad_running = true;
    notepad_hide_cursor();

    // 2. Clear the screen completely before drawing anything
    for (int i = 0; i < 25 * 80; i++) {
        vga_buffer[i] = 0x0720; // Space with gray on black
    }

    // Load file or clear buffer
    if (filename && filename[0] != '\0') {
        notepad_load_file(filename);
    } else {
        notepad_clear_buffer();
        current_filename[0] = '\0';
    }

    // Draw the initial UI
    notepad_draw_interface();
    notepad_update_cursor();

    // 3. Main Notepad Loop (This is the new, blocking part)
    while (is_notepad_running()) {
        // Halt the CPU until the next interrupt (e.g., a key press).
        // This is an efficient way to wait for input.
        asm("hlt");
    }

    // 4. Cleanup on Exit
    // The screen is already cleared by notepad_save_and_exit,
    // so no extra cleanup is needed here. Execution will now return
    // to kernel_main, which will print a fresh "rtcpp>" prompt.
}



void cmd_notepad(const char* filename) {
    strcpy(notepad_filename, filename);
    start_notepad(filename);
}



// --- SCROLLING AND DRAWING ---
void notepad_draw_interface();

void notepad_ensure_cursor_visible() {
    if (cursor_row < scroll_offset) {
        scroll_offset = cursor_row;
        notepad_draw_interface();
    } else if (cursor_row >= scroll_offset + visible_lines) {
        scroll_offset = cursor_row - visible_lines + 1;
        notepad_draw_interface();
    }
}

void notepad_draw_interface() {
    // Clear the entire screen in one operation before drawing
    for (int i = 0; i < 25 * 80; i++) {
        vga_buffer[i] = 0x0720; // Attribute 0x07 (light grey) on 0x20 (black)
    }

    // Title bar
    notepad_write_string_at(0, 0, "=== NOTEPAD === ", 0x0F);
    if (current_filename[0] != '\0') {
        notepad_write_string_at(0, 16, "File: ", 0x0F);
        notepad_write_string_at(0, 22, current_filename, 0x0F);
    } else {
        notepad_write_string_at(0, 16, "New File", 0x0F);
    }

    // Scroll position indicator
    if (current_line_count > visible_lines) {
        char scroll_info[32];
        strcpy(scroll_info, " Lines: ");
        char line_num[8];
        int_to_string(scroll_offset + 1, line_num);
        strcat(scroll_info, line_num);
        strcat(scroll_info, "-");
        int_to_string(scroll_offset + visible_lines, line_num);
        strcat(scroll_info, line_num);
        strcat(scroll_info, "/");
        int_to_string(current_line_count, line_num);
        strcat(scroll_info, line_num);
        notepad_write_string_at(0, 50, scroll_info, 0x0F);
    }

    // Help line
    notepad_write_string_at(1, 0, "ESC: Save & Exit | Arrows: Move | PgUp/PgDn: Scroll", 0x07);

    // Separator
    for (int i = 0; i < 80; i++) {
        notepad_write_char_at(2, i, '-', 0x07);
    }

    // Lines and numberings
    for (int i = 0; i < visible_lines; i++) {
        int buffer_line = scroll_offset + i;
        if (buffer_line < current_line_count) {
            char line_num_str[5];
            int_to_string(buffer_line + 1, line_num_str);
            notepad_write_string_at(NOTEPAD_START_ROW + i, 0, "   ", 0x08); // Clear area
            notepad_write_string_at(NOTEPAD_START_ROW + i, 3 - strlen(line_num_str), line_num_str, 0x08);
            notepad_write_char_at(NOTEPAD_START_ROW + i, 3, '|', 0x08);
            notepad_write_string_at(NOTEPAD_START_ROW + i, 4, notepad_buffer[buffer_line], 0x07);
        } else {
            notepad_write_string_at(NOTEPAD_START_ROW + i, 0, " ~ |", 0x08);
        }
    }
}

void notepad_update_cursor() {
    notepad_ensure_cursor_visible();
    int screen_row = cursor_row - scroll_offset;
    notepad_set_cursor_position(NOTEPAD_START_ROW + screen_row, 4 + cursor_col);
    notepad_show_cursor();
}

void notepad_redcurrent_line() {
    int screen_row = cursor_row - scroll_offset;
    if (screen_row >= 0 && screen_row < visible_lines) {
        for (int i = 4; i < 80; i++) {
            notepad_write_char_at(NOTEPAD_START_ROW + screen_row, i, ' ', 0x07);
        }
        notepad_write_string_at(NOTEPAD_START_ROW + screen_row, 4, notepad_buffer[cursor_row], 0x07);
    }
}

// --- TEXT BUFFER OPERATIONS ---
bool is_notepad_running() {
    return notepad_running;
}

void notepad_clear_buffer() {
    for (int i = 0; i < MAX_LINES; i++) {
        notepad_buffer[i][0] = '\0';
    }
    cursor_row = 0;
    cursor_col = 0;
    current_line_count = 1;
    scroll_offset = 0;
}

void notepad_insert_char(char c) {
    if (cursor_col >= MAX_LINE_LENGTH) return;
    char* line = notepad_buffer[cursor_row];
    int line_len = strlen(line);
    if (line_len < MAX_LINE_LENGTH) {
        for (int i = line_len; i >= cursor_col; i--) {
            line[i + 1] = line[i];
        }
        line[cursor_col] = c;
        cursor_col++;
        notepad_redcurrent_line();
    }
}

void notepad_delete_char() {
    if (cursor_col > 0) {
        char* line = notepad_buffer[cursor_row];
        int line_len = strlen(line);
        cursor_col--;
        for (int i = cursor_col; i < line_len; i++) {
            line[i] = line[i + 1];
        }
        notepad_redcurrent_line();
    } else if (cursor_row > 0) {
        char* prev_line = notepad_buffer[cursor_row - 1];
        char* curr_line = notepad_buffer[cursor_row];
        int prev_len = strlen(prev_line);
        int curr_len = strlen(curr_line);
        if (prev_len + curr_len < MAX_LINE_LENGTH) {
            strcat(prev_line, curr_line);
            for (int i = cursor_row; i < current_line_count - 1; i++) {
                strcpy(notepad_buffer[i], notepad_buffer[i + 1]);
            }
            notepad_buffer[current_line_count - 1][0] = '\0';
            current_line_count--;
            cursor_row--;
            cursor_col = prev_len;
            notepad_draw_interface();
        }
    }
}

void notepad_new_line() {
    if (current_line_count >= MAX_LINES) return;
    char* curr_line = notepad_buffer[cursor_row];
    char temp[MAX_LINE_LENGTH + 1];
    strcpy(temp, &curr_line[cursor_col]);
    curr_line[cursor_col] = '\0';
    for (int i = current_line_count; i > cursor_row + 1; i--) {
        strcpy(notepad_buffer[i], notepad_buffer[i - 1]);
    }
    strcpy(notepad_buffer[cursor_row + 1], temp);
    current_line_count++;
    cursor_row++;
    cursor_col = 0;
    notepad_draw_interface();
}

// Move cursor relative deltas, clamps values
void notepad_move_cursor(int delta_row, int delta_col) {
    cursor_row += delta_row;
    cursor_col += delta_col;
    if (cursor_row < 0) cursor_row = 0;
    if (cursor_row >= current_line_count) cursor_row = current_line_count - 1;
    int line_len = strlen(notepad_buffer[cursor_row]);
    if (cursor_col > line_len) cursor_col = line_len;
    if (cursor_col < 0) cursor_col = 0;
    if (cursor_col > MAX_LINE_LENGTH) cursor_col = MAX_LINE_LENGTH;
}

// Handle special keys like arrows, Home, End, Page Up/Down, and ESC
void notepad_handle_special_key(int scancode) {
    if (!notepad_running) return;
    switch (scancode) {
        case 0x48:  // Up arrow
            notepad_move_cursor(-1, 0);
            break;
        case 0x50:  // Down arrow
            notepad_move_cursor(1, 0);
            break;
        case 0x4B:  // Left arrow
            notepad_move_cursor(0, -1);
            break;
        case 0x4D:  // Right arrow
            notepad_move_cursor(0, 1);
            break;
        case 0x47:  // Home
            cursor_col = 0;
            break;
        case 0x4F:  // End
            cursor_col = strlen(notepad_buffer[cursor_row]);
            break;
        case 0x49:  // Page Up
            cursor_row -= visible_lines;
            if (cursor_row < 0) cursor_row = 0;
            break;
        case 0x51:  // Page Down
            cursor_row += visible_lines;
            if (cursor_row >= current_line_count) cursor_row = current_line_count - 1;
            break;
        case 0x01:  // ESC key
            notepad_save_and_exit(notepad_filename);
            return;
        default:
            break;
    }
    int line_len = strlen(notepad_buffer[cursor_row]);
    if (cursor_col > line_len) cursor_col = line_len;
    notepad_update_cursor();
}

// Handle character input in notepad
void notepad_handle_input(char key) {
    if (!notepad_running) return;
    switch (key) {
        case '\n': notepad_new_line(); break;
        case '\b': notepad_delete_char(); break;
        default:
            if (key >= 32 && key <= 126) { // Printable ASCII
                notepad_insert_char(key);
            }
            break;
    }
    notepad_update_cursor();
}


//=============================================================================
// KERNEL MAIN
//=============================================================================
extern "C" void kernel_main() {
    terminal_initialize();  
    init_terminal_io(); 
    init_keyboard();
    cout << "Kernel Initialized. Welcome to Real-Time C++.\n";
    
    uint64_t dma_base = 0xFED00000;
    if (dma_manager.initialize(dma_base)) { 
        cout << "DMA Manager Initialized.\n"; 
    }
    
    cout << "Probing SATA devices...\n";
    ahci_base = disk_init();
    int port = 0;

    bool fat32_initialized = false;
    if (fat32_init(ahci_base, port)) {
        fat32_initialized = true;
        cout << "FAT32 filesystem mounted on port 0.\n";
    }

    RTCompiler compiler(ahci_base, port);
    cout << "\n=== Real-Time C++ Compiler Online ===\nType 'help' for a list of commands.\n";
    
    char cmd[256];
    while (true) {
        cout << "rtcpp> ";
        cin >> cmd;
        
        if (strcmp(cmd, "exit") == 0) break;
        else if (strcmp(cmd, "help") == 0) {
            cout << "Available commands:\n";
            cout << "  list                  - Show available library functions\n";
            cout << "  run <file.cpp>        - Compile and run a script file\n";
            cout << "  notepad <file>        - Edit a file with notepad\n";
            cout << "  save <file> \"content\" - Save content to a file\n";
            cout << "  ls                    - List files in filesystem\n";
            cout << "  cat <file>            - Display file content\n";
            cout << "  vars                  - Show currently defined variables\n";
            cout << "  clear                 - Clear all script variables\n";
            cout << "  discover              - Discover hardware memory maps\n";
            cout << "  savemap <file>        - Save the hardware memory map to a file\n";
            cout << "  formatfs              - (DANGEROUS) Format the drive on port 0\n";
            cout << "  exit                  - Exit the shell\n";
            cout << "Any other input is treated as inline C++ code.\n";
        }
        else if (strncmp(cmd, "notepad ", 8) == 0) start_notepad(cmd + 8);
        else if (strcmp(cmd, "discover") == 0) {
            cout << "Discovering hardware devices...\n";
            populate_memory_map_data();
            cout << "Found " << memory_map_device_count << " devices:\n";
            for (int i = 0; i < memory_map_device_count; i++) {
                cout << memory_map_data[i] << "\n";
            }
        }
        else if (strncmp(cmd, "savemap ", 8) == 0) cmd_save_memmap(cmd + 8, ahci_base, 0);
        else if (strcmp(cmd, "formatfs") == 0) cmd_formatfs(ahci_base, 0);
        else if (strcmp(cmd, "list") == 0) compiler.list_available_functions();
        else if (strcmp(cmd, "vars") == 0) compiler.show_variables();
        else if (strcmp(cmd, "clear") == 0) compiler.clear_variables();
        else if (strncmp(cmd, "run ", 4) == 0) compiler.compile_and_run_file(cmd + 4);
        else if (strncmp(cmd, "save ", 5) == 0) {
            const char* args = cmd + 5;
            char filename[64];
            int i = 0;
            while (args[i] && args[i] != ' ' && i < 63) { filename[i] = args[i]; i++; }
            filename[i] = '\0';
            
            const char* content_start = strchr(args, '"');
            if (content_start) {
                content_start++;
                const char* content_end = strrchr(content_start, '"');
                if (content_end) {
                    int len = content_end - content_start;
                    int res = fat32_write_file(filename, content_start, len);
                    if (res == 0) cout << "File saved: " << filename << "\n";
                    else cout << "Error saving file.\n";
                } else cout << "Error: Missing closing quote.\n";
            } else cout << "Usage: save <filename> \"content\"\n";
        }
        else if (strcmp(cmd, "ls") == 0) {
            if (fat32_initialized) fat32_list_files(ahci_base, port);
            else cout << "Filesystem not mounted.\n";
        }
        else if (strncmp(cmd, "cat ", 4) == 0) {
            const char* filename = cmd + 4;
            static char view_buffer[4096];
            int bytes_read = fat32_read_file(ahci_base, port, filename, (unsigned char*)view_buffer, sizeof(view_buffer) - 1);
            
            if (bytes_read >= 0) {
                view_buffer[bytes_read] = '\0';
                cout << view_buffer << "\n";
            } else {
                cout << "Error: Could not read file " << filename << "\n";
            }
        }
        else if (strlen(cmd) > 0) {
            FunctionResult result = compiler.execute_inline(cmd);
            if (!result.success) {
                cout << "Error: " << result.error_message << "\n";
            } else if (result.type != FunctionResult::VOID_RESULT) {
                compiler.print_result(result);
            }
        }
    }
}


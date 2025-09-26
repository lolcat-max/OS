/*
ENHANCED TINYCC VM - STRING CONCATENATION IMPLEMENTATION

KEY FEATURES ADDED:

1. AUTOMATIC STRING CONCATENATION WITH + OPERATOR:
   - "Hello" + " " + "World" works naturally
   - Mixed types: "Count: " + int_to_str(42)
   - Enhanced ADD operation detects string operands automatically

2. STRING COMPARISON OPERATORS:
   - ==, !=, <, <=, >, >= all work with strings
   - Lexicographic ordering for relational comparisons
   - Mixed string/integer comparisons handled gracefully

3. BUILT-IN STRING FUNCTIONS:
   - str_length(str) - returns string length
   - str_substr(str, start, len) - extracts substring
   - int_to_str(num) - converts integer to string representation
   - str_compare(str1, str2) - explicit comparison (-1, 0, 1)

4. DYNAMIC STRING MEMORY MANAGEMENT:
   - 8KB string pool for runtime string allocation
   - Automatic garbage collection (naive reset when full)
   - Concatenation results stored in managed memory

5. STRING TYPE DETECTION:
   - VM automatically detects string vs integer operands
   - Pointer range checking for string identification
   - Seamless mixing of operations

EXAMPLE USAGE:

1. String Concatenation:
int main() {
    string greeting = "Hello";
    string name = "World";
    string message = greeting + ", " + name + "!";
    cout << message << endl;
    return 0;
}

2. String Operations:
int main() {
    string text = "Programming";
    cout << "Length: " << str_length(text) << endl;
    cout << "First 4 chars: " << str_substr(text, 0, 4) << endl;
    
    int number = 42;
    string num_str = int_to_str(number);
    cout << "Answer: " << num_str << endl;
    return 0;
}

3. File I/O with String Building:
int main() {
    int numbers[5];
    string output = "";
    
    // Fill array with squares
    for(int i = 0; i < 5; i++) {
        numbers[i] = i * i;
    }
    
    // Build output string
    output = "Array contents: ";
    for(int i = 0; i < array_size(numbers); i++) {
        output = output + int_to_str(numbers[i]);
        if(i < array_size(numbers) - 1) {
            output = output + ", ";
        }
    }
    output = output + "\n";
    
    // Write to file
    write_file("output.txt", output);
    cout << "Written: " << output;
    return 0;
}

4. String Comparison:
int main() {
    string a = "apple";
    string b = "banana";
    
    if(a == b) {
        cout << "Equal" << endl;
    } else if(a < b) {
        cout << a << " comes before " << b << endl;
    } else {
        cout << a << " comes after " << b << endl;
    }
    return 0;
}

5. Log File Generator:
int main() {
    string log_file = "system.log";
    string timestamp = "2024-01-01 12:00:00";
    
    // Create initial log entry
    string entry = "[" + timestamp + "] System started\n";
    write_file(log_file, entry);
    
    // Add numbered events
    for(int i = 1; i <= 5; i++) {
        string event = "[" + timestamp + "] Event #" + int_to_str(i) + " processed\n";
        append_file(log_file, event);
    }
    
    // Read and display full log
    string contents = read_file(log_file);
    cout << "Log contents:\n" << contents;
    return 0;
}

6. Interactive String Builder:
int main() {
    string result = "";
    string word;
    
    cout << "Enter words (empty line to finish):" << endl;
    
    while(1) {
        cout << "> ";
        cin >> word;
        
        if(str_length(word) == 0) {
            break;
        }
        
        if(str_length(result) > 0) {
            result = result + " ";
        }
        result = result + word;
    }
    
    cout << "Final sentence: " << result << endl;
    cout << "Length: " << str_length(result) << " characters" << endl;
    
    // Save to file
    string output = "User input: " + result + "\n";
    write_file("user_input.txt", output);
    
    return 0;
}

TECHNICAL IMPLEMENTATION NOTES:

1. String Pool Management:
   - Uses 8KB static pool for runtime strings
   - Simple bump allocator with reset on overflow
   - Strings from literals and user pool coexist

2. Type Detection Heuristic:
   - Checks if pointer falls within known string ranges
   - Literal pool (compile-time strings)
   - Runtime pool (concatenation results)

3. Enhanced Operators:
   - ADD operator overloaded for string concatenation
   - All comparison operators handle mixed string/int types
   - Automatic type promotion where sensible

4. Memory Safety:
   - Bounds checking on all string operations
   - Null pointer safety in all string functions
   - Graceful degradation on memory exhaustion

5. Integration:
   - Works seamlessly with existing file I/O
   - Compatible with array operations
   - Preserves all original VM functionality
*/_file("test.txt", content);
    
    string data = read_file("test.txt");
    cout << "File contents: " << data << endl;
    
    append_file("test.txt", "Appended text\n");
    string updated = read_file("test.txt");
    cout << "Updated contents: " << updated << endl;
    return 0;
}

3. Combined Example:
int main() {
    int numbers[5];
    string filename = "numbers.txt";
    
    // Fill array
    for(int i = 0; i < 5; i++) {
        numbers[i] = i * i;
    }
    
    // Write array to file (as text)
    string output = "";
    for(int i = 0; i < array_size(numbers); i++) {
        // Note: This would need string concatenation support
        // which isn't implemented yet
        cout << numbers[i] << " ";
    }
    cout << endl;
    
    return 0;
}
*/

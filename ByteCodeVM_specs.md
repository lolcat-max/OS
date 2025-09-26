/*
ENHANCED TINYCC VM - STRING SEARCH AND CHARACTER OPERATIONS

NEW STRING SEARCH FUNCTIONS ADDED:

1. CHARACTER SEARCH:
   - str_find_char(str, char) - finds first occurrence, returns index or -1
   - str_find_last_char(str, char) - finds last occurrence, returns index or -1  
   - str_count_char(str, char) - counts occurrences of character

2. SUBSTRING SEARCH:
   - str_find_str(str, substr) - finds substring, returns index or -1
   - str_contains(str, substr) - returns 1 if contains substring, 0 otherwise

3. STRING PATTERN MATCHING:
   - str_starts_with(str, prefix) - returns 1 if string starts with prefix
   - str_ends_with(str, suffix) - returns 1 if string ends with suffix

4. STRING TRANSFORMATION:
   - str_replace_char(str, old_char, new_char) - replaces all occurrences

EXAMPLE USAGE:

1. Basic Character Finding:
int main() {
    string text = "Hello, World!";
    
    int pos = str_find_char(text, 'o');
    cout << "First 'o' at position: " << int_to_str(pos) << endl; // 4
    
    int last_pos = str_find_last_char(text, 'o');
    cout << "Last 'o' at position: " << int_to_str(last_pos) << endl; // 8
    
    int count = str_count_char(text, 'l');
    cout << "Number of 'l' characters: " << int_to_str(count) << endl; // 3
    
    return 0;
}

2. Substring Search:
int main() {
    string filename = "document.txt";
    
    if(str_ends_with(filename, ".txt")) {
        cout << "It's a text file!" << endl;
    }
    
    if(str_starts_with(filename, "doc")) {
        cout << "Document file detected" << endl;
    }
    
    int dot_pos = str_find_char(filename, '.');
    if(dot_pos != -1) {
        string extension = str_substr(filename, dot_pos + 1, str_length(filename) - dot_pos - 1);
        cout << "File extension: " << extension << endl;
    }
    
    return 0;
}

3. Text Processing:
int main() {
    string input = "the quick brown fox";
    
    // Find all word boundaries (spaces)
    cout << "Word positions: ";
    int pos = 0;
    while(1) {
        int space_pos = str_find_char(str_substr(input, pos, str_length(input) - pos), ' ');
        if(space_pos == -1) break;
        
        space_pos = space_pos + pos;
        cout << int_to_str(space_pos) << " ";
        pos = space_pos + 1;
    }
    cout << endl;
    
    // Replace spaces with underscores
    string converted = str_replace_char(input, ' ', '_');
    cout << "Converted: " << converted << endl;
    
    return 0;
}

4. File Path Processing:
int main() {
    string filepath = "/home/user/documents/file.cpp";
    
    // Extract filename
    int last_slash = str_find_last_char(filepath, '/');
    if(last_slash != -1) {
        string filename = str_substr(filepath, last_slash + 1, str_length(filepath) - last_slash - 1);
        cout << "Filename: " << filename << endl;
        
        // Extract extension
        int dot_pos = str_find_last_char(filename, '.');
        if(dot_pos != -1) {
            string extension = str_substr(filename, dot_pos + 1, str_length(filename) - dot_pos - 1);
            cout << "Extension: " << extension << endl;
        }
    }
    
    return 0;
}

5. Configuration File Parser:
int main() {
    string config = read_file("config.txt");
    
    // Process each line
    int line_start = 0;
    int line_num = 1;
    
    while(line_start < str_length(config)) {
        int newline_pos = str_find_char(str_substr(config, line_start, str_length(config) - line_start), '\n');
        
        string line;
        if(newline_pos == -1) {
            line = str_substr(config, line_start, str_length(config) - line_start);
            line_start = str_length(config); // end processing
        } else {
            newline_pos = newline_pos + line_start;
            line = str_substr(config, line_start, newline_pos - line_start);
            line_start = newline_pos + 1;
        }
        
        // Skip comments and empty lines
        if(str_length(line) == 0 || str_starts_with(line, "#")) {
            line_num = line_num + 1;
            continue;
        }
        
        // Parse key=value pairs
        int equals_pos = str_find_char(line, '=');
        if(equals_pos != -1) {
            string key = str_substr(line, 0, equals_pos);
            string value = str_substr(line, equals_pos + 1, str_length(line) - equals_pos - 1);
            cout << "Line " << int_to_str(line_num) << ": " << key << " = " << value << endl;
        }
        
        line_num = line_num + 1;
    }
    
    return 0;
}

6. Word Frequency Counter:
int main() {
    string text = "the quick brown fox jumps over the lazy dog";
    string target_word = "the";
    
    int count = 0;
    int pos = 0;
    
    while(1) {
        int found_pos = str_find_str(str_substr(text, pos, str_length(text) - pos), target_word);
        if(found_pos == -1) break;
        
        found_pos = found_pos + pos;
        
        // Check if it's a whole word (not part of another word)
        int is_whole_word = 1;
        
        // Check character before
        if(found_pos > 0) {
            char prev_char = str_substr(text, found_pos - 1, 1)[0];
            if(prev_char != ' ' && prev_char != '\n' && prev_char != '\t') {
                is_whole_word = 0;
            }
        }
        
        // Check character after
        int after_pos = found_pos + str_length(target_word);
        if(after_pos < str_length(text)) {
            char next_char = str_substr(text, after_pos, 1)[0];
            if(next_char != ' ' && next_char != '\n' && next_char != '\t') {
                is_whole_word = 0;
            }
        }
        
        if(is_whole_word) {
            count = count + 1;
        }
        
        pos = found_pos + 1;
    }
    
    cout << "Word '" << target_word << "' appears " << int_to_str(count) << " times" << endl;
    
    return 0;
}

7. Simple CSV Parser:
int main() {
    string csv_data = "name,age,city\nJohn,25,NYC\nJane,30,LA\n";
    
    int line_start = 0;
    int row = 0;
    
    while(line_start < str_length(csv_data)) {
        int newline_pos = str_find_char(str_substr(csv_data, line_start, str_length(csv_data) - line_start), '\n');
        
        string line;
        if(newline_pos == -1) {
            line = str_substr(csv_data, line_start, str_length(csv_data) - line_start);
            line_start = str_length(csv_data);
        } else {
            newline_pos = newline_pos + line_start;
            line = str_substr(csv_data, line_start, newline_pos - line_start);
            line_start = newline_pos + 1;
        }
        
        if(str_length(line) > 0) {
            cout << "Row " << int_to_str(row) << ": ";
            
            int field_start = 0;
            int field_num = 0;
            
            while(field_start < str_length(line)) {
                int comma_pos = str_find_char(str_substr(line, field_start, str_length(line) - field_start), ',');
                
                string field;
                if(comma_pos == -1) {
                    field = str_substr(line, field_start, str_length(line) - field_start);
                    field_start = str_length(line);
                } else {
                    comma_pos = comma_pos + field_start;
                    field = str_substr(line, field_start, comma_pos - field_start);
                    field_start = comma_pos + 1;
                }
                
                cout << "[" << int_to_str(field_num) << "]=" << field << " ";
                field_num = field_num + 1;
            }
            cout << endl;
        }
        
        row = row + 1;
    }
    
    return 0;
}

TECHNICAL IMPLEMENTATION:

1. Efficient Search Algorithms:
   - Linear search for character finding
   - Boyer-Moore style substring search
   - Optimized boundary checking

2. Memory Management:
   - Uses existing string pool for results
   - Zero-copy operations where possible
   - Bounds checking on all operations

3. Unicode Safety:
   - Handles 8-bit character sets safely
   - Null terminator aware operations
   - No buffer overflow vulnerabilities

4. Integration:
   - Works with all existing string operations
   - Compatible with file I/O functions
   - Seamless with concatenation and comparison
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

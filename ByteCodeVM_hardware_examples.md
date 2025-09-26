TinyCC Hardware Discovery and MMIO Test Programs
Test Program 1: Basic Hardware Scanner
File: hw_scan.cpp
cppint main() {
    cout << "=== Hardware Discovery Test ===" << endl;
    
    // Discover all hardware devices
    int device_count = scan_hardware();
    cout << "Found " << device_count << " hardware devices" << endl << endl;
    
    // Display detailed information for each device
    int i = 0;
    while (i < device_count) {
        cout << "--- Device " << i << " ---" << endl;
        
        // Get device info array [vendor, device, addr_low, addr_high, size_low, size_high, type]
        int device_info = get_device_info(i);
        
        if (device_info > 0) {
            cout << "Vendor ID: 0x" << device_info[0] << endl;
            cout << "Device ID: 0x" << device_info[1] << endl;
            cout << "Base Address: 0x" << device_info[2] << endl;
            cout << "Size: 0x" << device_info[4] << endl;
            cout << "Device Type: " << device_info[6];
            
            // Decode device type
            if (device_info[6] == 1) cout << " (Storage)";
            else if (device_info[6] == 2) cout << " (Network)";
            else if (device_info[6] == 3) cout << " (Graphics)";
            else if (device_info[6] == 4) cout << " (Audio)";
            else if (device_info[6] == 5) cout << " (USB)";
            else cout << " (Unknown)";
            
            cout << endl << endl;
        }
        
        i = i + 1;
    }
    
    return 0;
}
Test Program 2: VGA Text Mode Controller
File: vga_test.cpp
cppint main() {
    cout << "=== VGA Text Mode Test ===" << endl;
    
    // VGA text buffer starts at 0xB8000
    int vga_base = 0xB8000;
    
    // Read current character at position (0,0)
    int current_char = mmio_read16(vga_base);
    cout << "Current VGA char at (0,0): 0x" << current_char << endl;
    
    // Write colorful text to VGA buffer
    // Format: low byte = ASCII character, high byte = color attributes
    // Color format: bits 7-4 = background, bits 3-0 = foreground
    
    int pos = 0;
    string message = "HARDWARE TEST";
    int msg_len = str_length(message);
    
    cout << "Writing '" << message << "' to VGA buffer..." << endl;
    
    int i = 0;
    while (i < msg_len) {
        // Get character from string
        char c = message[i]; // This would need proper string indexing
        
        // Create colored character: red on black (0x04)
        int colored_char = c + (0x04 << 8);
        
        // Write to VGA buffer at position i
        int addr = vga_base + (i * 2);
        int success = mmio_write16(addr, colored_char);
        
        if (success) {
            cout << "Wrote char '" << c << "' at position " << i << endl;
        } else {
            cout << "Failed to write at position " << i << endl;
        }
        
        i = i + 1;
    }
    
    // Test reading back what we wrote
    cout << "Reading back VGA buffer..." << endl;
    i = 0;
    while (i < msg_len) {
        int addr = vga_base + (i * 2);
        int read_char = mmio_read16(addr);
        int ascii_part = read_char & 0xFF;
        int color_part = (read_char >> 8) & 0xFF;
        
        cout << "Position " << i << ": ASCII=" << ascii_part << ", Color=0x" << color_part << endl;
        i = i + 1;
    }
    
    return 0;
}
Test Program 3: PCI Device Classifier
File: pci_classify.cpp
cppint main() {
    cout << "=== PCI Device Classification Test ===" << endl;
    
    int device_count = scan_hardware();
    cout << "Scanning " << device_count << " devices..." << endl << endl;
    
    // Arrays to count device types
    int storage_count = 0;
    int network_count = 0;
    int graphics_count = 0;
    int audio_count = 0;
    int usb_count = 0;
    int unknown_count = 0;
    
    // Get the full hardware array
    int hw_array = get_hardware_array();
    int total_entries = array_size(hw_array);
    int devices = total_entries / 7; // 7 fields per device
    
    cout << "Hardware array has " << total_entries << " entries (" << devices << " devices)" << endl << endl;
    
    int i = 0;
    while (i < devices) {
        int base_idx = i * 7;
        int vendor_id = hw_array[base_idx + 0];
        int device_id = hw_array[base_idx + 1];
        int device_type = hw_array[base_idx + 6];
        
        cout << "Device " << i << ": Vendor=0x" << vendor_id << " Device=0x" << device_id << " Type=";
        
        if (device_type == 1) {
            cout << "Storage";
            storage_count = storage_count + 1;
        } else if (device_type == 2) {
            cout << "Network";
            network_count = network_count + 1;
        } else if (device_type == 3) {
            cout << "Graphics";
            graphics_count = graphics_count + 1;
        } else if (device_type == 4) {
            cout << "Audio";
            audio_count = audio_count + 1;
        } else if (device_type == 5) {
            cout << "USB";
            usb_count = usb_count + 1;
        } else {
            cout << "Unknown";
            unknown_count = unknown_count + 1;
        }
        cout << endl;
        
        i = i + 1;
    }
    
    cout << endl << "=== Device Summary ===" << endl;
    cout << "Storage Controllers: " << storage_count << endl;
    cout << "Network Controllers: " << network_count << endl;
    cout << "Graphics Controllers: " << graphics_count << endl;
    cout << "Audio Controllers: " << audio_count << endl;
    cout << "USB Controllers: " << usb_count << endl;
    cout << "Unknown Devices: " << unknown_count << endl;
    cout << "Total: " << devices << endl;
    
    return 0;
}
Test Program 4: Memory-Mapped I/O Safety Test
File: mmio_safety.cpp
cppint main() {
    cout << "=== MMIO Safety Test ===" << endl;
    
    // Test safe addresses (VGA buffer area)
    int safe_addr = 0xB8000;
    cout << "Testing safe address 0x" << safe_addr << "..." << endl;
    
    // Try to read from VGA buffer (should succeed)
    int safe_read = mmio_read16(safe_addr);
    cout << "Safe read result: 0x" << safe_read << endl;
    
    // Try to write to VGA buffer (should succeed)
    int safe_write_result = mmio_write16(safe_addr, 0x4148); // 'H' with red foreground
    if (safe_write_result) {
        cout << "Safe write succeeded" << endl;
    } else {
        cout << "Safe write failed" << endl;
    }
    
    // Test unsafe addresses (random high memory)
    cout << endl << "Testing unsafe addresses..." << endl;
    
    // Try to read from unmapped memory (should be blocked)
    int unsafe_addr = 0xDEADBEEF;
    cout << "Attempting unsafe read from 0x" << unsafe_addr << "..." << endl;
    int unsafe_read = mmio_read32(unsafe_addr);
    cout << "Unsafe read result: 0x" << unsafe_read << endl;
    
    // Try to write to unmapped memory (should be blocked)
    cout << "Attempting unsafe write to 0x" << unsafe_addr << "..." << endl;
    int unsafe_write_result = mmio_write32(unsafe_addr, 0x12345678);
    if (unsafe_write_result) {
        cout << "Unsafe write unexpectedly succeeded!" << endl;
    } else {
        cout << "Unsafe write properly blocked" << endl;
    }
    
    // Test different data sizes
    cout << endl << "Testing different MMIO data sizes..." << endl;
    
    int test_addr = 0xB8002; // VGA buffer offset
    
    // 8-bit operations
    int write8_result = mmio_write8(test_addr, 0x41); // Write 'A'
    int read8_result = mmio_read8(test_addr);
    cout << "8-bit: wrote 0x41, read 0x" << read8_result << endl;
    
    // 16-bit operations  
    int write16_result = mmio_write16(test_addr, 0x4241); // Write 'AB'
    int read16_result = mmio_read16(test_addr);
    cout << "16-bit: wrote 0x4241, read 0x" << read16_result << endl;
    
    // 32-bit operations
    int write32_result = mmio_write32(test_addr, 0x44434241); // Write 'ABCD'
    int read32_result = mmio_read32(test_addr);
    cout << "32-bit: wrote 0x44434241, read 0x" << read32_result << endl;
    
    return 0;
}
Test Program 5: Hardware Device File Logger
File: hw_logger.cpp
cppint main() {
    cout << "=== Hardware Device Logger ===" << endl;
    
    // Scan hardware first
    int device_count = scan_hardware();
    cout << "Found " << device_count << " devices, logging to file..." << endl;
    
    // Create log file header
    string log_header = "Hardware Device Log\\n";
    log_header = log_header + "===================\\n\\n";
    
    int write_result = write_file("hardware.log", log_header);
    if (write_result) {
        cout << "Created hardware.log file" << endl;
    } else {
        cout << "Failed to create log file" << endl;
        return 1;
    }
    
    // Log each device
    int i = 0;
    while (i < device_count) {
        int device_info = get_device_info(i);
        
        if (device_info > 0) {
            // Build device entry string
            string device_entry = "Device ";
            device_entry = device_entry + int_to_str(i);
            device_entry = device_entry + ":\\n";
            
            device_entry = device_entry + "  Vendor ID: 0x";
            device_entry = device_entry + int_to_str(device_info[0]);
            device_entry = device_entry + "\\n";
            
            device_entry = device_entry + "  Device ID: 0x";
            device_entry = device_entry + int_to_str(device_info[1]);
            device_entry = device_entry + "\\n";
            
            device_entry = device_entry + "  Base Address: 0x";
            device_entry = device_entry + int_to_str(device_info[2]);
            device_entry = device_entry + "\\n";
            
            device_entry = device_entry + "  Size: 0x";
            device_entry = device_entry + int_to_str(device_info[4]);
            device_entry = device_entry + "\\n";
            
            device_entry = device_entry + "  Type: ";
            device_entry = device_entry + int_to_str(device_info[6]);
            device_entry = device_entry + "\\n\\n";
            
            // Append to log file
            int append_result = append_file("hardware.log", device_entry);
            if (append_result) {
                cout << "Logged device " << i << endl;
            } else {
                cout << "Failed to log device " << i << endl;
            }
        }
        
        i = i + 1;
    }
    
    // Read back and display the log
    cout << endl << "Reading back log file..." << endl;
    string log_contents = read_file("hardware.log");
    cout << "Log file contents:" << endl;
    cout << log_contents << endl;
    
    return 0;
}
Test Program 6: Interactive Hardware Explorer
File: hw_explorer.cpp
cppint main() {
    cout << "=== Interactive Hardware Explorer ===" << endl;
    cout << "Commands: scan, list, info <n>, mmio <addr>, quit" << endl << endl;
    
    int device_count = 0;
    int hw_array = 0;
    
    while (1) {
        cout << "hw> ";
        string command;
        cin >> command;
        
        if (str_compare(command, "quit") == 0) {
            cout << "Goodbye!" << endl;
            break;
        }
        else if (str_compare(command, "scan") == 0) {
            device_count = scan_hardware();
            hw_array = get_hardware_array();
            cout << "Scanned and found " << device_count << " devices" << endl;
        }
        else if (str_compare(command, "list") == 0) {
            if (device_count == 0) {
                cout << "No devices scanned yet. Use 'scan' first." << endl;
                continue;
            }
            
            cout << "Device List:" << endl;
            int i = 0;
            while (i < device_count) {
                int base_idx = i * 7;
                cout << "  " << i << ": Vendor=0x" << hw_array[base_idx + 0];
                cout << " Device=0x" << hw_array[base_idx + 1];
                cout << " Type=" << hw_array[base_idx + 6] << endl;
                i = i + 1;
            }
        }
        else if (str_starts_with(command, "info")) {
            if (device_count == 0) {
                cout << "No devices scanned yet. Use 'scan' first." << endl;
                continue;
            }
            
            int device_num;
            cin >> device_num;
            
            if (device_num < 0 || device_num >= device_count) {
                cout << "Invalid device number. Use 0-" << (device_count - 1) << endl;
                continue;
            }
            
            int device_info = get_device_info(device_num);
            cout << "Device " << device_num << " Details:" << endl;
            cout << "  Vendor ID: 0x" << device_info[0] << endl;
            cout << "  Device ID: 0x" << device_info[1] << endl;
            cout << "  Base Address: 0x" << device_info[2] << endl;
            cout << "  Size: 0x" << device_info[4] << endl;
            cout << "  Type: " << device_info[6] << endl;
        }
        else if (str_starts_with(command, "mmio")) {
            int addr;
            cin >> addr;
            
            cout << "Reading 32-bit value from 0x" << addr << "..." << endl;
            int value = mmio_read32(addr);
            cout << "Result: 0x" << value << endl;
        }
        else {
            cout << "Unknown command. Available: scan, list, info <n>, mmio <addr>, quit" << endl;
        }
        
        cout << endl;
    }
    
    return 0;
}
How to Use These Test Programs

Compile each program:

   compile hw_scan.cpp
   compile vga_test.cpp
   compile pci_classify.cpp
   compile mmio_safety.cpp
   compile hw_logger.cpp
   compile hw_explorer.cpp

Run the programs:

   run hw_scan.obj
   run vga_test.obj
   run pci_classify.obj
   run mmio_safety.obj
   run hw_logger.obj
   run hw_explorer.obj

Expected behavior:

hw_scan: Lists all discovered hardware devices with details
vga_test: Writes colored text to VGA buffer and reads it back
pci_classify: Categorizes devices by type and shows summary
mmio_safety: Tests safety mechanisms for memory access
hw_logger: Saves hardware info to a file and reads it back
hw_explorer: Interactive shell for exploring hardware



These test programs demonstrate the key capabilities of the enhanced TinyCC VM's hardware discovery and memory-mapped I/O system.

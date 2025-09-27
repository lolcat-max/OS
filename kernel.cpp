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









// --- MACROS AND CONSTANTS ---
#define SECTOR_SIZE 512
#define ENTRY_SIZE 32
#define ATTR_LONG_NAME 0x0F
#define ATTR_DIRECTORY 0x10
#define ATTR_VOLUME_ID 0x08
#define ATTR_ARCHIVE 0x20
#define DELETED_ENTRY 0xE5




// Function to print hex value with label (Keep this function)
void print_hex(const char* label, uint32_t value) {
    cout << label;

    // Convert to hex
    char hex_chars[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
    char buffer[11]; // Increased size for 0x + 8 digits + null

    buffer[0] = '0';
    buffer[1] = 'x';

    // Fill from right to left for potentially shorter numbers if desired, but fixed 8 is fine too.
    for (int i = 0; i < 8; i++) {
        uint8_t nibble = (value >> (28 - i * 4)) & 0xF;
        buffer[2 + i] = hex_chars[nibble];
    }
    buffer[10] = '\0';

    cout << buffer << "\n";
}




// Debug function to fully examine SATA controller state
uint64_t disk_init() {
    cout << "Disk initilisation\n";
    cout << "--------------------\n";

    // Find AHCI controller via PCI
    uint64_t ahci_base = 0;
    uint16_t bus, dev, func;
    uint16_t ahci_bus = 0, ahci_dev = 0, ahci_func = 0; // Store location

    for (bus = 0; bus < 256 && !ahci_base; bus++) {
        for (dev = 0; dev < 32 && !ahci_base; dev++) {
            for (func = 0; func < 8 && !ahci_base; func++) {
                // Check if device exists first (Vendor ID != 0xFFFF)
                uint32_t vendor_device_check = pci_read_config_dword(bus, dev, func, 0x00);
                if ((vendor_device_check & 0xFFFF) == 0xFFFF) {
                    continue; // No device here
                }

                uint32_t class_reg = pci_read_config_dword(bus, dev, func, 0x08);
                uint8_t class_code = (class_reg >> 24) & 0xFF;
                uint8_t subclass = (class_reg >> 16) & 0xFF;
                uint8_t prog_if = (class_reg >> 8) & 0xFF;

                if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01) {
                    uint32_t bar5 = pci_read_config_dword(bus, dev, func, 0x24);
                    // Check if BAR5 is memory mapped and non-zero
                    if ((bar5 & 0x1) == 0 && (bar5 & ~0xF) != 0) {
                        ahci_base = bar5 & ~0xF;
                        ahci_bus = bus;
                        ahci_dev = dev;
                        ahci_func = func;

                        cout << "Found AHCI controller at PCI ";
                        cout << (int)bus << ":" << (int)dev << "." << (int)func << "\n"; // Use dot separator common practice

                        // Get vendor and device ID
                        uint32_t vendor_device = pci_read_config_dword(bus, dev, func, 0x00);
                        uint16_t vendor_id = vendor_device & 0xFFFF;
                        uint16_t device_id = (vendor_device >> 16) & 0xFFFF;

                        // Use print_hex for consistency (need to adapt for 16-bit)
                        print_hex(" Vendor ID: ", vendor_id); // Assuming print_hex handles width ok
                        print_hex(" Device ID: ", device_id);

                    }
                }
            }
        }
    }

    if (!ahci_base) {
        cout << "No AHCI controller found or BAR5 not valid.\n";
        return -1;
    }
    return ahci_base;
}



// Forward declarations for tinycc VM glue
extern "C" void cmd_compile(uint64_t ahci_base, int port, const char* filename);
extern "C" void cmd_run(uint64_t ahci_base, int port, const char* filename);
extern "C" void cmd_exec(const char* code_text);

void usb_keyboard_self_test();




static inline int simple_atoi(const char* str) {
    int result = 0;
    bool negative = false;
    if (*str == '-') {
        negative = true;
        str++;
    }
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    return negative ? -result : result;
}

static inline int simple_strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}


static const uint32_t FAT_FREE_CLUSTER = 0x00000000;
static const uint32_t FAT_END_OF_CHAIN = 0x0FFFFFFF;
static const uint32_t FAT_BAD_CLUSTER  = 0x0FFFFFF7;



// --- Add to FORWARD DECLARATIONS ---
int fat32_rename_file(uint64_t ahci_base, int port, const char* old_name, const char* new_name);
int fat32_copy_file(uint64_t ahci_base, int port, const char* src_name, const char* dest_name);


// --- FORWARD DECLARATIONS ---
// Utilities
static inline void* simple_memcpy(void* dst, const void* src, size_t n);
static inline void* simple_memset(void* s, int c, size_t n);
static inline int simple_memcmp(const void* s1, const void* s2, size_t n);
static inline int stricmp(const char* s1, const char* s2);

static inline char* simple_strchr(const char* s, int c);
static inline char* simple_strcat(char* dest, const char* src);

// FAT32 Helpers
static void to_83_format(const char* filename, char* out);
void from_83_format(const char* fat_name, char* out);
static inline uint64_t cluster_to_lba(uint32_t cluster);
uint32_t clusters_needed(uint32_t size);

// Core FAT32 Functions
bool fat32_init(uint64_t ahci_base, int port);
uint32_t read_fat_entry(uint64_t ahci_base, int port, uint32_t cluster);
bool write_fat_entry(uint64_t ahci_base, int port, uint32_t cluster, uint32_t value);
uint32_t find_free_cluster(uint64_t ahci_base, int port, uint32_t start_cluster);
uint32_t allocate_cluster(uint64_t ahci_base, int port);
uint32_t allocate_cluster_chain(uint64_t ahci_base, int port, uint32_t num_clusters);
void free_cluster_chain(uint64_t ahci_base, int port, uint32_t start_cluster);
bool read_data_from_clusters(uint64_t ahci_base, int port, uint32_t start_cluster, void* data, uint32_t size);
bool write_data_to_clusters(uint64_t ahci_base, int port, uint32_t start_cluster, const void* data, uint32_t size);

// File Operations
void fat32_list_files(uint64_t ahci_base, int port);
int fat32_add_file(uint64_t ahci_base, int port, const char* filename, const void* data, uint32_t size);
int fat32_remove_file(uint64_t ahci_base, int port, const char* filename);
int fat32_read_file(uint64_t ahci_base, int port, const char* filename, void* data_buffer, uint32_t buffer_size);
int fat32_write_file(uint64_t ahci_base, int port, const char* filename, const void* data, uint32_t size);

// Commands
void cmd_help();
void cmd_formatfs(uint64_t ahci_base, int port);
void fat32_show_filesystem_info();
void show_cluster_stats(uint64_t ahci_base, int port);


// --- DATA STRUCTURES ---
typedef struct {
    uint8_t  jmp_boot[3];
    char     oem_name[8];
    uint16_t bytes_per_sec;
    uint8_t  sec_per_clus;
    uint16_t rsvd_sec_cnt;
    uint8_t  num_fats;
    uint16_t root_ent_cnt;
    uint16_t tot_sec16;
    uint8_t  media;
    uint16_t fat_sz16;
    uint16_t sec_per_trk;
    uint16_t num_heads;
    uint32_t hidd_sec;
    uint32_t tot_sec32;
    uint32_t fat_sz32;
    uint16_t ext_flags;
    uint16_t fs_ver;
    uint32_t root_clus;
    uint16_t fs_info;
    uint16_t bk_boot_sec;
    uint8_t  reserved[12];
    uint8_t  drv_num;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t vol_id;
    char     vol_lab[11];
    char     fil_sys_type[8];
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    char name[11];
    uint8_t attr;
    uint8_t ntres;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} __attribute__((packed)) fat_dir_entry_t;


// --- GLOBAL VARIABLES ---
static fat32_bpb_t fat32_bpb;
static uint32_t fat_start_sector = 0;
static uint32_t data_start_sector = 0;
static uint32_t current_directory_cluster = 2;
static uint32_t next_free_cluster = 3;
uint64_t ahci_base;
DMAManager dma_manager;

// --- UTILITY FUNCTION IMPLEMENTATIONS ---
static inline void* simple_memcpy(void* dst, const void* src, size_t n) { char* d = (char*)dst; const char* s = (const char*)src; for (size_t i = 0; i < n; i++) d[i] = s[i]; return dst; }
static inline void* simple_memset(void* s, int c, size_t n) { char* p = (char*)s; for (size_t i = 0; i < n; i++) p[i] = (char)c; return s; }
static inline int simple_memcmp(const void* s1, const void* s2, size_t n) { const unsigned char* p1 = (const unsigned char*)s1; const unsigned char* p2 = (const unsigned char*)s2; for (size_t i = 0; i < n; i++) { if (p1[i] != p2[i]) return p1[i] - p2[i]; } return 0; }

static inline char* simple_strchr(const char* s, int c) { while (*s != (char)c) if (!*s++) return nullptr; return (char*)s; }
static inline char* simple_strcat(char* dest, const char* src) { char* ptr = dest + simple_strlen(dest); while (*src != '\0') *ptr++ = *src++; *ptr = '\0'; return dest; }
static inline int stricmp(const char* s1, const char* s2) { while (*s1 && *s2) { char c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 + 32 : *s1; char c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 + 32 : *s2; if (c1 != c2) return c1 - c2; s1++; s2++; } return *s1 - *s2; }

// --- FAT32 HELPER IMPLEMENTATIONS ---
static void to_83_format(const char* filename, char* out) { simple_memset(out, ' ', 11); uint8_t i = 0, j = 0; while (filename[i] && filename[i] != '.' && j < 8) { char c = filename[i++]; out[j++] = (c >= 'a' && c <= 'z') ? c - 32 : c; } if (filename[i] == '.') i++; j = 8; while (filename[i] && j < 11) { char c = filename[i++]; out[j++] = (c >= 'a' && c <= 'z') ? c - 32 : c; } }
void from_83_format(const char* fat_name, char* out) { int i, j = 0; for (i = 0; i < 8 && fat_name[i] != ' '; i++) out[j++] = fat_name[i]; if (fat_name[8] != ' ') { out[j++] = '.'; for (i = 8; i < 11 && fat_name[i] != ' '; i++) out[j++] = fat_name[i]; } out[j] = '\0'; }
uint64_t cluster_to_lba(uint32_t cluster) {
    return data_start_sector + (cluster - 2) * fat32_bpb.sec_per_clus;
}uint32_t clusters_needed(uint32_t size) { uint32_t cluster_size = fat32_bpb.sec_per_clus * fat32_bpb.bytes_per_sec; return (size + cluster_size - 1) / cluster_size; }


// --- Add to FILE OPERATION IMPLEMENTATIONS ---
// --- Add near the top of your file, after the includes ---
// A memory-efficient bitmap class to track cluster usage.
// Uses 1 bit per cluster instead of 1 byte, reducing memory usage by 8x.
class Bitmap {
private:
    uint8_t* buffer;
    size_t size_in_bits;

public:
    Bitmap(size_t bits) : size_in_bits(bits) {
        // Calculate size in bytes, rounding up.
        size_t buffer_size = (bits + 7) / 8;
        buffer = new uint8_t[buffer_size];
        if (buffer) {
            // Clear the bitmap initially.
            simple_memset(buffer, 0, buffer_size);
        }
    }

    ~Bitmap() {
        delete[] buffer;
    }

    // Returns true if the memory was successfully allocated.
    bool is_valid() const {
        return buffer != nullptr;
    }

    // Set a bit to 1 (true).
    void set(size_t bit) {
        if (bit >= size_in_bits) return;
        buffer[bit / 8] |= (1 << (bit % 8));
    }

    // Test if a bit is 1.
    bool test(size_t bit) const {
        if (bit >= size_in_bits) return false;
        return (buffer[bit / 8] & (1 << (bit % 8))) != 0;
    }
};

void scan_directory_for_chkdsk(uint64_t ahci_base, int port, uint32_t dir_cluster, Bitmap& cluster_map, uint32_t max_clusters) {
    if (dir_cluster < 2 || dir_cluster >= max_clusters) return;
    
    uint8_t buffer[SECTOR_SIZE];
    uint32_t current_dir_cluster = dir_cluster;
    
    while (current_dir_cluster >= 2 && current_dir_cluster < FAT_BAD_CLUSTER) {
        // Mark the directory cluster itself as used
        cluster_map.set(current_dir_cluster);
        
        uint64_t lba = cluster_to_lba(current_dir_cluster);
        bool end_of_directory = false;
        
        // Process all sectors in this cluster
        for (uint8_t s = 0; s < fat32_bpb.sec_per_clus && !end_of_directory; s++) {
            if (read_sectors(ahci_base, port, lba + s, 1, buffer) != 0) return;
            
            // Process all entries in this sector
            for (uint16_t e = 0; e < SECTOR_SIZE / ENTRY_SIZE; e++) {
                fat_dir_entry_t *entry = (fat_dir_entry_t *)(buffer + e * ENTRY_SIZE);
                
                // End of directory marker - stop processing entirely
                if (entry->name[0] == 0x00) {
                    end_of_directory = true;
                    break;
                }
                
                // Skip deleted entries
                if (entry->name[0] == DELETED_ENTRY) continue;
                
                // Skip long filename entries (LFN)
                if (entry->attr == 0x0F) continue;
                
                // Skip volume label entries  
                if (entry->attr & 0x08) continue;
                
                uint32_t file_cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
                
                // Handle zero cluster (empty files)
                if (file_cluster == 0) continue;
                
                // Validate cluster range
                if (file_cluster < 2 || file_cluster >= max_clusters) continue;
                
                if (entry->attr & ATTR_DIRECTORY) {
                    // Skip . and .. entries
                    if (entry->name[0] == '.') continue;
                    
                    // Recursively scan subdirectory
                    scan_directory_for_chkdsk(ahci_base, port, file_cluster, cluster_map, max_clusters);
                } else {
                    // Mark all clusters in file's chain as used
                    uint32_t current_file_cluster = file_cluster;
                    int chain_length = 0; // Prevent infinite loops
                    
                    while (current_file_cluster >= 2 && current_file_cluster < FAT_BAD_CLUSTER && chain_length < 65536) {
                        cluster_map.set(current_file_cluster);
                        uint32_t next_cluster = read_fat_entry(ahci_base, port, current_file_cluster);
                        
                        // Detect circular references
                        if (next_cluster == current_file_cluster) break;
                        
                        current_file_cluster = next_cluster;
                        chain_length++;
                    }
                }
            }
        }
        
        // Move to next cluster in directory chain
        if (current_dir_cluster < FAT_BAD_CLUSTER && !end_of_directory) {
            current_dir_cluster = read_fat_entry(ahci_base, port, current_dir_cluster);
        } else {
            break;
        }
    }
}

// Renames a file by finding its directory entry and changing the name field.
int fat32_rename_file(uint64_t ahci_base, int port, const char* old_name, const char* new_name) {
    uint8_t buffer[SECTOR_SIZE];
    uint64_t lba = cluster_to_lba(current_directory_cluster);
    char old_target[11], new_target[11];
    to_83_format(old_name, old_target);
    to_83_format(new_name, new_target);

    for (uint8_t s = 0; s < fat32_bpb.sec_per_clus; s++) {
        if (read_sectors(ahci_base, port, lba + s, (uint32_t)1, buffer) != 0) return -1; // Read error

        for (uint16_t e = 0; e < SECTOR_SIZE / ENTRY_SIZE; e++) {
            fat_dir_entry_t *entry = (fat_dir_entry_t *)(buffer + e * ENTRY_SIZE);
            if (entry->name[0] == 0x00) return -2; // File not found

            if (entry->name[0] != DELETED_ENTRY && simple_memcmp(entry->name, old_target, 11) == 0) {
                // Found the file, now rename it
                simple_memcpy(entry->name, new_target, 11);
                
                // Write the modified directory sector back to disk
                if (write_sectors(ahci_base, port, lba + s, (uint32_t)1, buffer) != 0) {
                    return -3; // Write error
                }
                return 0; // Success
            }
        }
    }
    return -2; // File not found
}

// Copies a file by reading it into memory and creating a new file with its contents.
int fat32_copy_file(uint64_t ahci_base, int port, const char* src_name, const char* dest_name) {
    uint8_t dir_sector_buffer[SECTOR_SIZE];
    uint64_t lba = cluster_to_lba(current_directory_cluster);
    char src_target[11];
    to_83_format(src_name, src_target);
    
    // 1. Find the source file to get its size
    uint32_t file_size = 0;
    bool found = false;
    for (uint8_t s = 0; s < fat32_bpb.sec_per_clus; s++) {
        if (read_sectors(ahci_base, port, lba + s, (uint32_t)1, dir_sector_buffer) != 0) return -1;
        for (uint16_t e = 0; e < SECTOR_SIZE / ENTRY_SIZE; e++) {
            fat_dir_entry_t* entry = (fat_dir_entry_t*)(dir_sector_buffer + e * ENTRY_SIZE);
            if (entry->name[0] == 0) break;
            if (entry->name[0] != DELETED_ENTRY && simple_memcmp(entry->name, src_target, 11) == 0) {
                file_size = entry->file_size;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) return -2; // Source file not found
    if (file_size == 0) { // Handle empty file case
        return fat32_add_file(ahci_base, port, dest_name, "", 0);
    }

    // 2. Allocate memory and read the source file into the buffer
    char* file_buffer = new char[file_size];
    if (!file_buffer) return -4; // Memory allocation failed

    int bytes_read = fat32_read_file(ahci_base, port, src_name, file_buffer, file_size + 1);
    if (bytes_read < 0) {
        delete[] file_buffer;
        return -1; // Read error
    }

    // 3. Write the buffer to the destination file
    int result = fat32_add_file(ahci_base, port, dest_name, file_buffer, file_size);
    
    // 4. Clean up and return
    delete[] file_buffer;
    return result;
}



// --- The main chkdsk command (now uses the Bitmap) ---
void cmd_chkdsk(uint64_t ahci_base, int port) {
    cout << "Checking filesystem for errors...\n";
    
    // DEBUG: Verify boot sector and root cluster
    cout << "DEBUG: Root cluster from BPB: " << fat32_bpb.root_clus << "\n";
    cout << "DEBUG: Sectors per cluster: " << fat32_bpb.sec_per_clus << "\n";
    cout << "DEBUG: Data start sector: " << data_start_sector << "\n";
    
    uint32_t total_data_sectors = fat32_bpb.tot_sec32 - data_start_sector;
    uint32_t max_clusters = total_data_sectors / fat32_bpb.sec_per_clus + 2;
    
    cout << "DEBUG: Max clusters: " << max_clusters << "\n";
    
    // Validate root cluster number
    if (fat32_bpb.root_clus < 2 || fat32_bpb.root_clus >= max_clusters) {
        cout << "ERROR: Invalid root cluster " << fat32_bpb.root_clus << ". Boot sector may be corrupted.\n";
        return;
    }
    
    // Test if we can actually read the root directory
    uint64_t root_lba = cluster_to_lba(fat32_bpb.root_clus);
    uint8_t test_buffer[SECTOR_SIZE];
    if (read_sectors(ahci_base, port, root_lba, 1, test_buffer) != 0) {
        cout << "ERROR: Cannot read root directory at LBA " << (int)root_lba << "\n";
        return;
    }
    
    // Check if root directory looks valid
    fat_dir_entry_t *first_entry = (fat_dir_entry_t *)test_buffer;
    cout << "DEBUG: First root entry name: ";
    for (int i = 0; i < 11; i++) {
        cout << (char)(first_entry->name[i] == 0 ? '.' : first_entry->name[i]);
    }
    cout << " (attr: 0x" << std::hex << (int)first_entry->attr << std::dec << ")\n";
    
    // Continue with normal chkdsk...
    Bitmap cluster_map(max_clusters);
    if (!cluster_map.is_valid()) {
        cout << "Error: Not enough memory to run chkdsk.\n";
        return;
    }
    
    cout << "Phase 1: Verifying files and directories...\n";
    scan_directory_for_chkdsk(ahci_base, port, fat32_bpb.root_clus, cluster_map, max_clusters);

    cout << "Phase 2: Verifying file allocation table...\n";
    uint32_t orphaned_clusters_found = 0;
    for (uint32_t cluster = 2; cluster < max_clusters; cluster++) {
        uint32_t fat_entry = read_fat_entry(ahci_base, port, cluster);

        // If the FAT says this cluster is in use, but our map says it's not...
        if (fat_entry != FAT_FREE_CLUSTER && !cluster_map.test(cluster)) {
            cout << "Found orphaned cluster: " << cluster << ". Reclaiming...\n";
            write_fat_entry(ahci_base, port, cluster, FAT_FREE_CLUSTER);
            orphaned_clusters_found++;
        }
    }

    if (orphaned_clusters_found > 0) {
        cout << "\nCHKDSK finished. Reclaimed " << orphaned_clusters_found << " orphaned clusters.\n";
    } else {
        cout << "\nCHKDSK finished. No errors found.\n";
    }
}


bool fat32_init(uint64_t ahci_base, int port) {
    uint8_t buffer[SECTOR_SIZE];
    if (read_sectors(ahci_base, port, 0, (uint32_t)1, buffer) != 0) return false;
    simple_memcpy(&fat32_bpb, buffer, sizeof(fat32_bpb_t));
    if (simple_memcmp(fat32_bpb.fil_sys_type, "FAT32   ", 8) != 0) return false;
    fat_start_sector = fat32_bpb.rsvd_sec_cnt;
    data_start_sector = fat_start_sector + (fat32_bpb.num_fats * fat32_bpb.fat_sz32);
    current_directory_cluster = fat32_bpb.root_clus;
    return true;
}

uint32_t read_fat_entry(uint64_t ahci_base, int port, uint32_t cluster) {
    if (cluster < 2) return FAT_BAD_CLUSTER;
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / SECTOR_SIZE);
    uint32_t entry_offset = fat_offset % SECTOR_SIZE;
    uint8_t buffer[SECTOR_SIZE];
    if (read_sectors(ahci_base, port, fat_sector, (uint32_t)1, buffer) != 0) return FAT_BAD_CLUSTER;
    return (*(uint32_t*)(buffer + entry_offset)) & 0x0FFFFFFF;
}

bool write_fat_entry(uint64_t ahci_base, int port, uint32_t cluster, uint32_t value) {
    if (cluster < 2) return false;
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector_offset = fat_offset / SECTOR_SIZE;
    uint32_t entry_offset = fat_offset % SECTOR_SIZE;
    uint8_t buffer[SECTOR_SIZE];
    if (read_sectors(ahci_base, port, fat_start_sector + fat_sector_offset, (uint32_t)1, buffer) != 0) return false;
    uint32_t* fat_entry_ptr = (uint32_t*)(buffer + entry_offset);
    *fat_entry_ptr = (*fat_entry_ptr & 0xF0000000) | (value & 0x0FFFFFFF);
    for (uint8_t i = 0; i < fat32_bpb.num_fats; i++) {
        uint32_t current_fat_sector = fat_start_sector + (i * fat32_bpb.fat_sz32) + fat_sector_offset;
        if (write_sectors(ahci_base, port, current_fat_sector, (uint32_t)1, buffer) != 0) return false;
    }
    return true;
}

uint32_t find_free_cluster(uint64_t ahci_base, int port, uint32_t start_cluster) {
    uint32_t max_clusters = (fat32_bpb.tot_sec32 - data_start_sector) / fat32_bpb.sec_per_clus + 2;
    for (uint32_t cluster = start_cluster; cluster < max_clusters; cluster++) {
        if (read_fat_entry(ahci_base, port, cluster) == FAT_FREE_CLUSTER) return cluster;
    }
    if (start_cluster > 2) {
        for (uint32_t cluster = 2; cluster < start_cluster; cluster++) {
            if (read_fat_entry(ahci_base, port, cluster) == FAT_FREE_CLUSTER) return cluster;
        }
    }
    return 0; // No free clusters
}

uint32_t allocate_cluster(uint64_t ahci_base, int port) {
    uint32_t cluster = find_free_cluster(ahci_base, port, next_free_cluster);
    if (cluster == 0) { cout << "Disk full\n"; return 0; }
    if (!write_fat_entry(ahci_base, port, cluster, FAT_END_OF_CHAIN)) { cout << "Failed to update FAT\n"; return 0; }
    uint8_t zero_buffer[SECTOR_SIZE];
    simple_memset(zero_buffer, 0, SECTOR_SIZE);
    uint64_t cluster_lba = cluster_to_lba(cluster);
    for (uint8_t s = 0; s < fat32_bpb.sec_per_clus; s++) {
        if (write_sectors(ahci_base, port, cluster_lba + s, (uint32_t)1, zero_buffer) != 0) { cout << "Failed to clear cluster\n"; }
    }
    next_free_cluster = cluster + 1;
    return cluster;
}

void free_cluster_chain(uint64_t ahci_base, int port, uint32_t start_cluster) {
    uint32_t current_cluster = start_cluster;
    while (current_cluster >= 2 && current_cluster < FAT_BAD_CLUSTER) {
        uint32_t next = read_fat_entry(ahci_base, port, current_cluster);
        if (!write_fat_entry(ahci_base, port, current_cluster, FAT_FREE_CLUSTER)) { cout << "Warning: Failed to free cluster " << current_cluster << "\n"; }
        if (current_cluster < next_free_cluster) next_free_cluster = current_cluster;
        current_cluster = next;
    }
}

uint32_t allocate_cluster_chain(uint64_t ahci_base, int port, uint32_t num_clusters) {
    if (num_clusters == 0) return 0;
    uint32_t first_cluster = allocate_cluster(ahci_base, port);
    if (first_cluster == 0) return 0;
    uint32_t current_cluster = first_cluster;
    for (uint32_t i = 1; i < num_clusters; i++) {
        uint32_t next_cluster = allocate_cluster(ahci_base, port);
        if (next_cluster == 0) { free_cluster_chain(ahci_base, port, first_cluster); return 0; }
        if (!write_fat_entry(ahci_base, port, current_cluster, next_cluster)) { free_cluster_chain(ahci_base, port, first_cluster); return 0; }
        current_cluster = next_cluster;
    }
    return first_cluster;
}

bool read_data_from_clusters(uint64_t ahci_base, int port, uint32_t start_cluster, void* data, uint32_t size) {
    uint8_t* data_ptr = (uint8_t*)data;
    uint32_t remaining = size;
    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size = fat32_bpb.sec_per_clus * SECTOR_SIZE;
    while (current_cluster >= 2 && current_cluster < FAT_BAD_CLUSTER && remaining > 0) {
        uint64_t lba = cluster_to_lba(current_cluster);
        uint32_t to_read = (remaining > cluster_size) ? cluster_size : remaining;
        uint32_t full_sectors = to_read / SECTOR_SIZE;
        if (full_sectors > 0) {
            if (read_sectors(ahci_base, port, lba, full_sectors, data_ptr) != 0) return false;
            data_ptr += full_sectors * SECTOR_SIZE;
            remaining -= full_sectors * SECTOR_SIZE;
        }
        uint32_t partial_bytes = to_read % SECTOR_SIZE;
        if (partial_bytes > 0) {
            uint8_t sector_buffer[SECTOR_SIZE];
            if (read_sectors(ahci_base, port, lba + full_sectors, (uint32_t)1, sector_buffer) != 0) return false;
            simple_memcpy(data_ptr, sector_buffer, partial_bytes);
            remaining -= partial_bytes;
        }
        current_cluster = read_fat_entry(ahci_base, port, current_cluster);
    }
    return remaining == 0;
}

bool write_data_to_clusters(uint64_t ahci_base, int port, uint32_t start_cluster, const void* data, uint32_t size) {
    const uint8_t* data_ptr = (const uint8_t*)data;
    uint32_t remaining = size;
    uint32_t current_cluster = start_cluster;
    uint32_t cluster_size = fat32_bpb.sec_per_clus * SECTOR_SIZE;
    while (current_cluster >= 2 && current_cluster < FAT_BAD_CLUSTER && remaining > 0) {
        uint64_t lba = cluster_to_lba(current_cluster);
        uint32_t to_write = (remaining > cluster_size) ? cluster_size : remaining;
        uint32_t full_sectors = to_write / SECTOR_SIZE;
        if (full_sectors > 0) {
            if (write_sectors(ahci_base, port, lba, full_sectors, (void*)data_ptr) != 0) return false;
            data_ptr += full_sectors * SECTOR_SIZE;
            remaining -= full_sectors * SECTOR_SIZE;
        }
        uint32_t partial_bytes = to_write % SECTOR_SIZE;
        if (partial_bytes > 0) {
            uint8_t sector_buffer[SECTOR_SIZE];
            simple_memset(sector_buffer, 0, SECTOR_SIZE);
            simple_memcpy(sector_buffer, data_ptr, partial_bytes);
            if (write_sectors(ahci_base, port, lba + full_sectors, (uint32_t)1, sector_buffer) != 0) return false;
            remaining -= partial_bytes;
        }
        current_cluster = read_fat_entry(ahci_base, port, current_cluster);
    }
    return remaining == 0;
}

// --- FILE OPERATION IMPLEMENTATIONS ---
void fat32_list_files(uint64_t ahci_base, int port) {
    uint8_t buffer[SECTOR_SIZE];
    uint64_t lba = cluster_to_lba(current_directory_cluster);
    cout << "Directory Listing:\nName          Size\n--------------------\n";
    for (uint8_t s = 0; s < fat32_bpb.sec_per_clus; s++) {
        if (read_sectors(ahci_base, port, lba + s, (uint32_t)1, buffer) != 0) { cout << "Error reading directory\n"; return; }
        for (uint16_t e = 0; e < SECTOR_SIZE / ENTRY_SIZE; e++) {
            fat_dir_entry_t *entry = (fat_dir_entry_t *)(buffer + e * ENTRY_SIZE);
            if (entry->name[0] == 0x00) return; // End of directory
            if (entry->name[0] == DELETED_ENTRY || (entry->attr & (ATTR_LONG_NAME | ATTR_VOLUME_ID))) continue;
            char fname[13];
            from_83_format(entry->name, fname);
            cout << fname;
            for (int i = simple_strlen(fname); i < 14; i++) cout << " ";
            cout << entry->file_size << "\n";
        }
    }
}

int fat32_add_file(uint64_t ahci_base, int port, const char* filename, const void* data, uint32_t size) {
    uint8_t buffer[SECTOR_SIZE];
    uint64_t dir_lba = cluster_to_lba(current_directory_cluster);
    char target_83[11];
    to_83_format(filename, target_83);

    uint32_t first_cluster = 0;
    if (size > 0) {
        uint32_t needed = clusters_needed(size);
        first_cluster = allocate_cluster_chain(ahci_base, port, needed);
        if (first_cluster == 0) return -6; // Disk full
        if (!write_data_to_clusters(ahci_base, port, first_cluster, data, size)) {
            free_cluster_chain(ahci_base, port, first_cluster);
            return -7; // Write failed
        }
    }

    for (uint8_t s = 0; s < fat32_bpb.sec_per_clus; s++) {
        if (read_sectors(ahci_base, port, dir_lba + s, (uint32_t)1, buffer) != 0) { if (first_cluster) free_cluster_chain(ahci_base, port, first_cluster); return -1; }
        for (uint16_t e = 0; e < SECTOR_SIZE / ENTRY_SIZE; e++) {
            fat_dir_entry_t *entry = (fat_dir_entry_t *)(buffer + e * ENTRY_SIZE);
            if (entry->name[0] == 0x00 || entry->name[0] == DELETED_ENTRY) {
                simple_memcpy(entry->name, target_83, 11);
                entry->attr = ATTR_ARCHIVE;
                entry->file_size = size;
                entry->fst_clus_lo = first_cluster & 0xFFFF;
                entry->fst_clus_hi = (first_cluster >> 16) & 0xFFFF;
                // Timestamps can be set here
                if (write_sectors(ahci_base, port, dir_lba + s, (uint32_t)1, buffer) != 0) { if (first_cluster) free_cluster_chain(ahci_base, port, first_cluster); return -2; }
                return 0; // Success
            }
        }
    }
    if (first_cluster) free_cluster_chain(ahci_base, port, first_cluster);
    return -4; // No space in directory
}
int fat32_remove_file(uint64_t ahci_base, int port, const char* filename) {
    uint8_t buffer[SECTOR_SIZE];
    uint64_t lba = cluster_to_lba(current_directory_cluster);
    char target[11];
    to_83_format(filename, target);
    
    for (uint8_t s = 0; s < fat32_bpb.sec_per_clus; s++) {
        if (read_sectors(ahci_base, port, lba + s, (uint32_t)1, buffer) != 0) return -1;
        
        for (uint16_t e = 0; e < SECTOR_SIZE / ENTRY_SIZE; e++) {
            fat_dir_entry_t *entry = (fat_dir_entry_t *)(buffer + e * ENTRY_SIZE);
            if (entry->name[0] == 0) return -4; // End of directory
            if (entry->name[0] == DELETED_ENTRY || (entry->attr & (ATTR_LONG_NAME | ATTR_VOLUME_ID))) continue;
            
            if (simple_memcmp(entry->name, target, 11) == 0) {
                uint32_t cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
                
                // COMPLETE DIRECTORY ENTRY CLEANUP
                entry->name[0] = DELETED_ENTRY;
                // Clear the remaining filename bytes to prevent garbage characters
                simple_memset(&entry->name[1], 0x00, 10);
                // Clear all other fields to prevent data leakage
                entry->attr = 0;
                entry->file_size = 0;
                entry->fst_clus_hi = 0;
                entry->fst_clus_lo = 0;
                entry->crt_time = 0;
                entry->crt_date = 0;
                entry->wrt_time = 0;
                entry->wrt_date = 0;
                entry->lst_acc_date = 0;
                
                if (write_sectors(ahci_base, port, lba + s, (uint32_t)1, buffer) != 0) return -2;
                if (cluster >= 2) free_cluster_chain(ahci_base, port, cluster);
                return 0;
            }
        }
    }
    return -4; // File not found
}


int fat32_read_file(uint64_t ahci_base, int port, const char* filename, void* data_buffer, uint32_t buffer_size) {
    uint8_t dir_sector_buffer[SECTOR_SIZE];
    uint64_t lba = cluster_to_lba(current_directory_cluster);
    char target[11];
    to_83_format(filename, target);
    for (uint8_t s = 0; s < fat32_bpb.sec_per_clus; s++) {
        if (read_sectors(ahci_base, port, lba + s, (uint32_t)1, dir_sector_buffer) != 0) { return -1; }
        for (uint16_t e = 0; e < SECTOR_SIZE / ENTRY_SIZE; e++) {
            fat_dir_entry_t* entry = (fat_dir_entry_t*)(dir_sector_buffer + e * ENTRY_SIZE);
            if (entry->name[0] == 0) return -2; // Not found
            if (entry->name[0] == DELETED_ENTRY || (entry->attr & (ATTR_LONG_NAME | ATTR_DIRECTORY | ATTR_VOLUME_ID))) continue;
            if (simple_memcmp(entry->name, target, 11) == 0) {
                uint32_t cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
                uint32_t size = entry->file_size;
                if (size == 0) { ((char*)data_buffer)[0] = '\0'; return 0; }
                uint32_t read_size = (size < buffer_size) ? size : buffer_size - 1;
                if (cluster >= 2 && read_data_from_clusters(ahci_base, port, cluster, data_buffer, read_size)) {
                    ((char*)data_buffer)[read_size] = '\0';
                    return read_size;
                }
                return -1; // Read error
            }
        }
    }
    return -2; // Not found
}

int fat32_write_file(uint64_t ahci_base, int port, const char* filename, const void* data, uint32_t size) {
    fat32_remove_file(ahci_base, port, filename); // Ignore error if file doesn't exist
    return fat32_add_file(ahci_base, port, filename, data, size);
}

bool fat32_format(uint64_t ahci_base, int port, uint32_t total_sectors, uint8_t sectors_per_cluster) {
    uint8_t sector[SECTOR_SIZE];
    simple_memset(sector, 0, SECTOR_SIZE);

    // --- 1. Validate Parameters ---
    if (total_sectors < 65536) {
        cout << "Error: Disk too small. Minimum 65536 sectors required for FAT32.\n";
        return false;
    }
    if ((sectors_per_cluster & (sectors_per_cluster - 1)) != 0 || sectors_per_cluster == 0) {
        cout << "Error: Sectors per cluster must be a power of 2.\n";
        return false;
    }

    // --- 2. Calculate Geometry ---
    uint32_t reserved_sectors = 32;
    uint32_t fat_size;
    uint32_t clusters;

    // A standard, direct formula to calculate FAT size correctly
    uint32_t numerator = total_sectors - reserved_sectors;
    uint32_t denominator = sectors_per_cluster + (512 / SECTOR_SIZE); // (sectors_per_cluster + 2*4/512) -> simplified
    clusters = numerator / denominator;
    
    // Check if cluster count is sufficient for FAT32
    if (clusters < 65525) {
        cout << "Error: Not enough clusters for FAT32.\n";
        cout << "  Calculated Clusters: " << clusters << " (Need >= 65525)\n";
        cout << "  Suggestion: Use a larger disk or smaller cluster size.\n";
        return false;
    }

    fat_size = (clusters * 4 + SECTOR_SIZE - 1) / SECTOR_SIZE;

    // --- 3. Create BPB ---
    fat32_bpb_t bpb = {};
    bpb.jmp_boot[0] = 0xEB; bpb.jmp_boot[1] = 0x58; bpb.jmp_boot[2] = 0x90;
    simple_memcpy(bpb.oem_name, "MSDOS5.0", 8);
    bpb.bytes_per_sec = SECTOR_SIZE;
    bpb.sec_per_clus = sectors_per_cluster;
    bpb.rsvd_sec_cnt = reserved_sectors;
    bpb.num_fats = 2;
    bpb.root_ent_cnt = 0;
    bpb.tot_sec16 = 0;
    bpb.media = 0xF8;
    bpb.fat_sz16 = 0;
    bpb.sec_per_trk = 63;
    bpb.num_heads = 255;
    bpb.hidd_sec = 0;
    bpb.tot_sec32 = total_sectors;
    bpb.fat_sz32 = fat_size;
    bpb.ext_flags = 0;
    bpb.fs_ver = 0;
    bpb.root_clus = 2;
    bpb.fs_info = 1;
    bpb.bk_boot_sec = 6;
    bpb.drv_num = 0x80;
    bpb.boot_sig = 0x29;
    bpb.vol_id = 0x12345678; // Example volume ID
    simple_memcpy(bpb.vol_lab, "NO NAME    ", 11);
    simple_memcpy(bpb.fil_sys_type, "FAT32   ", 8);

    // --- 4. Write Boot Sectors ---
    simple_memcpy(sector, &bpb, sizeof(bpb));
    sector[510] = 0x55;
    sector[511] = 0xAA; // FIXED: Correct boot signature
    
    sector[510] = 0x00;
    sector[511] = 0x00; // dud boot for testing

    cout << "Writing boot sector...\n";
    if (write_sectors(ahci_base, port, 0, 1, sector) != 0) return false;
    if (write_sectors(ahci_base, port, 6, 1, sector) != 0) return false; // Backup boot sector

    // --- 5. Write FSInfo Sector ---
    simple_memset(sector, 0, SECTOR_SIZE);
    *(uint32_t*)(sector + 0)   = 0x41615252; // FSI_LeadSig
    *(uint32_t*)(sector + 484) = 0x61417272; // FSI_StrucSig
    *(uint32_t*)(sector + 488) = clusters - 1; // FSI_Free_Count
    *(uint32_t*)(sector + 492) = 3;            // FSI_Nxt_Free
    sector[510] = 0x55;
    sector[511] = 0xAA;

    cout << "Writing FSInfo sector...\n";
    if (write_sectors(ahci_base, port, 1, 1, sector) != 0) return false;

    // --- 6. Initialize FATs ---
    cout << "Initializing FAT tables...\n";
    simple_memset(sector, 0, SECTOR_SIZE);
    *(uint32_t*)(sector + 0) = 0x0FFFFFF8; // Media descriptor & reserved
    *(uint32_t*)(sector + 4) = 0x0FFFFFFF; // Reserved
    *(uint32_t*)(sector + 8) = 0x0FFFFFFF; // EOC for root directory cluster

    uint32_t fat_start = reserved_sectors;
    for (int i = 0; i < bpb.num_fats; ++i) {
        if (write_sectors(ahci_base, port, fat_start, 1, sector) != 0) return false;
        fat_start += fat_size;
    }
    
    // Clear remaining FAT sectors
    simple_memset(sector, 0, SECTOR_SIZE);
    fat_start = reserved_sectors;
    for (int i = 0; i < bpb.num_fats; ++i) {
        for (uint32_t j = 1; j < fat_size; ++j) {
            if (write_sectors(ahci_base, port, fat_start + j, 1, sector) != 0) return false;
        }
        fat_start += fat_size;
    }

    // --- 7. Initialize Root Directory ---
    cout << "Initializing root directory...\n";
    uint32_t data_start = reserved_sectors + (bpb.num_fats * fat_size);
    uint64_t root_lba = data_start + ((bpb.root_clus - 2) * sectors_per_cluster);
    simple_memset(sector, 0, SECTOR_SIZE);
    for (uint8_t i = 0; i < sectors_per_cluster; ++i) {
        if (write_sectors(ahci_base, port, root_lba + i, 1, sector) != 0) return false;
    }

    cout << "Format completed successfully!\n";
    return true;
}


// --- COMMAND IMPLEMENTATIONS ---

bool fat32_format(uint64_t ahci_base, int port, uint32_t total_sectors, uint8_t sectors_per_cluster); // Defined above
void cmd_formatfs(uint64_t ahci_base, int port) {
    cout << "=== FAT32 Format Utility ===\n";
    uint32_t total_sectors = 2097152; // 1GB
    uint8_t sec_per_clus;
    if (total_sectors >= 33554432) sec_per_clus = 64; else if (total_sectors >= 16777216) sec_per_clus = 32;
    else if (total_sectors >= 524288) sec_per_clus = 16; else sec_per_clus = 8;
    cout << "Disk size: " << ((uint32_t)total_sectors * 512) / (1024 * 1024) << " MB. Cluster size: " << (int)sec_per_clus << " sectors.\n";
    cout << "WARNING: This will erase all data! Continue? (y/N): ";
    char confirm[10]; cin >> confirm;
    if (confirm[0] != 'y' && confirm[0] != 'Y') { cout << "Format cancelled.\n"; return; }
    if (fat32_format(ahci_base, port, total_sectors, sec_per_clus)) { cout << "\n=== Format Successful! ===\n"; }
    else { cout << "\n=== Format Failed! ===\n"; }
}

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
void cmd_cat(uint64_t ahci_base, int port, const char* filename) {
    if (!filename) {
        cout << "Usage: cat <filename>\n";
        return;
    }

    // Use a static buffer to avoid heap allocation in the kernel if possible
    static char file_buffer[4096]; 

    int bytes_read = fat32_read_file(ahci_base, port, filename, file_buffer, sizeof(file_buffer));

    if (bytes_read < 0) {
        cout << "Error: File not found or could not be read.\n";
    } else if (bytes_read == 0) {
        // File is empty, print nothing.
    } else {
        cout << file_buffer << "\n";
    }
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

struct LibraryFunction {
    const char* name;
    void* function_ptr;
    const char* signature;
    const char* description;
    const char* library;
};

// Macro to register functions easily
#define REGISTER_FUNCTION(func, sig, desc, lib) \
    {#func, (void*)func, sig, desc, lib}

// Global function registry - auto-populated from includes
static const LibraryFunction library_functions[] = {
    // Core kernel functions
    REGISTER_FUNCTION(memory_map_data, "string[]()", "Get hardware device list", "kernel"),
    REGISTER_FUNCTION(fat32_read_file, "int(char*,void*,int)", "Read file", "filesystem"),
    REGISTER_FUNCTION(fat32_write_file, "int(char*,void*,int)", "Write file", "filesystem"),
    REGISTER_FUNCTION(fat32_list_files, "void()", "List files", "filesystem"),
    REGISTER_FUNCTION(mmio_read8, "int(long)", "Read 8-bit MMIO", "hardware"),
    REGISTER_FUNCTION(mmio_read16, "int(long)", "Read 16-bit MMIO", "hardware"),
    REGISTER_FUNCTION(mmio_read32, "int(long)", "Read 32-bit MMIO", "hardware"),
    REGISTER_FUNCTION(mmio_write8, "int(long,int)", "Write 8-bit MMIO", "hardware"),
    REGISTER_FUNCTION(mmio_write16, "int(long,int)", "Write 16-bit MMIO", "hardware"),
    REGISTER_FUNCTION(mmio_write32, "int(long,int)", "Write 32-bit MMIO", "hardware"),
    REGISTER_FUNCTION(print_hex, "void(char*,int)", "Print hex value", "display"),
    REGISTER_FUNCTION(terminal_clear, "void()", "Clear screen", "display"),
    
    // Math library functions (from math_lib.cpp)
    REGISTER_FUNCTION(fibonacci, "int(int)", "Calculate Fibonacci number", "math"),
    REGISTER_FUNCTION(factorial, "int(int)", "Calculate factorial", "math"),
    REGISTER_FUNCTION(power, "int(int,int)", "Calculate power", "math"),
    REGISTER_FUNCTION(gcd, "int(int,int)", "Greatest common divisor", "math"),
    REGISTER_FUNCTION(sqrt_approx, "int(int)", "Approximate square root", "math"),
    REGISTER_FUNCTION(is_prime, "int(int)", "Check if prime", "math"),
    
    // Crypto library functions (from crypto_lib.cpp)
    REGISTER_FUNCTION(simple_hash, "int(char*)", "Simple hash function", "crypto"),
    REGISTER_FUNCTION(xor_encrypt, "void(char*,int)", "XOR encryption", "crypto"),
    REGISTER_FUNCTION(caesar_cipher, "char*(char*,int)", "Caesar cipher", "crypto"),
    REGISTER_FUNCTION(checksum, "int(void*,int)", "Calculate checksum", "crypto"),
  
    
    {nullptr, nullptr, nullptr, nullptr, nullptr} // Sentinel
};

//=============================================================================
// REAL-TIME COMPILER ENGINE
//=============================================================================

class RTCompiler {
private:
    uint64_t ahci_base;
    int port;
    
    // Variable storage for runtime execution
    struct Variable {
        char name[64];
        int int_value;
        char string_value[256];
        bool is_string;
        bool is_array;
        int array_size;
        int* array_data;
    };
    
    Variable variables[128];
    int variable_count = 0;
    
public:
    RTCompiler(uint64_t ahci, int disk_port) : ahci_base(ahci), port(disk_port) {}
    
    // Compile and run a program file that can use all included library functions
    int compile_and_run_file(const char* program_file) {
        cout << "=== Real-Time Compilation: " << program_file << " ===\n";
        
        // Read program file
        static char program_source[16384];
        int bytes_read = fat32_read_file(ahci_base, port, program_file,
                                                  (unsigned char*)program_source,
                                                  sizeof(program_source) - 1);
        if (bytes_read <= 0) {
            cout << "Error: Could not read " << program_file << "\n";;
            return -1;
        }
        program_source[bytes_read] = '\0';
        
        cout << "Program loaded (" << bytes_read << " bytes)\n";
        
        // Show available functions from included libraries
        list_available_functions();
        
        // Execute the program
        return execute_program(program_source);
    }
    
    void list_available_functions() {
        cout << "=== Available Library Functions ===\n";
        
        const char* current_lib = nullptr;
        for (int i = 0; library_functions[i].name; i++) {
            const LibraryFunction& func = library_functions[i];
            
            // Print library header when we encounter a new library
            if (!current_lib || strcmp(current_lib, func.library) != 0) {
                current_lib = func.library;
				cout << "Press enter to continue...\n";
				char pass[1];
				cin >> pass;
                cout << "\n" << func.library << " Library:\n";

            }
            
            cout << "  " << func.name <<  " " << func.signature << " - " << func.description << "\n";;
        }
        cout << "\n";;
    }
    
private:
    int execute_program(const char* source) {
        // Find main function or execute statements directly
        const char* main_func = strstr(source, "int main(");
        if (main_func) {
            const char* brace = strchr(main_func, '{');
            if (brace) {
                execute_block(brace + 1);
            }
        } else {
            execute_statements(source);
        }
        return 0;
    }
    
    void execute_statements(const char* source) {
        const char* line = source;
        
        while (*line) {
            char statement[512];
            int i = 0;
            
            // Extract statement
            while (*line && *line != '\n' && *line != ';' && i < 511) {
                statement[i++] = *line++;
            }
            statement[i] = '\0';
            
            if (*line == '\n' || *line == ';') line++;
            
            // Skip empty lines and comments
            char* trimmed = statement;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            if (*trimmed == '\0' || strncmp(trimmed, "//", 2) == 0) continue;
            
            execute_statement(trimmed);
        }
    }
    
    void execute_block(const char* block_start) {
        const char* line = block_start;
        int brace_count = 0;
        
        while (*line && brace_count >= 0) {
            char statement[512];
            int i = 0;
            
            while (*line && *line != '\n' && *line != ';' && i < 511) {
                if (*line == '{') brace_count++;
                else if (*line == '}') brace_count--;
                
                if (brace_count < 0) break;
                
                statement[i++] = *line++;
            }
            
            if (*line == '\n' || *line == ';') line++;
            statement[i] = '\0';
            
            char* trimmed = statement;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            if (*trimmed == '\0' || *trimmed == '}') continue;
            
            execute_statement(trimmed);
        }
    }
    
    void execute_statement(const char* stmt) {
        // Variable declarations
        if (strncmp(stmt, "int ", 4) == 0) {
            execute_int_declaration(stmt + 4);
        }
        else if (strncmp(stmt, "string ", 7) == 0) {
            execute_string_declaration(stmt + 7);
        }
        // Function calls - this is where the magic happens
        else if (strchr(stmt, '(') && strchr(stmt, ')')) {
            execute_function_call(stmt);
        }
        // Assignments
        else if (strchr(stmt, '=') && !strstr(stmt, "==")) {
            execute_assignment(stmt);
        }
        else if (strlen(stmt) > 0) {
            cout << "Unknown statement: " << stmt << "\n";;
        }
    }
    
    void execute_function_call(const char* call) {
        char func_name[64];
        int i = 0;
        
        // Extract function name
        while (call[i] && call[i] != '(' && i < 63) {
            func_name[i] = call[i];
            i++;
        }
        func_name[i] = '\0';
        
        // Find the function in our registry
        const LibraryFunction* func = find_function(func_name);
        if (!func) {
            cout << "Error: Unknown function " << func_name << "\n";;
            return;
        }
        
        cout << "Calling: " << func_name << " (from " << func->library << " library)\n";
        
        // Execute the function based on its signature and library
        execute_library_function_call(func, call);
    }
    
    const LibraryFunction* find_function(const char* name) {
        for (int i = 0; library_functions[i].name; i++) {
            if (strcmp(library_functions[i].name, name) == 0) {
                return &library_functions[i];
            }
        }
        return nullptr;
    }
    
    void execute_library_function_call(const LibraryFunction* func, const char* call) {
        // Parse arguments from the function call
        const char* args = strchr(call, '(');
        if (!args) return;
        args++; // Skip '('
        
        // Execute based on function name and signature
        if (strcmp(func->name, "fibonacci") == 0) {
            int n = parse_int_argument(args);
            int result = fibonacci(n);
            cout << "fibonacci(" << n << ") = " << result << "\n";;
        }
        else if (strcmp(func->name, "factorial") == 0) {
            int n = parse_int_argument(args);
            int result = factorial(n);
            cout << "factorial(" << n << ") = " << result << "\n";;
        }
        else if (strcmp(func->name, "power") == 0) {
            int base, exp;
            parse_two_int_arguments(args, &base, &exp);
            int result = power(base, exp);
            cout << "power(" << base << ", " << exp << ") = " << result << "\n";;
        }
        else if (strcmp(func->name, "gcd") == 0) {
            int a, b;
            parse_two_int_arguments(args, &a, &b);
            int result = gcd(a, b);
            cout << "gcd(" << a << ", " << b << ") = " << result << "\n";;
        }
        else if (strcmp(func->name, "simple_hash") == 0) {
            char* str = parse_string_argument(args);
            int result = simple_hash(str);
            cout << "simple_hash(\"" << str << "\") = 0x" << std::hex << result << "\n";;
        }
       
        else if (strcmp(func->name, "memory_map_data") == 0) {
            populate_memory_map_data();
            cout << "Hardware devices discovered:\n";
            for (int i = 0; i < memory_map_device_count; i++) {
                cout << memory_map_data[i] << "\n";;
            }
        }
        else if (strncmp(func->name, "mmio_read", 9) == 0) {
            uint32_t addr = parse_hex_or_int_argument(args);
            
            if (strcmp(func->name, "mmio_read8") == 0) {
                uint8_t val = mmio_read8(addr);
                cout << "mmio_read8(0x" << std::hex << addr << ") = 0x" << std::hex << (int)val << "\n";;
            }
            else if (strcmp(func->name, "mmio_read16") == 0) {
                uint16_t val = mmio_read16(addr);
                cout << "mmio_read16(0x" << std::hex << addr << ") = 0x" << std::hex << (int)val << "\n";;
            }
            else if (strcmp(func->name, "mmio_read32") == 0) {
                uint32_t val = mmio_read32(addr);
                cout << "mmio_read32(0x" << std::hex << addr << ") = 0x" << std::hex << val << "\n";;
            }
        }
        else if (strcmp(func->name, "terminal_clear") == 0) {
            terminal_clear_screen();
        }
        else if (strcmp(func->name, "fat32_list_files") == 0) {
            fat32_list_files(ahci_base, port);
        }
        else if (strcmp(func->name, "print_hex") == 0) {
            char* label;
            int value;
            parse_string_and_int_arguments(args, &label, &value);
            print_hex(label, value);
        }

        else {
            cout << "Function " << func->name << " execution not yet implemented\n";
            cout << "But it's available and callable from: " << func->library << " library\n";
        }
    }
    
    // Argument parsing helpers
    int parse_int_argument(const char* args) {
        while (*args == ' ') args++;
        return parse_number(args);
    }
    
    uint64_t parse_hex_or_int_argument(const char* args) {
        while (*args == ' ') args++;
        return parse_number(args);
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
    
    void parse_three_int_arguments(const char* args, int* a, int* b, int* c) {
        *a = parse_int_argument(args);
        const char* comma1 = strchr(args, ',');
        if (comma1) {
            *b = parse_int_argument(comma1 + 1);
            const char* comma2 = strchr(comma1 + 1, ',');
            if (comma2) {
                *c = parse_int_argument(comma2 + 1);
            }
        }
    }
    
    void parse_string_and_int_arguments(const char* args, char** str, int* val) {
        *str = parse_string_argument(args);
        const char* comma = strchr(args, ',');
        if (comma) {
            *val = parse_int_argument(comma + 1);
        }
    }
    
    // Custom string to number parser to replace strtoull (not available in kernel)
static uint64_t kernel_parse_number(const char* str) {
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

// Replace the parse_number function in RTCompiler
uint64_t parse_number(const char* str) {
    return kernel_parse_number(str);
}
    
    // Variable management (simplified implementations)
    void execute_int_declaration(const char* decl) {
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
            value = parse_number(equals);
        }
        
        set_variable(var_name, value, false);
        cout << "int " << var_name << " = " << value << "\n";;
    }
    
    void execute_string_declaration(const char* decl) {
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
                static char string_buf[256];
                int j = 0;
                while (*equals && *equals != '"' && j < 255) {
                    string_buf[j++] = *equals++;
                }
                string_buf[j] = '\0';
                value = string_buf;
            }
        }
        
        set_string_variable(var_name, value);
        cout << "string " << var_name << " = \"" << value << "\"\n";
    }
    
    void execute_assignment(const char* assign) {
        const char* equals = strchr(assign, '=');
        if (!equals) return;
        
        char var_name[64];
        int i = 0;
        const char* p = assign;
        
        while (p < equals && *p != ' ' && i < 63) {
            var_name[i++] = *p++;
        }
        var_name[i] = '\0';
        
        equals++;
        while (*equals == ' ') equals++;
        
        int value = parse_number(equals);
        set_variable(var_name, value, false);
        
        cout << var_name << " = " << value << "\n";
    }
    
    void set_variable(const char* name, int value, bool is_string) {
        Variable* var = find_variable(name);
        if (!var) {
            if (variable_count >= 128) return;
            var = &variables[variable_count++];
            strcpy(var->name, name);
        }
        
        var->int_value = value;
        var->is_string = is_string;
    }
    
    void set_string_variable(const char* name, const char* value) {
        Variable* var = find_variable(name);
        if (!var) {
            if (variable_count >= 128) return;
            var = &variables[variable_count++];
            strcpy(var->name, name);
        }
        
        strcpy(var->string_value, value);
        var->is_string = true;
    }
    
    Variable* find_variable(const char* name) {
        for (int i = 0; i < variable_count; i++) {
            if (strcmp(variables[i].name, name) == 0) {
                return &variables[i];
            }
        }
        return nullptr;
    }
};

//=============================================================================
// KERNEL INTEGRATION
//=============================================================================



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
        else if (strcmp(cmd, "list") == 0) {
            compiler.list_available_functions();
        }
        else if (strncmp(cmd, "run ", 4) == 0) {
            compiler.compile_and_run_file(cmd + 4);
        }
        
        // NEW: Load and run file from filesystem
		else if (strncmp(cmd, "load ", 5) == 0) {
			const char* filename = cmd + 5;
			cout << "Loading and running: " << filename << "\n";
			
			// Read file content
			static char code_buffer[8192];
			int bytes_read = fat32_read_file(ahci_base, port, filename,
													 (unsigned char*)code_buffer,
													 sizeof(code_buffer) - 1);
			
			if (bytes_read > 0) {
				code_buffer[bytes_read] = '\0';
				cout << "File loaded (" << bytes_read << " bytes)\n";
				
				// Use the existing compile_and_run_file method with a temporary file
				static char temp_filename[] = "temp_exec.cpp";
				
				int save_result = fat32_write_file(ahci_base, port, temp_filename, 
												  code_buffer, bytes_read);
				
				if (save_result == 0) {
					compiler.compile_and_run_file(temp_filename);
					fat32_remove_file(ahci_base, port, temp_filename);
				} else {
					cout << "Error: Could not create temporary execution file\n";
				}
			} else {
				cout << "Error: Could not load file " << filename << "\n";
			}
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
            cout << "  load <file.cpp>         - Load and execute file directly\n";
            cout << "  notepad <file.cpp>      - Edit file with notepad\n";
            cout << "  save <file> \"content\"   - Save content to file\n";
            cout << "  ls                      - List files in filesystem\n";
            cout << "  cat <file>              - Display file content\n";
            cout << "  help                    - Show this help\n";
            cout << "  exit                    - Exit compiler\n";
        }
        else if (strlen(cmd) > 0) {
            cout << "Unknown command. Type 'help' for available commands.\n";
        }
    }
}

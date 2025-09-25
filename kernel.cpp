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

void cmd_notepad(unsigned long long, int, char const*);



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
void fat32_read_file(uint64_t ahci_base, int port, const char* filename);
int fat32_add_file(uint64_t ahci_base, int port, const char* filename, const void* data, uint32_t size);
int fat32_remove_file(uint64_t ahci_base, int port, const char* filename);
int fat32_read_file_to_buffer(uint64_t ahci_base, int port, const char* filename, void* data_buffer, uint32_t buffer_size);
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

    int bytes_read = fat32_read_file_to_buffer(ahci_base, port, src_name, file_buffer, file_size + 1);
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


int fat32_read_file_to_buffer(uint64_t ahci_base, int port, const char* filename, void* data_buffer, uint32_t buffer_size) {
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
         << "  cat <file>, kbtest, gui\n"
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

    int bytes_read = fat32_read_file_to_buffer(ahci_base, port, filename, file_buffer, sizeof(file_buffer));

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



// tinycc_vm_kernel.cpp
// Self-hosted tinycc-style compiler to bytecode + VM runner inside kernel.
// Features:
// - Types: int, char, string (string is pointer to char, token-based I/O)
// - Decls: int/char/string name [= expr] ;
// - Expr: + - * /, unary -, comparisons (== != < <= > >=), parentheses
// - Control: if/else, while, break, continue
// - I/O: cin >> chains (int/char/string), cout << chains (int/char/string/argv(i)/endl)
// - Built-ins: argc (int), argv(i) (string pointer)
// - Program form: int main() { ... } with implicit return 0 if missing
// - Object I/O: saves/loads TVM1 object via FAT32 helpers
//
// Requires existing kernel wrappers and helpers already present in your tree:
// - cout, cin (iostream_wrapper.h) for console printing/input
// - simple_strcmp, simple_strcpy, simple_memcpy, simple_atoi, int_to_string
// - FAT32 I/O: fat32_read_file_to_buffer, fat32_write_file
// - Command shell provides: parts[], part_count (tokenized command line)
// - Keep cmd_compile/cmd_run signatures; command_prompt dispatch remains unchanged.

// kernel.cpp — tinycc VM edition
// Self-hosted tiny compiler -> bytecode VM with console I/O and filesystem I/O.
// Features:
// - Types: int, char, string (string is pointer to char for printing/assignment)
// - Decls: int/char/string name [= expr] ;
// - Expr: + - * /, unary -, comparisons (== != < <= > >=), parentheses
// - Control: if/else, while, break, continue
// - I/O: cin >> chains (int/char/string), cout << chains (int/char/string/argv(i)/endl)
// - Built-ins: argc (int), argv(i) (string pointer)
// - Program: int main() { ... } with implicit return 0 if missing
// - Object I/O: TVM1 object saved/loaded via existing FAT32 helpers
//
// Kernel dependencies expected (already present in your tree):
// - cout, cin from iostream_wrapper.h
// - simple_strcmp, simple_strcpy, simple_memcpy, simple_atoi, int_to_string
// - fat32_read_file_to_buffer, fat32_write_file
// - parts[], part_count (command shell tokenization)

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

// Forward declarations consumed by the command shell
extern "C" void cmd_compile(uint64_t ahci_base, int port, const char* filename);
extern "C" void cmd_run(uint64_t ahci_base, int port, const char* filename);
extern "C" void cmd_exec(const char* code_text);

// Provided by the command shell tokenizer
char* parts[32];
int   part_count;

// ---- tiny helpers ----
static inline int tcc_is_digit(char c){ return c>='0' && c<='9'; }
static inline int tcc_is_alpha(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
static inline int tcc_is_alnum(char c){ return tcc_is_alpha(c)||tcc_is_digit(c); }
static inline int tcc_strlen(const char* s){ int n=0; while(s && s[n]) ++n; return n; }

// ============================================================
// Bytecode ISA
// ============================================================
enum TOp : unsigned char {
    // stack/data
    T_NOP=0, T_PUSH_IMM, T_PUSH_STR, T_LOAD_LOCAL, T_STORE_LOCAL, T_POP,

    // arithmetic / unary
    T_ADD, T_SUB, T_MUL, T_DIV, T_NEG,

    // comparisons
    T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE,

    // control flow
    T_JMP, T_JZ, T_JNZ, T_RET,

    // I/O and args
    T_PRINT_INT, T_PRINT_CHAR, T_PRINT_STR, T_PRINT_ENDL,
    T_READ_INT, T_READ_CHAR, T_READ_STR,
    T_PUSH_ARGC, T_PUSH_ARGV_PTR
};

// ============================================================
// Program buffers
// ============================================================
struct TProgram {
    static const int CODE_MAX = 8192;
    unsigned char code[CODE_MAX];
    int pc = 0;

    static const int LIT_MAX = 4096;
    char lit[LIT_MAX];
    int lit_top = 0;

    static const int LOC_MAX = 32;
    char  loc_name[LOC_MAX][32];
    unsigned char loc_type[LOC_MAX]; // 0=int,1=char,2=string
    int   loc_count = 0;

    int add_local(const char* name, unsigned char t){
        for(int i=0;i<loc_count;i++){ if(simple_strcmp(loc_name[i], name)==0) return i; }
        if(loc_count>=LOC_MAX) return -1;
        simple_strcpy(loc_name[loc_count], name);
        loc_type[loc_count]=t;
        return loc_count++;
    }
    int get_local(const char* name){
        for(int i=0;i<loc_count;i++){ if(simple_strcmp(loc_name[i], name)==0) return i; }
        return -1;
    }
    int get_local_type(int idx){ return (idx>=0 && idx<loc_count)? loc_type[idx] : 0; }

    void emit1(unsigned char op){ if(pc<CODE_MAX) code[pc++]=op; }
    void emit4(int v){ if(pc+4<=CODE_MAX){ code[pc++]=v&0xff; code[pc++]=(v>>8)&0xff; code[pc++]=(v>>16)&0xff; code[pc++]=(v>>24)&0xff; } }
    int  mark(){ return pc; }
    void patch4(int at, int v){ if(at+4<=CODE_MAX){ code[at+0]=v&0xff; code[at+1]=(v>>8)&0xff; code[at+2]=(v>>16)&0xff; code[at+3]=(v>>24)&0xff; } }

    const char* add_lit(const char* s){
        int n = tcc_strlen(s)+1;
        if(lit_top+n > LIT_MAX) return "";
        char* p = &lit[lit_top];
        simple_memcpy(p, s, n);
        lit_top += n;
        return p;
    }
};

// ============================================================
// Tokenizer
// ============================================================
enum TTokType { TT_EOF, TT_ID, TT_NUM, TT_STR, TT_CH, TT_KW, TT_OP, TT_PUNC };
struct TTok { TTokType t; char v[64]; int ival; };

struct TLex {
    const char* src; int pos; int line;
    void init(const char* s){ src=s; pos=0; line=1; }

    void skipws(){
        for(;;){
            char c=src[pos];
            if(c==' '||c=='\t'||c=='\r'||c=='\n'){ if(c=='\n') line++; pos++; continue; }
            if(c=='/' && src[pos+1]=='/'){ pos+=2; while(src[pos] && src[pos]!='\n') pos++; continue; }
            if(c=='/' && src[pos+1]=='*'){ pos+=2; while(src[pos] && !(src[pos]=='*'&&src[pos+1]=='/')) pos++; if(src[pos]) pos+=2; continue; }
            break;
        }
    }

    TTok number(){
        TTok t; t.t=TT_NUM; t.ival=0; int i=0;
        while(tcc_is_digit(src[pos])){ t.v[i++]=src[pos]; t.ival = t.ival*10 + (src[pos]-'0'); pos++; if(i>=63) break; }
        t.v[i]=0; return t;
    }

    TTok ident(){
        TTok t; t.t=TT_ID; int i=0;
        while(tcc_is_alnum(src[pos])){ t.v[i++]=src[pos++]; if(i>=63) break; } t.v[i]=0;
        const char* kw[]={"int","char","string","return","if","else","while","break","continue","cin","cout","endl","argc","argv",0};
        for(int k=0; kw[k]; ++k){ if(simple_strcmp(t.v,kw[k])==0){ t.t=TT_KW; break; } }
        return t;
    }

    TTok string(){
        TTok t; t.t=TT_STR; int i=0; pos++;
        while(src[pos] && src[pos]!='"'){ if(i<63) t.v[i++]=src[pos]; pos++; }
        t.v[i]=0; if(src[pos]=='"') pos++; return t;
    }

    TTok chlit(){
        TTok t; t.t=TT_CH; t.v[0]=0; int v=0; pos++; // skip '
        if(src[pos] && src[pos+1]=='\''){ v = (unsigned char)src[pos]; pos+=2; }
        t.ival = v; return t;
    }

    TTok op_or_punc(){
        TTok t; t.t=TT_OP; t.v[0]=src[pos]; t.v[1]=0; char c=src[pos];
        if(c=='<' && src[pos+1]=='<'){ t.v[0]='<'; t.v[1]='<'; t.v[2]=0; pos+=2; return t; }
        if(c=='>' && src[pos+1]=='>'){ t.v[0]='>'; t.v[1]='>'; t.v[2]=0; pos+=2; return t; }
        if((c=='='||c=='!'||c=='<'||c=='>') && src[pos+1]=='='){ t.v[0]=c; t.v[1]='='; t.v[2]=0; pos+=2; return t; }
        pos++; if(c=='('||c==')'||c=='{'||c=='}'||c==';'||c==',' ) t.t=TT_PUNC; return t;
    }

    TTok next(){
        skipws();
        if(src[pos]==0){ TTok t; t.t=TT_EOF; t.v[0]=0; return t; }
        if(src[pos]=='"') return string();
        if(src[pos]=='\'') return chlit();
        if(tcc_is_digit(src[pos])) return number();
        if(tcc_is_alpha(src[pos])) return ident();
        return op_or_punc();
    }
};

// ============================================================
// Parser / Compiler
// ============================================================
struct TCompiler {
    TLex lx; TTok tk; TProgram pr;

    int brk_pos[32]; int brk_cnt=0;
    int cont_pos[32]; int cont_cnt=0;

    void adv(){ tk = lx.next(); }
    int  accept(const char* s){ if(simple_strcmp(tk.v,s)==0){ adv(); return 1; } return 0; }
    void expect(const char* s){ if(!accept(s)) { cout << "Parse error near: "; cout << tk.v; cout << "\n"; } }

    void parse_primary(){
        if(tk.t==TT_NUM){ pr.emit1(T_PUSH_IMM); pr.emit4(tk.ival); adv(); return; }
        if(tk.t==TT_CH){ pr.emit1(T_PUSH_IMM); pr.emit4(tk.ival); adv(); return; }
        if(tk.t==TT_STR){ const char* p=pr.add_lit(tk.v); pr.emit1(T_PUSH_STR); pr.emit4((int)p); adv(); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"argc")==0){ pr.emit1(T_PUSH_ARGC); adv(); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"argv")==0){ adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_PUSH_ARGV_PTR); return; }
        if(tk.t==TT_PUNC && tk.v[0]=='('){ adv(); parse_expression(); expect(")"); return; }
        if(tk.t==TT_ID){
            int idx = pr.get_local(tk.v);
            if(idx<0){ cout << "Unknown var "; cout << tk.v; cout << "\n"; }
            pr.emit1(T_LOAD_LOCAL); pr.emit4(idx); adv(); return;
        }
    }

    void parse_unary(){
        if(accept("-")){ parse_unary(); pr.emit1(T_NEG); return; }
        parse_primary();
    }

    void parse_term(){
        parse_unary();
        while(tk.v[0]=='*' || tk.v[0]=='/'){
            char op=tk.v[0]; adv(); parse_unary();
            pr.emit1(op=='*'?T_MUL:T_DIV);
        }
    }

    void parse_arith(){
        parse_term();
        while(tk.v[0]=='+' || tk.v[0]=='-'){
            char op=tk.v[0]; adv(); parse_term();
            pr.emit1(op=='+'?T_ADD:T_SUB);
        }
    }

    void parse_cmp(){
        parse_arith();
        while(tk.t==TT_OP && (simple_strcmp(tk.v,"==")==0 || simple_strcmp(tk.v,"!=")==0 ||
               simple_strcmp(tk.v,"<")==0 || simple_strcmp(tk.v,"<=")==0 ||
               simple_strcmp(tk.v,">")==0 || simple_strcmp(tk.v,">=")==0)){
            char opv[3]; simple_strcpy(opv, tk.v); adv(); parse_arith();
            if(simple_strcmp(opv,"==")==0) pr.emit1(T_EQ);
            else if(simple_strcmp(opv,"!=")==0) pr.emit1(T_NE);
            else if(simple_strcmp(opv,"<")==0)  pr.emit1(T_LT);
            else if(simple_strcmp(opv,"<=")==0) pr.emit1(T_LE);
            else if(simple_strcmp(opv,">")==0)  pr.emit1(T_GT);
            else pr.emit1(T_GE);
        }
    }

    void parse_expression(){ parse_cmp(); }

    void parse_decl(unsigned char tkind){
        adv(); // past type keyword
        if(tk.t!=TT_ID){ cout << "Expected identifier\n"; return; }
        char nm[32]; simple_strcpy(nm, tk.v); adv();
        int idx = pr.add_local(nm, tkind);
        if(accept("=")){ 
            if(tkind==2){ // string
                if(tk.t==TT_STR){ const char* p=pr.add_lit(tk.v); pr.emit1(T_PUSH_STR); pr.emit4((int)p); adv(); }
                else if(tk.t==TT_KW && simple_strcmp(tk.v,"argv")==0){ adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_PUSH_ARGV_PTR); }
                else if(tk.t==TT_ID){ int j=pr.get_local(tk.v); adv(); pr.emit1(T_LOAD_LOCAL); pr.emit4(j); }
                else { parse_expression(); }
            } else {
                parse_expression();
            }
            pr.emit1(T_STORE_LOCAL); pr.emit4(idx);
        }
        expect(";");
    }

    void parse_assign_or_coutcin(){
        if(tk.t==TT_KW && simple_strcmp(tk.v,"cout")==0){ adv();
            for(;;){
                expect("<<");
                if(tk.t==TT_KW && simple_strcmp(tk.v,"endl")==0){ adv(); pr.emit1(T_PRINT_ENDL); }
                else if(tk.t==TT_STR){ const char* p=pr.add_lit(tk.v); pr.emit1(T_PUSH_STR); pr.emit4((int)p); adv(); pr.emit1(T_PRINT_STR); }
                else if(tk.t==TT_KW && simple_strcmp(tk.v,"argv")==0){ adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_PUSH_ARGV_PTR); pr.emit1(T_PRINT_STR); }
                else if(tk.t==TT_ID){
                    int idx = pr.get_local(tk.v); int ty = pr.get_local_type(idx); adv();
                    pr.emit1(T_LOAD_LOCAL); pr.emit4(idx);
                    if(ty==2) pr.emit1(T_PRINT_STR);
                    else if(ty==1) pr.emit1(T_PRINT_CHAR);
                    else pr.emit1(T_PRINT_INT);
                } else { parse_expression(); pr.emit1(T_PRINT_INT); }
                if(tk.t==TT_PUNC && tk.v[0]==';'){ adv(); break; }
            }
            return;
        }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"cin")==0){ adv();
            for(;;){
                expect(">>"); if(tk.t!=TT_ID){ cout << "cin expects identifier\n"; return; }
                int idx = pr.get_local(tk.v); int ty = pr.get_local_type(idx); adv();
                if(ty==2) pr.emit1(T_READ_STR);
                else if(ty==1) pr.emit1(T_READ_CHAR);
                else pr.emit1(T_READ_INT);
                pr.emit1(T_STORE_LOCAL); pr.emit4(idx);
                if(tk.t==TT_PUNC && tk.v[0]==';'){ adv(); break; }
            }
            return;
        }

        if(tk.t==TT_ID){
            int idx = pr.get_local(tk.v);
            if(idx<0){ cout << "Unknown var "; cout << tk.v; cout << "\n"; }
            int ty = pr.get_local_type(idx);
            adv(); expect("=");
            if(ty==2){
                if(tk.t==TT_STR){ const char* p=pr.add_lit(tk.v); pr.emit1(T_PUSH_STR); pr.emit4((int)p); adv(); }
                else if(tk.t==TT_KW && simple_strcmp(tk.v,"argv")==0){ adv(); expect("("); parse_expression(); expect(")"); pr.emit1(T_PUSH_ARGV_PTR); }
                else if(tk.t==TT_ID){ int j=pr.get_local(tk.v); adv(); pr.emit1(T_LOAD_LOCAL); pr.emit4(j); }
                else { parse_expression(); }
            } else {
                parse_expression();
            }
            pr.emit1(T_STORE_LOCAL); pr.emit4(idx);
            expect(";");
            return;
        }

        while(!(tk.t==TT_PUNC && tk.v[0]==';') && tk.t!=TT_EOF) adv();
        if(tk.t==TT_PUNC && tk.v[0]==';') adv();
    }

    void parse_if(){
        adv(); expect("("); parse_expression(); expect(")");
        pr.emit1(T_JZ); int jz_at = pr.mark(); pr.emit4(0);
        parse_block();
        int has_else = (tk.t==TT_KW && simple_strcmp(tk.v,"else")==0);
        if(has_else){
            pr.emit1(T_JMP); int j_at = pr.mark(); pr.emit4(0);
            int here = pr.pc; pr.patch4(jz_at, here);
            adv(); // else
            parse_block();
            int end = pr.pc; pr.patch4(j_at, end);
        } else {
            int here = pr.pc; pr.patch4(jz_at, here);
        }
    }

    void parse_while(){
        adv(); expect("("); int cond_ip = pr.pc; parse_expression(); expect(")");
        pr.emit1(T_JZ); int jz_at = pr.mark(); pr.emit4(0);
        int brk_base=brk_cnt, cont_base=cont_cnt;
        parse_block();
        for(int i=cont_base;i<cont_cnt;i++){ pr.patch4(cont_pos[i], cond_ip); }
        cont_cnt = cont_base;
        pr.emit1(T_JMP); pr.emit4(cond_ip);
        int end_ip = pr.pc; pr.patch4(jz_at, end_ip);
        for(int i=brk_base;i<brk_cnt;i++){ pr.patch4(brk_pos[i], end_ip); }
        brk_cnt = brk_base;
    }

    void parse_block(){
        if(accept("{")){
            while(!(tk.t==TT_PUNC && tk.v[0]=='}') && tk.t!=TT_EOF) parse_stmt();
            expect("}");
        } else {
            parse_stmt();
        }
    }

    void parse_stmt(){
        if(tk.t==TT_KW && simple_strcmp(tk.v,"int")==0){ parse_decl(0); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"char")==0){ parse_decl(1); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"string")==0){ parse_decl(2); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"return")==0){ adv(); parse_expression(); pr.emit1(T_RET); expect(";"); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"if")==0){ parse_if(); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"while")==0){ parse_while(); return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"break")==0){ adv(); expect(";"); pr.emit1(T_JMP); int at=pr.mark(); pr.emit4(0); brk_pos[brk_cnt++]=at; return; }
        if(tk.t==TT_KW && simple_strcmp(tk.v,"continue")==0){ adv(); expect(";"); pr.emit1(T_JMP); int at=pr.mark(); pr.emit4(0); cont_pos[cont_cnt++]=at; return; }
        parse_assign_or_coutcin();
    }

    int compile(const char* source){
        lx.init(source); adv();
        if(!(tk.t==TT_KW && simple_strcmp(tk.v,"int")==0)) { cout<<"Expected 'int' at start\n"; return -1; }
        adv();
        if(!(tk.t==TT_ID && simple_strcmp(tk.v,"main")==0)){ cout<<"Expected main\n"; return -1; }
        adv(); expect("("); expect(")"); parse_block();
        pr.emit1(T_PUSH_IMM); pr.emit4(0); pr.emit1(T_RET);
        return pr.pc;
    }
};

// ============================================================
// VM
// ============================================================
struct TinyVM {
    static const int STK_MAX = 1024;
    int   stk[STK_MAX]; int sp=0;
    int   locals[TProgram::LOC_MAX];   // NEW: dedicated locals storage
    int   argc; const char** argv;
    TProgram* P;
    char str_in[256];

    inline void push(int v){ if(sp<STK_MAX) stk[sp++]=v; }
    inline int  pop(){ return sp?stk[--sp]:0; }

    int run(TProgram& prog, int ac, const char** av){
        P=&prog; argc=ac; argv=av; sp=0;
        for (int i=0;i<TProgram::LOC_MAX;i++) locals[i]=0;  // NEW: init locals
        int ip=0;
        while(ip < P->pc){
            TOp op = (TOp)P->code[ip++];
            switch(op){
                case T_NOP: break;
                case T_PUSH_IMM: { int v= *(int*)&P->code[ip]; ip+=4; push(v); } break;
                case T_PUSH_STR: { int p= *(int*)&P->code[ip]; ip+=4; push(p); } break;
                case T_LOAD_LOCAL:{ int i=*(int*)&P->code[ip]; ip+=4; push(locals[i]); } break;   // CHANGED
                case T_STORE_LOCAL:{ int i=*(int*)&P->code[ip]; ip+=4; locals[i]=pop(); } break;   // CHANGED
                case T_POP: { if(sp) --sp; } break;

                case T_ADD: { int b=pop(), a=pop(); push(a+b);} break;
                case T_SUB: { int b=pop(), a=pop(); push(a-b);} break;
                case T_MUL: { int b=pop(), a=pop(); push(a*b);} break;
                case T_DIV: { int b=pop(), a=pop(); push(b? a/b:0);} break;
                case T_NEG: { int a=pop(); push(-a);} break;

                case T_EQ: { int b=pop(), a=pop(); push(a==b);} break;
                case T_NE: { int b=pop(), a=pop(); push(a!=b);} break;
                case T_LT: { int b=pop(), a=pop(); push(a<b);} break;
                case T_LE: { int b=pop(), a=pop(); push(a<=b);} break;
                case T_GT: { int b=pop(), a=pop(); push(a>b);} break;
                case T_GE: { int b=pop(), a=pop(); push(a>=b);} break;

                case T_JMP: { int t=*(int*)&P->code[ip]; ip=t; } break;
                case T_JZ:  { int t=*(int*)&P->code[ip]; ip+=4; int v=pop(); if(v==0) ip=t; } break;
                case T_JNZ: { int t=*(int*)&P->code[ip]; ip+=4; int v=pop(); if(v!=0) ip=t; } break;

                case T_PRINT_INT: { int v=pop(); char b[16]; int_to_string(v,b); cout << b; } break;
                case T_PRINT_CHAR:{ int v=pop(); char b[2]; b[0]=(char)(v&0xff); b[1]=0; cout << b; } break;
                case T_PRINT_STR: { const char* p=(const char*)pop(); if(p) cout << p; } break;
                case T_PRINT_ENDL:{ cout << "\n"; } break;

                case T_READ_INT: { char t[32]; cin >> t; push(simple_atoi(t)); } break;
                case T_READ_CHAR:{ char t[4]; cin >> t; push((unsigned char)t[0]); } break;
                case T_READ_STR: { cin >> str_in; push((int)str_in); } break;

                case T_PUSH_ARGC: { push(argc); } break;
                case T_PUSH_ARGV_PTR: { int idx=pop(); const char* p=(idx>=0 && idx<argc && argv)? argv[idx]:""; push((int)p); } break;

                case T_RET: { int rv=pop(); return rv; }
                default: return -1;
            }
        }
        return 0;
    }
};

// ============================================================
// Object I/O (TVM1)
// ============================================================
struct TVMObject {
    static int save(uint64_t base, int port, const char* path, const TProgram& P){
        static unsigned char buf[ TProgram::CODE_MAX + TProgram::LIT_MAX + 32 ];
        int off=0;
        buf[off++]='T'; buf[off++]='V'; buf[off++]='M'; buf[off++]='1';
        *(int*)&buf[off]=P.pc; off+=4;
        *(int*)&buf[off]=P.lit_top; off+=4;
        *(int*)&buf[off]=P.loc_count; off+=4;
        simple_memcpy(&buf[off], P.code, P.pc); off+=P.pc;
        simple_memcpy(&buf[off], P.lit, P.lit_top); off+=P.lit_top;
        return fat32_write_file(base, port, path, buf, off);
    }
    static int load(uint64_t base, int port, const char* path, TProgram& P){
        static unsigned char buf[ TProgram::CODE_MAX + TProgram::LIT_MAX + 32 ];
        int n = fat32_read_file_to_buffer(base, port, path, buf, sizeof(buf));
        if(n<16) return -1;
        if(!(buf[0]=='T'&&buf[1]=='V'&&buf[2]=='M'&&buf[3]=='1')) return -2;
        int cp=*(int*)&buf[4], lp=*(int*)&buf[8], lc=*(int*)&buf[12];
        if(cp<0||cp>TProgram::CODE_MAX||lp<0||lp>TProgram::LIT_MAX||lc<0||lc>TProgram::LOC_MAX) return -3;
        P.pc=cp; P.lit_top=lp; P.loc_count=lc;
        int off=16;
        simple_memcpy(P.code, &buf[off], cp); off+=cp;
        simple_memcpy(P.lit, &buf[off], lp); off+=lp;
        return 0;
    }
};

// ============================================================
// Public compile/run entry points for shell
// ============================================================
static int tinyvm_compile_to_obj(uint64_t ahci_base, int port, const char* src_path, const char* obj_path){
    static char srcbuf[8192];
    int n = fat32_read_file_to_buffer(ahci_base, port, src_path, (unsigned char*)srcbuf, sizeof(srcbuf)-1);
    if(n<=0){ cout << "read fail\n"; return -1; }
    srcbuf[n]=0;
    TCompiler C; int ok = C.compile(srcbuf);
    if(ok<0){ cout << "Compilation failed!\n"; return -2; }
    int w = TVMObject::save(ahci_base, port, obj_path, C.pr);
    if(w<0){ cout << "write fail\n"; return -3; }
    return 0;
}

static int tinyvm_run_obj(uint64_t ahci_base, int port, const char* obj_path, int argc, const char** argv){
    TProgram P; int r = TVMObject::load(ahci_base, port, obj_path, P);
    if(r<0){ cout << "load fail\n"; return -1; }
    TinyVM vm; int rv = vm.run(P, argc, argv);
    char b[16]; int_to_string(rv,b); cout << b;
    return rv;
}

// ============================================================
// Shell glue
// ============================================================
extern "C" void cmd_compile(uint64_t ahci_base, int port, const char* filename){
    if (!filename) { cout << "Usage: compile <file.cpp>\n"; return; }
    static char obj[64]; int i=0; while(filename[i] && i<60){ obj[i]=filename[i]; i++; }
    while(i>0 && obj[i-1] != '.') i--; obj[i]=0; simple_strcpy(&obj[i], "obj");
    cout << "Compiling "; cout << filename; cout << "...\n";
    int r = tinyvm_compile_to_obj(ahci_base, port, filename, obj);
    if(r==0) { cout << "OK -> "; cout << obj; cout << "\n"; } else { cout << "Compilation failed!\n"; }
}

extern "C" void cmd_run(uint64_t ahci_base, int port, const char* filename){
    if (!filename) { cout << "Usage: run <file.obj> [args...]\n"; return; }
    static const char* argvv[16];
    int argc=0;
    for(int i=2;i<part_count && argc<16;i++){ argvv[argc++] = parts[i]; }
    cout << "Executing "; cout << filename; cout << "...\n";
    tinyvm_run_obj(ahci_base, port, filename, argc, argvv);
}

extern "C" void cmd_exec(const char* code_text){
    if(!code_text){ cout<<"No code\n"; return; }
    TCompiler C; int ok = C.compile(code_text);
    if(ok<0){ cout << "Compilation failed!\n"; return; }
    TinyVM vm; TProgram& P = C.pr;
    static const char* argvv[1] = { };
    int rv = vm.run(P, 0, argvv);
    char b[16]; int_to_string(rv,b); cout << b;
}









// 1) Keep Framebuffer defined before any get_boot_framebuffer usage
struct Framebuffer {
    uint8_t* ptr;      // Linear framebuffer base (kernel virtual)
    uint32_t width;    // pixels
    uint32_t height;   // pixels
    uint32_t pitch;    // bytes per scanline
    uint32_t bpp;      // bits per pixel (expect 32)
    uint32_t pixel_format; // 0 = XRGB8888, 1 = BGRA8888 (implementation choice)
};

// 2) C-linkage globals written by boot.S (define exactly once)
extern "C" {
    uint32_t multiboot_magic;
    uint32_t multiboot_info_ptr;
}

// 3) Provide only a prototype before start_gui
extern "C" bool get_boot_framebuffer(Framebuffer& out);



// ---- Multiboot info layout (includes framebuffer fields when flags bit 12 set)
#pragma pack(push,1)
struct multiboot_info_t {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr;
    uint32_t drives_length, drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t framebuffer_addr;   // present if flags bit 12 set
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;   // 0=indexed, 1=RGB, 2=text
    uint8_t  color_info[6];      // RGB: r_pos,r_size,g_pos,g_size,b_pos,b_size
};
#pragma pack(pop)

// Detect 32bpp pixel ordering from Multiboot color_info
static uint32_t detect_pixel_format(const multiboot_info_t* mb) {
    if (mb->framebuffer_type != 1 || mb->framebuffer_bpp != 32) return 0; // default
    uint8_t rpos = mb->color_info[0], rsz = mb->color_info[1];
    uint8_t gpos = mb->color_info[2], gsz = mb->color_info[3];
    uint8_t bpos = mb->color_info[4], bsz = mb->color_info[5];
    if (rsz == 8 && gsz == 8 && bsz == 8 && rpos == 16 && gpos == 8 && bpos == 0) return 0;
    if (rsz == 8 && gsz == 8 && bsz == 8 && rpos == 0 && gpos == 8 && bpos == 16) return 1;
    return 0;
}

// 4) Full implementation placed AFTER Framebuffer and Multiboot structs
extern "C" bool get_boot_framebuffer(Framebuffer& out) {
    // EAX magic must be 0x2BADB002 (captured by boot.S)
    if (multiboot_magic != 0x2BADB002u) return false;
    const auto* mb = (const multiboot_info_t*)(uintptr_t)multiboot_info_ptr;
    if (!mb) return false;
    if (!(mb->flags & (1u << 12))) return false; // framebuffer fields not present

    // GRUB provides a physical LFB; ensure it is mapped or identity-mapped
    uint64_t phys = mb->framebuffer_addr;
    out.ptr   = (uint8_t*)(uintptr_t)phys;
    out.width = mb->framebuffer_width;
    out.height= mb->framebuffer_height;
    out.pitch = mb->framebuffer_pitch;
    out.bpp   = mb->framebuffer_bpp;
    out.pixel_format = detect_pixel_format(mb);

    // Require 32bpp linear framebuffer
    if (!out.ptr) return false;
    if (out.bpp != 32) return false;
    if (out.pitch < out.width * 4) return false;
    return true;
}



// ---- Minimal GUI environment (framebuffer + PSF font + demo) ----



static Framebuffer g_fb = {};
static uint32_t*   g_back = nullptr;

// Platform stub: provide a framebuffer from boot services/bootloader.
// Implement this to populate out with a valid linear framebuffer mapping.
extern "C" bool get_boot_framebuffer(Framebuffer& out);

// Simple terminal fallback if no framebuffer is present.
static void gui_fallback_text() {
    cout << "[GUI] No framebuffer available. Implement get_boot_framebuffer() (e.g., VBE/Limine/UEFI GOP)\\n";
}

// Allocate/destroy backbuffer
static bool fb_alloc_backbuffer() {
    if (!g_fb.ptr || g_fb.bpp != 32 || g_fb.pitch < g_fb.width * 4) return false;
    size_t bytes = size_t(g_fb.pitch) * g_fb.height;
    g_back = (uint32_t*) new uint8_t[bytes];
    if (!g_back) return false;
    // Clear
    for (uint32_t y = 0; y < g_fb.height; ++y) {
        uint32_t* row = (uint32_t*)((uint8_t*)g_back + y * g_fb.pitch);
        for (uint32_t x = 0; x < g_fb.width; ++x) row[x] = 0xFF101010; // dark gray
    }
    return true;
}
static void fb_free_backbuffer() { if (g_back) { delete[] (uint8_t*)g_back; g_back = nullptr; } }
// Present: copy backbuffer -> front
static void fb_present() {
    if (!g_back || !g_fb.ptr) return;
    for (uint32_t y = 0; y < g_fb.height; ++y) {
        void* dst = g_fb.ptr + y * g_fb.pitch;
        void* src = ((uint8_t*)g_back) + y * g_fb.pitch;
        memcpy(dst, src, g_fb.pitch);
    }
}

// Pixel write (XRGB8888 default)
static inline void put_px(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_back) return;
    if (x >= g_fb.width || y >= g_fb.height) return;
    uint32_t* row = (uint32_t*)((uint8_t*)g_back + y * g_fb.pitch);
    if (g_fb.pixel_format == 0) { // XRGB8888
        row[x] = color;
    } else { // BGRA8888-like, swap channels if needed
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        row[x] = (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r) | 0xFF000000u;
    }
}
static void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int xx = x; xx < x + w; ++xx) {
        put_px(xx, y, color);
        put_px(xx, y + h - 1, color);
    }
    for (int yy = y; yy < y + h; ++yy) {
        put_px(x, yy, color);
        put_px(x + w - 1, yy, color);
    }
}


static void fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!g_back) return;
    if (w <= 0 || h <= 0) return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w; if (x1 > int(g_fb.width)) x1 = int(g_fb.width);
    int y1 = y + h; if (y1 > int(g_fb.height)) y1 = int(g_fb.height);
    for (int yy = y0; yy < y1; ++yy) {
        uint32_t* row = (uint32_t*)((uint8_t*)g_back + yy * g_fb.pitch);
        for (int xx = x0; xx < x1; ++xx) row[xx] = color;
    }
}


// ----- PSF1 loader (tiny) -----
#pragma pack(push,1)
struct PSF1Header {
    uint8_t magic[2];   // 0x36,0x04
    uint8_t mode;       // bit0=512glyphs
    uint8_t charsize;   // bytes per glyph
};
#pragma pack(pop)

struct PSFFont {
    const uint8_t* glyphs = nullptr;
    uint32_t glyph_count = 0;  // 256 or 512
    uint32_t char_w = 8;       // PSF1 is 8 pixels wide
    uint32_t char_h = 0;       // derived from charsize
};

static bool load_psf_from_memory(const uint8_t* buf, size_t size, PSFFont& out) {
    if (size < sizeof(PSF1Header)) return false;
    const PSF1Header* h = (const PSF1Header*)buf;
    if (h->magic[0] != 0x36 || h->magic[1] != 0x04) return false;
    out.glyph_count = (h->mode & 0x01) ? 512u : 256u;
    out.char_h = h->charsize;
    size_t need = sizeof(PSF1Header) + out.glyph_count * out.char_h;
    if (size < need) return false;
    out.glyphs = buf + sizeof(PSF1Header);
    return true;
}

static void draw_glyph(const PSFFont& f, int x, int y, uint32_t fg, uint32_t bg, uint32_t ch) {
    uint32_t idx = ch;
    if (idx >= f.glyph_count) idx = '?';
    const uint8_t* g = f.glyphs + idx * f.char_h;
    for (uint32_t row = 0; row < f.char_h; ++row) {
        uint8_t bits = g[row];
        for (uint32_t col = 0; col < 8; ++col) {
            bool on = (bits >> (7 - col)) & 1;
            put_px(x + col, y + row, on ? fg : bg);
        }
    }
}

static void draw_text(const PSFFont& f, int x, int y, uint32_t fg, uint32_t bg, const char* s) {
    int cx = x;
    for (; *s; ++s) {
        if (*s == '\n') { y += int(f.char_h); cx = x; continue; }
        draw_glyph(f, cx, y, fg, bg, (uint8_t)*s);
        cx += 8;
    }
}

// Demo desktop
static void draw_desktop(const PSFFont& font) {
    // Background gradient
    for (uint32_t y = 0; y < g_fb.height; ++y) {
        uint8_t c = uint8_t((y * 255u) / (g_fb.height ? g_fb.height : 1));
        uint32_t color = 0xFF000030u | (uint32_t(c) << 8);
        for (uint32_t x = 0; x < g_fb.width; ++x) put_px(x, y, color);
    }
    // Taskbar
    fill_rect(0, int(g_fb.height) - 28, g_fb.width, 28, 0xFF202020);
    draw_rect(0, int(g_fb.height) - 28, g_fb.width, 28, 0xFF404040);
    draw_text(font, 8, int(g_fb.height) - 22, 0xFFFFFFFF, 0xFF202020, "gui demo  (ESC to exit)");
}

static void draw_window(const PSFFont& font, int x, int y, int w, int h, const char* title) {
    fill_rect(x, y, w, h, 0xFF2A2A2A);
    draw_rect(x, y, w, h, 0xFF707070);
    // Title bar
    fill_rect(x+1, y+1, w-2, 18, 0xFF3A70B0);
    draw_text(font, x+8, y+4, 0xFFFFFFFF, 0xFF3A70B0, title);
    // Content area
    fill_rect(x+1, y+20, w-2, h-21, 0xFF1C1C1C);
}

static bool load_font_file(uint64_t ahci_base, int port, PSFFont& outfont) {
    static uint8_t buf[64*1024]; // big enough for PSF1/512*charsize
    fat32_read_file_to_buffer(ahci_base, port, "/font.psf", buf, sizeof(buf));
    return load_psf_from_memory(buf, (size_t)buf, outfont);
}

// Expose a start_gui(...) entry
extern "C" void start_gui(uint64_t ahci_base, int port) {
    if (!get_boot_framebuffer(g_fb)) {
        gui_fallback_text();
        return;
    }
    if (!fb_alloc_backbuffer()) {
        cout << "[GUI] Failed to allocate backbuffer.\\n";
        return;
    }

    PSFFont font = {};
    if (!load_font_file(ahci_base, port, font)) {
        // Provide a minimal 8x8 fallback if font.psf missing
        cout << "[GUI] Could not load /font.psf. Using blank background without text.\\n";
    }

    int wx = 60, wy = 60, ww = 360, wh = 220;
    bool running = true;
    while (running) {
        // Draw frame
        if (font.glyphs) draw_desktop(font);
        else fill_rect(0,0,g_fb.width,g_fb.height,0xFF202028);
        draw_window(font, wx, wy, ww, wh, "Demo Window");
        fb_present();

        // Simple input: arrow keys move window, ESC exits (using existing cin)
        // Non-blocking read is ideal; if not available, do a lightweight poll loop.
        // For demonstration, use a tiny blocking read of one character.
        char cbuf[4] = {0};
        cin >> cbuf; // adapt to your non-blocking input if available
        char c = cbuf[0];
        if (c == 27 /*ESC*/) { running = false; }
        else if (c == 'h') { wx -= 10; }
        else if (c == 'l') { wx += 10; }
        else if (c == 'k') { wy -= 10; }
        else if (c == 'j') { wy += 10; }

        // Clamp
        if (wx < 0) wx = 0; if (wy < 0) wy = 0;
        if (wx + ww > int(g_fb.width))  wx = int(g_fb.width)  - ww;
        if (wy + wh > int(g_fb.height)) wy = int(g_fb.height) - wh;
    }

    fb_free_backbuffer();
    cout << "[GUI] Exited.\\n";
}

// ---- End minimal GUI ----




// --- COMMAND PROMPT (Rewritten for better argument parsing) ---
void command_prompt() {
    char line[MAX_COMMAND_LENGTH + 1];
    ahci_base = disk_init(); 
    int port = 0; 
    bool fat32_initialized = false;

    cout << "Kernel Command Prompt. Type 'help' for commands.\n\n";

    while (true) {
        cout << "> ";
        cin >> line; // Use getline to read the whole line

        // --- Argument Parser ---
        char* parts[10] = {nullptr}; // Increased parts for more args
        int part_count = 0;
        char* next_part = line;
        
        while (part_count < 10 && next_part && *next_part != '\0') {
            // Skip leading spaces
            while (*next_part == ' ') next_part++;
            if (*next_part == '\0') break;

            parts[part_count++] = next_part;
            char* space = simple_strchr(next_part, ' ');
            if (space) {
                *space = '\0';
                next_part = space + 1;
            } else {
                next_part = nullptr;
            }
        }
        
        char* cmd = parts[0];
        char* arg1 = parts[1];
        char* arg2 = parts[2];

        if (!cmd || *cmd == '\0') continue;

        // --- Command Handling ---
        if (stricmp(cmd, "help") == 0) cmd_help();
		// inside commandprompt() command handling:
		else if (stricmp(cmd, "gui") == 0) {
			start_gui(ahci_base, port);
		}
		else if (stricmp(cmd, "compile") == 0) {
			cmd_compile(ahci_base, port, arg1);
		}
		
		else if (stricmp(cmd, "run") == 0) {
			cmd_run(ahci_base, port, arg1);
		}
		else if (stricmp(cmd, "exec") == 0) {
			// For exec, we need to handle the rest of the line as code
			char* code_start = line;
			while (*code_start && *code_start != ' ') code_start++; // Skip "exec"
			while (*code_start == ' ') code_start++; // Skip spaces
			if (*code_start != '\0') {
				cmd_exec(code_start);
			} else {
				cout << "Usage: exec <code>\n";
			}
		}
        else if (stricmp(cmd, "clear") == 0) terminal_clear_screen();
        else if (stricmp(cmd, "formatfs") == 0) cmd_formatfs(ahci_base, port);
        else if (stricmp(cmd, "mount") == 0) {
            if (fat32_init(ahci_base, port)) { 
                fat32_initialized = true; 
                cout << "FAT32 mounted.\n"; 
            } else { 
                cout << "Failed to mount. Is disk formatted?\n"; 
            }
        }
        else if (stricmp(cmd, "unmount") == 0) { 
            fat32_initialized = false; 
            cout << "Filesystem unmounted.\n"; 
        }
        else {
            if (!fat32_initialized) {
                 cout << "Filesystem not mounted. Use 'mount' first.\n";
            } else {
                if (stricmp(cmd, "ls") == 0) fat32_list_files(ahci_base, port);
                else if (stricmp(cmd, "rm") == 0) { 
                    if(arg1) fat32_remove_file(ahci_base, port, arg1); 
                    else cout << "Usage: rm <filename>\n"; 
                }
				else if (stricmp(cmd, "kbtest") == 0) {
					xhci_init();
					usb_keyboard_self_test();
				}
                else if (stricmp(cmd, "pong") == 0) {
                  start_pong_game();
                }
                else if (stricmp(cmd, "chkdsk") == 0) {
                  cmd_chkdsk(ahci_base, port);
                }
                else if (stricmp(cmd, "notepad") == 0) { // RENAME command
                    if(arg1) {
                      cmd_notepad(arg1);  // arg1 can be nullptr for new file
                    } else cout << "Usage: notepad <file_name>\n";
                }
                else if (stricmp(cmd, "cat") == 0) {
                    cmd_cat(ahci_base, port, arg1);
                }
                else if (stricmp(cmd, "mv") == 0) { // RENAME command
                    if(arg1 && arg2) {
                        if (fat32_rename_file(ahci_base, port, arg1, arg2) == 0) cout << "File renamed.\n";
                        else cout << "Error renaming file.\n";
                    } else cout << "Usage: mv <old_name> <new_name>\n";
                }
                else if (stricmp(cmd, "cp") == 0) { // COPY command
                    if(arg1 && arg2) {
                        if (fat32_copy_file(ahci_base, port, arg1, arg2) == 0) cout << "File copied.\n";
                        else cout << "Error copying file.\n";
                    } else cout << "Usage: cp <source> <destination>\n";
                }
                else { cout << "Unknown command: '" << cmd << "'\n"; }
            }
        }
    }
}


// --- KERNEL ENTRY POINT ---
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
    
    command_prompt();
}
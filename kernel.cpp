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
#undef MAX_COMMAND_LENGTH
#define MAX_COMMAND_LENGTH 256
#define SECTOR_SIZE 512
#define ENTRY_SIZE 32
#define ATTR_LONG_NAME 0x0F
#define ATTR_DIRECTORY 0x10
#define ATTR_VOLUME_ID 0x08
#define ATTR_ARCHIVE 0x20
#define DELETED_ENTRY 0xE5

// ADD THESE THREE LINES RIGHT AFTER THE EXISTING FORWARD DECLARATIONS:
void cmd_compile(uint64_t ahci_base, int port, const char* filename);
void cmd_run(uint64_t ahci_base, int port, const char* filename);
void cmd_exec(const char* code_text);
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
static inline uint64_t cluster_to_lba(uint32_t cluster) { if (cluster < 2) return 0; return data_start_sector + ((uint64_t)(cluster - 2) * fat32_bpb.sec_per_clus); }
uint32_t clusters_needed(uint32_t size) { uint32_t cluster_size = fat32_bpb.sec_per_clus * fat32_bpb.bytes_per_sec; return (size + cluster_size - 1) / cluster_size; }


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


// --- Helper function for chkdsk (now uses the Bitmap) ---
void scan_directory_for_chkdsk(uint64_t ahci_base, int port, uint32_t dir_cluster, Bitmap& cluster_map, uint32_t max_clusters) {
    if (dir_cluster < 2 || dir_cluster >= max_clusters) return;
    
    uint8_t buffer[SECTOR_SIZE];
    uint32_t current_dir_cluster = dir_cluster;

    while (current_dir_cluster >= 2 && current_dir_cluster < FAT_BAD_CLUSTER) {
        // Mark the directory cluster itself as used
        cluster_map.set(current_dir_cluster);

        uint64_t lba = cluster_to_lba(current_dir_cluster);
        for (uint8_t s = 0; s < fat32_bpb.sec_per_clus; s++) {
            if (read_sectors(ahci_base, port, lba + s, 1, buffer) != 0) return;
            for (uint16_t e = 0; e < SECTOR_SIZE / ENTRY_SIZE; e++) {
                fat_dir_entry_t *entry = (fat_dir_entry_t *)(buffer + e * ENTRY_SIZE);

                if (entry->name[0] == 0x00) {
                    current_dir_cluster = FAT_END_OF_CHAIN;
                    break;
                }
                if (entry->name[0] == DELETED_ENTRY) continue;

                uint32_t file_cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
                
                if ((entry->attr & ATTR_DIRECTORY) && entry->name[0] != '.') {
                    scan_directory_for_chkdsk(ahci_base, port, file_cluster, cluster_map, max_clusters);
                } else {
                    uint32_t current_file_cluster = file_cluster;
                    while (current_file_cluster >= 2 && current_file_cluster < FAT_BAD_CLUSTER) {
                        cluster_map.set(current_file_cluster);
                        current_file_cluster = read_fat_entry(ahci_base, port, current_file_cluster);
                    }
                }
            }
        }
        if (current_dir_cluster < FAT_BAD_CLUSTER) {
             current_dir_cluster = read_fat_entry(ahci_base, port, current_dir_cluster);
        }
    }
}


// --- The main chkdsk command (now uses the Bitmap) ---
void cmd_chkdsk(uint64_t ahci_base, int port) {
    cout << "Checking filesystem for errors...\n";

    uint32_t total_data_sectors = fat32_bpb.tot_sec32 - data_start_sector;
    uint32_t max_clusters = total_data_sectors / fat32_bpb.sec_per_clus + 2;
    
    // Use the memory-efficient Bitmap class for the cluster map.
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
            if (entry->name[0] == 0) return -4;
            if (entry->name[0] == DELETED_ENTRY || (entry->attr & (ATTR_LONG_NAME | ATTR_VOLUME_ID))) continue;
            if (simple_memcmp(entry->name, target, 11) == 0) {
                uint32_t cluster = (entry->fst_clus_hi << 16) | entry->fst_clus_lo;
                entry->name[0] = DELETED_ENTRY;
                if (write_sectors(ahci_base, port, lba + s, (uint32_t)1, buffer) != 0) return -2;
                if (cluster >= 2) free_cluster_chain(ahci_base, port, cluster);
                return 0;
            }
        }
    }
    return -4;
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
         << "  touch <file> [content], cat <file>\n"
         << "  cp <src> <dest>, mv <old> <new>\n"
         << "  compile <file.cpp>, run <file.obj>\n"
         << "  exec <inline_code>\n"
         << "  formatfs, mount, unmount, fsinfo\n"
         << "  kbtest\n";
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

// REPLACE THE EXISTING COMPILER SYSTEM WITH THIS KERNEL-COMPATIBLE VERSION
// This version avoids new/delete and uses static allocation instead

// --- C++ COMPILER SYSTEM (KERNEL COMPATIBLE) ---
// COMPLETE ENHANCED C++ COMPILER SYSTEM FOR KERNEL
// Add this after your existing includes and before your forward declarations

// COMPLETE ENHANCED C++ COMPILER SYSTEM FOR KERNEL.CPP
// Add this entire section after your existing includes and before your forward declarations

// --- ENHANCED C++ COMPILER SYSTEM ---
enum TokenType {
    TOKEN_KEYWORD, TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_STRING, 
    TOKEN_OPERATOR, TOKEN_DELIMITER, TOKEN_EOF
};

struct Token {
    TokenType type;
    char value[64];
    int line;
};

// Variable table for local variables
struct Variable {
    char name[32];
    int stack_offset;
    bool in_use;
};

class VariableTable {
private:
    Variable vars[16];
    int next_offset;
    
public:
    void init() {
        for (int i = 0; i < 16; i++) {
            vars[i].in_use = false;
        }
        next_offset = -4;
    }
    
    int add_variable(const char* name) {
        for (int i = 0; i < 16; i++) {
            if (!vars[i].in_use) {
                simple_strcpy(vars[i].name, name);
                vars[i].stack_offset = next_offset;
                vars[i].in_use = true;
                next_offset -= 4;
                return vars[i].stack_offset;
            }
        }
        return 0;
    }
    
    int get_variable_offset(const char* name) {
        for (int i = 0; i < 16; i++) {
            if (vars[i].in_use && simple_strcmp(vars[i].name, name) == 0) {
                return vars[i].stack_offset;
            }
        }
        return 0;
    }
    
    int get_stack_size() {
        return -(next_offset + 4);
    }
};

class SimpleTokenizer {
private:
    const char* source;
    int pos;
    int line;
    
    void skip_whitespace() {
        while (source[pos] == ' ' || source[pos] == '\t' || source[pos] == '\n' || source[pos] == '\r') {
            if (source[pos] == '\n') line++;
            pos++;
        }
    }
    
    bool is_digit(char c) { return c >= '0' && c <= '9'; }
    bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
    
    Token read_number() {
        Token token;
        token.type = TOKEN_NUMBER;
        token.line = line;
        int i = 0;
        while (is_digit(source[pos]) && i < 63) {
            token.value[i++] = source[pos++];
        }
        token.value[i] = '\0';
        return token;
    }
    
    Token read_identifier() {
        Token token;
        token.line = line;
        int i = 0;
        while ((is_alpha(source[pos]) || is_digit(source[pos]) || source[pos] == '_') && i < 63) {
            token.value[i++] = source[pos++];
        }
        token.value[i] = '\0';
        
        if (simple_strcmp(token.value, "int") == 0 || 
            simple_strcmp(token.value, "void") == 0 ||
            simple_strcmp(token.value, "return") == 0 ||
            simple_strcmp(token.value, "if") == 0 ||
            simple_strcmp(token.value, "else") == 0 ||
            simple_strcmp(token.value, "while") == 0) {
            token.type = TOKEN_KEYWORD;
        } else {
            token.type = TOKEN_IDENTIFIER;
        }
        return token;
    }
    
    Token read_operator() {
        Token token;
        token.type = TOKEN_OPERATOR;
        token.line = line;
        
        if (source[pos] == '=' && source[pos + 1] == '=') {
            token.value[0] = '=';
            token.value[1] = '=';
            token.value[2] = '\0';
            pos += 2;
        } else if (source[pos] == '!' && source[pos + 1] == '=') {
            token.value[0] = '!';
            token.value[1] = '=';
            token.value[2] = '\0';
            pos += 2;
        } else if (source[pos] == '<' && source[pos + 1] == '=') {
            token.value[0] = '<';
            token.value[1] = '=';
            token.value[2] = '\0';
            pos += 2;
        } else if (source[pos] == '>' && source[pos + 1] == '=') {
            token.value[0] = '>';
            token.value[1] = '=';
            token.value[2] = '\0';
            pos += 2;
        } else {
            token.value[0] = source[pos];
            token.value[1] = '\0';
            pos++;
        }
        return token;
    }
    
public:
    void init(const char* src) { 
        source = src; 
        pos = 0; 
        line = 1; 
    }
    
    Token next_token() {
        Token token;
        skip_whitespace();
        
        if (source[pos] == '\0') {
            token.type = TOKEN_EOF;
            token.value[0] = '\0';
            return token;
        }
        
        if (is_digit(source[pos])) return read_number();
        if (is_alpha(source[pos]) || source[pos] == '_') return read_identifier();
        
        char c = source[pos];
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '=' || 
            c == '<' || c == '>' || c == '!' || c == '&' || c == '|') {
            return read_operator();
        }
        
        token.type = TOKEN_DELIMITER;
        token.value[0] = source[pos];
        token.value[1] = '\0';
        token.line = line;
        pos++;
        
        return token;
    }
};

class X86Generator {
private:
    uint8_t* code_buffer;
    int code_pos;
    int buffer_size;
    
    void emit_byte(uint8_t byte) {
        if (code_pos < buffer_size) {
            code_buffer[code_pos++] = byte;
        }
    }
    
    void emit_dword(uint32_t dword) {
        emit_byte(dword & 0xff);
        emit_byte((dword >> 8) & 0xff);
        emit_byte((dword >> 16) & 0xff);
        emit_byte((dword >> 24) & 0xff);
    }
    
public:
    void init(uint8_t* buffer, int size) { 
        code_buffer = buffer; 
        code_pos = 0; 
        buffer_size = size; 
    }
    
    void emit_function_prologue(int stack_space = 0) {
        emit_byte(0x55);
        emit_byte(0x89); emit_byte(0xe5);
        if (stack_space > 0) {
            emit_byte(0x83); emit_byte(0xec); emit_byte(stack_space & 0xff);
        }
    }
    
    void emit_function_epilogue() {
        emit_byte(0x89); emit_byte(0xec);
        emit_byte(0x5d);
        emit_byte(0xc3);
    }
    
    void emit_mov_eax_immediate(int value) {
        emit_byte(0xb8);
        emit_dword(value);
    }
    
    void emit_mov_eax_variable(int offset) {
        emit_byte(0x8b); emit_byte(0x45); emit_byte(offset & 0xff);
    }
    
    void emit_mov_variable_eax(int offset) {
        emit_byte(0x89); emit_byte(0x45); emit_byte(offset & 0xff);
    }
    
    void emit_push_eax() {
        emit_byte(0x50);
    }
    
    void emit_pop_ebx() {
        emit_byte(0x5b);
    }
    
    void emit_add_eax_ebx() {
        emit_byte(0x01); emit_byte(0xd8);
    }
    
    void emit_sub_ebx_eax() {
        emit_byte(0x29); emit_byte(0xc3);
    }
    
    void emit_mov_eax_ebx() {
        emit_byte(0x89); emit_byte(0xd8);
    }
    
    void emit_imul_eax_ebx() {
        emit_byte(0x0f); emit_byte(0xaf); emit_byte(0xc3);
    }
    
    void emit_div_ebx() {
        emit_byte(0x99);
        emit_byte(0xf7); emit_byte(0xfb);
    }
    
    void emit_cmp_ebx_eax() {
        emit_byte(0x39); emit_byte(0xc3);
    }
    
    void emit_sete_al() {
        emit_byte(0x0f); emit_byte(0x94); emit_byte(0xc0);
        emit_byte(0x0f); emit_byte(0xb6); emit_byte(0xc0);
    }
    
    void emit_setne_al() {
        emit_byte(0x0f); emit_byte(0x95); emit_byte(0xc0);
        emit_byte(0x0f); emit_byte(0xb6); emit_byte(0xc0);
    }
    
    void emit_setl_al() {
        emit_byte(0x0f); emit_byte(0x9c); emit_byte(0xc0);
        emit_byte(0x0f); emit_byte(0xb6); emit_byte(0xc0);
    }
    
    void emit_setle_al() {
        emit_byte(0x0f); emit_byte(0x9e); emit_byte(0xc0);
        emit_byte(0x0f); emit_byte(0xb6); emit_byte(0xc0);
    }
    
    void emit_setg_al() {
        emit_byte(0x0f); emit_byte(0x9f); emit_byte(0xc0);
        emit_byte(0x0f); emit_byte(0xb6); emit_byte(0xc0);
    }
    
    void emit_setge_al() {
        emit_byte(0x0f); emit_byte(0x9d); emit_byte(0xc0);
        emit_byte(0x0f); emit_byte(0xb6); emit_byte(0xc0);
    }
    
    void emit_test_eax() {
        emit_byte(0x85); emit_byte(0xc0);
    }
    
    int emit_jz_forward() {
        emit_byte(0x0f); emit_byte(0x84);
        int patch_pos = code_pos;
        emit_dword(0x00000000);
        return patch_pos;
    }
    
    int emit_jmp_forward() {
        emit_byte(0xe9);
        int patch_pos = code_pos;
        emit_dword(0x00000000);
        return patch_pos;
    }
    
    void emit_jmp_backward(int target_pos) {
        emit_byte(0xe9);
        int displacement = target_pos - (code_pos + 4);
        emit_dword(displacement);
    }
    
    void patch_jump(int jump_pos, int target_pos) {
        int displacement = target_pos - (jump_pos + 4);
        code_buffer[jump_pos] = displacement & 0xff;
        code_buffer[jump_pos + 1] = (displacement >> 8) & 0xff;
        code_buffer[jump_pos + 2] = (displacement >> 16) & 0xff;
        code_buffer[jump_pos + 3] = (displacement >> 24) & 0xff;
    }
    
    int get_code_pos() { 
        return code_pos; 
    }
    
    int get_current_pos() {
        return code_pos;
    }
    
    int get_code_size() { 
        return code_pos; 
    }
};

class SimpleCppCompiler {
private:
    SimpleTokenizer tokenizer;
    Token current_token;
    X86Generator generator;
    VariableTable variables;
    
    void advance() { current_token = tokenizer.next_token(); }
    
    int simple_atoi(const char* str) {
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
    
    bool expect(const char* expected) {
        if (simple_strcmp(current_token.value, expected) == 0) {
            advance();
            return true;
        }
        return false;
    }
    
    bool parse_program() {
        while (current_token.type != TOKEN_EOF) {
            if (!parse_function()) return false;
        }
        return true;
    }
    
    bool parse_function() {
        if (current_token.type != TOKEN_KEYWORD) return false;
        if (simple_strcmp(current_token.value, "int") != 0) return false;
        
        advance();
        if (current_token.type != TOKEN_IDENTIFIER) return false;
        advance();
        if (!expect("(")) return false;
        if (!expect(")")) return false;
        if (!expect("{")) return false;
        
        variables.init();
        generator.emit_function_prologue(64);
        
        while (current_token.value[0] != '}' && current_token.type != TOKEN_EOF) {
            if (!parse_statement()) return false;
        }
        
        if (!expect("}")) return false;
        
        generator.emit_function_epilogue();
        return true;
    }
    
    bool parse_statement() {
        if (current_token.type == TOKEN_KEYWORD) {
            if (simple_strcmp(current_token.value, "int") == 0) {
                return parse_variable_declaration();
            } else if (simple_strcmp(current_token.value, "return") == 0) {
                return parse_return_statement();
            } else if (simple_strcmp(current_token.value, "if") == 0) {
                return parse_if_statement();
            } else if (simple_strcmp(current_token.value, "while") == 0) {
                return parse_while_statement();
            }
        } else if (current_token.type == TOKEN_IDENTIFIER) {
            return parse_assignment();
        }
        
        while (current_token.value[0] != ';' && current_token.type != TOKEN_EOF) {
            advance();
        }
        if (current_token.value[0] == ';') advance();
        return true;
    }
    
    bool parse_variable_declaration() {
        advance();
        if (current_token.type != TOKEN_IDENTIFIER) return false;
        
        char var_name[32];
        simple_strcpy(var_name, current_token.value);
        advance();
        
        variables.add_variable(var_name);
        
        if (current_token.value[0] == '=') {
            advance();
            if (!parse_expression()) return false;
            
            int offset = variables.get_variable_offset(var_name);
            generator.emit_mov_variable_eax(offset);
        }
        
        return expect(";");
    }
    
    bool parse_assignment() {
        char var_name[32];
        simple_strcpy(var_name, current_token.value);
        advance();
        
        if (!expect("=")) return false;
        
        if (!parse_expression()) return false;
        
        int offset = variables.get_variable_offset(var_name);
        if (offset != 0) {
            generator.emit_mov_variable_eax(offset);
        }
        
        return expect(";");
    }
    
    bool parse_return_statement() {
        advance();
        if (!parse_expression()) return false;
        return expect(";");
    }
    
    bool parse_if_statement() {
        advance();
        if (!expect("(")) return false;
        
        if (!parse_expression()) return false;
        if (!expect(")")) return false;
        
        generator.emit_test_eax();
        int jump_pos = generator.emit_jz_forward();
        
        if (!expect("{")) return false;
        
        while (current_token.value[0] != '}' && current_token.type != TOKEN_EOF) {
            if (!parse_statement()) return false;
        }
        
        if (!expect("}")) return false;
        
        generator.patch_jump(jump_pos, generator.get_current_pos());
        
        return true;
    }
    
    bool parse_while_statement() {
        advance();
        if (!expect("(")) return false;
        
        int loop_start = generator.get_current_pos();
        
        if (!parse_expression()) return false;
        if (!expect(")")) return false;
        
        generator.emit_test_eax();
        int exit_jump = generator.emit_jz_forward();
        
        if (!expect("{")) return false;
        
        while (current_token.value[0] != '}' && current_token.type != TOKEN_EOF) {
            if (!parse_statement()) return false;
        }
        
        if (!expect("}")) return false;
        
        generator.emit_jmp_backward(loop_start);
        generator.patch_jump(exit_jump, generator.get_current_pos());
        
        return true;
    }
    
    bool parse_expression() {
        if (!parse_comparison()) return false;
        return true;
    }
    
    bool parse_comparison() {
        if (!parse_term()) return false;
        
        while (current_token.type == TOKEN_OPERATOR &&
               (simple_strcmp(current_token.value, "==") == 0 ||
                simple_strcmp(current_token.value, "!=") == 0 ||
                simple_strcmp(current_token.value, "<") == 0 ||
                simple_strcmp(current_token.value, "<=") == 0 ||
                simple_strcmp(current_token.value, ">") == 0 ||
                simple_strcmp(current_token.value, ">=") == 0)) {
            
            char op[3];
            simple_strcpy(op, current_token.value);
            advance();
            
            generator.emit_push_eax();
            
            if (!parse_term()) return false;
            
            generator.emit_pop_ebx();
            generator.emit_cmp_ebx_eax();
            
            if (simple_strcmp(op, "==") == 0) generator.emit_sete_al();
            else if (simple_strcmp(op, "!=") == 0) generator.emit_setne_al();
            else if (simple_strcmp(op, "<") == 0) generator.emit_setl_al();
            else if (simple_strcmp(op, "<=") == 0) generator.emit_setle_al();
            else if (simple_strcmp(op, ">") == 0) generator.emit_setg_al();
            else if (simple_strcmp(op, ">=") == 0) generator.emit_setge_al();
        }
        
        return true;
    }
    
    bool parse_term() {
        if (!parse_factor()) return false;
        
        while (current_token.type == TOKEN_OPERATOR &&
               (current_token.value[0] == '+' || current_token.value[0] == '-')) {
            char op = current_token.value[0];
            advance();
            
            generator.emit_push_eax();
            
            if (!parse_factor()) return false;
            
            generator.emit_pop_ebx();
            
            if (op == '+') {
                generator.emit_add_eax_ebx();
            } else {
                generator.emit_sub_ebx_eax();
                generator.emit_mov_eax_ebx();
            }
        }
        
        return true;
    }
    
    bool parse_factor() {
        if (!parse_primary()) return false;
        
        while (current_token.type == TOKEN_OPERATOR &&
               (current_token.value[0] == '*' || current_token.value[0] == '/')) {
            char op = current_token.value[0];
            advance();
            
            generator.emit_push_eax();
            
            if (!parse_primary()) return false;
            
            generator.emit_pop_ebx();
            
            if (op == '*') {
                generator.emit_imul_eax_ebx();
            } else {
                generator.emit_mov_eax_ebx();
                generator.emit_pop_ebx();
                generator.emit_push_eax();
                generator.emit_pop_ebx();
                generator.emit_div_ebx();
            }
        }
        
        return true;
    }
    
    bool parse_primary() {
        if (current_token.type == TOKEN_NUMBER) {
            int value = simple_atoi(current_token.value);
            generator.emit_mov_eax_immediate(value);
            advance();
            return true;
        } else if (current_token.type == TOKEN_IDENTIFIER) {
            int offset = variables.get_variable_offset(current_token.value);
            if (offset != 0) {
                generator.emit_mov_eax_variable(offset);
            }
            advance();
            return true;
        } else if (current_token.value[0] == '(') {
            advance();
            if (!parse_expression()) return false;
            return expect(")");
        }
        
        return false;
    }
    
public:
    bool compile(const char* source_code, uint8_t* output_buffer, int buffer_size) {
        tokenizer.init(source_code);
        generator.init(output_buffer, buffer_size);
        
        current_token = tokenizer.next_token();
        
        return parse_program();
    }
};

// Executable memory management
static uint8_t executable_memory_pool[8192];
static bool memory_used[128];
static bool memory_pool_initialized = false;

void init_executable_memory_pool() {
    if (!memory_pool_initialized) {
        simple_memset(memory_used, 0, sizeof(memory_used));
        memory_pool_initialized = true;
    }
}

void* allocate_executable_block(int size) {
    init_executable_memory_pool();
    int blocks_needed = (size + 63) / 64;
    
    for (int i = 0; i <= 128 - blocks_needed; i++) {
        bool can_allocate = true;
        for (int j = 0; j < blocks_needed; j++) {
            if (memory_used[i + j]) {
                can_allocate = false;
                break;
            }
        }
        
        if (can_allocate) {
            for (int j = 0; j < blocks_needed; j++) {
                memory_used[i + j] = true;
            }
            return &executable_memory_pool[i * 64];
        }
    }
    return nullptr;
}

void free_executable_block(void* ptr) {
    if (!ptr || ptr < executable_memory_pool || 
        ptr >= executable_memory_pool + 8192) return;
    
    int block_index = ((uint8_t*)ptr - executable_memory_pool) / 64;
    if (block_index >= 0 && block_index < 128) {
        memory_used[block_index] = false;
    }
}

struct CodeExecutor {
    uint8_t* executable_memory;
    int memory_size;
    bool initialized;
    
    void init() {
        memory_size = 2048;
        executable_memory = (uint8_t*)allocate_executable_block(memory_size);
        initialized = (executable_memory != nullptr);
    }
    
    void cleanup() {
        if (executable_memory && initialized) {
            free_executable_block(executable_memory);
            executable_memory = nullptr;
            initialized = false;
        }
    }
    
    int execute_code(uint8_t* code, int code_size) {
        if (!initialized || !executable_memory) {
            cout << "Error: Could not allocate executable memory\n";
            return -1;
        }
        
        simple_memcpy(executable_memory, code, code_size);
        
        typedef int (*compiled_function)();
        compiled_function func = (compiled_function)executable_memory;
        
        cout << "Executing compiled code...\n";
        int result = func();
        cout << "Execution completed. Return value: " << result << "\n";
        
        return result;
    }
};

static SimpleCppCompiler cpp_compiler;
static CodeExecutor code_executor;
static bool compiler_system_initialized = false;

void init_compiler_system() {
    if (!compiler_system_initialized) {
        code_executor.init();
        compiler_system_initialized = true;
    }
}
// --- END C++ COMPILER SYSTEM ---

// UPDATED COMMAND IMPLEMENTATIONS (replace the previous ones):
void cmd_compile(uint64_t ahci_base, int port, const char* filename) {
    init_compiler_system(); // Initialize on first use
    
    if (!filename) {
        cout << "Usage: compile <filename.cpp>\n";
        return;
    }
    
    // Read C++ source file
    static char source_buffer[8192];
    int bytes_read = fat32_read_file_to_buffer(ahci_base, port, filename, source_buffer, sizeof(source_buffer));
    
    if (bytes_read < 0) {
        cout << "Error: Could not read source file '" << filename << "'\n";
        return;
    }
    
    cout << "Compiling " << filename << "...\n";
    cout << "Source code:\n" << source_buffer << "\n---\n";
    
    // Compile to x86 machine code
    static uint8_t compiled_code[2048];
    bool success = cpp_compiler.compile(source_buffer, compiled_code, sizeof(compiled_code));
    
    if (!success) {
        cout << "Compilation failed!\n";
        return;
    }
    
    // Generate output filename
    char obj_filename[64];
    simple_strcpy(obj_filename, filename);
    // Replace .cpp with .obj
    char* dot = simple_strchr(obj_filename, '.');
    if (dot) {
        simple_strcpy(dot, ".obj");
    } else {
        simple_strcat(obj_filename, ".obj");
    }
    
    // Save compiled object file
    int result = fat32_write_file(ahci_base, port, obj_filename, compiled_code, 2048);
    
    if (result == 0) {
        cout << "Compilation successful! Object file: " << obj_filename << "\n";
    } else {
        cout << "Error saving object file\n";
    }
}

void cmd_run(uint64_t ahci_base, int port, const char* filename) {
    init_compiler_system(); // Initialize on first use
    
    if (!filename) {
        cout << "Usage: run <filename.obj>\n";
        return;
    }
    
    // Read compiled object file
    static uint8_t object_code[2048];
    int bytes_read = fat32_read_file_to_buffer(ahci_base, port, filename, object_code, sizeof(object_code));
    
    if (bytes_read < 0) {
        cout << "Error: Could not read object file '" << filename << "'\n";
        return;
    }
    
    cout << "Loading and executing " << filename << "...\n";
    
    // Execute the compiled code
    code_executor.execute_code(object_code, bytes_read);
}

void cmd_exec(const char* code_text) {
    init_compiler_system(); // Initialize on first use
    
    if (!code_text) {
        cout << "Usage: exec <inline_code>\n";
        cout << "Example: exec \"int main() { return 42; }\"\n";
        return;
    }
    
    cout << "Compiling and executing inline code...\n";
    
    static uint8_t compiled_code[1024];
    bool success = cpp_compiler.compile(code_text, compiled_code, sizeof(compiled_code));
    
    if (!success) {
        cout << "Compilation failed!\n";
        return;
    }
    
    code_executor.execute_code(compiled_code, 1024);
}
// --- END C++ COMPILER SYSTEM ---

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
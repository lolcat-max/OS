Features: keyboard interrupts, iostream_wrapper, AHCI sata, XHCI USB(WIP), DMA, FAT32 filesystem, format, chkdsk(WIP), copy and move/rename file, delete.


ByteCodeVM features:

Keywords:

"int","char","string","return","if","else","while","break","continue",
"cin","cout","endl","argc","argv","read_file","write_file","append_file",
"array_size","array_resize","str_length","str_substr","int_to_str","str_compare",
"str_find_char","str_find_str","str_find_last_char","str_contains",
"str_starts_with","str_ends_with","str_count_char","str_replace_char",
"scan_hardware","get_device_info","get_hardware_array","display_memory_map",
"mmio_read8","mmio_read16","mmio_read32","mmio_read64",
"mmio_write8","mmio_write16","mmio_write32","mmio_write64"

hardware features soon (scan_hardware works, read VGA works)...

sudo dd if=/home/user/Desktop/Text_OS-main/main.iso of=/dev/sdX (replace X with your USB drive, the OS is programmed to select sata port 0 for all file operations recommended not to boot on drive 0, use USB or arbitrary drive to install)

TODO: 
1. inspect functionality of bytecodeVM
2. inspect array functionality 
3. complete hardware IO datasets in bytecodeVM.
4. fix notepad memory limit glitch
5. setup USB keyboard HID

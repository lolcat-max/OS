Features: keyboard interrupts, iostream_wrapper, AHCI sata, XHCI USB(WIP), DMA, FAT32 filesystem, format, chkdsk(WIP), copy and move/rename file, delete.


ByteCodeVM features:

"int","char","string","return","if","else","while","break","continue",
"cin","cout","endl","argc","argv","read_file","write_file","append_file",
"array_size","array_resize","str_length","str_substr","int_to_str","str_compare",
"str_find_char","str_find_str","str_find_last_char","str_contains",
"str_starts_with","str_ends_with","str_count_char","str_replace_char"

hardware features soon...

sudo dd if=/home/user/Desktop/Text_OS-main/main.iso of=/dev/sdX (replace X with your USB drive, the OS is programmed to select sata port 0 for all file operations recommended not to boot on drive 0, use USB or arbitrary drive to install)

TODO: 
1. inspect functionality of bytecodeVM 
3. add hardware IO in bytecodeVM.
4. setup USB keyboard HID in kernel init.

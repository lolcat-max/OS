Features: BytecodeVM, keyboard interrupts, iostream_wrapper, AHCI sata, XHCI USB(WIP), DMA, FAT32 filesystem, format, chkdsk(WIP), copy and move/rename file, delete.

sudo dd if=/home/user/Desktop/Text_OS-main/main.iso of=/dev/sdX (replace X with your USB drive, the OS is programmed to select sata port 0 for all file operations recommended not to boot on drive 0, use USB or arbitrary drive to install)

TODO: 
1. inspect function call functionality
2. add file save/load in self hosted compiler.
3. add inline assembly for hardware IO in self hosted compiler.
4. setup USB keyboard HID in kernel init.

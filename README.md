cin >> input, acts as getline for char[n], perhaps an oversimplification...

limitations are: cin cannot directly accept int, use atoi afterward

Features: BytecodeVM, keyboard interrupts, iostream_wrapper, AHCI sata, XHCI USB(WIP), DMA, FAT32 filesystem, format, chkdsk(WIP), copy and move/rename file, delete.

sudo dd if=/home/user/Desktop/Text_OS-main/main.iso of=/dev/sdX (replace X with your sata drive, the OS is programmed to select port 0 for all file operations recommended not to boot on drive 0, or it's use once OS, use USB or arbitrary drive )

TODO: 
1. add inline assembly for hardware IO in self hosted compiler.
2. setup USB keyboard HID in kernel init.

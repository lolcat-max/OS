cin >> input, acts as getline for char[n], perhaps an oversimplification...

limitations are: cin cannot directly accept int, use atoi afterward

Features: keyboard interrupts, iostream_wrapper, AHCI sata, XHCI USB(WIP), DMA, FAT32 filesystem, format, chkdsk(WIP), game engine, copy and move/rename file, delete.

sudo dd if=/home/user/Desktop/Text_OS-main/main.iso of=/dev/sdX (replace X with your sata drive, the OS is programmed to select port 0 for all file operations recommended not to boot on drive 0, or it's use once OS, use USB or arbitrary drive )

TODO:
1. add cin & cout to self hosted compiler
2. add inline assembly for hardware IO in self hosted compiler.
3. save/load file for self hosted compiler...
4. setup USB keyboard HID in kernel init.

Perhaps a modified open source kernel to only allow authenticated applications from accessing mutually exclusive drives and WiFi (not with the same virtual machine) with a root access prompts to transfer data to & from secure locations...




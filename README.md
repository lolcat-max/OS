Features: drop-in kernel and real-time code compiler, keyboard interrupts, iostream_wrapper, AHCI sata, XHCI USB(WIP), DMA, FAT32 filesystem, format, chkdsk(WIP).

sudo dd if=/home/user/Desktop/Text_OS-main/main.iso of=/dev/sdX (replace X with your USB drive, the OS is programmed to select sata port 0 for all file operations recommended not to boot on drive 0, use USB or arbitrary drive to install)

TODO: 
1. inspect hardware IO
2. fix notepad glitches
3. improve variable/function dynamics in compiler
4. deconstructor and constructor for REGISTER_FUNCTION_EXT dataset 
5. setup USB keyboard HID

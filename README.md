Features: drop-in kernel and code executor, keyboard interrupts, iostream_wrapper, AHCI sata, XHCI USB(WIP), DMA, FAT32 filesystem, format, chkdsk(WIP).

hardware features soon...

sudo dd if=/home/user/Desktop/Text_OS-main/main.iso of=/dev/sdX (replace X with your USB drive, the OS is programmed to select sata port 0 for all file operations recommended not to boot on drive 0, use USB or arbitrary drive to install)

TODO: 
1. add formatfs
2. fix hex representation
3. stringify functions
4. inspect hardware IO
5. add notepad
6. setup USB keyboard HID

cin >> input, acts as getline for char[n], perhaps an oversimplification...

limitations are: cin cannot directly accept int, use atoi afterward

Features: keyboard interrupts, iostream, AHCI sata, XHCI USB(WIP), DMA, FAT32 filesystem, format, chkdsk(WIP), game engine, copy and paste file, delete, cp and mv commands. 

TODO: implement notepad robustly, dump DMA and PCIE to file...

sudo dd if=/home/user/Desktop/Text_OS-main/main.iso of=/dev/sdX (replace X with your sata drive, the OS is programmed to select port 0 for all file operations recommended not to boot on drive 0, or it's use once OS, use USB)

TODO: setup USB keyboard HID in kernel init.




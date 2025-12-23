A Linux GUI interface OS with essential applications.

Script works on Debian.




TODO:

quote:

I have read GRUB: "invalid arch independent ELF magic" after install on SSD but the solution is no solution for me since installing grub via live cd only means installing the mbr version and I can't seem to find any manual on how to install grub-efi while booted into the live cd.

So my question is: How can I either edit the grubx64.efi file in my EFI Partition, reinstall grub-efi with a live CD / DVD or use grub rescue commands to fix this issue?

The solution for me was (and probably for anyone having that problem):

Boot into the live cd and type into the terminal (of course you must edit the mounting operations respecting your own partition table):

sudo apt-get install grub-efi-amd64
sudo mount /dev/sda3 /mnt
sudo mount /dev/sda1 /mnt/boot 
sudo grub-install --root-directory=/mnt /dev/sda
Now grubx64.efi should boot without any problems.

Running update-grub while booted into ubuntu restored the more eye friendly looks of the grub boot menu.

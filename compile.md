sudo apt-get update
sudo apt-get install -y debootstrap ca-certificates gnupg xz-utils
sudo apt-get install debian-archive-keyring
sudo update-ca-certificates



add:

deb https://deb.debian.org/debian bookworm main non-free-firmware
deb-src https://deb.debian.org/debian bookworm main non-free-firmware
deb https://security.debian.org/debian-security bookworm-security main non-free-firmware
deb-src https://security.debian.org/debian-security bookworm-security main non-free-firmware
deb https://deb.debian.org/debian bookworm-updates main non-free-firmware
deb-src https://deb.debian.org/debian bookworm-updates main non-free-firmware

to /etc/apt/sources.list.d/ubuntu.sources

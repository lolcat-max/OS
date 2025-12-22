sudo apt-get update
sudo apt-get install -y debootstrap ca-certificates gnupg xz-utils
sudo apt-get install debian-archive-keyring
sudo update-ca-certificates



add:

Types: deb
URIs: http://deb.debian.org/debian/
Suites: bookworm
Components: main contrib non-free
Signed-By: /etc/apt/trusted.gpg

to /etc/apt/sources.list.d/ubuntu.sources

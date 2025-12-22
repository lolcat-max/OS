sudo apt-get update
sudo apt-get install -y \
  debootstrap \
  debian-archive-keyring \
  ca-certificates \
  gnupg \
  xz-utils

sudo update-ca-certificates

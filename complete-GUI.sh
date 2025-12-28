#!/bin/bash

# Build custom Debian ISO
lb config --binary-image iso-hybrid --bootappend-live "toram"
echo '#!/bin/bash\neject /dev/sr0\nexit 0' > config/includes.chroot/etc/rc.local
chmod +x config/includes.chroot/etc/rc.local
sudo lb build


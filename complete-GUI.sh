#!/bin/bash
# Debian Live ISO Builder: Eject at Boot & Terminal Lockdown
# Requirements: live-build, xorriso

set -e

# 1. Initialize live-build workspace
mkdir -p custom-live-iso && cd custom-live-iso
lb config \
    --binary-image iso-hybrid \
    --bootappend-live "boot=live components quiet splash toram" \
    --distribution bookworm \
    --archive-areas "main"

# 2. Add Eject Script
# Runs at the end of the boot process to eject media
mkdir -p config/includes.chroot/etc
cat <<EOF > config/includes.chroot/etc/rc.local
#!/bin/bash
(sleep 10 && eject /dev/sr0) &
exit 0
EOF
chmod +x config/includes.chroot/etc/rc.local

# 3. Disable Virtual Terminals (X11)
# Prevents switching to TTY with Ctrl+Alt+F1-F6
mkdir -p config/includes.chroot/etc/X11/xorg.conf.d
cat <<EOF > config/includes.chroot/etc/X11/xorg.conf.d/10-no-vt.conf
Section "Serverflags"
    Option "DontVTSwitch" "yes"
    Option "DontZap" "yes"
EndSection
EOF

# 4. Disable Getty (TTY Login) Services
# Completely masks the login prompts on virtual consoles
mkdir -p config/hooks/normal
cat <<EOF > config/hooks/normal/0500-disable-tty.hook.chroot
#!/bin/sh
for i in 1 2 3 4 5 6; do
    systemctl mask getty@tty\$i.service
done
usermod -s /usr/sbin/nologin user
passwd -l root
EOF
chmod +x config/hooks/normal/0500-disable-tty.hook.chroot

# 5. GNOME Lockdown
# Disables terminal and command line in GNOME environments
mkdir -p config/includes.chroot/etc/dconf/db/local.d/locks
cat <<EOF > config/includes.chroot/etc/dconf/db/local.d/00-lockdown
[org/gnome/desktop/lockdown]
disable-command-line=true
EOF
echo "/org/gnome/desktop/lockdown" > config/includes.chroot/etc/dconf/db/local.d/locks/lockdown

# 6. Build the ISO
sudo lb build

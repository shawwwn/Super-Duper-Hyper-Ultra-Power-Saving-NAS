# Super Duper Hyper Ultra Power Saving NAS Driver Module
Driver module for my super power-saving NAS.

Tl;dr: This module monitors accesses to a mountpoint directory, powers up(thru gpio) and mounts underlying hdd upon access.\
Automatically unmounted and power down hdd in order to save power.

Sloppily made compatible with kernel 5.19 for ARM/ARM64.\
I am not responsible for any potential bugs/caveats. Please submit a Issue/PR if you find one.\
For kernel 4.9 refer to branch **4.9**.

## Installation
**Prerequisites:**
```bash
apt-get install mountpoint build-essential linux-headers-`uname -r`
```

**Auto Install:**
```bash
make
make install
# edit module parameters in /etc/modprobe.d/nas_pm.conf
make uninstall
```

**Manual Install:**
```bash
make
insmod nas_pm.ko uuid="<uuid-of-hdd>" mountpoint="/path/to/mountpoint/directory" gpio=123
rmmod nas_pm.ko
```

**Manual Control:**
```bash
# power on harddrive
echo on >/sys/module/nas_pm/parameters/control
cat /sys/module/nas_pm/parameters # 'on'

# power off harddrive
echo off >/sys/module/nas_pm/parameters/control
cat /sys/module/nas_pm/parameters # 'off'
```

## Module Parameters:
* **`uuid`** - UUID of harddrive partition to mount.
* **`mountpoint`** - Path of directory the partition will be mounted at.
* **`mountscript`** - Path to a custom shell script that do the mounting. (default: `/etc/nas_pm/mountscript.sh`)
* **`gpio`** - GPIO pin(use kernel numbering) to switch harddrive on and off.

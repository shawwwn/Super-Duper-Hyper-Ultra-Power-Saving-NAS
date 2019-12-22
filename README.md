# Super Duper Hyper Ultra Power Saving NAS Driver Module

This is the driver module for my super power-saving NAS.\
Essentially, the driver monitors access to a given mountpoint(directory), and powers up the underlying harddrive upon access.\
If that harddrive has been idle for too long, it will be unmounted and powerdown in order to save power.

Developed on Kernel 4.9+ for **ARM64**, should also work on **ARM**.\
Has not been make compatible with **X86(_64)**, yet.


## Installation:
**Prerequisites:**
```bash
apt-get install mountpoint build-essential linux-headers-`uname -r`
```

**Auto Install:** (systemd version)
```bash
make
make install
# edit /etc/modprobe.d/nas_pm.conf
make uninstall
```
Module will be installed as a system module to run at every boot.

**Manual Install:**
```bash
make
insmod nas_pm.ko uuid="xxxxxx" mountpoint="/path/to/mountpoint/directory" gpio=123

rmmod nas_pm.ko
```

## Module Parameters:
* **`uuid`** - UUID of disk to be mounted.
* **`mountpoint`** - Path to the directory for which the disk will be mounted at.
* **`mountscript`** - Path to the shell script that mounts our disk. (optional)
* **`gpio`** - GPIO pin number that is able to switch the disk on and off.

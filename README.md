# Super Duper Hyper Ultra Power Saving NAS Driver Module

This is the driver module for my super power-saving NAS.\
Essentially, this module monitors accesses to a given directory(mountpoint), and powers up backing harddrive upon access.\
If the backing harddrive has been idle for too long, it will be unmounted and power down in order to save power.

Developed on Kernel 4.9+ for **ARM64**, should also work on **ARM**.\
Has not been make compatible with **X86(_64)**, yet.


## Installation
**Prerequisites:**
```bash
apt-get install mountpoint build-essential linux-headers-`uname -r`
```

**Auto Install:** (for systemd)
```bash
make
make install
# edit module parameters in /etc/modprobe.d/nas_pm.conf
make uninstall
```
Module will be installed as a system module to run at every boot.

**Manual Install:**
```bash
make
insmod nas_pm.ko uuid="xxxxxx" mountpoint="/path/to/mountpoint/directory" gpio=123

rmmod nas_pm.ko
```

After reboot, harddrive will be automatically powered on whenever mountpoint directory is accessed, and powered off when idle timeout.

**Manual On/Off:**
```bash
# power on harddrive
echo on >/sys/module/nas_pm/parameters/control
cat /sys/module/nas_pm/parameters # 'on'

# power off harddrive
echo off >/sys/module/nas_pm/parameters/control
cat /sys/module/nas_pm/parameters # 'off'
```

## Module Parameters:
* **`uuid`** - UUID of the disk to be mounted.
* **`mountpoint`** - Path to the directory for which the disk will be mounted at.
* **`mountscript`** - Path to the shell script that mounts our disk. (default: `/etc/nas_pm/mountscript.sh`)
* **`gpio`** - GPIO pin number to switch the disk on and off.

# Super Duper Hyper Ultra Power Saving NAS Driver Module

This kernel module does mainly two things:

1. Monitor a given directory(mountpoint) for user accesses.\
    Whenever mountpoint gets accessed, power up a nas harddrive and mount it onto the mountpoint.\
    The whole process is done transparently by hooking `sys_openat()`.\
    The actually mounting process is delegated to a shell script in userspace.

2. Timeout an idle harddrive for it to be unmount, eject, and power down.

Developed for Kernel 4.9+ on **ARM64**, should also work on **ARM**.\
Has not been make compatible with **X86(_64)**, and probably never will.


## HOWTOs:
Install:
```bash
apt-get install build-essential linux-headers-`uname -r`
make
insmod nas_pm.ko
```

Uninstall:
```bash
rmmod nas_pm
```

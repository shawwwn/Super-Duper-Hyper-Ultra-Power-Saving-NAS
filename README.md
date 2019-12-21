# Super Duper Hyper Ultra Power Saving NAS Driver Module

This is the driver module for my super power-saving NAS.\
Essentially, the driver monitors access to a given mountpoint(directory), and power-ups the underlying harddrive upon access.\
If that harddrive has been idle for too long, it will be unmounted and powerdown in order to save power.

Developed on Kernel 4.9+ for **ARM64**, should also work on **ARM**.\
Has not been make compatible with **X86(_64)**, yet.


## HOWTOs:
**Install:**
```bash
apt-get install mountpoint build-essential linux-headers-`uname -r`
make
make install
```

**Uninstall:**
```bash
make uninstall
```

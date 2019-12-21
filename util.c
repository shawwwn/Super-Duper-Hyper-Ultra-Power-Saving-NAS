#include <linux/kernel.h>
#include <linux/mount.h>
#include <linux/kobject.h>
#include <linux/blkdev.h>
#include <linux/kallsyms.h>
#include <linux/usb.h>
#include <linux/kmod.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/kallsyms.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <linux/preempt.h>
#include <linux/semaphore.h>
#include "util.h"
#include "thread.h" // nas_timer_ticks
#include "gpio.h"

#define SILENT_SEC 5

int nas_try_poweron(void) {
	static DEFINE_SEMAPHORE(sem);
	static unsigned long volatile last_jf;
	unsigned long jf;
	int ret;

	if (in_interrupt())
		return EINTR;

	// early task rejection (optional)
	jf = jiffies - last_jf;
	if (jf < SILENT_SEC*HZ)
		return 99; // during silent period

	// enter critical section
	if ((ret = down_interruptible(&sem)) != 0) {
		return -ret; // EINTR, ETIME
	}

	// reject task if called within @SILENT_SEC seconds since last call
	jf = jiffies - last_jf;
	if (jf < SILENT_SEC*HZ) {
		ret = 99;
		goto out;
	}
	last_jf = jiffies;

	// TODO: check if already mounted

	// refresh ticks
	nas_timer_ticks = TIMER_TICKS;

	// pullup gpio
	if (get_gpio(gpio_pin) == 0)
		set_gpio(gpio_pin, 1);

	// invoke mount script
	ret = call_mountscript();
	if (ret == 0) {
		printk("mount successful\n");
	} else if (ret == 1) {
		printk("already mounted\n");
	} else {
		printk("mount failed (%d)\n", ret);
		set_gpio(gpio_pin, 0);
	}

out:
	// leave critical section
	up(&sem);
	return 0;
}

inline int nas_path_match_with_str(const char* pathname, const char* str) {
	if (strncmp(str, pathname, PATH_MAX) == 0) {
		printk(KERN_INFO "openat = %s\n", str);
		return 1;
	}
	return 0;
}

inline int nas_path_match_with_fd(const char* pathname, int fd) {
	struct file* f;
	char buf[PATH_MAX];
	char* pwd;

	if (fd < 0)
		return 0;

	f = fget_raw(fd); // hold
	if (f == NULL)
		goto false;

	// pwd = dentry_path_raw(current->fs->pwd.dentry, buf, PATH_MAX);
	pwd = d_absolute_path(&f->f_path, buf, PATH_MAX); // get full path
	if (pwd == NULL)
		goto false;

	// printk(KERN_INFO "openat = %s\n", pwd);

	return nas_path_match_with_str(pathname, pwd);

false:
	fput(f); //release
	return 0;
}

/**
 * Umount @pathname.
 * Wrapper for sys_unmount().
 */
int nas_unmount(const char* pathname)
{
	long ret;
	asmlinkage long (*sys_umount)(char *pathname, int flags);
	mm_segment_t old_fs;

	sys_umount = (typeof(sys_umount))kallsyms_lookup_name("sys_umount");

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	ret = (*sys_umount)((char *)pathname, 0);

	set_fs(old_fs);
	return ret;
}

/**
 * Return 0 if @pathname is a mountpoint and the underlying block 
 * device is not busy.
 */
int nas_check_mnt(const char *pathname)
{
	int ret = 0;
	struct path fpath;
	static char buf[PATH_MAX];
	char* ppath; // ptr to buf

	if (kern_path(pathname, LOOKUP_RCU, &fpath) != 0)
		return ENOENT; // pathname not exist

	// printk("1 dentry.ref=%u\n", fpath.dentry->d_lockref.count);

	if(!S_ISDIR(fpath.dentry->d_inode->i_mode)) {
		ret = ENOTBLK; // not a directory/mountpoint
		goto out;
	}

	ppath = dentry_path_raw(fpath.dentry, buf, PATH_MAX);
	if (ppath[0]!='/' || ppath[1]!=0) {
		ret = ENOTBLK; // not a mountpoint
		goto out;
	}

	if (!may_umount(fpath.mnt)) {
		ret = EBUSY; // device probly busy
		goto out;
	}

out:
	path_put(&fpath);
	// printk("2 dentry.ref=%u\n", fpath.dentry->d_lockref.count);
	return ret;
}

/**
 * Wrapper for vfs_stat, return file mode.
 */
int file_exist(char* pathname)
{
	struct path fpath;
	if (kern_path(pathname, 0, &fpath) != 0)
		return 0;
	path_put(&fpath);
	return 1;
}

/**
 * Call a predefined shell script in usermode to mount disk.
 */
int call_mountscript(void)
{
	int ret;
	char gpio_str[32];
	char * envp[] = { "HOME=/", NULL };
	char * argv[] = { "/bin/sh", mntscript, uuid, mntpt, NULL, NULL };

	snprintf(gpio_str, sizeof(gpio_str), "%d", gpio_pin);
	argv[4] = gpio_str;

	ret = ENOENT;
	if (file_exist(mntscript)) {
		ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
		ret >>= 8;
	} else
		printk(KERN_ERR "mountscript not found (%s)\n", mntscript);
	return ret;
}

/**
 * Find and open the underlying block device to the mountpoint's @pathname.
 * Assume @pathname is already a mountpoint.
 * On success, the returned block_device has reference count of one.
 * Need to be followed by `blkdev_put(bdev, 0)`
 */
struct block_device *blkdev_get_by_mountpoint(char* pathname)
{
	int ret;
	dev_t devt;
	struct block_device* bdev;
	static char buf[PATH_MAX];
	char* ppath; // ptr to buf
	struct path fpath;

	ret = kern_path(pathname, LOOKUP_RCU, &fpath);
	if (ret)
		return ERR_PTR(-ret);

	if(!S_ISDIR(fpath.dentry->d_inode->i_mode)) {
		ret = ENOTBLK; // not a directory/mountpoint
		goto err;
	}

	ppath = dentry_path_raw(fpath.dentry, buf, PATH_MAX);
	if (ppath[0]!='/' || ppath[1]!=0) {
		ret = ENOTBLK; // not a mountpoint
		goto err;
	}

	if (fpath.mnt->mnt_sb == NULL) {
		ret = ENOTBLK; // not a mountpoint
		goto err;
	}
	devt = fpath.mnt->mnt_sb->s_dev;
	path_put(&fpath);

	bdev = blkdev_get_by_dev(devt, 0, NULL); // find and open block device
	if (IS_ERR(bdev))
		return bdev;

	// char diskname[BDEVNAME_SIZE];
	// bdevname(bdev, diskname);
	// printk("diskname=%s\n", diskname);

	// int partno = bdev->bd_part->partno;
	// printk("partno=%d\n", partno);

	return bdev;

err:
	path_put(&fpath);
	return ERR_PTR(-ret);
}

/**
 * From block device, traverse its parent list, stop at the first 'usb_device'
 * typed device, return it.
 * Returned device has its ref increased by 1 (is being held).
 */
struct device *get_first_usb_device(struct block_device *bdev)
{
	struct device *dev, *dev2;

	/* Device Type Hierarchy:
	 * partition->disk
	 * ->scsi_device->scsi_target->scsi_host
	 * ->usb_interface->usb_device(x3)
	 * ->(null)
	 */
	bdev = bdgrab(bdev); // hold bdev
	dev = get_device(bdev->bd_part->__dev.parent); // starts from 'disk', hold dev
	bdput(bdev); // release bdev

	while (
		((dev2 = get_device(dev->parent)) != NULL) && // hold new dev
		(dev2->type != NULL) &&
		(dev2->type->name != NULL)
	) {
		put_device(dev); // release old dev
		dev = dev2;

		if (strcmp(dev->type->name, "usb_device") == 0)
			return dev; // first found
	}

	// relase all devs
	put_device(dev);
	put_device(dev2);

	// not found
	return NULL;
}

/**
 * Remove usb via sysfs.
 * Assuming @dev is type 'usb_device'.
 */
static int vfs_remove_usb_device(struct device* dev)
{
	int ret;
	const char *devname = dev_name(dev);
	char pathname[512] = "/sys/bus/usb/devices/";
	struct file *f;
	loff_t pos;
	mm_segment_t old_fs = get_fs();

	strcat(pathname, devname);
	strcat(pathname, "/remove");
	// printk("pathname = %s\n", pathname);

	set_fs(KERNEL_DS);
	f = filp_open(pathname, O_WRONLY, 0);
	if (IS_ERR(f)) {
		ret = PTR_ERR(f);
		goto out;
	}

	pos = 0;
	ret = vfs_write(f, "1", 1, &pos);

out:
	filp_close(f, NULL);
	set_fs(old_fs);
	return ret;
}

/**
 * Remove usb via kernel.
 * Fallback method when /sys is not mounted.
 * Assuming @dev is type 'usb_device'.
 */
static int kern_remove_usb_device(struct  device* dev)
{
	int (*usb_remove_device)(struct usb_device *udev) = \
		(typeof(usb_remove_device))kallsyms_lookup_name("usb_remove_device");

	int ret = 0;
	struct usb_device *udev = to_usb_device(dev);
	usb_lock_device(udev);
	if (udev->state != USB_STATE_NOTATTACHED) {
		usb_set_configuration(udev, -1); // unconfigure
		ret = usb_remove_device(udev);
	}
	usb_unlock_device(udev);
	return ret;
}

/**
 * Remove usb_device via sysfs first;
 * if /sys not mounted, fallback to remove via kernel.
 */
int remove_usb_device(struct device* dev)
{
	// via sysfs
	if (vfs_remove_usb_device(dev) == 0)
		return 0;

	// via kernel
	return kern_remove_usb_device(dev);
}
EXPORT_SYMBOL(remove_usb_device);

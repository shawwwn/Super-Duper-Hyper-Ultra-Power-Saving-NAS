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
#include <linux/preempt.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/fs_struct.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include "util.h"
#include "thread.h" // nas_timer_ticks
#include "gpio.h"
#include "patch.h"

#include <linux/module.h>
#include <linux/unistd.h>

#define SILENT_SEC 10

/**
 * Stop at the first null character, returns success.
 */
static inline int strncmp2(const char *cs, const char *ct, size_t count)
{
	unsigned char c1, c2;
	while (count) {
		c1 = *cs++;
		c2 = *ct++;
		if (!c1)
			break;
		if (c1 != c2)
			return c1 < c2 ? -1 : 1;
		count--;
	}
	return 0;
}

void _set_fs_pwd(struct fs_struct *fs, const struct path *path)
{
	struct path old_pwd;

	path_get(path);
	spin_lock(&fs->lock);
	write_seqcount_begin(&fs->seq);
	old_pwd = fs->pwd;
	fs->pwd = *path;
	write_seqcount_end(&fs->seq);
	spin_unlock(&fs->lock);

	if (old_pwd.dentry)
		path_put(&old_pwd);
}

int nas_try_poweron(void) {
	static DEFINE_SEMAPHORE(sem);
	static unsigned long volatile last_jf;
	int ret;
	struct path fpath;

	if (in_interrupt())
		return EINTR;

	// check if already mounted
	if (kern_path(mntpt, 0, &fpath) == 0) {
		if IS_ROOT(fpath.dentry) {
			path_put(&fpath);
			printk(KERN_INFO "already mounted\n");
			nas_timer_ticks = TIMER_TICKS; // refresh ticks
			return 88;
		}
		path_put(&fpath);
	}

	// enter critical section
	if ((ret = down_interruptible(&sem)) != 0) {
		return -ret; // EINTR, ETIME
	}

	// reject task if called within @SILENT_SEC seconds since last call
	if (jiffies-last_jf < SILENT_SEC*HZ) {
		ret = 99;
		goto out;
	}
	last_jf = jiffies;

	// suspend nas monitor thread first
	kthread_park(nas_thread);

	// refresh ticks
	nas_timer_ticks = TIMER_TICKS;

	// pullup gpio
	if (get_gpio(gpio_pin) == 0)
		set_gpio(gpio_pin, 1);

	// invoke mount script
	ret = call_mountscript();
	if (ret == 0) {
		printk(KERN_INFO "mount successful\n");
	} else if (ret == 1) {
		printk(KERN_INFO "already mounted\n");
		ret = 88;
	} else {
		printk(KERN_ERR "mount failed (%d), reset gpio\n", ret);
		set_gpio(gpio_pin, 0);
	}

	// resume monitor thread
	kthread_unpark(nas_thread);

out:
	// leave critical section
	up(&sem);
	return ret;
}

inline int nas_path_match_with_str(const char* pathname, const char* str) {
	if (strncmp2(pathname, str, PATH_MAX) == 0) { // PATH_MAX
		// printk(KERN_INFO "openat=%s\n", str);
		return 1;
	}
	return 0;
}

inline int nas_path_match_with_fd(const char* pathname, int fd) {
	static char buf[PATH_MAX];
	char* ppath;

	ppath = get_fd_pathname(fd, buf);
	if (ppath == NULL)
		return 0;

	return nas_path_match_with_str(pathname, ppath);
}

static int kern_umount(const char *name, int flags) {
	int lookup_flags = LOOKUP_MOUNTPOINT;
	struct path path;
	int ret;
	asmlinkage long (*path_umount)(struct path *path, int flags);

	// basic validity checks done first
	if (flags & ~(MNT_FORCE | MNT_DETACH | MNT_EXPIRE | UMOUNT_NOFOLLOW))
		return -EINVAL;

	if (!(flags & UMOUNT_NOFOLLOW))
		lookup_flags |= LOOKUP_FOLLOW;
	ret = kern_path(name, lookup_flags, &path);
	if (ret != 0)
		return ret;

	path_umount = (typeof(path_umount))my_kallsyms_lookup_name("path_umount");
	return path_umount(&path, flags);
}

/**
 * Umount @pathname.
 */
int nas_unmount(const char* pathname) {
	return kern_umount(pathname, 0);
}

/**
 * Return 0 if @pathname is a mountpoint and the underlying block 
 * device is not busy.
 */
int nas_check_mnt(const char *pathname)
{
	int ret = 0;
	struct path fpath;

	if (kern_path(pathname, LOOKUP_RCU, &fpath) != 0)
		return ENOENT; // pathname not exist

	// printk("1 dentry.ref=%u\n", fpath.dentry->d_lockref.count);

	if(!S_ISDIR(fpath.dentry->d_inode->i_mode)) {
		ret = ENOTBLK; // not a directory/mountpoint
		goto out;
	}

	if (!IS_ROOT(fpath.dentry)) {
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
	// bdev = bdgrab(bdev); // hold bdev
	// dev = get_device(bdev->bd_part->__dev.parent); // starts from 'disk', hold dev
	// bdput(bdev); // release bdev

	ihold(bdev->bd_inode);
	dev = get_device(bdev->bd_device.parent); // starts from 'disk', hold dev
	iput(bdev->bd_inode);

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
	// mm_segment_t old_fs = get_fs();

	strcat(pathname, devname);
	strcat(pathname, "/remove");
	// printk("pathname = %s\n", pathname);

	// set_fs(KERNEL_DS);
	f = filp_open(pathname, O_WRONLY, 0);
	if (IS_ERR(f)) {
		ret = PTR_ERR(f);
		goto out;
	}

	pos = 0;
	ret = vfs_write(f, "1", 1, &pos);

out:
	filp_close(f, NULL);
	// set_fs(old_fs);
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
		(typeof(usb_remove_device))my_kallsyms_lookup_name("usb_remove_device");

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

/**
 * Get absolute pathname from given @fd
 */
inline char* get_fd_pathname(int fd, char* buf)
{
	char* ppath = NULL;
	struct file* f;

	if (fd < 0)
		return NULL;

	f = fget_raw(fd); // hold
	if (f == NULL)
		goto out;

	ppath = d_path(&f->f_path, buf, PATH_MAX); // get full path

out:
	fput(f); //release
	return ppath;
}
EXPORT_SYMBOL(get_fd_pathname);

/**
 * Get pathname of current directory (pwd)
 */
inline char* get_pwd_pathname(char* buf, int buflen)
{
	struct path fpath;
	char *bufp;
	get_fs_pwd(current->fs, &fpath);
	bufp = d_path(&fpath, buf, buflen); // absolute pwd
	path_put(&fpath);
	return bufp;
}
EXPORT_SYMBOL(get_pwd_pathname);

/**
 * Reset current pwd so it reflects the most recent mount on its path.
 */
inline int reset_pwd(void) {
	struct path fpath;
	static char buf[PATH_MAX];
	char* pwd;

	get_fs_pwd(current->fs, &fpath);
	pwd = d_path(&fpath, buf, PATH_MAX);
	path_put(&fpath);

	kern_path(pwd, 0, &fpath);
	_set_fs_pwd(current->fs, &fpath);
	path_put(&fpath);

	printk(KERN_INFO "reset_pwd=%s\n", pwd);

	return 0;
}

/**
 * Is @fd on top of current pwd's mount?
 */
inline int fd_on_current_mnt(int fd) {
	struct file* f;
	struct path fs_pwd;
	int result = 0;

	if (fd < 0)
		return 0;

	f = fget_raw(fd); // hold
	if (f == NULL)
		goto out;

	get_fs_pwd(current->fs, &fs_pwd);
	result = (fs_pwd.mnt == f->f_path.mnt);
	path_put(&fs_pwd);

out:
	fput(f); //release
	return result;
}

/**
 * Is current pwd mounted?
 */
inline int is_pwd_mounted(void) {
	int result;
	struct path root, pwd;
	struct fs_struct *fs = current->fs;
	spin_lock(&fs->lock);
	root = fs->root;
	path_get(&root);
	pwd = fs->pwd;
	path_get(&pwd);
	spin_unlock(&fs->lock);

	result = (root.mnt != pwd.mnt);
	path_put(&root);
	path_put(&pwd);
	return result;
}

/**
 * Resolve a base path and a
 * Return NULL if path string exhausted.
 */
#define TK_NONE 0 // token is empty string (usually indicates invalid syntax)
#define TK_NEXT 1 // token is a directory name
#define TK_CURR 2 // token is "."
#define TK_PREV 3 // token is ".."
char *join_path(const char *base, const char *relative, char *resolved)
{
	/**
	 * Find next token in a path string, delimit by '/'.
	 * Return NULL if path string exhausted.
	 */
	inline const char* next_token(const char *path, char *token)
	{
		while ((*path != '\0') && (*path != '/'))
			*(token++) = *(path++);
		*token = '\0';
		if (*path == '/')
			path++;
		return (*path == '\0') ? NULL : path;
	}

	/**
	 * Parse token into different types.
	 */
	inline int categorize_token(char *token)
	{
		if (*token == '\0')
			return TK_NONE;
		else if (*token == '.') {
			char* next = token+1;
			if (*next == '\0')
				return TK_CURR;
			else if ((*next == '.') && (*(next+1) == '\0'))
				return TK_PREV;
		}
		return TK_NEXT;
	}

	/*
	 * Like strcpy(),
	 * but return the end of destination string.
	 */
	inline char *strcpy2(char *dest, const char *src)
	{
		while (*src != '\0')
			*dest++ = *src++;
		*dest = '\0';
		return dest;
	}

	const char *rel_path = relative;
	char token[NAME_MAX];
	char *cursor; // cursor in resolved

	// prepare resolved
	if (resolved == base)
		cursor = resolved + strlen(base) - 1;
	else
		cursor = strcpy2(resolved, base) - 1;
	if (*cursor == '/')
		*cursor = '\0';
	else
		++cursor;

	// push/pop token to resolved
	while (rel_path != NULL) {
		rel_path = next_token(rel_path, token);
		switch (categorize_token(token)) {
			case TK_PREV:
				// pop
				while (*cursor != '/') {
					if (cursor < resolved) // behind root
						return NULL;
					--cursor;
				}
				*cursor = '\0';
				break;
			case TK_CURR:
				// do nothing
				break;
			case TK_NEXT:
				// push
				*cursor++ = '/';
				cursor = strcpy2(cursor, token);
				break;
			case TK_NONE:
			default:
				return NULL; // unknown token
		}
	}

	// root directory
	if (resolved == cursor) {
		*cursor++ = '/';
		*cursor = '\0';
	}

	return resolved;
}



#include "util.h"
#include "thread.h" // nas_timer_ticks

char* mnt_path = "";

int nas_poweron(void) {
	printk("nas '%s' is powering on ...\n", mnt_path);
	nas_timer_ticks = TIMER_TICKS;
	return 0;
}

inline int nas_path_match_with_str(const char* pathname, const char* str) {
	if (strstr(str, "journal") == NULL)
		printk(KERN_INFO "openat = %s\n", str);
	return (strncmp(str, pathname, PATH_MAX) == 0);
}

inline int nas_path_match_with_fd(const char* pathname, int fd) {
	struct file* f;
	char buf[PATH_MAX];
	char* pwd;

	if (fd < 0)
		return 0;

	f = fget_raw(fd);
	if (f == NULL)
		return 0;

	// pwd = dentry_path_raw(current->fs->pwd.dentry, buf, PATH_MAX);
	pwd = d_absolute_path(&f->f_path, buf, PATH_MAX); // get full path
	if (pwd == NULL)
		return 0;

	// printk(KERN_INFO "openat = %s\n", pwd);

	return nas_path_match_with_str(pathname, pwd);
}

/*
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

/*
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
		ret = ENOTBLK; // not a mountpoint
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

/*
 * Call a predefined shell script in usermode to mount disk.
 */
int call_mountscript(void)
{
	char * envp[] = { "HOME=/", NULL };
	char * argv[] = { "/bin/sh", "/root/cdir.sh", NULL };
	int ret;
	
	ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	ret >>= 8;
	printk("mountscript = %d\n", ret);
	return ret;
}

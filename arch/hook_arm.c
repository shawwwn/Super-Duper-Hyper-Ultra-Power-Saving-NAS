/* codebase for both hook_arm.h and hook_arm64.h */
#include "../page.h"

// TODO: pass in as params
static char* mnt_path;
static size_t mnt_path_len;







// TODO: move below to util.o

static int turn_on_nas(void) {
	return 0;
}

static inline int nas_path_match_with_str(const char* path) {
	if (strstr(path, "journal") == NULL)
		printk(KERN_INFO "openat = %s\n", path);
	return (strncmp(mnt_path, path, mnt_path_len) == 0);
}

static inline int nas_path_match_with_fd(int fd) {
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

	return nas_path_match_with_str(pwd);
}







/*
 * Hook routine
 */
asmlinkage long (*org_sys_openat)(int dfd, const char __user *filename, int flags, umode_t mode);
asmlinkage long my_sys_openat(int dfd, const char __user *filename, int flags, umode_t mode)
{
	if (*filename != '/') {
		// match after running openat()
		int fd = org_sys_openat(dfd, filename, flags, mode);
		if (nas_path_match_with_fd(fd))
			turn_on_nas();
		return fd;
	} else {
		// match before running openat()
		if (nas_path_match_with_str(filename))
			turn_on_nas();
	}

	return org_sys_openat(dfd, filename, flags, mode);
}

/*
 * Save init_mm to be used later
 */
static struct mm_struct* my_init_mm = NULL;
static inline int get_init_mm(void)
{
	if (my_init_mm == NULL)
		my_init_mm = (struct mm_struct*)kallsyms_lookup_name("init_mm");
	if (my_init_mm == NULL)
		return -1;
	return 0;
}

/*
 * Save sys_call_table to be used later
 */
static void **my_sys_call_table = NULL;
static inline int get_sys_call_table_arm(void)
{
	if (my_sys_call_table == NULL)
		my_sys_call_table = (void *)kallsyms_lookup_name("sys_call_table");
	if (my_sys_call_table == NULL)
		return -1;
	return 0;
}

/*
 * Hook sys_openat() for both ARM and ARM64
 */
static inline void hook_sys_call_table_arm(void)
{
	pte_t* pte;
	printk("Hooking sys_call_table\n");

	printk(KERN_INFO "sys_call_table: %p\n", my_sys_call_table);
	org_sys_openat = (void *)(my_sys_call_table[__NR_openat]);
	printk(KERN_INFO "sys_openat: %p\n", org_sys_openat);

	pte = get_pte(my_init_mm, (unsigned long)&my_sys_call_table[__NR_openat]);
	pte_enable_write(pte);
	my_sys_call_table[__NR_openat] = &my_sys_openat;
	printk(KERN_INFO "replace sys_openat %p => %p\n", org_sys_openat, my_sys_openat);
	pte_disable_write(pte);
}

static inline void unhook_sys_call_table_arm(void)
{
	pte_t* pte = get_pte(my_init_mm, (unsigned long)&my_sys_call_table[__NR_openat]);
	pte_enable_write(pte);
	my_sys_call_table[__NR_openat] = org_sys_openat;
	pte_disable_write(pte);
}

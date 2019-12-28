/* codebase for both hook_arm.h and hook_arm64.h */
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/fdtable.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include "../page.h"
#include "../util.h"

/*
 * Hook routine
 */
asmlinkage long (*org_sys_openat)(int dfd, const char __user *filename, int flags, umode_t mode);
asmlinkage long my_sys_openat(int dfd, const char __user *filename, int flags, umode_t mode)
{
	if (*filename != '/') {
		// relative path
		char buf[PATH_MAX*2];
		char* abs_path;
		abs_path = get_pwd_pathname(buf, PATH_MAX);
		if (abs_path == NULL)
			goto out;
		if (join_path(abs_path, filename, abs_path) == NULL)
			goto out;

		if (nas_path_match_with_str(mntpt, abs_path)) {
			if (nas_try_poweron() == 0 || \
				!is_pwd_mounted())
				reset_pwd();
		}

	} else {
		// absolute path
		if (nas_path_match_with_str(mntpt, filename))
			nas_try_poweron();
	}

out:
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
	printk(KERN_INFO "init_mm: %p\n", my_init_mm);
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
	printk(KERN_INFO "sys_call_table: %p\n", my_sys_call_table);
	return 0;
}

/*
 * Hook sys_openat() for both ARM and ARM64
 */
static inline void hook_sys_call_table_arm(void)
{
	pte_t* pte;

	org_sys_openat = (void *)(my_sys_call_table[__NR_openat]);

	pte = get_pte(my_init_mm, (unsigned long)&my_sys_call_table[__NR_openat]);
	pte_enable_write(pte);
	my_sys_call_table[__NR_openat] = &my_sys_openat;
	printk(KERN_INFO "sys_openat: %p => %p\n", org_sys_openat, my_sys_openat);
	pte_disable_write(pte);
}

static inline void unhook_sys_call_table_arm(void)
{
	pte_t* pte = get_pte(my_init_mm, (unsigned long)&my_sys_call_table[__NR_openat]);
	pte_enable_write(pte);
	my_sys_call_table[__NR_openat] = org_sys_openat;
	pte_disable_write(pte);
}

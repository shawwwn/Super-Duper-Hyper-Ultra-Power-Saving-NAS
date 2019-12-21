#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include "hook_arm.c"

/*
 * Hook routine
 */
asmlinkage long (*org_compat_sys_openat)(int dfd, const char __user *filename, int flags, umode_t mode);
asmlinkage long my_compat_sys_openat(int dfd, const char __user *filename, int flags, umode_t mode)
{
	if (*filename != '/') {
		// match after running openat()
		int fd = org_compat_sys_openat(dfd, filename, flags, mode);
		if (nas_path_match_with_fd(mntpt, fd))
			nas_try_poweron();
		return fd;
	} else {
		// match before running openat()
		if (nas_path_match_with_str(mntpt, filename)) 
			nas_try_poweron();
	}

	return org_compat_sys_openat(dfd, filename, flags, mode);
}

/*
 * Save compat_sys_call_table to be used later
 */
static void **my_compat_sys_call_table = NULL;
static inline int get_sys_call_table_arm64(void)
{
	if (my_compat_sys_call_table == NULL)
		my_compat_sys_call_table = (void *)kallsyms_lookup_name("compat_sys_call_table");
	if (my_compat_sys_call_table == NULL)
		return -1;
	return 0;
}

/*
 * Hook sys_compat_openat() for ARM64 only
 */
#define __NR_compat_openat 322 // for arm64
static inline void hook_sys_call_table_arm64(void)
{
	pte_t* pte;
	printk("Hooking compat_sys_call_table\n");

	printk(KERN_INFO "compat_sys_call_table: %p\n", my_compat_sys_call_table);
	org_compat_sys_openat=(void*)(my_compat_sys_call_table[__NR_compat_openat]);
	printk(KERN_INFO "compat_sys_openat: %p\n", org_compat_sys_openat);

	pte = get_pte(my_init_mm, (unsigned long)&my_compat_sys_call_table[__NR_compat_openat]);
	pte_enable_write(pte);
	my_compat_sys_call_table[__NR_compat_openat] = my_compat_sys_openat;
	printk(KERN_INFO "replace compat_sys_openat %p => %p\n", org_compat_sys_openat, my_compat_sys_openat);
	pte_disable_write(pte);
}

static inline void unhook_sys_call_table_arm64(void)
{
	pte_t* pte = get_pte(my_init_mm, (unsigned long)&my_compat_sys_call_table[__NR_compat_openat]);
	pte_enable_write(pte);
	my_compat_sys_call_table[__NR_compat_openat] = org_compat_sys_openat;
	pte_disable_write(pte);
}





/*
 * Export functions
 * to be used in top level functions
 */
static int install_hook(void)
{
	printk("install ARM64 hooks\n");

	if (get_init_mm() != 0) {
		printk(KERN_ERR "Couldn't file init_mm.\n");
		return 1;
	}

	if (get_sys_call_table_arm() != 0) {
		printk(KERN_ERR "Couldn't find sys_call_table.\n");
		return 2;
	}

	if (get_sys_call_table_arm64() != 0) {
		printk(KERN_ERR "Couldn't find compat_sys_call_table.\n");
		return 3;
	}

	hook_sys_call_table_arm();
	hook_sys_call_table_arm64();
	return 0;
}

static void uninstall_hook(void)
{
	unhook_sys_call_table_arm();
	unhook_sys_call_table_arm64();
}

/* codebase for both hook_arm.h and hook_arm64.h */
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include "../page.h"
#include "../util.h"
#include "linux/delay.h"

/*
 * Hook routine
 */
asmlinkage long (*org_sys_openat)(int dfd, const char __user *filename, int flags, umode_t mode);
asmlinkage long my_sys_openat(int dfd, const char __user *filename, int flags, umode_t mode)
{
	if (*filename != '/') {
		// match after running openat()
		int fd = org_sys_openat(dfd, filename, flags, mode);
		if (nas_path_match_with_fd(mntpt, fd))
			nas_try_poweron();
		return fd;
	} else {
		// match before running openat()
		if (nas_path_match_with_str(mntpt, filename))
			nas_try_poweron();
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

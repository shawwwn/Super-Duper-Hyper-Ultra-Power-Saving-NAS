#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include "hook_arm.c"

/*
 * Hook routine
 */
asmlinkage long (*org_compat_sys_openat)(const struct pt_regs *regs);
asmlinkage long my_compat_sys_openat(const struct pt_regs *regs) {
	static char kfilename[PATH_MAX];

	// int dfd = (int)regs->regs[0];
	char __user *filename = (char*)regs->regs[1];
	// int flags = (int)regs->regs[2];
	// int mode = (int)regs->regs[3];

	if (!filename)
		goto out;

	// optional, for safety reason
	if (strncpy_from_user(kfilename, filename, PATH_MAX) == -EFAULT) {
		goto out;
	}

	if (*kfilename != '/') {
		// relative path
		static char buf[PATH_MAX*2];
		char* abs_path;
		abs_path = get_pwd_pathname(buf, PATH_MAX);
		if (abs_path == NULL)
			goto out;
		if (join_path(abs_path, kfilename, abs_path) == NULL)
			goto out;

		if (nas_path_match_with_str(mntpt, abs_path)) {
			if (nas_try_poweron() == 0 || \
				!is_pwd_mounted())
				reset_pwd();
		}
	} else {
		// absolute path
		if (nas_path_match_with_str(mntpt, kfilename))
			nas_try_poweron();
	}

out:
	return org_compat_sys_openat(regs);
}

/*
 * Save compat_sys_call_table to be used later
 */
static void **my_compat_sys_call_table = NULL;
static inline int get_sys_call_table_arm64(void)
{
	if (my_compat_sys_call_table == NULL)
		my_compat_sys_call_table = (void *)my_kallsyms_lookup_name("compat_sys_call_table");
	if (my_compat_sys_call_table == NULL)
		return -1;
	printk(KERN_INFO "compat_sys_call_table: %px\n", my_compat_sys_call_table);
	return 0;
}

/*
 * Hook sys_compat_openat() for ARM64 only
 */
#define __NR_compat_openat 322 // for arm64
static inline void hook_sys_call_table_arm64(void)
{
	pte_t* pte;

	printk(KERN_INFO "__NR_compat_openat: %u\n", __NR_compat_openat);
	org_compat_sys_openat=(void*)(my_compat_sys_call_table[__NR_compat_openat]);

	pte = get_pte(my_init_mm, (unsigned long)&my_compat_sys_call_table[__NR_compat_openat]);
	if (pte == NULL) {
		printk(KERN_INFO "get_pte() for sys_openat() failed!", (unsigned long)pte);
		return;
	}

	pte_enable_write(pte);
	my_compat_sys_call_table[__NR_compat_openat] = my_compat_sys_openat;
	printk(KERN_INFO "compat_sys_openat: 0x%px => 0x%px\n", org_compat_sys_openat, my_compat_sys_openat);
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
	printk(KERN_INFO "nas_pm: install hooks for ARM64\n");

	if (get_init_mm() != 0) {
		printk(KERN_ERR "Couldn't find init_mm.\n");
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

	preempt_disable();
	hook_sys_call_table_arm();
	hook_sys_call_table_arm64();
	preempt_enable();
	return 0;
}

static void uninstall_hook(void)
{
	preempt_disable();
	unhook_sys_call_table_arm();
	unhook_sys_call_table_arm64();
	preempt_enable();
}

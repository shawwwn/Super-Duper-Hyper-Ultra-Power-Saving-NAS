#include <linux/syscalls.h>
#include <linux/kallsyms.h>
// util.o
#include <linux/file.h>

#include "hook_arm.c"

/*
 * Export functions
 * to be used in top level functions
 */
static int install_hook(void)
{
	printk(KERN_INFO "nas_pm: install hooks for ARM\n");

	if (get_init_mm() != 0) {
		printk(KERN_ERR "Couldn't file init_mm.\n");
		return 1;
	}

	if (get_sys_call_table_arm() != 0) {
		printk(KERN_ERR "Couldn't find sys_call_table.\n");
		return 2;
	}

	preempt_disable();
	hook_sys_call_table_arm();
	preempt_enable();
	return 0
}

static void uninstall_hook(void)
{
	preempt_disable();
	unhook_sys_call_table_arm();
	preempt_enable();
}

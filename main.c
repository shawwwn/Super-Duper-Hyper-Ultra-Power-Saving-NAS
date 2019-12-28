#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/sched.h>
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

	if (!mntpt || !uuid || (gpio_pin<0)) {
		printk(KERN_ERR "Invalid kernel module parameters.");
		return -EINVAL;
	}
	printk(KERN_INFO "nas_pm: mountscript = %s\n", mntscript);
	printk(KERN_INFO "nas_pm: mountpoint = %s\n", mntpt);
	printk(KERN_INFO "nas_pm: uuid = %s\n", uuid);
	printk(KERN_INFO "nas_pm: gpio = %d\n", gpio_pin);

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


static int set_state(const char *val, const struct kernel_param *kp)
{
	int ret;
	char buf[8];
	int last;
	strncpy(buf, val, 7); // last char is '\0'
	last = strlen(buf) - 1;
	if (last <= 0 || last > 6)
		return -EINVAL;
	if (buf[last] == '\n')
		buf[last] = '\0';

	if (strcmp("on", buf) == 0) {
		int state_org = state;
		state = ST_BUSY;
		printk(KERN_DEBUG "nas_pm: powering on disk ...\n");

		if ((ret = nas_try_poweron()) == 0) {
			printk(KERN_DEBUG "nas_pm: disk powered on\n");
			state = ST_ON;
		} else
			state = state_org;
		return ret;

	} else if (strcmp("off", buf) == 0) {
		static asmlinkage unsigned long (*wait_task_inactive)(struct task_struct *, long match_state);
		int state_org = state;
		state = ST_BUSY;

		ret = nas_check_mnt(mntpt);
		if (ret != 0) {
			state = state_org;
			return -ret;
		}

		printk(KERN_DEBUG "nas_pm: powering off disk ...\n");

		// wait for thread's current operation to finish
		// set unmount flag
		// restart thread to unmount disk
		set_current_state(TASK_INTERRUPTIBLE);
		if ((ret = kthread_park(nas_thread)) != 0) { // for current operation to finish
			state = state_org;
			return ret;
		}
		nas_timer_ticks = 0;
		kthread_unpark(nas_thread); // restart thread to unmount disk

		// wait for thread to reach its first sleep()
		#ifdef CONFIG_KALLSYMS
		if (!wait_task_inactive)
			wait_task_inactive = (typeof(wait_task_inactive))kallsyms_lookup_name("wait_task_inactive");
		set_current_state(TASK_INTERRUPTIBLE);
		wait_task_inactive(nas_thread, TASK_INTERRUPTIBLE);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1); // yield?
		#else
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(500));
		#endif

		// wait for thread to finish unmount
		set_current_state(TASK_INTERRUPTIBLE);
		if ((ret = kthread_park(nas_thread)) == 0)
			printk(KERN_DEBUG "nas_pm: disk powered off\n");
		kthread_unpark(nas_thread); // restart thread
		state = ST_OFF;
		return ret;
	}

	// default
	return -EINVAL;
}

static int get_state(char *buffer, const struct kernel_param *kp)
{
	switch (state) {
		case ST_OFF:
			strcpy(buffer, "off");
			break;
		case ST_ON:
			strcpy(buffer, "on");
			break;
		case ST_BUSY:
			strcpy(buffer, "busy");
			break;
		default:
			strcpy(buffer, "error");
	}
	return strlen(buffer);
}

static const struct kernel_param_ops control_ops = {
	.set	= set_state,
	.get	= get_state,
};


state_t state = ST_OFF;
module_param_cb(control, &control_ops, NULL, 0600);

char *uuid = NULL;
module_param_named(uuid, uuid, charp, 0600);
MODULE_PARM_DESC(uuid, "UUID of the disk to be mounted.");

char* mntpt = NULL;
module_param_named(mountpoint, mntpt, charp, 0600);
MODULE_PARM_DESC(uuid, "Target directory to be mounted at.");

char* mntscript = "/etc/nas_pm/mountscript.sh";
module_param_named(mountscript, mntscript, charp, 0600);
MODULE_PARM_DESC(mountscript, "Which script to execute when mounting a directory. Default: /etc/nas_pm/mountscript.sh");

int gpio_pin = -1;
module_param_named(gpio, gpio_pin, int, 0600);
MODULE_PARM_DESC(gpio, "GPIO pin # to power on disk,");

MODULE_AUTHOR("Shawwwn");
MODULE_INFO(email, "shawwwn1@gmail.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Super Duper Hyper Ultra Power Saving NAS Power Management Driver");

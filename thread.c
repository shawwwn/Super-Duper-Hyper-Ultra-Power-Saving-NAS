#include "thread.h"
#include "util.h"
#include "gpio.h"

int nas_timer_ticks = TIMER_TICKS;

static struct task_struct *nas_thread = NULL;

/*
 * Periodically check if disk can be turned off.
 */
static int thread_func(void *data) {

	while (!kthread_should_stop()) {
		printk("ticks = %d\n", nas_timer_ticks);
		/* ticks  < 0 : inactive
		 *       == 0 : just expired
		 *        > 0 : active
		 */

		// Check mount point can umount
		if (nas_timer_ticks >= 0) {
			int ret;
			ret = nas_check_mnt(mnt_path);
			if (ret == EBUSY) {
				// hdd is busy
				printk(KERN_INFO "device busy\n");
				nas_timer_ticks = TIMER_TICKS; // reset ticks
				goto restart;

			} else if (ret==ENOENT || ret==ENOTBLK) {
				// hdd disappeared
				printk(KERN_INFO "disk/mount not found\n");
				nas_timer_ticks = -1;
				goto restart;

			} else if (ret != 0) {
				printk(KERN_ERR "unknow error(%d)\n", ret);
			}
		} else
			goto restart;

		// Check timer expiration
		if (nas_timer_ticks == 0) {
			printk("timer expired\n");

			// umount
			printk(KERN_INFO "unmount(%s)\n", mnt_path);
			if (nas_unmount(mnt_path) != 0) {
				printk(KERN_ERR "umount() failed, retry\n");
				goto restart;
			}

			// pulldown gpio
			kthread_ssleep(1);
			printk(KERN_INFO "pulldown gpio(%d)\n", GPIO_PIN);
			set_gpio(GPIO_PIN, 0);
			// TODO: pulldown gpio
		}

		// tick-tock
		nas_timer_ticks--;

	restart:
		kthread_ssleep(TIMER_INTERVAL);
	}

	return 0;
}

/*
 * Start background monitoring for nas disk.
 * Set timeout for idle disk to be turned down.
 */
int start_nas_mon(void) {
	char thread_name[8]="nas_mon";
	printk("start thread '%s'\n", thread_name);
	nas_thread = kthread_run(thread_func, NULL, thread_name);
	if (nas_thread == ERR_PTR(-ENOMEM))
		return ENOMEM;
	return 0;
}

int stop_nas_mon(void) {
	printk("stop thread '%s'\n", nas_thread->comm);
	return kthread_stop(nas_thread);
}

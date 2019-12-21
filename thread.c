#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <asm/errno.h>
#include <linux/cdrom.h>
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
			ret = nas_check_mnt(mntpt);
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
			struct device *dev;
			struct block_device *bdev;
			printk("timer expired\n");

			bdev = blkdev_get_by_mountpoint(mntpt); // hold bdev
			if (IS_ERR(bdev)){
				printk(KERN_ERR "%s is not a mountpoint\n", mntpt);
				goto restart;
			}
			dev = get_first_usb_device(bdev); // hold dev
			printk(KERN_INFO "usb_device = %s\n", dev_name(dev));

			// umount
			printk(KERN_INFO "unmount(%s)\n", mntpt);
			if (nas_unmount(mntpt) != 0) {
				printk(KERN_ERR "umount() failed, retry\n");
				goto restart;
			}

			// eject
			kthread_ssleep(0.5);
			printk(KERN_INFO "eject(%s)\n", mntpt);
			if (ioctl_by_bdev(bdev, CDROMEJECT, 0) !=0 ) {
				printk(KERN_ERR "eject() failed\n");
			}

			// remove(optional)
			kthread_ssleep(0.5);
			printk(KERN_INFO "remove(%s)\n", dev_name(dev));
			if (remove_usb_device(dev) != 0) {
				printk(KERN_ERR "remove() failed\n");
			}
			put_device(dev); // release dev
			blkdev_put(bdev, 0); // release bdev

			// pulldown gpio
			kthread_ssleep(1);
			printk(KERN_INFO "pulldown gpio(%d)\n", GPIO_PIN);
			set_gpio(GPIO_PIN, 0);
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

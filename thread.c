#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <asm/errno.h>
#include <linux/cdrom.h>
#include <linux/blkdev.h>
#include "thread.h"
#include "util.h"
#include "gpio.h"

int nas_timer_ticks = TIMER_TICKS;
struct task_struct *nas_thread = NULL;

int blkdev_driver_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
	struct gendisk *disk = bdev->bd_disk;

	if (disk->fops->ioctl)
		return disk->fops->ioctl(bdev, mode, cmd, arg);

	return -ENOTTY;
}

/*
 * Periodically check if disk can be turned off.
 */
static int thread_func(void *data) {
	bool is_active=true;
	kthread_ssleep_retval(0.1, 0); // slightly delayed start

	while (!kthread_should_stop()) {
		if (is_active)
			printk(KERN_DEBUG "nas_mon: ticks = %d\n", nas_timer_ticks);

		/* ticks  < 0 : inactive
		 *       == 0 : just expired
		 *        > 0 : active
		 */

		// Check mount point can umount
		if (nas_timer_ticks >= 0) {
			int ret;
			is_active = true;
			ret = nas_check_mnt(mntpt);
			if (ret == EBUSY) {
				// hdd is busy
				printk(KERN_INFO "nas_mon: device busy\n");
				nas_timer_ticks = TIMER_TICKS; // reset ticks
				goto restart;

			} else if (ret==ENOENT || ret==ENOTBLK) {
				// hdd disappeared
				printk(KERN_INFO "nas_mon: disk/mount not found\n");
				nas_timer_ticks = -1;
				goto restart;

			} else if (ret != 0) {
				printk(KERN_ERR "nas_mon: unknow error(%d)\n", ret);
			}
		} else {
			is_active = false;
			goto restart;
		}

		// Check timer expiration
		if (nas_timer_ticks == 0) {
			struct device *dev;
			struct block_device *bdev;
			printk(KERN_INFO "nas_mon: timer expired\n");

			bdev = blkdev_get_by_mountpoint(mntpt); // hold bdev
			if (IS_ERR(bdev)){
				printk(KERN_ERR "nas_mon: %s is not a mountpoint\n", mntpt);
				goto restart;
			}
			dev = get_first_usb_device(bdev); // hold dev
			printk(KERN_INFO "usb_device = %s\n", dev_name(dev));

			// umount
			printk(KERN_INFO "nas_mon: unmount(%s)\n", mntpt);
			if (nas_unmount(mntpt) != 0) {
				printk(KERN_ERR "nas_mon: umount() failed, retry\n");
				put_device(dev); // release dev
				blkdev_put(bdev, 0); // release bdev
				goto restart;
			}

			// eject
			kthread_ssleep(1);
			printk(KERN_INFO "nas_mon: eject(%s)\n", mntpt);
			if (blkdev_driver_ioctl(bdev, 0, CDROMEJECT, 0) != 0) {
				printk(KERN_ERR "nas_mon: eject() failed\n");
			}

			// remove(optional)
			kthread_ssleep(1);
			printk(KERN_INFO "nas_mon: remove(%s)\n", dev_name(dev));
			if (remove_usb_device(dev) != 0) {
				printk(KERN_ERR "nas_mon: remove() failed\n");
			}
			put_device(dev); // release dev
			blkdev_put(bdev, 0); // release bdev

			// pulldown gpio
			kthread_ssleep(2);
			printk(KERN_INFO "nas_mon: pulldown gpio(%d)\n", gpio_pin);
			set_gpio(gpio_pin, 0);
		}

		// tick-tock
		nas_timer_ticks--;

	restart:
		if (kthread_should_park()) {
			printk(KERN_DEBUG "nas_mon: parkme\n");
			kthread_parkme();
		}
		else
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
	nas_thread = kthread_run(thread_func, NULL, thread_name);
	if (nas_thread == ERR_PTR(-ENOMEM))
		return ENOMEM;
	printk(KERN_INFO "nas_pm: start thread - nas_mon\n");
	return 0;
}

int stop_nas_mon(void) {
	return kthread_stop(nas_thread);
}

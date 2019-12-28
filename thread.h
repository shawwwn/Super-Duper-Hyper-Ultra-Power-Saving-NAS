#ifndef HEADER_THREAD
#define HEADER_THREAD

extern int nas_timer_ticks;
extern struct task_struct *nas_thread;

// timeout = interval x ticks
#define TIMER_INTERVAL 60 // sec
#define TIMER_TICKS 10

/*
 * Interuptible sleep in kthread
 */
#define _kthread_ssleep(sec, ACTION)                             \
	if (1) {                                                     \
		set_current_state(TASK_INTERRUPTIBLE);                   \
		if (kthread_should_stop()) {                             \
			set_current_state(TASK_RUNNING);                     \
			ACTION;                                              \
		}                                                        \
		schedule_timeout(msecs_to_jiffies((int)(sec*1000)));     \
	} else                                                       \
		do {} while (0)
#define kthread_ssleep(sec) _kthread_ssleep(sec, break)
#define kthread_ssleep_ret(sec) _kthread_ssleep(sec, return)
#define kthread_ssleep_retval(sec, ret) _kthread_ssleep(sec, return ret)

int start_nas_mon(void);
int stop_nas_mon(void);

#endif

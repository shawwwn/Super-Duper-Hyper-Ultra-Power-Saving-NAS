#ifndef HEADER_THREAD
#define HEADER_THREAD

extern int nas_timer_ticks;

#define TIMER_INTERVAL 10 // sec
#define TIMER_TICKS 10
#define GPIO_PIN 495

/*
 * Interuptible sleep in kthread
 */
#define kthread_ssleep(sec)                                      \
	if (1) {                                                     \
		set_current_state(TASK_INTERRUPTIBLE);                   \
		if (kthread_should_stop()) {                             \
			set_current_state(TASK_RUNNING);                     \
			break;                                               \
		}                                                        \
		schedule_timeout(msecs_to_jiffies((int)(sec*1000)));     \
		set_current_state(TASK_RUNNING);                         \
	} else                                                       \
		do {} while (0)

int start_nas_mon(void);
int stop_nas_mon(void);

#endif

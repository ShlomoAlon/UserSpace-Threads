#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>
#include <malloc.h>
#include "malloc369.h"
#include "common.h"
#include "thread.h"
#include "interrupt.h"
#include "test_thread.h"

static struct wait_queue *queue;
static int done;
static int nr_sleeping;

#define WAKE_DELAY 5000 /* spin time between waking threads */

static void
test_wakeup_thread(int num)
{
	int i;
	int ret;
	struct timeval start, end, diff;

	for (i = 0; i < LOOPS; i++) {
		int enabled;
		gettimeofday(&start, NULL);

		/* track the number of sleeping threads with interrupts
		 * disabled to avoid wakeup races. */
		enabled = interrupts_off();
		assert(enabled);
		__sync_fetch_and_add(&nr_sleeping, 1);
		ret = thread_sleep(queue);
		assert(thread_ret_ok(ret));
		interrupts_set(enabled);

		gettimeofday(&end, NULL);
		timersub(&end, &start, &diff);

		/* thread_sleep should wait at least 4-5 ms */
		if (diff.tv_sec == 0 && diff.tv_usec < 4000) {
			unintr_printf("%s took %ld us. That's too fast."
				      " You must be busy looping\n",
				      __FUNCTION__, diff.tv_usec);
			goto out;
		}
	}
out:
	__sync_fetch_and_add(&done, 1);
}

void
test_wakeup(bool all)
{
	Tid ret;
	long ii;
	static Tid child[NTHREADS];
	long start_bytes = get_current_bytes_malloced();

	unintr_printf("starting wakeup test\n");

	done = 0;
	nr_sleeping = 0;

	queue = wait_queue_create();
	assert(queue);

	/* initial thread sleep and wake up tests */
	ret = thread_sleep(NULL);
	assert(ret == THREAD_INVALID);
	unintr_printf("initial thread returns from sleep(NULL)\n");

	ret = thread_sleep(queue);
	assert(ret == THREAD_NONE);
	unintr_printf("initial thread returns from sleep(NONE)\n");

	ret = thread_wakeup(NULL, 0);
	assert(ret == 0);
	ret = thread_wakeup(queue, 1);
	assert(ret == 0);

	if (all) {
		unintr_printf("starting wake_all test, expected duration < 1 second\n");
	} else {
		long dur_usec = NTHREADS * LOOPS * WAKE_DELAY;
		long exptd_dur = (dur_usec + USEC_PER_SEC) / USEC_PER_SEC;
		unintr_printf("starting wake_one test, "
			      "expected duration < %ld seconds\n", exptd_dur);
	}
	/* create all threads */
	for (ii = 0; ii < NTHREADS; ii++) {
		child[ii] = thread_create((void (*)(void *))test_wakeup_thread,
					  (void *)ii);
		assert(thread_ret_ok(child[ii]));
	}
out:
	while (__sync_fetch_and_add(&done, 0) < NTHREADS) {
		if (all) {
			/* wait until all threads have slept */
			if (__sync_fetch_and_add(&nr_sleeping, 0) < NTHREADS) {
				goto out;
			}
			/* we will wake up all threads in the thread_wakeup
			 * call below so set nr_sleeping to 0 */
			nr_sleeping = 0;
		} else {
			/* wait until at least one thread has slept */
			if (__sync_fetch_and_add(&nr_sleeping, 0) < 1) {
				goto out;
			}
			/* wake up one thread in the wakeup call below */
			__sync_fetch_and_add(&nr_sleeping, -1);
		}
		/* spin for 5 ms. this allows testing that the sleeping thread
		 * sleeps for at least 5 ms. */
		spin(WAKE_DELAY);

		/* tests thread_wakeup */
		assert(interrupts_enabled());
		ret = thread_wakeup(queue, all);
		assert(interrupts_enabled());
		assert(ret >= 0);
		assert(all ? ret == NTHREADS : ret == 1);
	}
	/* we expect nr_sleeping is 0 at this point */
	assert(nr_sleeping == 0);
	assert(interrupts_enabled());

	/* no thread should be waiting on queue */
	wait_queue_destroy(queue);

	/* wait for other threads to exit */
	while (thread_yield(THREAD_ANY) != THREAD_NONE) {
	}

	/* No check for memory leaks. At this point, the only thread that should
	 * be running is the main thread, but we don't assume thread_wait() is
	 * implemented yet, and we don't wait for the 128 child threads.
	 * If thread_wait() is *not* implemented, there should be no unaccounted
	 * for memory. But if thread_wait() *is* implemented, then there should
	 * be 128 thread structs in zombie state until someone waits for them. 
	 * 
	 * It is hard to guess how much memory different implementations might
	 * use, but we can at least do a rough check that exited thread stacks 
	 * have been freed.
	 */

	if ((get_current_bytes_malloced() - start_bytes) > 16*THREAD_MIN_STACK) {
		unintr_printf("thread stacks may not be getting freed\n");
	}
	unintr_printf("wakeup test done\n");
}

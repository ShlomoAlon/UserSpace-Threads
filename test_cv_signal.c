#include "malloc369.h"
#include "common.h"
#include "thread.h"
#include "interrupt.h"
#include "test_thread.h"

#define WAKE_DELAY 5000
/* Shared variables used by all the threads */
static int done;
static struct lock *testlock;
static volatile unsigned long turn;

/* Each child thread uses its own cv, signaled by another child thread. */
static struct cv *testcv_signal[NTHREADS];

/* Function run by child threads. 
 * Threads do a cv_wait if it is not their turn to print output. On their turn,
 * a thread will print output, set 'turn' to the next lower-numbered thread 
 * (in creation order -- independent of thread id assigned by thread library)
 * and do a cv_signal on the cv used by the thread whose turn it is now.
 */ 
static void
test_cv_signal_thread(unsigned long num)
{
	int i;
	struct timeval start, end, diff;

	for (i = 0; i < LOOPS; i++) {
		assert(interrupts_enabled());
		lock_acquire(testlock);
		assert(interrupts_enabled());
		while (turn != num) {
			gettimeofday(&start, NULL);
			assert(interrupts_enabled());
			cv_wait(testcv_signal[num], testlock);
			assert(interrupts_enabled());
			gettimeofday(&end, NULL);
			timersub(&end, &start, &diff);

			/* cv_wait should wait at least 4-5 ms */
			if (diff.tv_sec == 0 && diff.tv_usec < 4000) {
				unintr_printf("%s took %ld us. That's too fast."
					      " You must be busy looping\n",
					      __FUNCTION__, diff.tv_usec);
				goto out;
			}
		}
		unintr_printf("%d: thread %3d passes\n", i, num);
		turn = (turn + NTHREADS - 1) % NTHREADS;

		/* spin for 5 ms */
		spin(5000);

		assert(interrupts_enabled());
		cv_signal(testcv_signal[turn], testlock);
		assert(interrupts_enabled());
		lock_release(testlock);
		assert(interrupts_enabled());
	}
out:
	/* In case thread_wait() is not working correctly, use atomic counter
	 * to record that child test_lock_thread is finished.
	 */
	__atomic_add_fetch(&done, 1,__ATOMIC_SEQ_CST);

}

/* Function run by main (initial) cv_signal test thread */
void
test_cv_signal()
{

	long i;
	int result[NTHREADS];
	int kids_done = 0;
	long start_mallocs = get_current_num_mallocs();
	long start_bytes = get_current_bytes_malloced();

	__atomic_store(&done, &kids_done, __ATOMIC_SEQ_CST); 
	
	long dur_usec = NTHREADS * LOOPS * WAKE_DELAY;
	long exptd_dur = (dur_usec + USEC_PER_SEC) / USEC_PER_SEC;
	unintr_printf("starting cv signal test, should take < %ld seconds\n",
		      exptd_dur);
	unintr_printf("threads should print out in reverse order\n");

	for (i = 0; i < NTHREADS; i++) {
		testcv_signal[i] = cv_create();
	}
	testlock = lock_create();

	/* Set 'turn' to highest thread number we are going to create */	
	turn = NTHREADS - 1;

	/* Create the threads. */
	for (i = 0; i < NTHREADS; i++) {
		result[i] = thread_create((void (*)(void *))
					  test_cv_signal_thread, (void *)i);
		assert(thread_ret_ok(result[i]));
	}

	/* Busy wait with yield until all child threads are done. */
	while(kids_done < NTHREADS) {
		thread_yield(THREAD_ANY);
		__atomic_load(&done, &kids_done, __ATOMIC_SEQ_CST);
	}

	unintr_printf("destroying all cvs\n");
	assert(interrupts_enabled());
	for (i = 0; i < NTHREADS; i++) {
		cv_destroy(testcv_signal[i]);
	}
	assert(interrupts_enabled());

	unintr_printf("destroying lock\n");
	lock_destroy(testlock);

	/* We don't want to rely on thread_wait() for the main cv_signal test 
	 * but we need to wait on all threads we created to check for 
	 * memory leaks since threads should not be fully cleaned up until
	 * after they have been waited on.
	 */
	unintr_printf("waiting for test_cv_signal_thread threads to finish\n");
	for (i = 0; i < NTHREADS; i++) {
		thread_wait(result[i], NULL);
	}	
	
	/* Check for memory leaks. At this point, the only thread that should
	 * be running is the main thread, and so no memory should have been
	 * allocated using malloc after thread_init() happened. This assumes 
	 * that the thread structures are allocated statically. 
	 */
	if (is_leak_free(start_mallocs, start_bytes)) {
		unintr_printf("No memory leaks detected.\n");
	} else {
		long bytes_leaked = get_current_bytes_malloced() - start_bytes;
		long unfreed_mallocs = get_current_num_mallocs() - start_mallocs;
		unintr_printf("Detected %lu bytes leaked from %lu un-freed mallocs.\n",
			      bytes_leaked, unfreed_mallocs);
	}
	
	unintr_printf("cv signal test done\n");
}


int
main(int argc, char **argv)
{
	/* Catch fatal signals in case thread functions crash. */
	install_fatal_handlers((void *)main);
	/* Initialize malloc tracking */
	init_csc369_malloc(false);
	/* Initialize threads library */
	thread_init();

	/* Register interrupt handler & start timer interrupts. 
	 * Don't show handler output
	 */
	register_interrupt_handler(false);

	
	/* Test cv signal */
	test_cv_signal();

	return 0;
}

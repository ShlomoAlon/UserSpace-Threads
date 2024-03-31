#include "malloc369.h"
#include "common.h"
#include "thread.h"
#include "interrupt.h"
#include "test_thread.h"

/******************************************************************************
 * The initial thread will create NTHREADS child threads and store their 
 * thread id's into the wait_on[] array. This array is used by each child 
 * thread to find the id of the thread created before them.
 *****************************************************************************/

/* We use 'done' as a flag to synchronize threads to ensure particular orders
 * of creation, waiting, and exiting. We need it because we don't rely on 
 * a working lock or condition variable implementation.
 */
static int done;

/* This array of thread ids lets each child thread find the id of the thread 
 * that it is supposed to call thread_wait() on.
 */
static Tid wait_on[NTHREADS];


/* Each child thread does the following:
 * 1. Busy waits using the 'done' variable until the initial thread has 
 *    finished creating all the child threads.
 * 2. Spins for a random time up to 1s to mix up the thread scheduling.
 * 3. If not the first child thread, thread_wait()'s for the previously
 *    created thread to thread_exit(), retrieving its exit code. 
 *    - the first child thread just exits without waiting.
 * 4. Checks the return value of thread_wait() and the exitcode from the 
 *    thread it was waiting for to make sure they match expected values.
 * 5. Spins again to delay for a random amount of time before printing out
 *    its own argument 'num'. 
 * 6. Calls thread_exit() with a unique value based of the 'num' argument
 *    passed to the test_wait_thread() function (num + THREAD_MAX_THREADS). 
 */
static void
test_wait_thread(int num)
{
	Tid waited_for;
	int exitcode;
	int expected;
	unsigned long rand = ((double)random()) / RAND_MAX * 1000000;
	
	/* 1. Make sure that all threads are created before continuing. */
	/* We use atomic operations for synchronization because we don't 
	 * want to rely on implementations of locks and cvs. 
	 */
	while (__sync_fetch_and_add(&done, 0) < 1)
		;

	/* 2. Spin for a random time between 0-1 s to mix up thread order */
	spin(rand);

	/* 3. If not the first child thread created, wait for previous thread */
	if (num > 0) {
		expected = num - 1 + THREAD_MAX_THREADS;
		assert(interrupts_enabled());
		/* wait on previous thread */
		waited_for = thread_wait(wait_on[num - 1], &exitcode);
		assert(interrupts_enabled());
		//assert(exitcode == (num - 1 + THREAD_MAX_THREADS));

		/* 4. Check return value and exitcode from thread_wait() */
		if (waited_for != wait_on[num - 1]) {
			unintr_printf("%s: bad return value from thread_wait, "
				      "expected %d got %d\n",
				      __FUNCTION__, wait_on[num-1], waited_for);
		} else {
			unintr_printf("%s: good thread_wait returned expected value\n",
				      __FUNCTION__);
		}
		if (exitcode != expected) {
			unintr_printf("%s: bad exit code from thread_wait, "
				      "expected %d got %d\n",
				      __FUNCTION__, expected, exitcode);
		} else {
			unintr_printf("%s: good thread_wait retrieved expected exitcode\n",
				      __FUNCTION__);
		}
		
		/* 5. Busy wait and then print own argument 'num' */
		spin(rand / 10);
		/* num should print in ascending order, from 1-127 */
		unintr_printf("num = %d\n", num);
	}

	/* 6. Exit with unique value. */
	thread_exit(num+THREAD_MAX_THREADS); 
}

/* Create NTHREADS child threads, each of which will wait for the one created
 * before it (except the first child, which does not wait). 
 * Waits for the last child created to thread_exit. 
 * Once the last child has exited, there should be no more runnable threads
 * and all threads should have exited and been fully cleaned up 
 * (no memory leak).
 */
void
test_wait(void)
{
	long i;
	int exitcode;
	int expected;
	Tid waited_for;
	long start_mallocs = get_current_num_mallocs();
	long start_bytes = get_current_bytes_malloced();
	
	unintr_printf("starting wait test\n");
	srandom(0);

	/* The test_wait() test starts here. */
	
	done = 0;
	/* create all threads */
	for (i = 0; i < NTHREADS; i++) {
		wait_on[i] = thread_create((void (*)(void *))test_wait_thread,
					(void *)i);
		assert(thread_ret_ok(wait_on[i]));
	}

	/* Increment 'done' flag to let threads know creation is finished. */
	__sync_fetch_and_add(&done, 1);

	/* Wait for the last thread created. Every other child thread should
	 * be waited-for by the one created after it. 
	 */
	waited_for = thread_wait(wait_on[NTHREADS-1], &exitcode);

	/* Check return value and exitcode from thread_wait() */
	if (waited_for != wait_on[NTHREADS-1]) {
		unintr_printf("%s: bad return value from thread_wait, "
			      "expected %d got %d\n",
			      __FUNCTION__, wait_on[NTHREADS-1], waited_for);
	} else {
		unintr_printf("%s: good thread_wait returned expected value\n",
			      __FUNCTION__);
	}
	

	/* Last thread created was passed num == NTHREADS-1, 
	 * exits with num+THREAD_MAX_THREADS.
	 */
	expected = NTHREADS - 1 + THREAD_MAX_THREADS; 
	if (exitcode != expected) {
		unintr_printf("%s: bad exit code from thread_wait, "
			      "expected %d got %d\n",
			      __FUNCTION__, expected, exitcode);
	} else {
		unintr_printf("%s: good thread_wait retrieved expected exitcode\n",
			      __FUNCTION__);
	}	

	/* Check for memory leaks. At this point, the only thread that should
	 * have not exited is the main thread, and so no extra memory should 
	 * be allocated above what was in use after thread_init() happened. 
	 */
	if (is_leak_free(start_mallocs, start_bytes)) {
		unintr_printf("No memory leaks detected.\n");
	} else {
		long bytes_leaked = get_current_bytes_malloced() - start_bytes;
		long unfreed_mallocs = get_current_num_mallocs() - start_mallocs;
		unintr_printf("Detected %lu bytes leaked from %lu un-freed mallocs.\n",
			      bytes_leaked, unfreed_mallocs);
	}

	unintr_printf("wait test done\n");
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


	/* Test wait */
	test_wait();
	return 0;
}

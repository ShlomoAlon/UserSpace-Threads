#include "malloc369.h"
#include "common.h"
#include "thread.h"
#include "interrupt.h"
#include "test_thread.h"

/* Shared variables used by all the threads */
static int done;
static struct lock *testlock;
static volatile unsigned long testval1;
static volatile unsigned long testval2;
static volatile unsigned long testval3;

#define NLOCKLOOPS    100

static void
test_lock_thread(unsigned long num)
{
	int i, j;

	/* For each iteration of the i loop:
	 *   - repeatedly acquire lock, update shared variables, release lock
	 *   - print success message for the current iteration
	 */
	for (i = 0; i < LOOPS; i++) {
		/* In each iteration of j loop:
		 *   - Acquire lock.
		 *   - Modify shared variables testval1, testval2, and testval3
		 *     yielding after each variable update to let other threads
		 *     run.
                 *   - Do some checks that the shared variables hold the 
		 *     expected values (i.e., have not been modified by other
		 *     threads while the lock was held by this thread).
		 *   - Release the lock.
		 */ 
		for (j = 0; j < NLOCKLOOPS; j++) {
			int ret;

			assert(interrupts_enabled());
			lock_acquire(testlock);
			assert(interrupts_enabled());

			testval1 = num;

			/* let's yield to make sure that even when other threads
			 * run, they cannot access the critical section. */
			assert(interrupts_enabled());
			ret = thread_yield(THREAD_ANY);
			assert(thread_ret_ok(ret) || ret == THREAD_NONE);

			testval2 = num * num;

			/* yield again */
			assert(interrupts_enabled());
			ret = thread_yield(THREAD_ANY);
			assert(thread_ret_ok(ret) || ret == THREAD_NONE);

			testval3 = num % 3;

			assert(testval2 == testval1 * testval1);
			assert(testval2 % 3 == (testval3 * testval3) % 3);
			assert(testval3 == testval1 % 3);
			assert(testval1 == num);
			assert(testval2 == num * num);
			assert(testval3 == num % 3);

			assert(interrupts_enabled());
			lock_release(testlock);
			assert(interrupts_enabled());
		}
		unintr_printf("%d: thread %3d passes\n", i, num);
	}
	/* In case thread_wait() is not working correctly, use atomic counter
	 * to record that child test_lock_thread is finished.
	 */
	__atomic_add_fetch(&done, 1,__ATOMIC_SEQ_CST);

}

void
test_lock()
{
	long i;
	Tid result[NTHREADS];
	/* don't depend on thread_wait() */
	int kids_done = 0;
	long start_mallocs = get_current_num_mallocs();
	long start_bytes = get_current_bytes_malloced();

	__atomic_store(&done, &kids_done, __ATOMIC_SEQ_CST); 

	unintr_printf("starting lock test\n");

	assert(interrupts_enabled());
	testlock = lock_create();
	assert(interrupts_enabled());
	for (i = 0; i < NTHREADS; i++) {
		result[i] = thread_create((void (*)(void *))test_lock_thread,
					  (void *)i);
		assert(thread_ret_ok(result[i]));
	}

	while(kids_done < NTHREADS) {
		thread_yield(THREAD_ANY);
		__atomic_load(&done, &kids_done, __ATOMIC_SEQ_CST);
	}

	unintr_printf("destroying lock\n");
	assert(interrupts_enabled());
	lock_destroy(testlock);
	assert(interrupts_enabled());
	
	/* We don't want to rely on thread_wait() for the main lock test 
	 * but we need to wait on all threads we created to check for 
	 * memory leaks since threads should not be fully cleaned up until
	 * after they have been waited on.
	 */
	unintr_printf("waiting for test_lock_thread threads to finish\n");
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

	unintr_printf("lock test done\n");

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


	/* Test locking */
	test_lock();

	return 0;
}

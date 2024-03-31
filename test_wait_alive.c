#include "malloc369.h"
#include "common.h"
#include "thread.h"
#include "interrupt.h"
#include "test_thread.h"

/******************************************************************************
 * test_wait_alive is the simplest test case - waiting for a thread that is 
 * still running.
 * The initial thread creates one child thread and thread_wait()'s for it.  
 * The child yields until there are no other runnable threads, indicating that 
 * the parent must be blocked (waiting for the child to exit), and then exits.
 * The child thread is started with an argument 'status' that it should pass to
 * thread_exit(), and the parent expects to get this value back when its 
 * thread_wait() call returns.
 *****************************************************************************/


/* Function executed by the child thread */
static void
test_wait_alive_thread(int status)
{

	unintr_printf("wait_alive test child thread started\n");

        /* Child yields until there are no other runnable threads, to
	 * ensure that parent has executed its thread_wait() on the child. */
	while (thread_yield(THREAD_ANY) != THREAD_NONE)
		;

	/* Time to go... */
	unintr_printf("wait_alive test child thread exiting with 0x%x\n",
		      status);
	thread_exit(status);
}

/* Function executed by the main (initial) test thread */
void
test_wait_alive(void)
{
	Tid child;
	Tid ret;
	int childcode;
	int expected_status = 0x1173c0de; /* livecode (l=1, i=1, v=7, e=3) */
	long start_mallocs = get_current_num_mallocs();
	long start_bytes = get_current_bytes_malloced();

	unintr_printf("starting wait_alive test\n");

	/* We include some simple checks for invalid calls to thread_wait()
	 * in this simple test.
	 */
	
	/* initial thread wait tests */

	/* wait on own id should return THREAD_INVALID or it would deadlock */
	ret = thread_wait(thread_id(), NULL);
	assert(ret == THREAD_INVALID);
	unintr_printf("initial thread returns from waiting on self\n");

	/* wait on THREAD_SELF should also return THREAD_INVALID */
	ret = thread_wait(THREAD_SELF, NULL);
	assert(ret == THREAD_INVALID);
	unintr_printf("initial thread returns from waiting on THREAD_SELF\n");

	/* wait on a thread id that doesn't match any thread */
	ret = thread_wait(110, NULL);
	assert(ret == THREAD_INVALID);
	unintr_printf("initial thread returns from waiting on 110\n");

	/* Waiting for a live thread test starts now. */
	
	/* create a thread */
	child = thread_create((void (*)(void *))test_wait_alive_thread,
			      (void *)(unsigned long)expected_status);
	assert(thread_ret_ok(child));

	ret = thread_wait(child, &childcode);
	if (ret != child) {
		unintr_printf("%s: bad return value from thread_wait, "
			      "expected %d got %d\n",
			      __FUNCTION__, child, ret);
	} else {
		unintr_printf("%s: good thread_wait returned expected value\n",
			      __FUNCTION__);
	}		
	if (childcode != expected_status) {
		unintr_printf("%s: bad exit code from thread_wait, "
			      "expected 0x%x got 0x%x\n",
			      __FUNCTION__, expected_status, childcode);
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
	
	unintr_printf("wait_alive test done, ret is %d, childcode is %p\n",
		      ret, (void *)(unsigned long)childcode);
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


	/* Test wait when target thread is still running. */
	test_wait_alive();	
	return 0;
}

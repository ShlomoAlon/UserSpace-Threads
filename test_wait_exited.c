#include "malloc369.h"
#include "common.h"
#include "thread.h"
#include "interrupt.h"
#include "test_thread.h"

/******************************************************************************
 * test_wait_exited is another simple test case - waiting for a thread that has 
 * already exited. No blocking is needed in this case. 
 * The initial thread creates one child thread and yields until there are no 
 * other runnable threads, indicating that the child has exited. The initial
 * thread then thread_wait()'s for the child.  
 * The child thread just thread_exits with the 'status' that it was started
 * with. The parent expects to get this value back when its thread_wait() call 
 * returns.
 *****************************************************************************/


/* Function executed by the child thread */
static void
test_wait_exited_thread(int status)
{
	unintr_printf("%s: exiting with status 0x%x\n", __FUNCTION__, status);
	thread_exit(status);
}

/* Function executed by the main (initial) test thread */
void
test_wait_exited(void)
{
	Tid child;
	Tid ret;
	int childcode;
	int expected_status = 0x7357dead; /* testdead (t=7, e=3, s=5) */
	long start_mallocs = get_current_num_mallocs();
	long start_bytes = get_current_bytes_malloced();

	unintr_printf("starting wait_exited test\n");
	
	/* create a thread */
	child = thread_create((void (*)(void *))test_wait_exited_thread,
			      (void *)(unsigned long)expected_status);
	assert(thread_ret_ok(child));

	/* yield so child can run and exit */
	while (thread_yield(THREAD_ANY) != THREAD_NONE)
		;

	/* No other runnable threads, so child must have exited. Wait now. */
	unintr_printf("%s: waiting for %d\n", __FUNCTION__, child);
	ret = thread_wait(child, &childcode);

	/* Check return values. */
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

	unintr_printf("wait_exited test done, childcode is 0x%x\n", childcode);
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


	/* Test wait when target thread has exited prior to wait. */
	test_wait_exited();	
	return 0;
}

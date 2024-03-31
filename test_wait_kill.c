#include "malloc369.h"
#include "common.h"
#include "thread.h"
#include "interrupt.h"
#include "test_thread.h"

/******************************************************************************
 * There are 3 subtests in the test_wait_kill test program. 
 * 1. Create a child thread, kill it, and then wait on it. 
 * 2. Create 2 child threads, one waits on the other; parent kills child that 
 *    is not waiting. 
 * 3. Create a child thread and wait on it; child kills the parent
 *
 * We do not check for memory leaks in this test, because the final child 
 * thread is expected to be using some malloc'd memory (e.g., its stack) 
 * at the end.
 *****************************************************************************/

/* We use 'done' as a flag to synchronize threads to ensure particular orders
 * of creation, waiting, and exiting. We need it because we don't rely on 
 * a working lock or condition variable implementation.
 */
static int done;


/* Function run by victim thread that will be killed (subtest 1 and 2).
 * Prints a starting message and enters an infinite loop. 
 * Should never exit loop -- when killed, thread should exit in thread library
 * code and not come back here. 
 */
static void
test_wait_victim_thread(int val)
{
	unintr_printf("%s: starting with value 0x%x\n", __FUNCTION__, val);
	while(1) ;

	/* Only way out of the infinite loop is by being killed.
	 * Thread can't get here.
	 */
	assert(false);
}

/* Function run by child thread that waits for a sibling that will be killed.
 * (subtest 2)
 */
static void
test_wait_sibling_thread(int sibling_tid)
{
	Tid waited_for;
	int exit_code;
	int expected_status = -SIGKILL;
	
	unintr_printf("%s: starting, will wait on sibling tid %d\n",
		      __FUNCTION__, sibling_tid);

	/* Let parent know we are ready to call thread_wait */
	__sync_fetch_and_add(&done, 1);
	waited_for = thread_wait(sibling_tid, &exit_code);
	if (waited_for != sibling_tid) {
		unintr_printf("%s: bad return value from thread_wait, "
			      "expected %d got %d\n",
			      __FUNCTION__, sibling_tid, waited_for);
	} else {
		unintr_printf("%s: good thread_wait returned expected value\n",
			      __FUNCTION__);
	}		
	if (exit_code != -SIGKILL) {
		unintr_printf("%s: bad exit code from thread_wait, "
			      "expected %d got %d\n",
			      __FUNCTION__, expected_status, exit_code);
	} else {
		unintr_printf("%s: good thread_wait retrieved expected exitcode\n",
			      __FUNCTION__);
	}

	/* Our work here is done. */
	thread_exit(0);
}


/* Function for thread that kills parent (third subtest) */
static void
test_wait_parricide_thread(Tid parent)
{
	Tid ret;

	/* Child yields until there are no other runnable threads, to
	 * ensure that parent has executed its thread_wait() on the child. */
	while (thread_yield(THREAD_ANY) != THREAD_NONE)
		;

	ret = thread_kill(parent);	/* ouch */
	if (ret != parent) {
		unintr_printf("%s: bad thread_kill expected %d got %d\n",
			      __FUNCTION__, parent, ret);
	} else {
		unintr_printf("%s: good thread_kill returned expected result\n",
			      __FUNCTION__);
	}

	unintr_printf("%s: finished\n", __FUNCTION__);
}

/* Function run by the main (initial) test_wait_kill thread. 
 * Creates child threads to run the 3 subtests.
 */
void
test_wait_kill(void)
{
	Tid child;
	Tid victim;
	Tid ret;
	int exit_code;

	unintr_printf("%s: starting wait_kill test\n", __FUNCTION__);

	/* Subtest 1: Wait on an already-killed thread. Should return error. */
	unintr_printf("%s: creating first victim\n", __FUNCTION__);
	
	/* Create victim thread that will be killed and then waited on. */
	victim = thread_create((void (*)(void *))test_wait_victim_thread,
			      (void *)0xd1ed);
	assert(thread_ret_ok(victim));

	/* Delay for a while to give the victim a chance to print message. */
	spin(1000);

	/* Kill the child. */
	ret = thread_kill(victim);

	/* Delay for a while to give the victim a chance to exit. */
	spin(1000);

	/* Wait for the victim and get its exit status. This should fail. */
	ret = thread_wait(victim, &exit_code);

	if (ret != THREAD_INVALID) {
		unintr_printf("%s: bad wait on killed thread returns %d\n",
			      __FUNCTION__, ret);
	} else {
		unintr_printf("%s: good wait on killed thread returns THREAD_INVALID\n",
			      __FUNCTION__);
	}
			
	/* Subtest 2: Wait on thread that later exits because it was killed. */

	/* Create victim thread that will be killed and then waited on. */
	unintr_printf("%s: creating second victim\n", __FUNCTION__);
	victim = thread_create((void (*)(void *))test_wait_victim_thread,
			      (void *)0xd1ed);
	assert(thread_ret_ok(victim));

	/* Create another child thread that will thread_wait on the first */
	unintr_printf("%s: creating sibling to wait on victim\n", __FUNCTION__);
	child = thread_create((void (*)(void *))test_wait_sibling_thread,
			      (void *)(unsigned long)victim);
	assert(thread_ret_ok(child));

	/* We want to wait until the child has waited on the victim, which is
	 * hard because we can't directly observe the state of other threads.
	 * We do this in two stages:
	 *   1. Busy wait for the child to set a flag so we know it is running.
	 *   2. Spin "long enough" that the child should be waiting.
	 */

	while (__sync_fetch_and_add(&done, 0) == 0)
		;
	spin(USEC_PER_SEC);
	
	/* Kill the victim. */
	ret = thread_kill(victim);
	assert(thread_ret_ok(ret));

	/* Wait for the child we didn't kill and get its exit status. */
	exit_code = 42; /* This should be over-written by thread_wait(). */
	ret = thread_wait(child, &exit_code);

	if (ret != child) {
		unintr_printf("%s: bad return value from thread_wait, "
			      "expected %d got %d\n",
			      __FUNCTION__, child, ret);
	} else {
		unintr_printf("%s: good thread_wait returned expected value\n",
			      __FUNCTION__);
	}		
	if (exit_code != 0) {
		unintr_printf("%s: bad exit code from thread_wait, "
			      "expected %d got %d\n",
			      __FUNCTION__, 0, exit_code);
	} else {
		unintr_printf("%s: good thread_wait retrieved expected exitcode\n",
			      __FUNCTION__);
	}


	/* Subtest 3: Wait on a child that kills this thread. */
	/* create a thread */
	child = thread_create((void (*)(void *))test_wait_parricide_thread,
			      (void *)(long)thread_id());
	assert(thread_ret_ok(child));
	ret = thread_wait(child, NULL);

	/* child kills parent! we shouldn't get here */
	assert(ret == child);
	unintr_printf("wait_kill test failed\n");
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


	/* Test how kill interacts with wait */
	test_wait_kill();	
	return 0;
}

#include "malloc369.h"
#include "common.h"
#include "thread.h"
#include "interrupt.h"
#include "test_thread.h"

/******************************************************************************
 * Function run by child threads. Each thread tries to wait for the parent. 
 * We want the first created child's thread_wait() to succeed, and all the 
 * others to fail with THREAD_INVALID, because the parent already has a thread
 * waiting for it (or because the parent is already gone -- we don't distinguish
 * between these two cases). 
 *****************************************************************************/

/* We use 'done' as a flag to synchronize threads to ensure particular orders
 * of creation, waiting, and exiting. We need it because we don't rely on 
 * a working lock or condition variable implementation.
 */
static int done;


static void
test_wait_parent_thread(int num_and_parent)
{
	int exitcode;
	int num;
	Tid parent;
	Tid ret;	
	unsigned long rand;

	/* Unpack num and parent. Neither should be larger than 
	 * THREAD_MAX_THREADS so we stuff them both into one 4-byte int
	 * to pass as the argument to the thread function.
	 */
	num = num_and_parent >> 16; /* num is stored in upper half of arg */
	parent = num_and_parent & 0xffff; /* parent in lower half of arg */

	if (num > 0) {
		/* random number between 1 and 2 seconds */
		rand = ((double)random()) / RAND_MAX * USEC_PER_SEC + USEC_PER_SEC;
		spin(rand);
	}
	ret = thread_wait(parent, &exitcode);

	/* Check ret and exitcode - parent exits with -42.
	 * First child must get exitcode. Others must get THREAD_INVALID.
	 */
	if (ret == parent && num == 0)
		unintr_printf("%s: thread %d woken, parent exited with %d\n",
			      __FUNCTION__, thread_id(), exitcode);
	else if (ret == THREAD_INVALID && num > 0)
		unintr_printf("%s: thread %d gets parent gone or already waited for\n",
			      __FUNCTION__, thread_id());
	else	/* unexpected return value */
		unintr_printf("%s: thread %d gets unexpected result %d\n",
			      __FUNCTION__, thread_id(), ret);

	if (__sync_fetch_and_add(&done, 1) == NTHREADS - 1)
		unintr_printf("%s: wait_parent test done\n", __FUNCTION__);
}

/*
 * Parent thread creates NTHREADS children threads. Children threads then wait
 * on the parent thread. Parent thread then delays for a while before calling 
 * thread_exit. All the children threads should run to completion. For each 
 * child thread, we should see one of "thread woken" or "parent gone".
 */
void
test_wait_parent(void)
{
	Tid wait[NTHREADS];
	Tid ret;
	long i;
        long thread_arg;
	Tid parent_tid = thread_id();
	
	srandom(0);
	done = 0;
	unintr_printf("%s: starting wait_parent test\n", __FUNCTION__);

	for (i = 0; i < NTHREADS; i++) {
		/* pack thread number and parent thread id into argument */
		thread_arg = (i << 16) | parent_tid;
		wait[i] = thread_create((void (*)(void *))
					test_wait_parent_thread,
					(void *)thread_arg);
		assert(thread_ret_ok(wait[i]));
	}

	/* make sure some threads start running before we exit */
	ret = thread_yield(THREAD_ANY);
	assert(thread_ret_ok(ret));
	
	spin(2*USEC_PER_SEC); /* wait 2 seconds */
	
	thread_exit(-42);

	/* should never get here */
	unintr_printf("wait_parent test failed\n");
	assert(0);
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


	/* Test waiting on thread 0 */
	test_wait_parent();
	return 0;
}

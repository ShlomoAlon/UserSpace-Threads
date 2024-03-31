#include "malloc369.h"
#include "common.h"
#include "thread.h"
#include "interrupt.h"
#include "test_thread.h"

static void hello(char *msg);
static int fact(int n);
static void self_exit();
static void finale();
static void grand_finale();

/* Note that test_basic will fail when you have Part 3 of A2 implemented,
 * because thread id's cannot be reused until the exited threads have been
 * waited for, and the test_basic test does not wait for its children.
 */

/* Records addresses of stack-allocated variables in different threads. */
long *stack_array[THREAD_MAX_THREADS];

/* Simple synchronization using atomic operations to control order of test
 * thread execution. 
 */ 
static int flag_value;

/* sets flag_value to val, returns old value of flag_value */
int
set_flag(int val)
{
	return __sync_lock_test_and_set(&flag_value, val);
}



/*** Test functions start here ***/

static void
hello(char *msg)
{
	Tid ret;
	char str[20];

	unintr_printf("message: %s\n", msg);
	ret = thread_yield(THREAD_SELF);
	assert(thread_ret_ok(ret));
	unintr_printf("thread returns from  first yield\n");

	/* we cast ret to a float because that helps to check
	 * whether the stack alignment of the frame pointer is correct */
	sprintf(str, "%3.0f\n", (float)ret);

	ret = thread_yield(THREAD_SELF);
	assert(thread_ret_ok(ret));
	unintr_printf("thread returns from second yield\n");

	while (1) {
		thread_yield(THREAD_ANY);
	}

}

static int
fact(int n)
{
	/* store address of some variable on stack */
	stack_array[thread_id()] = (long *)&n;
	if (n == 1) {
		return 1;
	}
	return n * fact(n - 1);
}

static void
self_exit()
{
	int ret = set_flag(1);
	assert(ret == 0);
	thread_exit(0);
	assert(0);
}

static void
finale()
{
	int ret;
	unintr_printf("finale running\n");
	ret = thread_yield(THREAD_ANY);
	assert(ret == THREAD_NONE);
	ret = thread_yield(THREAD_ANY);
	assert(ret == THREAD_NONE);
	unintr_printf("basic test done\n");
	/* 
	 * Stub should exit cleanly if there are no threads left to run.
	 */
	return;
}

static void
grand_finale()
{
	Tid ret;

	unintr_printf("for my grand finale, I will destroy myself\n");
	unintr_printf("while my talented assistant prints \"basic test done\"\n");
	ret = thread_create((void (*)(void *))finale, NULL);
	assert(thread_ret_ok(ret));
	thread_exit(ret);
	assert(0);

}

/* Important: these tests assume that preemptive scheduling is not enabled,
 * i.e., register_interrupt_handler is NOT called before this function is
 * called. */
static void
test_basic()
{
	Tid ret;
	Tid ret2;
	long start_mallocs = get_current_num_mallocs();
	long start_bytes = get_current_bytes_malloced();
	
	unintr_printf("starting basic test\n");
		
	assert(thread_id() == 0);

	/* Initial thread yields */
	ret = thread_yield(THREAD_SELF);
	assert(thread_ret_ok(ret));
	unintr_printf("initial thread returns from yield(SELF)\n");
	/* See thread.h -- initial thread must be Tid 0 */
	ret = thread_yield(0);
	assert(thread_ret_ok(ret));
	unintr_printf("initial thread returns from yield(0)\n");
	ret = thread_yield(THREAD_ANY);
	assert(ret == THREAD_NONE);
	unintr_printf("initial thread returns from yield(ANY)\n");
	ret = thread_yield(0xDEADBEEF);
	assert(ret == THREAD_INVALID);
	unintr_printf("initial thread returns from yield(INVALID)\n");
	ret = thread_yield(16);
	assert(ret == THREAD_INVALID);
	unintr_printf("initial thread returns from yield(INVALID2)\n");

	
	/* create a thread */
	ret = thread_create((void (*)(void *))hello, "hello from first thread");

	if (get_current_bytes_malloced() < THREAD_MIN_STACK) { 
		unintr_printf("it appears that the thread stack is not being"
			      "allocated dynamically\n");
		assert(0);
	}
	unintr_printf("my id is %d\n", thread_id());
	assert(thread_ret_ok(ret));
	ret2 = thread_yield(ret);
	assert(ret2 == ret);

	/* store address of some variable on stack */
	stack_array[thread_id()] = (long *)&ret;

	int ii, jj;
	/* we will be using THREAD_MAX_THREADS threads later */
	Tid child[THREAD_MAX_THREADS];
	char msg[NTHREADS][1024];
	/* create NTHREADS threads */
	for (ii = 0; ii < NTHREADS; ii++) {
		ret = snprintf(msg[ii], 1023, "hello from thread %3d", ii);
		assert(ret > 0);
		child[ii] = thread_create((void (*)(void *))hello, msg[ii]);
		assert(thread_ret_ok(child[ii]));
	}
	unintr_printf("my id is %d\n", thread_id());
	for (ii = 0; ii < NTHREADS; ii++) {
		ret = thread_yield(child[ii]);
		assert(ret == child[ii]);
	}

	/* destroy NTHREADS + 1 threads we just created */
	unintr_printf("destroying all threads\n");
	ret = thread_kill(ret2);
	assert(ret == ret2);
	for (ii = 0; ii < NTHREADS; ii++) {
		ret = thread_kill(child[ii]);
		assert(ret == child[ii]);
	}

	/* we destroyed other threads. yield so that these threads get to run
	 * and exit. */
	ii = 0;
	do {
		/* the yield should be needed at most NTHREADS+2 times */
		assert(ii <= (NTHREADS + 1));
		ret = thread_yield(THREAD_ANY);
		ii++;
	} while (ret != THREAD_NONE);

	/*
	 * create maxthreads-1 threads
	 */
	unintr_printf("creating  %d threads\n", THREAD_MAX_THREADS - 1);
	for (ii = 0; ii < THREAD_MAX_THREADS - 1; ii++) {
		ret = thread_create((void (*)(void *))fact, (void *)10);
		assert(thread_ret_ok(ret));
	}
	/*
	 * Now we're out of threads. Next create should fail.
	 */
	ret = thread_create((void (*)(void *))fact, (void *)10);
	assert(ret == THREAD_NOMORE);
	/*
	 * Now let them all run.
	 */
	unintr_printf("running   %d threads\n", THREAD_MAX_THREADS - 1);
	for (ii = 0; ii < THREAD_MAX_THREADS; ii++) {
		ret = thread_yield(ii);
		if (ii == 0) {
			/* 
			 * Guaranteed that first yield will find someone. 
			 * Later ones may or may not depending on who
			 * stub schedules  on exit.
			 */
			assert(thread_ret_ok(ret));
		}
	}

	/* check that the thread stacks are sufficiently far apart */
	for (ii = 0; ii < THREAD_MAX_THREADS; ii++) {
		for (jj = 0; jj < THREAD_MAX_THREADS; jj++) {
			if (ii == jj)
				continue;
			long stack_sep = (long)(stack_array[ii]) -
				(long)(stack_array[jj]);
			if ((labs(stack_sep) < THREAD_MIN_STACK)) {
				unintr_printf("stacks of threads %d and %d "
					      "are too close\n", ii, jj);
				assert(0);
			}
		}
	}

	/*
	 * They should have cleaned themselves up when
	 * they finished running. Create maxthreads-1 threads.
	 */
	unintr_printf("creating  %d threads\n", THREAD_MAX_THREADS - 1);
	for (ii = 0; ii < THREAD_MAX_THREADS - 1; ii++) {
		child[ii] = thread_create((void (*)(void *))fact, (void *)10);
		assert(thread_ret_ok(child[ii]));
	}
	/*
	 * Now destroy some explicitly and let the others run
	 */
	unintr_printf("destroying %d threads\n", THREAD_MAX_THREADS / 2);
	for (ii = 0; ii < THREAD_MAX_THREADS; ii += 2) {
		ret = thread_kill(child[ii]);
		assert(thread_ret_ok(ret));
	}
	for (ii = 0; ii < THREAD_MAX_THREADS; ii++) {
		ret = thread_yield(ii);
	}

	ret = thread_kill(thread_id());
	assert(ret == THREAD_INVALID);
	unintr_printf("testing some destroys even though I'm the only thread\n");

	ret = thread_kill(42);
	assert(ret == THREAD_INVALID);
	ret = thread_kill(-42);
	assert(ret == THREAD_INVALID);
	ret = thread_kill(THREAD_MAX_THREADS + 1000);
	assert(ret == THREAD_INVALID);

	/*
	 * Create a thread that destroys itself. Control should come back here
	 * after that thread runs.
	 */
	unintr_printf("testing destroy self\n");
	int flag = set_flag(0);
	ret = thread_create((void (*)(void *))self_exit, NULL);
	assert(thread_ret_ok(ret));
	ret = thread_yield(ret);
	assert(thread_ret_ok(ret));
	flag = set_flag(0);
	assert(flag == 1);	/* Other thread ran */
	/* That thread is gone now */
	ret = thread_yield(ret);
	assert(ret == THREAD_INVALID);

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
	
	grand_finale();
	unintr_printf("\n\nBUG: test should not get here\n\n");
	assert(0);
}


int
main(int argc, char **argv)
{
	/* Catch fatal signals in case thread functions crash. */
	install_fatal_handlers((void *)main);
	/* Initialize malloc tracking (minimize output) */
	init_csc369_malloc(false);
	/* Initialize threads library */
	thread_init();

	/* Basic tests do not turn on timer interrupts. 
	 * They are here to make sure the cooperative threading continues
	 * to work as you add code to enable preemptive threading.
	 */
	test_basic();

	return 0;
}


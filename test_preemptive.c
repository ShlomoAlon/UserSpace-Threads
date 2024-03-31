#include "malloc369.h"
#include "common.h"
#include "thread.h"
#include "interrupt.h"
#include "test_thread.h"

static void do_potato(int num);
static int try_move_potato(int num, int pass);

#define DURATION 30000000 /* How long to run test in usecs (secs=usecs/10^6). */
#define NPOTATO  NTHREADS

static int potato[NPOTATO];
static int potato_lock = 0;
static struct timeval pstart;

static void
test_preemptive()
{

	int ret;
	long ii;
	Tid potato_tids[NPOTATO];

	unintr_printf("starting preemptive test\n");
	unintr_printf("this test will take %d seconds\n", DURATION / 1000000);
	gettimeofday(&pstart, NULL);
	/* spin for some time, so you see the interrupt handler output */
	spin(SIG_INTERVAL * 5);
	interrupts_quiet();

	potato[0] = 1;
	for (ii = 1; ii < NPOTATO; ii++) {
		potato[ii] = 0;
	}

	for (ii = 0; ii < NPOTATO; ii++) {
		potato_tids[ii] =
			thread_create((void (*)(void *))do_potato, (void *)ii);
		if (!thread_ret_ok(potato_tids[ii])) {
			unintr_printf("test_preemptive: bad create %ld -> id %d\n",
			       ii, potato_tids[ii]);
		} 
		//assert(thread_ret_ok(potato_tids[ii]));
	}

	spin(DURATION);

	unintr_printf("cleaning hot potato\n");

	/* Note: we may still get some child thread output before the
	 * test done message, because the child threads might run again
	 * before they are killed in the loop below.
	 */
	for (ii = 0; ii < NPOTATO; ii++) {
		if (!interrupts_enabled()) {
			unintr_printf("test_preemptive: error, interrupts disabled\n");
		} 
		//assert(interrupts_enabled());
		ret = thread_kill(potato_tids[ii]);
		if (!thread_ret_ok(ret)) {
			unintr_printf("test_preemptive: bad thread_kill %ld on id %d\n",
			       ii, potato_tids[ii]);
		}			       
		//assert(thread_ret_ok(ret));
	}

	unintr_printf("preemptive test done\n");
	/* we don't check for memory leaks because while threads have exited,
	 * they may not have been destroyed yet. 
	 */
}

static void
do_potato(int num)
{
	int ret;
	int pass = 1;

	unintr_printf("0: thread %3d made it to %s\n", num, __FUNCTION__);
	while (1) {
		ret = try_move_potato(num, pass);
		if (ret) {
			pass++;
		}
		spin(1);
		/* Add some yields by some threads to scramble the list */
		if (num > 4) {
			int ii;
			for (ii = 0; ii < num - 4; ii++) {
				//assert(interrupts_enabled());
				if (!interrupts_enabled()) {
					unintr_printf("do_potato: error, "
						      "interrupts disabled\n");
				}
				ret = thread_yield(THREAD_ANY);				
				if (!thread_ret_ok(ret)) {
					unintr_printf("do_potato: bad thread_yield in %d\n",
					       num);
				}			       
				//assert(thread_ret_ok(ret));
			}
		}
	}
}

static int
try_move_potato(int num, int pass)
{
	int ret = 0;
	int err;
	struct timeval pend, pdiff;

	if (!interrupts_enabled()) {
		unintr_printf("try_move_potato: error, interrupts disabled\n");
	}
	//assert(interrupts_enabled());
	err = __sync_bool_compare_and_swap(&potato_lock, 0, 1);
	if (!err) {	/* couldn't acquire lock */
		return ret;
	}
	if (potato[num]) {
		potato[num] = 0;
		potato[(num + 1) % NPOTATO] = 1;
		gettimeofday(&pend, NULL);
		timersub(&pend, &pstart, &pdiff);
		unintr_printf("%d: thread %3d passes potato "
			      "at time = %9.6f\n", pass, num,
			      (float)pdiff.tv_sec +
				      (float)pdiff.tv_usec / 1000000);
		if ( (potato[(num + 1) % NPOTATO] != 1)
		     || (potato[(num) % NPOTATO] != 0) ) {
			unintr_printf("try_move_potato: unexpected potato move\n");
		}
		//assert(potato[(num + 1) % NPOTATO] == 1);
		//assert(potato[(num) % NPOTATO] == 0);
		ret = 1;
	}
	err = __sync_bool_compare_and_swap(&potato_lock, 1, 0);
	assert(err);
	return ret;
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
	 * Show handler output, we will turn it off later.
	 */
	register_interrupt_handler(1);
	
	/* Test preemptive threads */
	test_preemptive();

	return 0;
}

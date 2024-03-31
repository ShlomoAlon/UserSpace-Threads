#ifndef _TEST_THREAD_H_
#define _TEST_THREAD_H_

/* Constants used in thread tests */
#define NTHREADS       128
#define LOOPS	        10

/* test functions defined in separate files for ease of reuse */
extern void test_wakeup(bool all);


#endif /* _TEST_THREAD_H_ */

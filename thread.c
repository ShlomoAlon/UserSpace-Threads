#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdio.h>
#include "thread.h"
#include "stdbool.h"
#include "interrupt.h"
#include "malloc369.h"

#define ERR_EMPTY -2
//enum {
//    REG_R8,          // 0
//    REG_R9,          // 1
//    REG_R10,         // 2
//    REG_R11,         // 3
//    REG_R12,         // 4
//    REG_R13,         // 5
//    REG_R14,         // 6
//    REG_R15,         // 7
//    REG_RDI,         // 8
//    REG_RSI,         // 9
//    REG_RBP,         // 10
//    REG_RBX,         // 11
//    REG_RDX,         // 12
//    REG_RAX,         // 13
//    REG_RCX,         // 14
//    REG_RSP,         // 15
//    REG_RIP,         // 16
//    REG_EFL,         // 17
//    REG_CSGSFS,      // 18
//    REG_ERR,         // 19
//    REG_TRAPNO,      // 20
//    REG_OLDMASK,     // 21
//    REG_CR2          // 22
//};




/* For Assignment 1, you will need a queue structure to keep track of the 
 * runnable threads. You can use the tutorial 1 queue implementation if you 
 * like. You will probably find in Assignment 2 that the operations needed
 * for the wait_queue are the same as those needed for the ready_queue.
 */
typedef enum State{
    Destroyed = 0,
    Running,
    Sleep,
} state_t;

/* This is the thread control block. */
typedef struct thread {
    ucontext_t ucontext;
    long long * stack_start;
    bool is_main;
    state_t state;
    int waiter;
    int exit_code;


	/* ... Fill this in ... */
} thread_t;



typedef struct queue{
    int current_size;
    int start;
    int end;
    int threads[THREAD_MAX_THREADS * 30];
} queue_t;

/* This is the wait queue structure, needed for Assignment 2. */
struct wait_queue {
    queue_t queue;
};

thread_t threads[THREAD_MAX_THREADS];
queue_t thread_queue;
int current_thread;
int thread_to_destroy = -1;
bool administrative_mode = false;

int queue_index(queue_t queue, int index){
    if (index >= queue.current_size){
        return -1;
    }
    int offset = index + queue.start;
    if (offset < THREAD_MAX_THREADS){
        return offset;
    } else {
        return offset - THREAD_MAX_THREADS;
    }
};

int next(int index){
    int offset = index + 1;
    if (offset < THREAD_MAX_THREADS * 30){
        return offset;
    } else if (offset > THREAD_MAX_THREADS * 30){
//        exit(0);
        return -1;
    } else {
        return 0;
    }
}

int prev(int index){
    int offset = index - 1;
    if (offset >= 0){
        return offset;
    } else if (offset < -1){
//        exit(0);
        return -1;
    } else {
        return THREAD_MAX_THREADS * 30 - 1;
    }
}

void print_queue(queue_t queue){
    interrupts_off();
    unintr_printf("reachedprint\n");
    unintr_printf("size: %d\n ", queue.current_size);
//    int start = queue.start;
//    int end = queue.end;
//    while (start != end){
//        unintr_printf("thread number: %d threadstatus: %d\n", queue.threads[start], threads[queue.threads[start]].state);
//        start = next(start);
//    }
    unintr_printf("\n");
//    interrupts_on();
}

bool queue_push(queue_t * queue, int value){
    if (queue->current_size == THREAD_MAX_THREADS * 30){
//        exit(0);
        return false;
    }
    queue->threads[queue->end] = value;
    queue->end = next(queue->end);
    queue->current_size += 1;
    return true;
}

int queue_pop_last(queue_t * queue){
    if (queue->current_size == 0){
        return ERR_EMPTY;
    }
    queue->end = prev(queue->end);
    queue->current_size -= 1;
    return queue->threads[queue->end];
}

int queue_pop(queue_t * queue){
    if (queue->current_size == 0){
        return ERR_EMPTY;
    }
    int result = queue->threads[queue->start];
    queue->start = next(queue->start);
    queue->current_size -= 1;
    return result;
}

/**************************************************************************
 * Assignment 1: Refer to thread.h for the detailed descriptions of the six
 *               functions you need to implement. 
 **************************************************************************/
bool is_valid_thread(Tid tid){
    return tid >= 0 && tid < THREAD_MAX_THREADS;
}
void
handle_death(int thread_to_die);
void
thread_init(void)
{
    assert(thread_queue.current_size == 0);
    assert(thread_queue.start == 0);
    assert(thread_queue.end == 0);
    thread_queue.start = 0;
    thread_queue.end = 0;
    thread_queue.current_size = 0;
    interrupts_off();
    for (int i = 0; i < THREAD_MAX_THREADS; i++){
        thread_t uncreated_thread = {0};
        uncreated_thread.state = Destroyed;
        uncreated_thread.waiter = -1;
        uncreated_thread.exit_code = -SIGKILL;
        threads[i] = uncreated_thread;
    }
    thread_t main_thread;
    main_thread.is_main = true;
    current_thread = 0;
    main_thread.state = Running;
    main_thread.waiter = -1;
    threads[0] = main_thread;
    interrupts_on();
    assert(interrupts_enabled());
	/* Add necessary initialization for your threads library here. */
        /* Initialize the thread control block for the first thread */
}

Tid
thread_id()
{
    return current_thread;
//	return THREAD_INVALID;
}

/* New thread starts by calling thread_stub. The arguments to thread_stub are
 * the thread_main() function, and one argument to the thread_main() function. 
 */
void
thread_stub(void (*thread_main)(void *), void *arg)
{
        interrupts_on();
        thread_main(arg); // call thread_main() function with arg
        thread_exit(0);
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
    bool signal_state = interrupts_set(false);
    assert(!interrupts_enabled());
    assert(signal_state);
	int thread_num_to_create = 1;
    bool found = false;
    for (; thread_num_to_create < THREAD_MAX_THREADS; thread_num_to_create ++){
        if (threads[thread_num_to_create].state == Destroyed){
            found = true;
            break;
        }
    }
    if (found == false){
        interrupts_set(signal_state);
        return THREAD_NOMORE;
    }

    void * stack_pointer = malloc369(THREAD_MIN_STACK);
    ucontext_t new_thread_context = {0};
    void * stack_start = stack_pointer + THREAD_MIN_STACK - 8;
    thread_t new_thread = {0};
    new_thread.stack_start = stack_pointer;
    new_thread.state = Running;
    new_thread.waiter = -1;
    new_thread.exit_code = -SIGKILL;
    assert(!interrupts_enabled());
    getcontext(&new_thread_context);

    new_thread_context.uc_mcontext.gregs[15] = (greg_t) stack_start;
    new_thread_context.uc_mcontext.gregs[16] = (long long)&thread_stub;
    new_thread_context.uc_mcontext.gregs[8] = (long long)fn;
    new_thread_context.uc_mcontext.gregs[9] = (long long)parg;

    new_thread.ucontext = new_thread_context;

    threads[thread_num_to_create]= new_thread;
    queue_push(&thread_queue, thread_num_to_create);

    interrupts_set(signal_state);
	return thread_num_to_create;
}

int get_thread_any(int current){
    while (true){
        int result = queue_pop(&thread_queue);
        if (result == -2){
            return THREAD_NONE;
        }
        if (threads[result].state == Running && result != current){
            return result;
        }
    }
}

void admin_mode(){
    assert(!interrupts_enabled());
//    assert(thread_to_destroy == thread_id());
    thread_kill(thread_id());
    administrative_mode = false;
    thread_to_destroy = -1;
//    interrupts_on();

    assert(!interrupts_enabled());
    assert(current_thread != 0);
    int cur = current_thread;
    assert(threads[current_thread].state == Destroyed);
    int result = thread_yield(THREAD_ANY);

    unintr_printf("result: %d\ncurrent_thread: %d\n", result, cur);
    if (THREAD_NONE == result){
        exit(0);
    }
    assert(false);
//    int result = get_thread_any(-1);
//    if (THREAD_NONE == result){
//        if (threads[0].state == Running){
//            setcontext(&threads[0].ucontext);
//        }
//        exit(0);
//    } else {
//        setcontext(&threads[result].ucontext);
//    }
}
int counter = 0;
Tid
thread_yield(Tid want_tid)
{
    bool signal_state = interrupts_set(false);
    Tid actual_tid;
    if (want_tid == THREAD_SELF || want_tid == current_thread){
        interrupts_set(signal_state);
        return current_thread;
    } else if (want_tid == THREAD_ANY) {
        int result = get_thread_any(thread_id());
        if (result == THREAD_NONE) {
            interrupts_set(signal_state);
            return THREAD_NONE;
        }
        actual_tid = result;
    } else {
        actual_tid = want_tid;
    }
    assert(!interrupts_enabled());
    if (actual_tid >= THREAD_MAX_THREADS || actual_tid < 0){
        interrupts_set(signal_state);
        return THREAD_INVALID;
    }
    state_t state = threads[actual_tid].state;
    if (state == Destroyed){
        interrupts_set(signal_state);
        return THREAD_INVALID;
    }
    volatile bool switched = false;
    ucontext_t current_context = {0};
    assert(!interrupts_enabled());
    getcontext(&current_context);
    if (switched){
        assert(!interrupts_enabled());
        assert(!interrupts_enabled());
        if (administrative_mode){
            assert(current_thread == thread_to_destroy);
            assert(!interrupts_enabled());
//            if (current_thread != 0){
//            }
            admin_mode();
        } else{
            assert(!interrupts_enabled());
            interrupts_set(signal_state);
            return actual_tid;
        }
//        interrupts_set(signal_state);
//        return actual_tid;
//        assert(false);
    }
    switched = true;
    assert(!interrupts_enabled());
    threads[current_thread].ucontext = current_context;

    queue_push(&thread_queue, current_thread);
    current_thread = actual_tid;
    setcontext(&threads[actual_tid].ucontext);
    assert(false);
}

void
handle_death(int thread_to_die){
    bool signal_state = interrupts_off();
    threads[thread_to_die].state = Destroyed;
    int waiter = threads[thread_to_die].waiter;
    if (waiter != -1){
        assert(threads[waiter].state != Running);
        if (threads[waiter].state == Sleep){
            threads[waiter].state = Running;
//            threads[waiter].exit_code_storage = threads[]
            thread_yield(waiter);
        }
    }
    interrupts_set(signal_state);
}

void
thread_exit(int exit_code)
{
    assert(interrupts_enabled());
    bool signal_state = interrupts_set(false);
    if (current_thread == 0){
        threads[current_thread].exit_code = exit_code;
        handle_death(current_thread);
        thread_yield(THREAD_ANY);
    }
    thread_to_destroy = current_thread;
//    threads[current_thread].state = Done;
    administrative_mode = true;
    threads[current_thread].exit_code = exit_code;
    setcontext(&threads[0].ucontext);
    assert(false);
    interrupts_set(signal_state);
}

Tid
thread_kill(Tid tid)
{
    bool signal_state = interrupts_set(false);
    if (tid == current_thread && !administrative_mode){
        interrupts_set(signal_state);
        return THREAD_INVALID;
    }
    if (tid < 0 || tid >= THREAD_MAX_THREADS){
        interrupts_set(signal_state);
        return THREAD_INVALID;
    }
    if (threads[tid].state == Destroyed){
        interrupts_set(signal_state);
        return THREAD_INVALID;
    }

    if (tid != 0){
        free369(threads[tid].stack_start);
    }
    administrative_mode = false;
    handle_death(tid);
    interrupts_set(signal_state);
    return tid;
}

/**************************************************************************
 * Important: The rest of the code should be implemented in Assignment 2. *
 **************************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc369(sizeof(struct wait_queue));
	assert(wq);
    wq->queue.current_size = 0;
    wq->queue.start = 0;
    wq->queue.end = 0;
	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
//	TBD();
	free369(wq);
}

Tid
thread_sleep(struct wait_queue *queue)
{
    bool enabled = interrupts_off();
    if (queue == NULL){
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
//    print_queue(queue->queue);
//    print_queue(thread_queue);
    threads[thread_id()].state = Sleep;
    queue_push(&(queue->queue), thread_id());
    int thread_num = thread_yield(THREAD_ANY);
    if (thread_num == THREAD_NONE){
        threads[thread_id()].state = Running;
        assert(queue_pop_last(&(queue->queue)) == thread_id());
        interrupts_set(enabled);
        return THREAD_NONE;
    }
    interrupts_set(enabled);
	return thread_num;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
    if (queue == NULL){
        return 0;
    }
    bool enabled = interrupts_off();
    int num = 0;
//    print_queue(queue->queue);
//    print_queue(thread_queue);
    while (true){
        int id = queue_pop(&(queue->queue));
        if (id == ERR_EMPTY){
            break;
        }
        enum State state = threads[id].state;
        assert(state != Running);
        if (state == Sleep){
            threads[id].state = Running;
            queue_push(&thread_queue, id);
            num += 1;
            if (all == 0){
                break;
            }

        }
    }
    interrupts_set(enabled);
	return num;
}

/* suspend current thread until Thread tid exits */
Tid
thread_wait(Tid tid, int *exit_code)
{
    bool signal = interrupts_off();
//    print_queue(thread_queue);
    assert(signal);
    if (!is_valid_thread(tid)){
        interrupts_set(signal);
        return THREAD_INVALID;
    }
    // if the thread has been destroyed previously;
    if (threads[tid].state == Destroyed && threads[tid].exit_code == -SIGKILL){
        interrupts_set(signal);
        return THREAD_INVALID;
    }
    if (tid == thread_id()){
        interrupts_set(signal);
        return THREAD_INVALID;
    }
    if (threads[tid].state != Destroyed){
        if (threads[tid].waiter != -1){
            interrupts_set(signal);
            return THREAD_INVALID;
        }
        threads[tid].waiter = thread_id();
        threads[thread_id()].state = Sleep;
        thread_yield(THREAD_ANY);
        assert(!interrupts_enabled());
    }
//    threads[thread_id()].state = Sleep;
    if (exit_code != NULL){
        *exit_code = threads[tid].exit_code;
    }
    interrupts_set(signal);
    return tid;
}

struct lock {
    struct wait_queue * queue;
    int current;

	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;
	lock = malloc369(sizeof(struct lock));
    struct wait_queue * queue = wait_queue_create();
    lock->queue = queue;
    lock->current = -1;
	assert(lock);
	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);
    assert(lock->current == -1);
    free369(lock->queue);
	free369(lock);
}

void
lock_acquire(struct lock *lock)
{
    bool signals = interrupts_off();
	assert(lock != NULL);
    while (lock->current != -1){
        thread_sleep(lock->queue);
    }
    lock->current = thread_id();
    interrupts_set(signals);
    return;
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);
    bool signals = interrupts_off();
    assert(lock->current == current_thread);
    lock->current = -1;
    thread_wakeup(lock->queue, 1);
    interrupts_set(signals);
    return;
}

struct cv {
    struct wait_queue * queue;
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc369(sizeof(struct cv));
    cv->queue = wait_queue_create();
	assert(cv);
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);
    free369(cv->queue);
	free369(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
    bool signal = interrupts_off();
	assert(cv != NULL);
	assert(lock != NULL);
    assert(lock->current == thread_id());
    lock_release(lock);
    thread_sleep(cv->queue);
    assert(!interrupts_enabled());
    lock_acquire(lock);
    interrupts_set(signal);
    return;
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);
    bool signal = interrupts_off();
    assert(lock->current == current_thread);
    thread_wakeup(cv->queue, 0);
    interrupts_set(signal);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);
    bool signal = interrupts_off();
    assert(lock->current == current_thread);
    thread_wakeup(cv->queue, 1);
    interrupts_set(signal);
}


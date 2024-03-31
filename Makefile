CFLAGS := -g -Wall -Werror -D_GNU_SOURCE #-DDEBUG_USE_VALGRIND $(shell pkg-config --cflags valgrind)

TARGETS := test_basic test_preemptive test_wakeup test_wakeup_all \
        test_wait_alive test_wait_exited test_wait test_wait_kill test_wait_parent \
        test_lock test_cv_signal test_cv_broadcast

OBJS := interrupt.o common.o thread.o malloc369.o wakeup_tests.o

# Make sure that 'all' is the first target
all: depend $(TARGETS)

clean:
	rm -rf core *.o $(TARGETS)

realclean: clean
	rm -rf *~ *.bak .depend *.log *.out

tags:
	etags *.c *.h


$(TARGETS): $(OBJS)

depend:
	$(CC) -MM *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif

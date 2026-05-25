CC = gcc
CFLAGS = -Wall -pthread
TARGETS = scheduler thread_simple thread_scheduler thread_advanced

all: $(TARGETS)

scheduler: scheduler.c
	$(CC) $(CFLAGS) scheduler.c -o scheduler

thread_simple: thread_simple.c
	$(CC) $(CFLAGS) thread_simple.c -o thread_simple

thread_scheduler: thread_scheduler.c
	$(CC) $(CFLAGS) thread_scheduler.c -o thread_scheduler

thread_advanced: thread_advanced.c
	$(CC) $(CFLAGS) thread_advanced.c -o thread_advanced

clean:
	rm -f $(TARGETS)

run_scheduler:
	./scheduler

run_simple:
	./thread_simple

run_scheduler_thread:
	./thread_scheduler

run_advanced:
	./thread_advanced

.PHONY: all clean run_scheduler run_simple run_scheduler_thread run_advanced

# How Scheduler Puts Lock and Sleep

## Introduction
This document explains the flow of how a scheduler puts a thread to sleep and manages locks from scratch. It provides a detailed step-by-step explanation of the process.

## 1. Thread State Transition
When a thread requests a resource that is currently locked or unavailable, the scheduler transitions the thread into a **sleeping state**. The key steps are:

1. **Thread Requests Resource**:
   - The thread attempts to acquire a lock or access a resource.
   - If the resource is unavailable, the thread cannot proceed.

2. **Scheduler Intervenes**:
   - The scheduler identifies that the thread cannot continue execution.
   - It marks the thread as "waiting" or "blocked".

3. **Thread Context Save**:
   - The current state (registers, program counter, etc.) of the thread is saved.
   - This ensures the thread can resume execution later.

4. **Thread Sleep**:
   - The thread is moved to a wait queue associated with the resource.
   - The scheduler selects another thread from the ready queue to execute.

## 2. Lock Mechanism
Locks are used to ensure mutual exclusion when accessing shared resources. The process involves:

1. **Lock Acquisition**:
   - A thread attempts to acquire a lock.
   - If the lock is free, the thread acquires it and proceeds.
   - If the lock is held by another thread, the thread is put to sleep.

2. **Lock Release**:
   - When the thread holding the lock finishes its task, it releases the lock.
   - The scheduler wakes up one or more threads waiting for the lock.

## 3. Sleep and Wake-Up Flow
The flow of putting a thread to sleep and waking it up is as follows:

1. **Sleep**:
   - The thread is added to the wait queue of the resource.
   - The scheduler removes the thread from the ready queue.

2. **Wake-Up**:
   - When the resource becomes available, the scheduler wakes up one or more threads from the wait queue.
   - The threads are moved back to the ready queue.

## 4. Example Code
Here is a simplified example of how locks and sleep are managed:

```c
#include <pthread.h>
#include <stdio.h>

pthread_mutex_t lock;

void *thread_function(void *arg) {
    pthread_mutex_lock(&lock);
    printf("Thread %d acquired the lock\n", *(int *)arg);
    // Simulate work
    sleep(1);
    pthread_mutex_unlock(&lock);
    printf("Thread %d released the lock\n", *(int *)arg);
    return NULL;
}

int main() {
    pthread_t threads[2];
    int thread_ids[2] = {1, 2};

    pthread_mutex_init(&lock, NULL);

    for (int i = 0; i < 2; i++) {
        pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]);
    }

    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&lock);
    return 0;
}
```

## Conclusion
The scheduler plays a critical role in managing thread states and ensuring proper synchronization through locks. This document provides a foundational understanding of the process.
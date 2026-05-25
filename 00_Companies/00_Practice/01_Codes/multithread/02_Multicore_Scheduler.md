# How Scheduler Works in Multicore Environments

## Introduction
This document explains how a scheduler operates in a multicore environment, where multiple CPUs are available. It details the flow of thread management and synchronization across cores.

## 1. Multicore Basics
In a multicore system:
- Each core can execute threads independently.
- The scheduler must ensure efficient utilization of all cores.
- Threads may migrate between cores for load balancing.

## 2. Thread Scheduling in Multicore
The scheduler in a multicore environment performs the following tasks:

1. **Core Assignment**:
   - When a thread is ready to execute, the scheduler assigns it to an available core.
   - The assignment considers factors like core load and thread affinity.

2. **Load Balancing**:
   - The scheduler monitors the load on each core.
   - If one core is overloaded, threads may be migrated to less busy cores.

3. **Thread Synchronization**:
   - Threads running on different cores may need to synchronize.
   - Locks and barriers are used to ensure proper synchronization.

## 3. Locking in Multicore
Locks in a multicore environment must be designed to minimize contention and overhead. Common techniques include:

1. **Spinlocks**:
   - Threads spin in a loop while waiting for a lock.
   - Useful for short critical sections.

2. **Queue-Based Locks**:
   - Threads are queued while waiting for a lock.
   - Reduces contention and ensures fairness.

3. **Per-Core Locks**:
   - Each core has its own lock for certain resources.
   - Reduces contention by localizing access.

## 4. Thread Migration
Thread migration involves moving a thread from one core to another. The steps are:

1. **Identify Overloaded Core**:
   - The scheduler identifies cores with high load.

2. **Select Thread to Migrate**:
   - A thread is selected based on priority and state.

3. **Migrate Thread**:
   - The thread's context is saved on the source core.
   - The context is restored on the destination core.

## 5. Example Code
Here is an example of thread creation and synchronization in a multicore environment:

```c
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t lock;

void *thread_function(void *arg) {
    pthread_mutex_lock(&lock);
    printf("Thread %d running on core %d\n", *(int *)arg, sched_getcpu());
    sleep(1);
    pthread_mutex_unlock(&lock);
    return NULL;
}

int main() {
    pthread_t threads[4];
    int thread_ids[4] = {1, 2, 3, 4};

    pthread_mutex_init(&lock, NULL);

    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]);
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&lock);
    return 0;
}
```

## 6. Challenges
Some challenges in multicore scheduling include:
- **Cache Coherency**: Ensuring data consistency across cores.
- **Thread Affinity**: Balancing performance and resource utilization.
- **Scalability**: Handling a large number of threads efficiently.

## Conclusion
Schedulers in multicore environments must balance load, ensure synchronization, and minimize contention. This document provides an overview of the key concepts and mechanisms.
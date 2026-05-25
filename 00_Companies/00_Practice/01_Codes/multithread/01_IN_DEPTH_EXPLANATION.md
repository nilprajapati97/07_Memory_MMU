# MULTITHREADING: ODD-EVEN SYNC - COMPLETE IN-DEPTH EXPLANATION

## TABLE OF CONTENTS
1. Program Overview & Architecture
2. Thread Lifecycle & Management
3. Synchronization Primitives
4. Execution Flow & State Transitions
5. Memory Model & Ordering
6. Common Pitfalls & Solutions

---

## 1. PROGRAM OVERVIEW & ARCHITECTURE

### What the Program Does

The program uses two POSIX threads to print numbers sequentially from 1 to 100.
One thread prints odd numbers and the other prints even numbers.

The key challenge is synchronization: the threads must coordinate to ensure
that the numbers appear in the correct order and that shared state is
protected from concurrent modification.

### Architecture Diagram

```
MAIN THREAD
    ├─ creates odd thread
    ├─ creates even thread
    ├─ waits for both threads to finish (join)
    └─ performs cleanup

ODD THREAD
    ├─ waits for turn
    ├─ prints odd numbers
    ├─ increments shared counter
    └─ signals even thread

EVEN THREAD
    ├─ waits for turn
    ├─ prints even numbers
    ├─ increments shared counter
    └─ signals odd thread
```

### Shared State

```c
pthread_mutex_t mutex;
pthread_cond_t condition;
int counter;
int turn;
```

- `mutex`: Ensures mutually exclusive access to `counter` and `turn`
- `condition`: Used by threads to wait for the correct turn
- `counter`: Tracks the current number to print
- `turn`: Indicates whose turn it is (0 = odd, 1 = even)

---

## 2. THREAD LIFECYCLE & MANAGEMENT

### Thread Creation

The main thread creates two worker threads using `pthread_create()`.

```c
pthread_create(&thread_odd_id, NULL, thread_print_odd, NULL);
pthread_create(&thread_even_id, NULL, thread_print_even, NULL);
```

This does the following:
- Allocates a thread control block (TCB)
- Allocates stack memory for the new thread
- Sets up the initial CPU state
- Places the thread into the runnable queue

### Thread States

```
NEW -> RUNNABLE -> RUNNING -> BLOCKED/WAITING -> RUNNABLE -> TERMINATED
```

- `NEW`: Thread just created
- `RUNNABLE`: Ready to run but not currently on CPU
- `RUNNING`: Executing on CPU
- `BLOCKED/WAITING`: Waiting on a mutex or condition variable
- `TERMINATED`: Completed execution

### Thread Join

After creation, the main thread calls `pthread_join()` for each worker thread.

```c
pthread_join(thread_odd_id, NULL);
pthread_join(thread_even_id, NULL);
```

`pthread_join()` does the following:
- Blocks the calling thread (main thread)
- Waits until the specified worker thread terminates
- Releases any resources associated with the thread

This ensures that the main thread does not exit before the worker threads finish.

---

## 3. SYNCHRONIZATION PRIMITIVES

### Mutex

A mutex is a mutual exclusion lock.
When a thread locks a mutex, other threads attempting to lock it are blocked.

```c
pthread_mutex_lock(&mutex);
// critical section
pthread_mutex_unlock(&mutex);
```

#### Properties
- Only one thread can hold the lock at a time
- Protects shared state
- Avoids race conditions

#### Critical Section in this program

```c
pthread_mutex_lock(&mutex);
if (turn == 0 && counter % 2 == 1) {
    // print odd number
    counter++;
    turn = 1;
    pthread_cond_signal(&condition);
} else {
    pthread_cond_wait(&condition, &mutex);
}
pthread_mutex_unlock(&mutex);
```

### Condition Variable

A condition variable lets a thread wait until a condition is true.
It is always used together with a mutex.

```c
pthread_cond_wait(&condition, &mutex);
```

`pthread_cond_wait()` atomically:
1. releases the mutex
2. blocks the thread on the condition variable
3. reacquires the mutex when awakened

This avoids busy-waiting and reduces CPU usage.

### Signaling

The thread that changes the condition uses `pthread_cond_signal()`.
In this program, signaling tells the other thread that it can run.

```c
pthread_cond_signal(&condition);
```

---

## 4. EXECUTION FLOW & STATE TRANSITIONS

### Main execution sequence

1. Main thread starts
2. Creates odd thread
3. Creates even thread
4. Waits for both threads to finish via `pthread_join()`
5. Destroys mutex and condition variable
6. Exits

### Odd thread execution

Loop until `counter > LIMIT`:
- Acquire mutex
- If `turn == 0` and number is odd:
  - Print number
  - Increment counter
  - Set `turn = 1`
  - Signal condition
- Else:
  - Wait on condition
- Release mutex

### Even thread execution

Loop until `counter > LIMIT`:
- Acquire mutex
- If `turn == 1` and number is even:
  - Print number
  - Increment counter
  - Set `turn = 0`
  - Signal condition
- Else:
  - Wait on condition
- Release mutex

### Example state progression

At the start:
- `counter = 1`
- `turn = 0`

Odd thread prints 1, sets `turn = 1`.
Even thread prints 2, sets `turn = 0`.

This continues until 100.

---

## 5. MEMORY MODEL & ORDERING

### Why memory ordering matters

Modern CPUs can reorder instructions to optimize performance.
Mutex operations enforce ordering, so changes are visible to other threads.

### Acquire and release semantics

`pthread_mutex_lock` acts as a memory barrier before entering the critical section.
`pthread_mutex_unlock` acts as a memory barrier before leaving the critical section.

This guarantees:
- Writes before unlock are visible after lock by another thread
- Reads after lock see writes performed before the unlock

### Example with shared variables

Without synchronization:
- Thread A may write `counter = 2`
- Thread B may still see an old value

With mutex synchronization:
- Thread A releases the mutex after writing
- Thread B acquires the mutex and sees the updated value

---

## 6. COMMON PITFALLS & SOLUTIONS

### Pitfall 1: Missing mutex lock

If `counter` is accessed without a mutex, the result is undefined.

```c
counter++;
```

This can lead to lost updates and incorrect prints.

### Pitfall 2: Using `if` instead of `while` with condition variables

Condition variables can wake spuriously.
Always use a loop to recheck the condition.

```c
while (counter <= LIMIT && !(turn == 0 && counter % 2 == 1)) {
    pthread_cond_wait(&condition, &mutex);
}
```

### Pitfall 3: Deadlock

Deadlock occurs if a thread holds the mutex while waiting for a condition
that requires another thread to acquire the same mutex.

In this program, `pthread_cond_wait()` avoids deadlock by releasing the mutex
while waiting.

### Pitfall 4: Starvation

Starvation could occur if one thread never gets signaled.
In this design, explicit turn switching ensures fairness.

---

## SUMMARY

This program covers:
- POSIX thread creation
- Mutexes and condition variables
- Proper join and cleanup
- Safe shared state updates
- Prevention of race conditions

It is a strong foundation for interview discussions on threading and synchronization.

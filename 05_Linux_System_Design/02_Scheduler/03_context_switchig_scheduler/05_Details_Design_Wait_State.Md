
---

# Multithreading Deep Dive — Odd/Even Printer with Mutex + Condition Variable

---

## 1. Problem Statement (Design from Scratch)

**Goal:** Print numbers 1 to 100 using exactly two threads — one prints odd numbers, one prints even numbers — **in strict sequential order** (1, 2, 3, … 100).

**Why is this hard?**  
Two threads run concurrently. Without synchronization they will both grab `counter` at the same time, skip values, or print out of order. We need a way to make them **take turns**.

**Solution Design Choices:**

| Tool | Purpose |
|---|---|
| `pthread_mutex_t lock` | Ensures only one thread reads/writes `counter` at a time (mutual exclusion) |
| `pthread_cond_t cond` | Allows a thread to **sleep** until it's its turn (avoids busy-waiting) |
| `bool is_odd_turn` | Shared flag that says whose turn it is |

---

## 2. Global State (Shared Variables)

```c
int counter = 1;                          // shared counter, starts at 1
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;  // protects counter + is_odd_turn
pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;   // signaling mechanism
bool is_odd_turn = true;                  // odd thread goes first
```

**Memory Layout:**

```
┌────────────────────────────────────────────────────────┐
│                     SHARED MEMORY                      │
│   counter = 1                                          │
│   lock    = (unlocked)                                 │
│   cond    = (no waiters)                               │
│   is_odd_turn = true                                   │
└────────────────────────────────────────────────────────┘
```

---

## 3. Thread Lifecycle — All 4 States Explained

Every POSIX thread can be in one of these states:

```
  ┌──────────┐   pthread_create()    ┌──────────┐
  │  NEW /   │ ───────────────────▶  │  READY   │
  │ CREATED  │                       │(Runnable)│
  └──────────┘                       └────┬─────┘
                                          │  OS Scheduler picks it
                                          ▼
  ┌──────────┐   mutex_lock blocks   ┌──────────┐
  │ BLOCKED  │ ◀──────────────────── │ RUNNING  │
  │(on mutex)│                       │(on CPU)  │
  └────┬─────┘                       └────┬─────┘
       │  mutex becomes free              │  cond_wait()
       │  OS wakes this thread            ▼
       └──────────────────────▶   ┌──────────────┐
                                  │   WAITING    │
                                  │(on cond var) │
                                  └──────┬───────┘
                                         │  cond_signal() received
                                         │  + re-acquires mutex
                                         └──────────────────▶ RUNNING
```

| State | What the thread is doing | CPU usage |
|---|---|---|
| **READY** | Created, waiting for OS to schedule it | 0% |
| **RUNNING** | Holds the lock, doing real work (print, increment, signal) | 100% of 1 core |
| **BLOCKED** | Waiting to acquire the mutex (another thread holds it) | 0% |
| **WAITING** | Sleeping inside `pthread_cond_wait()` — voluntarily parked | 0% |

---

## 4. Execution Timeline — Step by Step from `main()`

### Phase 0 — `main()` starts

```
main thread
│
├── pthread_create(&t1, NULL, print_odd, NULL)   ← t1 born, enters READY state
├── pthread_create(&t2, NULL, print_even, NULL)  ← t2 born, enters READY state
├── pthread_join(t1, NULL)   ← main BLOCKS here, waiting for t1 to exit
└── pthread_join(t2, NULL)   ← main will BLOCK here after t1 exits
```

At this point `main` is suspended. OS schedules t1 and t2.

---

### Phase 1 — First Iteration (counter = 1)

```
TIME ──────────────────────────────────────────────────────────▶

t1 (Odd)   [READY]──▶[RUNNING: lock()]──▶[RUNNING: check is_odd_turn=TRUE → print 1]
                                          [RUNNING: counter=2, is_odd_turn=false]
                                          [RUNNING: signal(cond)]
                                          [RUNNING: unlock()]──▶[WAITING: cond_wait]

t2 (Even)  [READY]──▶[BLOCKED: lock()]...waiting for t1 to unlock
                                          [RUNNING: lock()]──▶[check is_odd_turn=FALSE → print 2]
                                          [RUNNING: counter=3, is_odd_turn=true]
                                          [RUNNING: signal(cond)]
                                          [RUNNING: unlock()]──▶[WAITING: cond_wait]
```

---

### Phase 2 — `pthread_cond_wait()` internals (Critical for Interviews)

When a thread calls `pthread_cond_wait(&cond, &lock)`:

```
Step 1: Atomically RELEASE the mutex
        ↓
Step 2: Put thread to sleep (WAITING state — 0% CPU)
        ↓
Step 3: (waits for pthread_cond_signal)
        ↓
Step 4: Wake up, re-acquire the mutex
        ↓
Step 5: Return from cond_wait, re-check condition in while() loop
```

**Why the `while` loop instead of `if`?**  
Because of **spurious wakeups** — the OS can wake a thread even without a signal. The `while` re-checks the condition and puts the thread back to sleep if it's not really its turn.

```c
// CORRECT — while loop re-checks condition
while (!is_odd_turn && counter <= MAX)
    pthread_cond_wait(&cond, &lock);

// WRONG — if would suffer from spurious wakeups
if (!is_odd_turn && counter <= MAX)
    pthread_cond_wait(&cond, &lock);
```

---

## 5. Thread State Table — Full 100-Iteration Breakdown

Each thread runs **50 times** (odd thread: 1,3,5…99 → 50 iterations; even thread: 2,4,6…100 → 50 iterations).

```
Iteration   t1 (Odd)                     t2 (Even)
─────────   ──────────────────────────   ──────────────────────────
    1       READY → RUNNING (prints 1)   READY → BLOCKED → WAITING
    2       WAITING                      WAITING → RUNNING (prints 2)
    3       WAITING → RUNNING (prints 3) WAITING
    4       WAITING                      WAITING → RUNNING (prints 4)
   ...
   99       WAITING → RUNNING (prints 99) WAITING
  100       WAITING → RUNNING (checks counter=101 > MAX → exits)
            [or t2 prints 100 and exits, t1 wakes and exits]
```

**Time distribution per thread (approximate):**

| Thread | RUNNING time | WAITING time | Ratio |
|---|---|---|---|
| t1 (Odd) | 50 × ~2µs = ~100µs | 50 × ~2µs = ~100µs | 50% / 50% |
| t2 (Even) | 50 × ~2µs = ~100µs | 50 × ~2µs = ~100µs | 50% / 50% |

> Note: actual µs values depend on hardware and OS scheduling. The key point is each thread spends roughly **equal time running and waiting**.

---

## 6. Termination Logic — How Threads Exit Cleanly

This is the trickiest part. When `counter > MAX`:

```
counter reaches 101

Thread that just incremented:
  - sets is_odd_turn (to signal the other thread)
  - calls pthread_cond_signal() — wakes the sleeping thread
  - unlocks mutex
  - loops back, locks mutex
  - checks: counter (101) > MAX → unlocks and breaks

Other thread (was sleeping in cond_wait):
  - wakes up
  - re-acquires mutex
  - exits the while() because counter (101) > MAX OR is_odd_turn is correct
  - checks: counter > MAX → unlocks and breaks
```

**Why `counter <= MAX` is inside the `while` condition:**

```c
while (!is_odd_turn && counter <= MAX)   // ← counter > MAX breaks the wait loop
    pthread_cond_wait(&cond, &lock);

if (counter > MAX) {                     // ← second check for the active thread
    pthread_mutex_unlock(&lock);
    break;
}
```

Without `counter <= MAX` in the `while`, the last sleeping thread would wait forever (deadlock) because no one will signal it again after both have finished.

---

## 7. Full Execution Flow Diagram

```
main()
  │
  ├─ pthread_create(t1) ─────────────────────────────────────────────────┐
  │                                                                       │
  ├─ pthread_create(t2) ─────────────────────────────────┐               │
  │                                                       │               │
  ├─ pthread_join(t1) ─ BLOCKS                           │               │
  │                                                       ▼               ▼
  │                                                   t2 (Even)      t1 (Odd)
  │                                                   READY          READY
  │
  │     ┌──────────── LOOP 50 times ────────────────────────────────────┐
  │     │                                                               │
  │     │  t1: lock() → check → print(odd) → counter++ → signal → unlock → cond_wait
  │     │  t2: lock() → check → print(even) → counter++ → signal → unlock → cond_wait
  │     │                                                               │
  │     └───────────────────────────────────────────────────────────────┘
  │
  │     t1 exits (counter > MAX) → pthread_join(t1) returns
  │     t2 exits (counter > MAX) → pthread_join(t2) returns
  │
  └─ printf("Both threads finished...")
     return 0
```

---

## 8. Interview Talking Points (Key Concepts)

### Q: Why not use two separate condition variables?

You could use `cond_odd` and `cond_even` for more precise signaling. The single `cond` works here because only one thread is ever waiting at a time (they alternate). A second `cond` would prevent any theoretical spurious cross-wakeup.

### Q: What is a race condition and is there one here?

A race condition occurs when the correctness of a program depends on the relative timing of threads. Here there is **no race condition** because `counter` and `is_odd_turn` are always accessed while holding `lock`.

### Q: What happens if you remove the mutex?

```
t1 reads counter = 5  (not yet printed)
t2 reads counter = 5  (same value!)
t1 prints 5, increments to 6
t2 prints 5 again (stale read), increments to 7
→ 5 printed twice, 6 skipped
```

### Q: What is the difference between `pthread_cond_signal` and `pthread_cond_broadcast`?

| | `signal` | `broadcast` |
|---|---|---|
| Wakes | 1 waiting thread | ALL waiting threads |
| Use here | Correct — only 1 thread waits on `cond` at a time | Would also work but wasteful |

### Q: Could this deadlock?

Only if the last signal is never sent. The code correctly calls `pthread_cond_signal` **before** `pthread_mutex_unlock` on every path, and the `while (!is_odd_turn && counter <= MAX)` condition ensures the waiting thread exits when `counter > MAX` even without a final signal.

---

## 9. Summary Table

```
┌─────────────────┬──────────────┬───────────────┬─────────────────────┐
│   Thread        │  Total Runs  │  Times Waited │  Total Iterations   │
├─────────────────┼──────────────┼───────────────┼─────────────────────┤
│ t1 (print_odd)  │     50       │      50       │  prints 1,3..99     │
│ t2 (print_even) │     50       │      50       │  prints 2,4..100    │
│ main            │ waits on join│      –        │  creates + joins    │
└─────────────────┴──────────────┴───────────────┴─────────────────────┘
```

**Key synchronization tools used:**
- `pthread_mutex_t` — mutual exclusion over shared data
- `pthread_cond_t` — voluntary sleeping while waiting for a condition
- `bool is_odd_turn` — logical token passed between threads
- `while()` loop around `cond_wait` — protection against spurious wakeups
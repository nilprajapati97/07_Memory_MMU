## Wake / Sleep Flow in this Program

There is **one shared condition variable** `cond` and **one mutex** `lock`. Both threads share them. Here is exactly what happens:

---

### Startup — both threads race for the lock

```
main() creates t1 (ODD) and t2 (EVEN) — both start simultaneously
```

```
ODD                                 EVEN
───────────────────────────────     ───────────────────────────────
pthread_mutex_lock(&lock)  ✓        pthread_mutex_lock(&lock)  ✗ BLOCKED
                                    (waiting for ODD to release)
```

ODD wins the lock first (race — OS decides).

---

### Cycle 1 — ODD runs, EVEN sleeps

```
ODD                                      EVEN
─────────────────────────────────────    ─────────────────────────────────
is_odd_turn = true → skip while loop
print "Odd Thread: 1"
counter = 2
is_odd_turn = false
pthread_cond_signal(&cond)  ──────────►  EVEN wakes (moves to runnable)
pthread_mutex_unlock(&lock) ──────────►  EVEN acquires lock
                                         while(is_odd_turn) → FALSE now
                                         → exits wait loop
                                         print "Even Thread: 2"
                                         counter = 3
                                         is_odd_turn = true
                                         pthread_cond_signal(&cond) ──►  ODD wakes
                                         pthread_mutex_unlock(&lock) ──► ODD acquires lock
ODD: while(!is_odd_turn) → FALSE
→ exits wait loop
print "Odd Thread: 3"
...
```

---

### What happens inside `pthread_cond_wait` — the critical detail

When EVEN calls `pthread_cond_wait(&cond, &lock)`:

```
Step 1:  EVEN atomically RELEASES lock  ←── ODD can now acquire it
Step 2:  EVEN goes to SLEEP             ←── zero CPU, kernel parks the thread
                   ...ODD is printing...
Step 3:  ODD calls pthread_cond_signal  ←── kernel marks EVEN as runnable
Step 4:  EVEN wakes, RE-ACQUIRES lock   ←── blocks until ODD releases it
Step 5:  EVEN returns from cond_wait    ←── now checks condition in while loop
```

Steps 1+2 are **atomic** — there is no gap where EVEN has released the lock but not yet gone to sleep. This prevents the **lost wakeup** bug.

---

### Lost wakeup — why atomic release+sleep matters

```c
// If they were separate (WRONG — hypothetical):
pthread_mutex_unlock(&lock);   // <-- ODD signals HERE (EVEN misses it!)
pthread_cond_wait_only(&cond); // <-- EVEN sleeps forever
```

`pthread_cond_wait` combines these two into one atomic operation so the signal can **never arrive in that gap**.

---

### State machine for each thread

```
         pthread_mutex_lock
              │
              ▼
         ┌─────────┐   condition false    ┌──────────────┐
         │ RUNNING │ ──────────────────►  │  SLEEPING    │
         │(has lock)│                     │(lock released│
         └─────────┘                      │ kernel queue)│
              ▲                           └──────────────┘
              │                                  │
              │   cond_signal + re-acquire lock   │
              └───────────────────────────────────┘
```

---

### Signal semantics — only wakes ONE waiter

```c
pthread_cond_signal(&cond);   // wakes exactly ONE sleeping thread
```

Since only one thread waits at a time here (ODD sleeps while EVEN runs and vice versa), this is fine. If multiple threads waited you'd need `pthread_cond_broadcast`.

---

### Complete ping-pong timeline for counter 1→4

```
Time  ODD                         EVEN                    is_odd_turn  counter
────  ──────────────────────────  ──────────────────────  ───────────  ───────
 t0   lock ✓                      lock ✗ (blocked)        true         1
 t1   condition OK, print 1       sleeping                true→false   1→2
 t2   signal ──────────────────►  wakes, waits for lock   false        2
 t3   unlock ──────────────────►  lock ✓                  false        2
 t4   lock ✗ (blocked)            condition OK, print 2   false→true   2→3
 t5   wakes, waits for lock        signal ───────────────► true        3
 t6   lock ✓                      unlock                   true        3
 t7   condition OK, print 3       sleeping                true→false   3→4
```

---

### One-line summary per API call

| Call | What it does |
|---|---|
| `pthread_mutex_lock` | One thread enters the critical section; other blocks |
| `pthread_cond_wait` | **Releases lock + sleeps atomically**; re-acquires lock on wakeup |
| `pthread_cond_signal` | Wakes **one** thread sleeping on that condition variable |
| `pthread_mutex_unlock` | Exits critical section; woken thread can now acquire lock | 


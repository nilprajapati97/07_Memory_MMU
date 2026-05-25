# 05 — Semaphore

## Problem
Implement a counting semaphore: `wait` (P) decrements the count, blocking if zero; `post` (V) increments, waking one waiter if any.

## Why It Matters
Generalises mutex (binary) and condition variables; models bounded resources directly (slots, permits, tokens). Foundation of producer-consumer and many classic concurrency problems.

## Approaches

### Approach 1 — Mutex + Condvar (User-Space Implementation)
```text
struct sem { int count; mutex m; cond c; }

wait(s):
    lock(s.m)
    while s.count == 0: cond_wait(s.c, s.m)
    s.count--
    unlock(s.m)

post(s):
    lock(s.m)
    s.count++
    cond_signal(s.c)
    unlock(s.m)
```
- Correct, portable; ~10 lines.
- Performance: every op grabs the mutex.

### Approach 2 — POSIX `sem_t`
```text
sem_t s; sem_init(&s, 0, initial);
sem_wait(&s);  /* P */
sem_post(&s);  /* V */
```
- Standard POSIX (unnamed) semaphore.
- `sem_post` is **async-signal-safe** — usable from signal handlers (special trick).
- On Linux, internally uses futex.

### Approach 3 — Binary Semaphore (Mutex Substitute)
Initial count = 1; `wait` is "lock", `post` is "unlock".
- **Important difference from a mutex**: a binary semaphore has no "owner" — any thread can `post` it. Useful for signalling between threads (one waits, another posts), but **wrong** for mutual exclusion when ownership matters (no priority inheritance, no double-lock detection, no error on cross-thread unlock).

### Approach 4 — Futex-Based (Linux Internals)
Atomic `count` + `futex_wait` / `futex_wake` for the slow path.
```text
wait:
    while true:
        c = atomic_load(count)
        if c > 0 and CAS(count, c, c-1): return
        futex_wait(count, 0)
post:
    atomic_fetch_add(count, 1)
    futex_wake(count, 1)
```
- Fast path: one CAS, no syscall.
- Slow path: only block when truly empty.
- Same family as glibc's `pthread_mutex_t`.

### Approach 5 — Counting Semaphore from Two Binary Semaphores + Counter
Historical (Dijkstra). Useful only as a teaching exercise; real implementations use futex.

## Comparison
| Approach | Speed | Code | Special property |
|---|---|---|---|
| Mutex + cv | medium | 10 lines | portable, easy |
| POSIX `sem_t` | high | one line | async-signal-safe `sem_post` |
| Binary as mutex | medium | n/a | no owner concept (good or bad) |
| Futex-based | very high (fast path) | medium | Linux only |
| Two-binary-from-counting | low | high | teaching |

## Key Insight
A semaphore is a **counted permit pool**:
- `wait` consumes a permit (block if none).
- `post` produces a permit (wake any one waiter).
This abstraction subsumes mutual exclusion (1 permit), producer-consumer (slots & items), barriers, and signal counting.

## Pitfalls
- Treating binary semaphore as a mutex when **priority inheritance** matters → priority inversion (Mars Pathfinder bug)
- Forgetting to handle `EINTR` on `sem_wait` — signals can interrupt it; retry in a loop
- `sem_post` from an ISR via `sem_t` is signal-safe but **not interrupt-safe on all OSes** — check your RTOS docs (FreeRTOS uses `xSemaphoreGiveFromISR`)
- Lost `post` if waiter starts after the post and count is already non-zero → fine, semaphore is stateful (unlike condvars)
- Maximum count overflow — POSIX limit is `SEM_VALUE_MAX` (usually `INT_MAX`)
- Named semaphores (`sem_open`) live in the file system; clean up with `sem_unlink`
- Destroying a semaphore while threads are waiting on it → UB; ensure quiesce

## Interview Tips
1. State P/V or `wait/post` definitions clearly; many candidates muddle them.
2. Lead with the mutex+condvar implementation — shows you understand the semantics, not just the API.
3. Mention `sem_post` async-signal-safety (rare and useful — write data, post to wake main loop).
4. Distinguish binary semaphore from mutex on **ownership** and **priority inheritance**.
5. Talk about producer-consumer as the natural application.

## Related / Follow-ups
- [02_producer_consumer](../02_producer_consumer/) — canonical use
- [08_mutex_semaphore_spinlock](../08_mutex_semaphore_spinlock/) — when to pick which
- [20_signal_handler](../20_signal_handler/) — `sem_post` as the safe wake
- Priority inversion / inheritance, `PTHREAD_PRIO_INHERIT`
- Linux futex API (`man 2 futex`)

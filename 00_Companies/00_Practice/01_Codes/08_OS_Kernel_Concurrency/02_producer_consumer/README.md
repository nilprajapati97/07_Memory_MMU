# 02 — Producer–Consumer (Mutex + Condition Variable)

## Problem
One or more producer threads put items into a bounded buffer; one or more consumers take them out. Producers block when full; consumers block when empty. No busy waiting, no lost wakeups.

## Why It Matters
The canonical synchronisation problem. Every real-world pipeline (logging, packet processing, request queue, audio playback) is a producer–consumer. Trains correct condvar usage and the **lost-wakeup** bug awareness.

## Approaches

### Approach 1 — Mutex + Two Condition Variables (Best)
Separate `not_full` (waited on by producers) and `not_empty` (waited on by consumers).
```text
mutex m
cond  not_full, not_empty
buffer Q (cap N)

produce(x):
    lock(m)
    while Q.size == N:  wait(not_full, m)
    Q.push(x)
    signal(not_empty)
    unlock(m)

consume():
    lock(m)
    while Q.empty:  wait(not_empty, m)
    x = Q.pop()
    signal(not_full)
    unlock(m)
    return x
```
- `while` (not `if`) protects against spurious wakeups.
- Signal **inside** the lock (or right after unlock) — either is correct; inside is simpler.
- Scales to multiple producers/consumers.

### Approach 2 — Mutex + One Condvar + Broadcast
Use a single condvar; producers and consumers both wait on it; signaller calls `broadcast`.
- Simpler but wastes wakeups (everyone wakes, all but one go back to sleep).
- Fine for low-contention, bad under load.

### Approach 3 — Two Counting Semaphores + Mutex
```text
sem_t empty_slots = N    // available producer slots
sem_t full_slots  = 0    // available items for consumers
mutex m                   // protects buffer

produce(x):
    sem_wait(empty_slots)
    lock(m); Q.push(x); unlock(m)
    sem_post(full_slots)

consume():
    sem_wait(full_slots)
    lock(m); x = Q.pop(); unlock(m)
    sem_post(empty_slots)
    return x
```
- Elegant; semaphore semantics naturally model "available count".
- The mutex protects the buffer state itself (still needed for non-trivial Q).
- For SPSC: drop the mutex — semaphores alone suffice.

### Approach 4 — Lock-Free SPSC Ring (See [01_ring_buffer](../01_ring_buffer/))
For single producer & single consumer, use the power-of-two ring with release/acquire — no mutex, no condvar.
- Producer/consumer cannot block; need a wait strategy (spin, sleep, condvar-on-empty).

### Approach 5 — Channel-Style (Higher-Level)
- POSIX message queue (`mq_*`)
- pipe / socketpair
- Linux `eventfd` for signalling
Wraps the synchronisation; trades flexibility for less code.

## Comparison
| Approach | Wakeups | Code complexity | Throughput | When to use |
|---|---|---|---|---|
| **Mutex + 2 cv** | precise | medium | high | **Default MPMC** |
| Mutex + 1 cv broadcast | wasteful | low | medium | Low contention only |
| 2 semaphores + mutex | precise | medium | high | Elegant; semaphore-favoured APIs |
| SPSC lock-free | n/a | high | very high | Strict 1-to-1, low latency |
| OS channel (mq/pipe) | precise | very low | OS-dependent | Cross-process or simple |

## Key Insight
The **two-condvar pattern** assigns one wake-source to each waiter class:
- Producers wait on "buffer has room" → consumers signal it after popping.
- Consumers wait on "buffer has items" → producers signal it after pushing.

This avoids broadcasting and ensures only a thread that *can* proceed gets woken.

## Pitfalls
- **`if` instead of `while`** on condvar wait → spurious wakeups + multiple producers/consumers race after wakeup → consume from empty buffer / push into full buffer
- **Lost wakeup**: signalling before any thread has called `wait` — but condvars are stateless so the signal is "lost". The `while (cond)` guard fixes it as long as state is checked under the lock before waiting.
- Signalling without holding the mutex → also fine, but you must update the predicate under the lock first.
- `pthread_cond_wait` releases the mutex atomically with the wait — without that atomicity, lost-wakeup is unavoidable.
- Using a single condvar and `signal` (not `broadcast`) when mixing producers and consumers can wake the wrong class → permanent deadlock.
- Semaphore approach: doing the buffer push before `sem_wait(empty_slots)` → races.
- Spurious wakeups happen on Linux (futex retry) and on macOS — never assume "I was signalled".

## Interview Tips
1. Always write `while (predicate) cond_wait(...)`. Interviewers grade on this.
2. State the lost-wakeup story: "cond signal is stateless, hence the while + locked predicate".
3. Offer two-semaphore version as an alternative; mention SPSC ring as the low-latency variant.
4. If asked for unbounded queue → drop the `not_full` wait; otherwise structure is identical.

## Related / Follow-ups
- [03_thread_safe_queue](../03_thread_safe_queue/)
- [05_semaphore](../05_semaphore/), [08_mutex_semaphore_spinlock](../08_mutex_semaphore_spinlock/)
- [07_deadlock](../07_deadlock/)
- Bounded buffer = bounded backpressure (real-world flow control)
- `eventfd` / `signalfd` for integrating with `epoll`

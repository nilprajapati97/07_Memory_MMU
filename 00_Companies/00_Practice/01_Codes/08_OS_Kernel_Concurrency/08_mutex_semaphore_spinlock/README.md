# 08 — Mutex vs Semaphore vs Spinlock — When to Use Which

## Problem
Three primitives that all provide synchronisation but differ in ownership, blocking behaviour, cost, and legal contexts.

## Why It Matters
Choosing wrong gives correctness bugs (binary semaphore as "mutex" → priority inversion) or performance disasters (spinlock around an I/O call → cores spin for milliseconds).

## The Primitives

### Mutex
- **Ownership**: yes — only the locking thread may unlock.
- **Blocking**: yes — sleeper goes to kernel wait queue.
- **Recursive**: optional (`PTHREAD_MUTEX_RECURSIVE`).
- **Priority inheritance**: available (`PTHREAD_PRIO_INHERIT`).
- **Use in ISR**: **no** (can sleep).
- Backed by futex on Linux: fast path is atomic CAS in user space; slow path syscalls.

### Counting Semaphore
- **Ownership**: none — any thread may `post`.
- **Blocking**: yes (on `wait` when count is zero).
- **Counts**: ≥ 0 — natural fit for "N permits".
- **Use in ISR**: `sem_post` is async-signal-safe; FreeRTOS provides `xSemaphoreGiveFromISR`.
- **No priority inheritance** in POSIX → don't use as a mutex if PI matters.

### Spinlock
- **Ownership**: usually no (kernel variants track owner for debug).
- **Blocking**: **no** — busy-waits; consumes CPU.
- **Use in ISR**: **yes** (kernel — `spin_lock_irqsave`).
- **Hold time**: must be **shorter than a context switch** (~1–10 µs) or you waste cores.

## Decision Tree
```
Critical section in interrupt handler?
 └─ yes → spinlock (kernel: spin_lock_irqsave)
 └─ no  → critical section longer than a few µs?
          └─ yes → mutex
          └─ no  → contention high? cores plenty?
                   └─ yes → spinlock OK (user-space)
                   └─ no  → mutex (adaptive ones spin a bit anyway)

Need to model N permits / produce-consume / signal?
 └─ semaphore

Need ownership / recursion / priority inheritance?
 └─ mutex (NOT binary semaphore)
```

## Cost Model (Order-of-Magnitude)
| Op | Uncontended | Contended (sleep) |
|---|---|---|
| Atomic CAS | ~10 cycles | n/a |
| Cached spinlock acquire | ~30 cycles | unbounded (busy) |
| Mutex acquire (futex fast path) | ~25 ns | µs (syscall + wake) |
| Semaphore post + wake | ~µs | µs |
| Context switch | several µs |

## Comparison Table
| Property | Mutex | Semaphore | Spinlock |
|---|---|---|---|
| Owner | yes | no | optional |
| Blocks (sleeps) | yes | yes | **no** |
| Usable in ISR | no | post: yes; wait: no | yes |
| Counts > 1 | no | yes | no |
| Priority inheritance | yes (opt-in) | no | n/a |
| Best hold time | µs–ms | any | sub-µs |
| Cost when uncontended | very low | low | very low |
| Cost when contended | sleep (OK) | sleep (OK) | burns CPU |
| Recursive variant | yes | no | depends |

## Key Insight
- **Mutex** = exclusion with ownership; the right default.
- **Semaphore** = signalling / counting permits across thread boundaries; not a mutex substitute.
- **Spinlock** = exclusion **when sleeping is forbidden or wasteful**; kernel ISR or sub-µs hot path.

Ownership and sleep-or-spin are orthogonal; treating them as the same axis is what causes mis-selection.

## Pitfalls
- Binary semaphore as a mutex → no PI → priority inversion (Mars Pathfinder)
- Mutex in an ISR → kernel panic / `BUG_ON` (sleeping in atomic context)
- Spinlock around any call that might sleep (`kmalloc(GFP_KERNEL)`, copy_from_user, I/O) → deadlock / panic
- Releasing a mutex from a thread that doesn't own it → UB (some platforms detect)
- Recursive mutex used to "cover up" calling self-with-lock-held — usually a design smell
- Using `pthread_mutex_t` across processes → must opt in (`PTHREAD_PROCESS_SHARED`) and live in shared memory
- `sem_t` named vs unnamed; named persist in the file system until `sem_unlink`
- Cancellation: `pthread_mutex_lock` is **not** a cancellation point; `sem_wait` **is** — design accordingly

## Interview Tips
1. State the three axes: ownership, sleep-vs-spin, count.
2. Always volunteer the ISR rule: only spinlocks (and certain "give from ISR" primitives) are legal in interrupt context.
3. Mention futex as the implementation under glibc mutex/semaphore — shows depth.
4. The Mars Pathfinder story (priority inversion) gets bonus points.

## Related / Follow-ups
- [04_spinlock](../04_spinlock/), [05_semaphore](../05_semaphore/), [07_deadlock](../07_deadlock/)
- Priority inheritance protocol (PIP), priority ceiling protocol (PCP)
- Linux futex (`man 2 futex`), `pthread_mutexattr_setprotocol`
- RT-Linux & PREEMPT_RT — sleeping spinlocks in user-process kernel context

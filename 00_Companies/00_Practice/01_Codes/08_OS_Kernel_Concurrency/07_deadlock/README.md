# 07 — Deadlock

## Problem
Two or more threads are each waiting on a resource the other holds → forward progress stops forever. Detect, prevent, recover.

## Why It Matters
The most expensive concurrency bug class: invisible in unit tests, devastating in production. Interviewers want the four conditions, a prevention strategy, and how to debug it post-mortem.

## Coffman's Four Necessary Conditions
1. **Mutual exclusion** — resource non-shareable
2. **Hold and wait** — thread holds resource A while requesting B
3. **No preemption** — resources released only voluntarily
4. **Circular wait** — cycle in the wait-for graph

Breaking **any one** prevents deadlock.

## Approaches (To Prevent / Avoid / Detect)

### Approach 1 — Lock Ordering (Break Circular Wait)
Define a global total order on all locks; always acquire in that order.
```text
if &lockA < &lockB:
    lock(lockA); lock(lockB)
else:
    lock(lockB); lock(lockA)
```
- Simple, scalable, the **standard real-world fix**.
- Requires discipline; static analysers (lockdep) help.

### Approach 2 — Lock Hierarchy / Levels
Assign each lock a level number; thread may only acquire locks of strictly higher levels than any it already holds.
- Generalises ordering across modules.
- Linux's `lockdep` enforces this at runtime; reports violations.

### Approach 3 — Try-Lock with Back-Off (Break Hold-and-Wait)
```text
loop:
    lock(A)
    if try_lock(B): break
    unlock(A)
    sleep(jittered backoff)
```
- Avoids deadlock without imposing global order.
- Risk: livelock — both threads back off and retry in lockstep. Jitter the backoff.

### Approach 4 — Single Coarse Lock (Break Mutual Exclusion at Higher Level)
Replace fine-grained locks with one big lock. Trades concurrency for safety. Sometimes the right call.

### Approach 5 — Acquire All At Once (Two-Phase Locking)
Acquire all needed locks atomically (`std::lock` in C++, multi-lock primitives). Eliminates partial-hold races.

### Approach 6 — Detection + Recovery (Break No-Preemption)
Build a wait-for graph, run cycle detection periodically; on cycle, abort one transaction and release its locks.
- Databases use this. Hard in general C, easy in DB world.

## Comparison
| Strategy | Breaks | Cost | Use when |
|---|---|---|---|
| **Lock ordering** | circular wait | discipline | **default in most code** |
| Hierarchy + lockdep | circular wait | runtime check | kernels, complex code |
| Try-lock + backoff | hold-and-wait | wasted work | order undefined dynamically |
| One big lock | mutual exclusion (at scale) | low concurrency | starting point |
| Acquire-all (`std::lock`) | partial hold | atomicity helper | known set of locks |
| Detection + abort | no preemption | overhead, rollback | databases, transactional code |

## Banker's Algorithm (Avoidance — Mostly Academic)
Given max resource needs of each thread, only grant a request if the resulting state is **safe** (some scheduling sequence completes). Practical drawbacks:
- Requires advance knowledge of maximum needs (rare in real code).
- Conservative; rejects safe states.
- Mostly taught for theory, not used in production.

## Pitfalls
- "I only have one lock" — false: malloc takes its own lock; printf takes stdout lock; logging your error path takes another lock
- Signals/interrupts triggering callbacks that grab a held lock → self-deadlock
- `fork()` in multithreaded program: child inherits a copy with locks held but not the threads → child must only call async-signal-safe functions
- Holding a lock across a blocking I/O call → everyone waits on disk
- Recursive locking when the mutex isn't recursive → self-deadlock
- ABBA across modules where one team owns lock A and another lock B, no one sees both
- Priority inversion masquerading as deadlock — high-priority thread waits on a lock held by low-priority thread that never gets scheduled; fixes: priority inheritance / ceiling

## Diagnosing
- `gdb` → `thread apply all bt` → look for matching pairs of `__lll_lock_wait`
- Linux `lockdep` (`CONFIG_PROVE_LOCKING`) catches violations in kernel
- `valgrind --tool=helgrind` / `--tool=drd` — user-space lock-order checker
- Thread Sanitizer (`-fsanitize=thread`) — race + some lock-order issues
- Application-level lock graph instrumentation

## Interview Tips
1. State the four Coffman conditions verbatim — interviewers want them.
2. Lead the fix with **lock ordering**; it's the standard answer.
3. Mention try-lock+backoff with jitter (avoid livelock).
4. Volunteer priority inversion as a related-but-distinct failure mode (Mars Pathfinder story).
5. Mention `fork()` + locks if asked about multi-process pitfalls.

## Related / Follow-ups
- [04_spinlock](../04_spinlock/), [05_semaphore](../05_semaphore/), [08_mutex_semaphore_spinlock](../08_mutex_semaphore_spinlock/)
- Priority inversion, priority inheritance/ceiling
- Linux `lockdep`, Valgrind Helgrind/DRD, ThreadSanitizer
- Database deadlock detection (wait-for graph)

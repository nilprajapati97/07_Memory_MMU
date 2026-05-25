# 06 — Reader–Writer Lock

## Problem
Allow multiple concurrent readers OR one exclusive writer. Trade-off: which class gets preference, and how to avoid starving the other.

## Why It Matters
Read-heavy workloads (caches, routing tables, config stores) benefit hugely from parallel reads. Picking the wrong policy starves writers (or vice versa) and the bug only shows up under load.

## Approaches

### Approach 1 — Reader-Preferring (Simple, Writer Starves)
```text
mutex m
cond  ok
int   readers = 0
bool  writer  = false

read_lock:    lock(m); while writer: wait(ok,m); readers++; unlock(m)
read_unlock:  lock(m); readers--; if readers==0: signal(ok); unlock(m)
write_lock:   lock(m); while writer or readers>0: wait(ok,m); writer=true; unlock(m)
write_unlock: lock(m); writer=false; broadcast(ok); unlock(m)
```
- New readers can stream in indefinitely → writer may never acquire.

### Approach 2 — Writer-Preferring
Add a counter `writers_waiting`; new readers block while it's > 0.
```text
read_lock:
    lock(m)
    while writer or writers_waiting > 0: wait(ok,m)
    readers++
    unlock(m)
write_lock:
    lock(m)
    writers_waiting++
    while writer or readers > 0: wait(ok,m)
    writers_waiting--
    writer = true
    unlock(m)
```
- Writers no longer starve.
- A flood of writers can starve readers; usually fine because writes are infrequent.

### Approach 3 — Fair (FIFO) — Ticket-Based
Both readers and writers take a ticket; readers grouped between writes can run in parallel.
- No starvation in either direction.
- Slightly slower (more bookkeeping).
- POSIX `pthread_rwlock_t` policy varies (Linux defaults to reader-preferring; `PTHREAD_RWLOCK_PREFER_WRITER_NP` switches).

### Approach 4 — Seqlock (Optimistic Read)
Writer increments a sequence number twice (odd while writing, even when done). Reader snapshots seq, reads data, re-checks seq — retries if it changed or was odd.
```text
read:
    loop:
        s = load(seq)
        if s & 1: continue       // writer in progress
        copy data
        if s == load(seq): break // unchanged
        // else retry
write:
    seq++   // make odd
    /* modify */
    seq++   // back to even
```
- Readers never block writers.
- Readers must be idempotent (may retry); data must be safely re-readable.
- Used in Linux for time-of-day, routing tables.

### Approach 5 — RCU (Read-Copy-Update)
Writers publish a new copy via a single atomic pointer swap; readers dereference the current pointer with no lock; old copy freed after a grace period (when no reader can still see it).
- Zero-overhead reads (a load + use).
- Writers do more work; reclamation deferred.
- Linux kernel's primary read-mostly mechanism.

## Comparison
| Lock | Reader cost | Writer cost | Starvation risk | Notes |
|---|---|---|---|---|
| Reader-pref | low (under lock) | high | **writer** | simple; rarely correct in prod |
| Writer-pref | low | medium | reader | usually right for config-store |
| Fair / ticket | low | medium | none | extra bookkeeping |
| Seqlock | 2 loads + retry | atomic incs | none | data must be safely re-readable |
| RCU | one load | high (defer free) | none | best read-side; complex writer |

## Key Insight
- Choosing "reader-pref vs writer-pref" is a **policy** decision, not a performance one — based on which class is allowed to starve.
- The faster end-states (seqlock, RCU) eliminate reader–writer contention entirely by making reads either retriable or stale-tolerant.

## Pitfalls
- POSIX `pthread_rwlock_t` default differs across systems; document/enforce policy via attributes
- Reader–writer locks aren't always faster than a plain mutex; for short critical sections the bookkeeping dominates
- Recursive read-acquire on a writer-preferring rwlock can deadlock (writer waiting, thread can't re-acquire as reader because writer is waiting)
- Upgrading a read lock to a write lock atomically is **not** in POSIX — usually deadlocks if two readers both try
- Seqlock reads must not have observable side effects (you may run them twice)
- RCU readers must run in bounded time (no sleep) so a grace period eventually elapses

## Interview Tips
1. Distinguish reader-pref / writer-pref / fair before writing any code — show you know it's a policy choice.
2. Mention seqlock and RCU as the read-mostly end-states.
3. Cite that POSIX has no atomic upgrade — common gotcha.
4. For "design a cache" interviews, propose RCU or copy-on-write as the read-side win.

## Related / Follow-ups
- [10_memory_barriers](../10_memory_barriers/) — seqlock and RCU rely on ordering
- Linux `seqlock_t`, `rcu_read_lock`, `synchronize_rcu`
- [17_kernel_linked_list](../17_kernel_linked_list/) — RCU list variants
- Hazard pointers as an alternative to RCU in user space

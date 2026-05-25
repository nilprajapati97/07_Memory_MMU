# 03 — Thread-Safe Queue / Stack

## Problem
Build a FIFO queue (or LIFO stack) that multiple threads can `push`/`pop` concurrently without races, deadlocks, or lost updates.

## Why It Matters
Backbone of work-stealing schedulers, request pipelines, log aggregators. Choice of locking strategy determines throughput under contention and latency tail.

## Approaches

### Approach 1 — Coarse-Grained Lock (Single Mutex)
Wrap any underlying container (linked list / vector) with one mutex.
```text
push(x):  lock(m); list.push_back(x); unlock(m)
pop():    lock(m); if list.empty: return NONE
                   x = list.pop_front(); unlock(m); return x
```
- Pros: trivially correct; works for any container.
- Cons: every operation serialises on one lock; contention bottleneck.

### Approach 2 — Two-Lock Queue (Head + Tail)
Linked list with separate `head_lock` (for `pop`) and `tail_lock` (for `push`). Producers and consumers don't fight each other.
- Requires a **dummy node** so head and tail are always distinct; that's the trick.
- Each op takes O(1) and holds only one lock.
- Improves throughput when producers ≠ consumers.

### Approach 3 — Blocking Variant
Add condition variable so `pop` blocks when empty (Approach 1 + `not_empty` condvar). See [02_producer_consumer](../02_producer_consumer/).

### Approach 4 — Lock-Free Queue (Michael–Scott)
Linked list of nodes; `head` and `tail` advanced with `CAS`. Handles ABA via tagged pointers or hazard pointers.
```text
enqueue(x):
    node = new(x)
    loop:
        t = load(tail)
        next = load(t.next)
        if t == load(tail):
            if next is NULL:
                if CAS(t.next, NULL, node): break
            else:
                CAS(tail, t, next)   // help advance
    CAS(tail, t, node)               // swing tail
```
- Lock-free progress, but **memory reclamation is hard** (need hazard pointers, epoch GC, or RCU).
- Complex; rarely beats a well-tuned mutex queue except under heavy contention.

### Approach 5 — Lock-Free Stack (Treiber Stack)
Simpler than MS-queue. Single `top` pointer; CAS to swap.
```text
push(x):
    n = new(x)
    loop:
        n.next = load(top)
        if CAS(top, n.next, n): break
pop():
    loop:
        t = load(top)
        if t == NULL: return NONE
        if CAS(top, t, t.next): return t.value
```
- ABA possible: another thread frees & reuses `t` between load and CAS → CAS succeeds with bogus state. Mitigate via tagged pointers (double-width CAS) or hazard pointers.

### Approach 6 — Per-Producer Queues + Steal
Each worker has its own queue; idle workers steal from busy ones. Used in TBB, Go runtime, Rust Rayon.
- Eliminates contention almost entirely; complex but scales beautifully.

## Comparison
| Approach | Throughput | Complexity | Memory reclaim | Notes |
|---|---|---|---|---|
| Coarse mutex | low | trivial | n/a | **default** |
| Two-lock queue | medium | low | n/a | producer ≠ consumer wins |
| Lock-free MS queue | high | very high | needs hazard ptrs / RCU | only worth at scale |
| Treiber stack | high | medium | ABA-prone | safer than queue |
| Per-thread + steal | very high | high | n/a | best for many cores |

## Key Insight
- **Locks are not slow; contention is slow.** A coarse mutex held briefly outperforms a complicated lock-free design in most real workloads.
- A two-lock queue separates head-from-tail contention with a single tiny structural trick: a **dummy node** that's always present, so the queue is never "head==tail with one element" — eliminating the need to coordinate the two locks.
- Lock-free's hardest problem isn't the algorithm — it's **safe memory reclamation** (when is it safe to `free` a popped node?).

## Pitfalls
- Returning a reference to internal storage that gets freed → use value-copy or move
- Using `try_lock` carelessly → spin-fail loop; add backoff
- Sleeping while holding a lock (e.g. malloc inside critical section) → blocks all other threads
- ABA in lock-free: pointer recycled to same address, CAS doesn't notice
- Missing memory barriers around CAS payload — most CAS instructions imply full fence on x86 but **not** on ARM/POWER
- Boundless queue + slow consumer → memory exhaustion (apply backpressure)
- Mixing blocking and non-blocking pops without care → starvation

## Interview Tips
1. Default to coarse-mutex; offer two-lock queue as the natural upgrade.
2. Mention lock-free is "the right answer only when contention is provably the bottleneck — and reclamation is the real challenge".
3. For LIFO, mention Treiber stack and the ABA problem unprompted.
4. Cite work-stealing as the modern preferred design when there are many cores.

## Related / Follow-ups
- [01_ring_buffer](../01_ring_buffer/) — bounded array alternative
- [02_producer_consumer](../02_producer_consumer/)
- [04_spinlock](../04_spinlock/)
- Hazard pointers, RCU
- C++ `boost::lockfree::queue`, `folly::MPMCQueue`, Java `ConcurrentLinkedQueue`

# 04 — Spinlock (Test-and-Set, TTAS, Ticket, MCS)

## Problem
Implement a mutual-exclusion primitive that **busy-waits** instead of sleeping. Suitable for short critical sections, IRQ context, kernels.

## Why It Matters
Used wherever sleeping is forbidden (interrupt handlers, scheduler internals) or where the critical section is shorter than a context switch (cache-line ops). Modern designs add fairness and cache-friendliness — important on many-core CPUs.

## Approaches

### Approach 1 — Naive Test-And-Set (TAS)
```text
lock(L):
    while atomic_exchange(L, 1) == 1: /* spin */
unlock(L):
    atomic_store(L, 0)
```
- One atomic instruction per attempt.
- **Cache thrashing**: every spinner's `xchg` invalidates the line in every other core. O(n²) bus traffic under contention.
- No fairness — last to arrive may win.

### Approach 2 — Test-Test-And-Set (TTAS)
Spin on a plain load until the lock looks free, then attempt the atomic.
```text
lock(L):
    loop:
        while atomic_load(L) == 1: pause()
        if atomic_exchange(L, 1) == 0: return
```
- Spinning load is shared-read (no cache line ping-pong).
- Reduces traffic dramatically.
- Still unfair.

### Approach 3 — Ticket Lock (Fair)
Two counters: `next_ticket` (taken at entry), `now_serving` (advanced at release).
```text
lock(L):
    my = atomic_fetch_add(&L.next_ticket, 1)
    while atomic_load(&L.now_serving) != my: pause()
unlock(L):
    atomic_store(&L.now_serving, L.now_serving + 1)
```
- FIFO fairness; no starvation.
- All spinners spin on the same `now_serving` line → still cache-line traffic on each release (one line, n readers).

### Approach 4 — MCS Lock (Fair + Cache-Local)
Each thread spins on **its own** local flag in a node it queued; the predecessor flips that flag when releasing.
```text
struct node { atomic<node*> next; atomic<bool> locked; };

lock(L, my_node):
    my_node.locked = true; my_node.next = NULL
    pred = atomic_exchange(&L.tail, &my_node)
    if pred != NULL:
        pred.next = &my_node                // publish ourselves
        while my_node.locked: pause()
unlock(L, my_node):
    if my_node.next == NULL:
        if CAS(&L.tail, &my_node, NULL): return
        while my_node.next == NULL: pause() // wait for successor to publish
    my_node.next.locked = false
```
- O(1) bus traffic per acquire/release regardless of contention.
- Scales to thousands of cores. Used in Linux `qspinlock`, Java biased locking.
- Per-acquire node — typically allocated on the caller's stack.

### Approach 5 — Backoff & Pause
Augment any spinlock with exponential backoff and the `pause` / `yield` instruction:
- `_mm_pause()` on x86, `yield` on ARM — hints the core to deprioritise the spinner, frees pipeline.
- Backoff reduces synchronised retries (thundering herd).

### Approach 6 — Disable Interrupts (Kernel/Bare-Metal Variant)
Hardware spinlocks in interrupt-disable mode (`spin_lock_irqsave` in Linux): never sleep, never get preempted, never get an IRQ on the same CPU while holding.

## Comparison
| Lock | Fairness | Cache traffic | Code | Best for |
|---|---|---|---|---|
| TAS | none | high | 3 lines | tiny CS, ≤ 2 cores |
| TTAS | none | medium | 5 lines | general user-space |
| Ticket | FIFO | medium | 8 lines | fairness-first |
| **MCS** | FIFO | **low** | ~25 lines | **many-core scalable** |
| `pthread_mutex_t` adaptive | mostly fair | OS-managed | n/a | most app code |

## Key Insight
- **Plain TAS** burns the cache; **TTAS** spins on a plain load to share the line.
- **Ticket locks** add fairness but everyone still sees every release.
- **MCS** is the cache-local end-state: each spinner watches only its own line; the unlocker writes only the next spinner's line.

## Pitfalls
- Spinlocks in user-space holding through a page fault → tens of ms held, all spinners burn CPU
- Spinning on a contended TAS in interrupt context can deadlock if the lock holder needs the same IRQ
- Forgetting acquire/release semantics → reordered memory accesses past the critical section
- MCS node lifetime: must outlive the call; per-thread storage works
- "Adaptive" mutexes (glibc): spin briefly then sleep — usually the right user-space default
- Spinning on a checked predicate without `pause` wastes power and stalls hyperthread siblings

## Interview Tips
1. Walk up the ladder: TAS → TTAS → ticket → MCS. Each fixes the previous flaw.
2. Cite the cache-line problem early — interviewer's looking for it.
3. Mention `pause`/`yield` for spinning; that's a real-world detail.
4. Distinguish kernel (IRQ-aware) vs user (futex-backed) spinlocks. Kernel must never sleep while held.
5. Default in app code is `pthread_mutex_t`; spinlock is a tool for specific hot, short paths.

## Related / Follow-ups
- [05_semaphore](../05_semaphore/), [08_mutex_semaphore_spinlock](../08_mutex_semaphore_spinlock/)
- [10_memory_barriers](../10_memory_barriers/) — acquire/release in spinlock primitives
- Linux `qspinlock` (MCS-based) and `spin_lock_irqsave`
- `futex` — the kernel primitive backing `pthread_mutex_t`
- Adaptive locks (spin a bit, then sleep)

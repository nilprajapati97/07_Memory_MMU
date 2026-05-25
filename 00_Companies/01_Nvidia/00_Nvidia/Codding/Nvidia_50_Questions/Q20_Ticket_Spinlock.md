# Q20: Ticket Spinlock Implementation

**Section:** Concurrency & Synchronization | **Difficulty:** Medium-Hard | **Topics:** ticket lock, `atomic_fetch_add`, `cpu_relax`, fairness, FIFO lock ordering, starvation prevention

---

## Question

Implement a ticket spinlock to ensure FIFO fairness between competing CPUs.

---

## Answer

```c
#include <linux/atomic.h>
#include <linux/processor.h> /* cpu_relax */

/* ─── Ticket Spinlock Structure ───────────────────────────────────────────
 * next:    next ticket number to hand out (counter incremented on lock())
 * serving: current ticket being served (incremented on unlock())
 *
 * A CPU holds the lock only when its ticket == serving.
 * Guarantees strict FIFO ordering: first to call lock() = first to enter.
 */
struct ticket_lock {
    atomic_t next;      /* next ticket counter  */
    atomic_t serving;   /* currently-held ticket */
};

#define TICKET_LOCK_INIT  { ATOMIC_INIT(0), ATOMIC_INIT(0) }

static inline void ticket_lock_init(struct ticket_lock *lock)
{
    atomic_set(&lock->next, 0);
    atomic_set(&lock->serving, 0);
}

/* ─── Lock ────────────────────────────────────────────────────────────────
 * Take a ticket and spin until our ticket is being served.
 */
static inline void ticket_lock(struct ticket_lock *lock)
{
    /*
     * atomic_fetch_add: atomically reads next and increments it.
     * Returns the OLD value = our unique ticket number.
     * Full memory barrier included (unlike atomic_add).
     */
    int my_ticket = atomic_fetch_add(1, &lock->next);

    /*
     * Spin until our ticket is at the front of the queue.
     * cpu_relax(): issues PAUSE instruction on x86 (reduces power,
     * hints the pipeline it's a spin loop, improves HyperThreading perf).
     */
    while (atomic_read(&lock->serving) != my_ticket)
        cpu_relax();

    /*
     * Full memory barrier: ensure all subsequent memory accesses
     * happen AFTER we enter the critical section.
     */
    smp_mb();
}

/* ─── Unlock ──────────────────────────────────────────────────────────────
 * Advance the serving counter — next waiting CPU's ticket now matches.
 */
static inline void ticket_unlock(struct ticket_lock *lock)
{
    /*
     * smp_wmb: ensure all writes within the critical section are
     * visible before we release the lock (increment serving).
     */
    smp_wmb();
    atomic_inc(&lock->serving);
}

/* ─── Try-lock (non-blocking) ─────────────────────────────────────────────
 * Returns 1 if lock acquired, 0 if contended.
 * Note: ticket spinlocks don't naturally support try-lock well —
 * if we take a ticket but don't spin, we stall everyone behind us.
 * This is a special case: only try if no contention (next == serving).
 */
static inline int ticket_trylock(struct ticket_lock *lock)
{
    int next    = atomic_read(&lock->next);
    int serving = atomic_read(&lock->serving);

    if (next != serving)
        return 0; /* contended */

    /*
     * CAS: only take the lock if serving hasn't moved
     * (no new waiter appeared between our check and CAS).
     */
    if (atomic_cmpxchg(&lock->next, next, next + 1) == next) {
        smp_mb();
        return 1;
    }
    return 0;
}

/* ─── Lock depth (for debugging contention) ───────────────────────────────*/
static inline int ticket_lock_depth(struct ticket_lock *lock)
{
    return atomic_read(&lock->next) - atomic_read(&lock->serving);
}

/* ─── Usage in GPU command ring ───────────────────────────────────────────*/
struct gpu_ring {
    struct ticket_lock lock;
    u32 head, tail;
    /* ... */
};

void gpu_ring_submit(struct gpu_ring *ring, u64 cmd)
{
    ticket_lock(&ring->lock);
    /* critical section: modify head/tail */
    ring->hw_ring[ring->head & RING_MASK] = cmd;
    ring->head++;
    ticket_unlock(&ring->lock);
}
```

---

## Explanation

### Core Concept

A standard `spinlock_t` in Linux has no fairness guarantee — a CPU that re-acquires a lock immediately after releasing it may starve other waiting CPUs (especially on NUMA where local cache makes the re-acquire faster).

**Ticket spinlock guarantees strict FIFO:**

```
State: next=0, serving=0 (lock free)

CPU A calls ticket_lock:
  my_ticket = fetch_add(next) = 0, next becomes 1
  serving == 0 == my_ticket → enters critical section immediately

CPU B calls ticket_lock while A holds lock:
  my_ticket = fetch_add(next) = 1, next becomes 2
  serving == 0 ≠ 1 → SPIN

CPU C calls ticket_lock:
  my_ticket = fetch_add(next) = 2, next becomes 3
  serving == 0 ≠ 2 → SPIN

CPU A calls ticket_unlock:
  serving becomes 1 → CPU B wakes (1 == 1) → CPU B enters
  CPU C still spins (serving=1 ≠ 2)
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `atomic_fetch_add(delta, v)` | Atomically add and return old value (full barrier) |
| `atomic_read(v)` | Relaxed read (no barrier) |
| `atomic_inc(v)` | Atomically increment |
| `atomic_cmpxchg(v, old, new)` | Compare-and-swap |
| `cpu_relax()` | PAUSE hint in spin loop (saves power, avoids memory bus saturation) |
| `smp_mb()` | Full memory barrier (read + write) |
| `smp_wmb()` | Write-only memory barrier (store-store ordering) |

### Trade-offs & Pitfalls

- **Not suitable for long critical sections.** Ticket locks are still spinlocks — they waste CPU cycles while spinning. Use only for short, bounded critical sections (< a few hundred nanoseconds). For longer sections, use `mutex`.
- **NUMA performance.** On NUMA systems, all CPUs spin-reading `serving` on the same cache line — causes cache line bouncing. The Linux kernel's `qspinlock` (MCS-based) solves this by having each CPU spin on a local variable.
- **No recursive locking.** A ticket lock does not track which CPU holds it. Attempting to re-acquire from the same CPU will deadlock (the CPU's `my_ticket` will never be served — `unlock` is never called).
- **IRQ safety.** Like all spinlocks, ticket locks must be used with `irqsave` variant if the lock might be acquired from an IRQ handler on the same CPU.

### NVIDIA / GPU Context

The Linux kernel's `spinlock_t` internally evolved from a ticket lock to `qspinlock` for scalability. NVIDIA GPU driver code:
- Uses `spinlock_t` (qspinlock internally on SMP) for GPU ring buffer submission
- Uses ticket-style ordering in the GPU hardware itself — the hardware GMMU processes page table updates in submission order using an internal ticket mechanism
- Ticket semantics appear in NVIDIA's NVLink fabric manager where PE (Processing Element) command slots are assigned tickets to ensure ordered retirement

---

## Cross Questions & Answers

**CQ1: Why does the Linux kernel use `qspinlock` instead of ticket spinlock on SMP?**
> Ticket spinlock has an O(N) cache coherency problem: all N waiting CPUs spin on the same `serving` variable. When `ticket_unlock` increments `serving`, all N CPUs observe the change and contend for the new value — cache line bouncing across all CPUs on a NUMA machine. `qspinlock` (MCS-based) gives each CPU its own local spin variable in a queue node. Only when the previous CPU unlocks does it set the next CPU's local variable — only 1 cache line invalidation occurs regardless of N waiters.

**CQ2: How would you add a backoff strategy to reduce bus contention in the spin loop?**
> Exponential backoff: after each failed `serving == my_ticket` check, wait 2^k `cpu_relax()` iterations (capped at some maximum). This reduces the rate of `serving` reads and decreases the coherency bus traffic. However, backoff introduces latency variance and can cause wakeup delay on lightly-contended locks. The MCS approach is generally preferred over backoff for predictable latency.

**CQ3: What is the ABA problem and does it affect ticket spinlocks?**
> The ABA problem occurs in CAS-based algorithms: a value changes A→B→A, and a CAS expecting A succeeds even though intermediate changes occurred. Ticket spinlocks are immune to ABA because: (1) `next` is monotonically increasing (never decremented), (2) we only compare `my_ticket == serving`, both monotonically increasing — there's no scenario where the value wraps and a CPU incorrectly believes it's the rightful holder.

**CQ4: What happens to ticket spinlock correctness when the ticket counters overflow?**
> With 32-bit `atomic_t`, after 2^32 acquisitions, `next` wraps to 0. If `serving` has also wrapped, the comparison `serving == my_ticket` still works correctly due to modular arithmetic. However, if the system has a pathological case where `next` - `serving` > INT_MAX (2^31 outstanding lockers), the comparison fails. In practice, this is impossible (a machine would have melted long before 2^31 CPUs queue on one lock).

**CQ5: How does `raw_spin_lock` differ from `spin_lock` in Linux kernel internals?**
> `spin_lock` in the kernel is the preemptible version — it calls `preempt_disable()` before spinning (so the spinning thread can't be preempted while holding the spinlock). `raw_spin_lock` skips preemption disabling — used in code paths where preemption is already disabled (low-level scheduling, interrupt entry). GPU drivers should always use `spin_lock`/`spin_lock_irqsave` (with preemption disable) unless they are certain they're operating in a context where preemption is already disabled.

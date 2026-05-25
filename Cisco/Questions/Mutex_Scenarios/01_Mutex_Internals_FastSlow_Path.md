# 01 — Mutex Internals: Fast Path, Slow Path, OSQ

> Audience: senior kernel engineer. Coverage: the actual machinery of
> `struct mutex` as implemented in `kernel/locking/mutex.c` — owner-encoded
> atomic, three-tier acquisition (fast → optimistic → slow), MCS-based
> OSQ, handoff, and the slow-path scheduler dance.

---

## 1. The Three Acquisition Tiers

`mutex_lock()` is *not* a single atomic op. It is a three-tier algorithm
designed to handle three very different contention regimes optimally:

| Tier | When it wins | Mechanism | Cost |
|------|--------------|-----------|------|
| **1. Fast path** | Uncontended | Single `cmpxchg` on `owner` | ~10 ns |
| **2. Optimistic spin (OSQ)** | Owner is *running on another CPU* | MCS queue + spin on owner field | ~100 ns – few µs |
| **3. Slow path** | Owner is *sleeping* / OSQ failed | Enqueue on `wait_list`, `schedule()` out | ~µs + context-switch cost |

Tier 2 exists precisely because going to sleep + waking up is enormously
more expensive than spinning for a few hundred cycles, **provided the owner
is making forward progress on another CPU**. If the owner is itself blocked,
spinning is pure waste — that's why tier 2 has a precise abort condition.

---

## 2. The `owner` Atomic Field (Heart of the Mutex)

```
atomic_long_t owner;   // single word encodes owner ptr + flags
```

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `MUTEX_FLAG_WAITERS` | Someone is on `wait_list` (slow-path waiter present). |
| 1 | `MUTEX_FLAG_HANDOFF` | Owner promises to hand the lock to a specific waiter on unlock. |
| 2 | `MUTEX_FLAG_PICKUP` | Lock has been handed off; designated waiter must `cmpxchg` itself in. |
| 3..63 | `task_struct *` | Pointer to the current owner task (aligned, so low bits are free). |

The encoding gives several powerful invariants in **one atomic load**:

- `owner == 0` → free.
- `owner & ~FLAGS == NULL` but flags set → transient state (handoff).
- Otherwise → `(struct task_struct *)(owner & ~FLAGS)` is the holder.

### Fast-path acquire

```
prev = atomic_long_cmpxchg_acquire(&lock->owner, 0UL, current);
if (prev == 0)
    return SUCCESS;     // uncontended — done in one instruction
```

The `_acquire` memory ordering pairs with the unlocker's `_release` store,
giving Release/Acquire semantics that publish all writes inside the critical
section to the next holder.

### Fast-path release

```
prev = atomic_long_cmpxchg_release(&lock->owner, current, 0UL);
if ((prev & MUTEX_FLAGS) == 0)
    return;             // uncontended unlock — done
goto slow_unlock;       // waiters present, must wake one
```

---

## 3. Tier 2 — Optimistic Spinning (OSQ + Spin-on-Owner)

Enabled by `CONFIG_MUTEX_SPIN_ON_OWNER` (always on in modern x86/arm64
configs). Two pieces:

### 3a. The premise

If the current owner is **running on another CPU**, it will likely release
the mutex within a few hundred cycles. Spinning is then cheaper than the
~µs context-switch overhead of going to sleep.

The abort condition is exactly this: as soon as we observe
`!owner_on_cpu(owner)` (owner descheduled), we bail to the slow path.

### 3b. The MCS queue (`osq_lock`)

Multiple optimistic spinners must not all hammer the same cache line —
that causes massive coherence traffic. Solution: **MCS lock** (Mellor-Crummey
& Scott) — each spinner enqueues a *per-CPU node* and spins on its **own**
cache line until its predecessor signals it.

```
              (mutex.osq)
                 │
                 ▼
   ┌──────┐    ┌──────┐    ┌──────┐
   │ CPU2 │ →  │ CPU3 │ →  │ CPU5 │   ← OSQ tail
   │ node │    │ node │    │ node │
   └──────┘    └──────┘    └──────┘
   spins on    spins on    spins on
   owner       its prev    its prev
   field       node.locked node.locked
```

Only the **OSQ head** spins on the mutex `owner`. Each subsequent CPU spins
on its own `node.locked` flag (a private cache line). When the head wins
the mutex it pops itself, marks the next node's `locked = true`, and that
CPU now becomes the new head.

Net effect:

- **O(1) cache-line bouncing** per acquisition regardless of contender count.
- **Fairness** within the OSQ — strict FIFO.
- **Bounded spinning** — any spinner aborts the moment its owner sleeps.

### 3c. Optimistic spin loop (conceptual)

```
osq_lock(&lock->osq);                       // enqueue MCS node, wait turn
for (;;) {
    owner = READ_ONCE(lock->owner);
    if ((owner & ~FLAGS) == 0) {            // free → try acquire
        if (cmpxchg_acquire(&lock->owner, owner, current | (owner & FLAGS)) == owner) {
            osq_unlock(&lock->osq);
            return SUCCESS;
        }
    }
    if (!owner_on_cpu(owner_task(owner)) || need_resched())
        break;                              // abort optimistic spin
    cpu_relax();                            // PAUSE / YIELD instruction
}
osq_unlock(&lock->osq);
goto slow_path;
```

---

## 4. Tier 3 — Slow Path

Triggered when:

- Optimistic spinning gave up (owner sleeping, or `need_resched()`), or
- `CONFIG_MUTEX_SPIN_ON_OWNER=n`, or
- Debug build (`CONFIG_DEBUG_MUTEXES`) disables OSQ.

### Sequence

1. **Take `wait_lock`** (`raw_spinlock_t`) — protects `wait_list`.
2. **Re-check owner**: between the failed OSQ and now, the lock may have
   become free. If so, try the fast path again under the wait_lock and exit.
3. **Enqueue** a `struct mutex_waiter { task; list_node; }` on `wait_list`
   in FIFO order.
4. **Set** `MUTEX_FLAG_WAITERS` on `owner` so the unlocker knows to wake.
5. **Loop:**
   ```
   for (;;) {
       set_current_state(TASK_UNINTERRUPTIBLE);   // or TASK_INTERRUPTIBLE
       if (__mutex_trylock_or_handoff(lock, first))
           break;                                  // got it
       spin_unlock(&lock->wait_lock);
       schedule_preempt_disabled();                // sleep
       spin_lock(&lock->wait_lock);
   }
   ```
6. **Dequeue**, clear `MUTEX_FLAG_WAITERS` if list empty, release `wait_lock`,
   return.

### Why `TASK_UNINTERRUPTIBLE` by default?

A vanilla `mutex_lock()` must not return spuriously — callers rely on
"either I hold the lock or I'm dead". `TASK_INTERRUPTIBLE` is opted into via
`mutex_lock_interruptible()`.

### Spurious wakeup handling

The `for` loop re-evaluates after every `schedule()`. If another waiter
beat us in, we go right back to sleep — there are no "lost" wakeups, but
also no guarantee that one wakeup ⇒ one acquisition.

---

## 5. Handoff Mechanism (Anti-Starvation)

Without intervention, optimistic spinners on incoming threads can starve
the FIFO waiters on `wait_list` — they keep grabbing the lock the moment
the owner releases.

To bound the unfairness, the slow path uses the **HANDOFF** mechanism:

- A long-waiting front-of-queue waiter sets `MUTEX_FLAG_HANDOFF` on the
  `owner` word.
- On unlock, the outgoing owner sees HANDOFF set and, instead of clearing
  `owner` to 0, writes `(waiter_task | MUTEX_FLAG_PICKUP)`.
- Optimistic spinners check for `PICKUP` and **back off** — they cannot
  steal a handed-off lock.
- The designated waiter `cmpxchg`-installs itself as owner and clears the
  flag.

This bounds worst-case wait time even under heavy optimistic-spin traffic.

---

## 6. The Unlock Path

```
__mutex_unlock_slowpath():
    1. owner = atomic_long_read(&lock->owner)
    2. If MUTEX_FLAG_HANDOFF set:
         pick first waiter, set owner = waiter | PICKUP
         wake_q_add(waiter)
       Else:
         atomic_long_andnot(__OWNER_MASK, &lock->owner)   // clear holder, keep flags
    3. spin_lock(&lock->wait_lock)
       Choose first waiter, wake it via wake_q
    4. spin_unlock(&lock->wait_lock)
    5. wake_up_q(&wake_q)
```

Crucial details:

- The wake is **deferred** via `wake_q` so it happens *outside* the
  `wait_lock`, avoiding a lock-inversion against the runqueue lock.
- `wake_up_process()` on the waiter sets it `TASK_RUNNING` and may
  reschedule on its CPU. That CPU re-runs the slow-path `for` loop,
  observes the lock as free (or PICKUP), and acquires.

---

## 7. Putting It Together — A Single Acquire Decision Tree

```
mutex_lock(L)
   │
   ▼
[Fast path] cmpxchg owner: 0 → current      ──── success ──► DONE (~10 ns)
   │ fail
   ▼
[OSQ enter] enqueue MCS node, wait turn
   │
   ▼
[Spin-on-owner] loop:
      if owner == 0 → cmpxchg-acquire     ──── success ──► DONE (~100 ns)
      if !owner_on_cpu OR need_resched    ──── break
      cpu_relax()
   │
   ▼
[Slow path] take wait_lock
            enqueue waiter, set WAITERS
            schedule() loop
            (HANDOFF if starved)
   │
   ▼
   wake → re-try → owner = current  ──── DONE (µs + ctx-switch)
```

---

## 8. Memory Ordering

| Operation | Ordering | Pairs with |
|-----------|----------|------------|
| Lock acquire (`cmpxchg_acquire`) | `acquire` | Unlocker's `release` |
| Lock release (`cmpxchg_release` / `andnot_release`) | `release` | Next locker's `acquire` |
| `wait_list` manipulations | Plain stores under `wait_lock` (`raw_spinlock_t`) | — |
| Waiter wake | `wake_up_process` provides full barrier vs schedule | Scheduler |

Net guarantee: writes inside the previous holder's critical section are
visible to the next holder when it returns from `mutex_lock`. This is the
standard *release-acquire* synchronization.

---

## 9. Debug Builds (`CONFIG_DEBUG_MUTEXES`)

Adds:

- `struct mutex_debug_info` — magic numbers, file/line of last lock site.
- Owner check on `mutex_unlock`: `WARN_ON(lock->owner != current)`.
- Disables OSQ (forces slow-path) to make instrumentation simpler.
- Integrates with `lockdep` for ordering and recursion checks (see
  [04_Mutex_Deadlocks_Lockdep_Debugging.md](04_Mutex_Deadlocks_Lockdep_Debugging.md)).

Performance drops ~30–50 %; never enable in production, always enable in CI.

---

## 10. Interview Q&A (Internals)

**Q1. Why does the owner field encode a pointer *and* flags in one atomic word?**
A. Allows a single `cmpxchg` to acquire and clear the WAITERS flag, or to
hand off the lock atomically with the new owner pointer. Splitting them
would require two atomics and introduce intermediate inconsistent states.

**Q2. Under what exact condition does the optimistic spinner give up?**
A. (a) The owner task is no longer running on a CPU (`!owner_on_cpu()`);
(b) `need_resched()` is set on the spinner's CPU; (c) the lock got `PICKUP`
flag (handoff in progress); (d) the owner pointer changed unexpectedly.

**Q3. Why is `osq_lock` an MCS lock and not a simple test-and-set?**
A. To avoid cache-line bouncing under contention. With test-and-set, every
spinner writes the same cache line, causing O(N²) coherence traffic. With
MCS, each spinner reads its own per-CPU node's `locked` flag — exactly one
remote write per acquire/release.

**Q4. What guarantees FIFO ordering across the slow path?**
A. `wait_list` is strict FIFO under `wait_lock`. But optimistic spinners
can "steal" the lock from the front-of-queue waiter — *that* is what
`MUTEX_FLAG_HANDOFF` exists to bound.

**Q5. Why must the wake-up be deferred via `wake_q`?**
A. `wake_up_process()` takes the target's runqueue lock. Holding
`wait_lock` while taking an rq lock creates a lock ordering that conflicts
with the scheduler's own ordering, risking deadlock. `wake_q` batches the
wakeup outside `wait_lock`.

**Q6. If I `mutex_lock` then call `schedule()` voluntarily, does the lock
stay held?**
A. Yes. Mutex ownership is independent of scheduling state. The lock
remains held across the sleep; other waiters block until you wake and
release. (Contrast with the spinlock rule: you *cannot* sleep while holding
one.)

---

## Navigation

⬅ [README](README.md) · ➡ [02 — Multi-Thread / Multi-CPU Scenarios](02_Mutex_MultiThread_MultiCPU_Scenarios.md)

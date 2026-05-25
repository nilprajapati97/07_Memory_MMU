# Mutex — Conceptual Detailed Design

> **Scope:** Pure-concept treatment of the kernel mutex. No code. Covers all
> execution contexts (process / softirq / hardirq / NMI / atomic / RT),
> low-level synchronization invariants, ownership semantics, scheduling
> interaction, memory ordering, and the full lifecycle from acquire through
> contention to release.

> **Companion:** [SpinLock_Detailed_Design.md](SpinLock_Detailed_Design.md).

---

## 1. The Conceptual Identity of a Mutex

A mutex is a **sleeping, owner-bound, single-holder mutual-exclusion**
primitive. Three words define its identity:

| Word | Meaning |
|------|---------|
| **Sleeping** | A contender that cannot acquire surrenders the CPU and waits in a non-runnable state until woken. It does not burn CPU cycles. |
| **Owner-bound** | The lock remembers *which task* holds it. Release is restricted to that task. This enables priority inheritance, deadlock detection, and audit. |
| **Single-holder** | At most one task may be inside the critical section at any moment. No reader/writer split, no recursion, no counting. |

These three properties together imply every other rule about how and where
mutex may be used.

---

## 2. The Execution-Context Map

The kernel partitions execution into contexts. The mutex is **legal in
exactly one** of them.

```
                       Execution Context Pyramid
                       ─────────────────────────

           ┌─────────────────────────────────┐
           │   NMI (non-maskable interrupt)  │   ← most restrictive
           ├─────────────────────────────────┤
           │   Hardware IRQ (hardirq)        │
           ├─────────────────────────────────┤
           │   Softirq / tasklet             │
           ├─────────────────────────────────┤
           │   preempt_disabled / atomic     │
           ├─────────────────────────────────┤
           │   Process context (sleepable)   │   ← only here is mutex legal
           └─────────────────────────────────┘
```

| Context | May call `mutex_lock`? | Why |
|---------|------------------------|-----|
| **Process, no atomic state** | **Yes** | The only context that may schedule. |
| **Process, holding a spinlock** | **No** | `preempt_count > 0`. Sleeping would corrupt the spinlock's holder identity and stall every other CPU that contends. |
| **Process, `preempt_disable()` region** | **No** | Same reason — scheduler is shut off; sleeping is impossible. |
| **Softirq / tasklet** | **No** | Atomic context by definition. `schedule()` here is fatal. |
| **Hardware IRQ handler** | **No** | Atomic; running with IRQs disabled; cannot sleep. |
| **NMI** | **No** | Even more restrictive than IRQ — almost nothing is safe. |

The rule reduces to: **a mutex may only be acquired when the caller is in a
position to call `schedule()`**. Anywhere `schedule()` is illegal, mutex is
illegal.

---

## 3. The Two Foundational Invariants

Everything the kernel mutex does is in service of two invariants:

### Invariant A — Mutual Exclusion

At any instant, **at most one task** is logically inside the critical
section. This is enforced by a single atomic *owner* word: the only way to
become the owner is to atomically transition that word from "free" to
"me". Failures funnel into the wait machinery; they do not produce a
second concurrent owner.

### Invariant B — Causality (Release-Acquire Ordering)

All memory writes performed by the previous holder while inside the
critical section are **visible** to the next holder when it exits the
acquire operation. This is enforced by hardware memory-barrier semantics
attached to the acquire and release primitives. The new holder sees a
consistent, fully-committed view of the protected state.

These invariants are *together* sufficient for correctness. Everything
else — optimistic spinning, priority inheritance, anti-starvation, debug
checks — is performance, fairness, or diagnostic policy on top.

---

## 4. The Three-Tier Acquisition Strategy (Concept)

The kernel chooses between three behaviors per acquire, transparently to
the caller. Selection is dynamic and based on what the lock state and the
owner are doing right now.

### Tier 1 — Uncontended Fast Path

Lock is free. The acquirer performs a single atomic claim and proceeds.
No queueing, no waiting, no wake-up later. This is the dominant case in
well-designed code and must be cheap (a handful of CPU cycles).

### Tier 2 — Optimistic Spin

Lock is held, but the holder is **currently running on another CPU**.
The contender bets that the holder will release "soon" (within hundreds
of nanoseconds). Sleeping would cost microseconds of context-switch
overhead — far more than the expected wait. So the contender spins.

Spinning is **bounded** by two conditions:

1. The holder becomes non-runnable (sleeps for any reason). At that
   instant the bet has lost — the contender abandons spinning.
2. The contender's own CPU asks to reschedule (a higher-priority task
   wants the CPU). The contender yields by abandoning spinning.

To avoid pathological cache-line contention when many contenders spin at
once, the spinners are arranged in a queue — each one spins on its own
private cache line, and only the queue head reads the lock state. This
keeps coherence traffic constant regardless of contender count.

### Tier 3 — Slow Path (Sleep)

Either spinning gave up, or spinning was never appropriate (debug build,
disabled by config). The contender enqueues itself on the lock's
**wait-list**, transitions to a non-runnable state, and yields the CPU
to the scheduler. It remains there until the holder's release path
explicitly wakes it.

The wait-list is strict FIFO: the next holder is the earliest task that
went to sleep. Strict FIFO is later softened by an anti-starvation
mechanism to balance latency vs throughput — see §6.

---

## 5. Ownership and Identity

A spinlock does not know who holds it; it only knows whether *somebody*
does. A mutex does — it embeds a reference to the holder task. This
unlocks four capabilities the spinlock cannot offer:

1. **Wrong-owner unlock detection.** A different task attempting to
   release the lock is a structural bug; the mutex can detect and report it.
2. **Priority inheritance** (variant). When a high-priority task waits on
   a low-priority holder, the holder can be temporarily elevated so it
   completes the critical section quickly, releasing the high-priority
   waiter. (Plain mutex does not do this on non-RT kernels; the RT mutex
   variant does, and PREEMPT_RT converts plain mutex into the RT variant
   automatically.)
3. **Deadlock chain analysis.** A cycle of "task A waits on lock held by
   task B, which waits on lock held by task A" is detectable because the
   holder identity is recorded at every link.
4. **Hold-time accounting.** Debug infrastructure can record where, by
   whom, and for how long each lock was held.

Ownership tracking is the conceptual feature that elevates the mutex from
"a way to serialize" to "a way to *reason about* serialization."

---

## 6. Fairness vs Throughput

Strict FIFO is fair: every waiter gets the lock in arrival order. But
strict FIFO is also slow under high churn: every acquire forces a
sleep + wake + context-switch cycle, even when the lock will be free
again in nanoseconds.

The mutex chooses a hybrid:

- Optimistic spinners (incoming contenders that arrive while the holder
  is running) may grab the lock **ahead of** existing FIFO sleepers.
  Throughput wins.
- This permits *bounded* starvation of the FIFO sleepers. To limit it,
  the front-of-queue waiter may set a "handoff" intent. The next
  unlocker then **must** hand the lock to that specific waiter, and
  optimistic spinners must back off.

The result is "mostly FIFO with a starvation cap" — high throughput in
the common case, bounded worst-case latency under stress.

---

## 7. Interaction With the Scheduler

The mutex relies on the scheduler for everything in the slow path. The
key interactions:

| Mutex event | Scheduler effect |
|-------------|------------------|
| Enter slow path | Caller transitions to a non-runnable state (uninterruptible by default), then voluntarily yields the CPU. |
| Be on the wait-list | The task is not on any run-queue. It consumes zero CPU. |
| Receive wake-up on release | Task transitions to runnable and is queued on a run-queue (typically the one closest to where it last ran, for cache warmth). |
| Be runnable but not yet holder | Wakes, re-checks lock state, may need to sleep again if a spinner won; otherwise installs itself as owner. |

A subtle point: the **wake itself** is deferred so the unlocker is not
holding the lock's internal coordination primitive when it touches the
scheduler's run-queue locks. Mixing the two would create an ordering
hazard (lock-A then lock-B in mutex; lock-B then lock-A in scheduler).
The wake is batched and dispatched after all internal locks are dropped.

---

## 8. Memory-Ordering Concept (Without Hardware Specifics)

The acquire side establishes a "release-acquire" pair with the previous
release. Concretely:

- All writes the previous holder performed **before** the release are
  observable to the new holder **after** the acquire returns.
- Reads and writes inside the new holder's critical section cannot move
  earlier than the acquire (the compiler and CPU must respect this).
- Reads and writes inside the holder's critical section cannot move
  later than the release.

This pair is the *only* guarantee the mutex makes about memory ordering.
It is necessary and sufficient for the canonical pattern "store data,
release; acquire, load data, see the store." Anything beyond this (e.g.
ordering between two different mutexes) requires additional barriers.

---

## 9. Why a Mutex Is Forbidden in Atomic Contexts (Conceptual Proof)

Suppose, for contradiction, you call `mutex_lock` inside a hard-IRQ
handler. The lock is held by some task `T` on another CPU. The mutex
would normally:

1. Note that the lock is contended.
2. Enter the slow path: place "the current execution" on the wait-list.
3. Call `schedule()` to give the CPU to another task.

Step 3 is impossible. The "current execution" is not a task — it is an
interrupt-handler frame interrupting some task `U`. There is no
schedulable entity to put to sleep. The scheduler has no concept of "an
IRQ handler waiting for a wake-up." If schedule were called, the kernel
would either (a) deadlock waiting for a wake-up that can never identify
the right target, or (b) corrupt its accounting and crash.

The same argument applies to softirq, tasklet, NMI, and any
`preempt_disable()` region: the act of preventing scheduling is
incompatible with the act of voluntarily scheduling.

Hence the rule: **you cannot block where you cannot sleep, and you
cannot sleep where you have promised not to.**

---

## 10. Interaction With Interrupt and ISR Contexts

Because a mutex cannot be acquired in IRQ context, any data structure
that is touched both by process-context code (using a mutex) and by an
ISR must be split or rethought. Three idiomatic resolutions:

1. **Split state.** The portion the ISR touches is protected by a
   spinlock (which IS legal in IRQ context, with `_irqsave`). The
   portion only process context touches is protected by a mutex. The
   two state subsets do not overlap.
2. **Two-stage processing.** The ISR does the minimum (acknowledge,
   timestamp, queue work) under a spinlock or atomic ops. A worker
   thread later picks up the queued work, runs in process context, and
   uses a mutex for the heavy state mutation.
3. **Mutex-free communication.** The ISR enqueues into a lock-free or
   RCU-protected structure; the process-context consumer drains it
   under its own mutex if needed.

A mutex is **never** the right answer for sharing state with an ISR. The
spinlock (or one of its IRQ-disabling variants) is.

---

## 11. The Mutex Under PREEMPT_RT

On a PREEMPT_RT kernel, almost every sleeping lock is reimplemented in
terms of the RT mutex (the priority-inheriting variant). For the user
of `struct mutex`:

- The API is unchanged; the source compiles and behaves the same.
- Acquisition acquires priority inheritance.
- Critical-section length now affects more tasks: under RT, even
  `spinlock_t` is a sleeping lock internally, so the consequences of
  holding any lock too long propagate further.
- Optimistic spinning is not used (it conflicts with strict priority
  semantics).
- Only `raw_spinlock_t` and a small set of explicitly raw primitives
  remain truly atomic.

Drivers written cleanly for the standard kernel (short critical
sections, no sleeping under spinlocks, correct context discipline)
transition to RT smoothly without source changes.

---

## 12. Recursive Locking — Deliberately Forbidden

A task that holds a mutex and calls `mutex_lock` on the same mutex
**deadlocks itself**. The kernel does not implement recursive mutexes.

The conceptual reason is design hygiene. A function that needs the
caller to already hold a lock should state that as a precondition (and
the kernel offers assertions for this). A function that wants to lock
"only if not already held" is almost always papering over a layering
mistake — two callers with conflicting assumptions about who owns the
lock. Recursive mutexes mask this; non-recursive mutexes expose it as a
crash on the first run, which is preferable.

---

## 13. Lifecycle of a Mutex Acquisition (Narrative)

1. **Entry.** A task calls into the mutex API. The lock state word is
   checked atomically.
2. **Free?** If yes, the task atomically claims it and proceeds into
   the critical section. Cost: tiny.
3. **Held, holder running?** If the holder is on a CPU, the task joins
   an optimistic-spin queue. It rotates its way to the head, then watches
   the lock state. When the lock becomes free it claims it and exits the
   queue. Cost: hundreds of nanoseconds to a few microseconds.
4. **Held, holder sleeping?** The task abandons optimistic spinning,
   takes the lock's internal coordination primitive, places itself on
   the wait-list (FIFO), marks itself non-runnable, releases the
   internal primitive, and yields the CPU.
5. **Wake.** Some time later, the previous holder releases the mutex.
   That code pops the head of the wait-list and sends a wake-up. The
   sleeping task is queued on a run-queue.
6. **Re-evaluate.** The woken task resumes. It re-checks lock state
   (because an incoming optimistic spinner could have raced in). If it
   has the lock, it returns to the caller. If not, it re-enters step 4.
7. **Inside the critical section.** The task performs its work.
   Ownership is exclusive; memory writes are private to it until
   release.
8. **Release.** The task asks to drop the lock. If no waiters exist, a
   single atomic clear ends the operation. If waiters exist, the head
   waiter is selected, the internal coordination primitive is taken
   briefly, the wake-up is dispatched, and the primitive is released.
9. **Exit.** Control returns to the caller. Any subsequent acquirer
   will observe all writes performed in step 7.

---

## 14. Conceptual Contrast With the Spinlock

| Property | Mutex | Spinlock |
|----------|-------|----------|
| What a contender does | Sleeps (after a bounded optimistic spin) | Busy-waits forever |
| Where it may be called | Process context only | Any context including IRQ (with `_irqsave`) |
| Holder identity tracked | Yes | No |
| Priority inheritance possible | Yes (RT variant / PREEMPT_RT) | No (qspinlock is FIFO-fair but priority-blind) |
| Cost when uncontended | Single atomic op | Single atomic op |
| Cost when contended | Microseconds (sleep + wake) in slow path | Constant — burns CPU |
| Critical-section length appropriate | Microseconds to milliseconds | Sub-microsecond |
| Behavior under PREEMPT_RT | Becomes RT mutex | Becomes sleeping RT mutex (except raw spinlock) |
| Recoverable from holder death? | No — lock leaks; system hangs | No |

The two primitives are not interchangeable. Picking the wrong one
trades either correctness (mutex in IRQ context) or performance (mutex
where a spinlock would do) for nothing.

---

## 15. When To Choose a Mutex (Decision Criteria)

Pick a mutex when **all** of the following hold:

1. The critical section runs **only** in process context — no ISR, no
   softirq, no tasklet, no `preempt_disable()` region above it.
2. The critical section is **non-trivial** — long enough that a sleep
   is cheaper than a spin (rule of thumb: more than a microsecond, or
   it contains anything that itself may sleep — a memory allocation
   with `GFP_KERNEL`, a copy to/from user space, an I/O wait).
3. There is **no requirement** for ISR-side participation in the same
   lock. If an ISR ever needs the same data, the lock must be a
   spinlock (the process side then uses the `_irqsave` variant).
4. Ownership semantics are useful — debug, PI, single-owner discipline.

If any of those fail, the choice shifts: to a spinlock (for IRQ
participation or extreme brevity), to RCU (for read-mostly data with
rare writers), to a counting semaphore (for resource pools), or to a
lock-free design (for ultimate throughput with careful memory ordering).

---

## 16. Summary

- A mutex is a **sleeping, owner-bound, single-holder** mutual-exclusion
  primitive.
- It is legal **only in process context** — anywhere that may call
  `schedule()`.
- It uses a three-tier strategy: fast path, optimistic spin, slow path
  with FIFO + anti-starvation handoff.
- It guarantees mutual exclusion and release-acquire memory ordering,
  and nothing else.
- It cannot be used to share state with an ISR; the spinlock exists for
  that.
- It is the default sleeping lock in the kernel; under PREEMPT_RT it
  gains priority inheritance transparently.

For the complementary primitive — what to use when you cannot sleep,
when an ISR participates, or when the critical section is sub-microsecond
— read [SpinLock_Detailed_Design.md](SpinLock_Detailed_Design.md).

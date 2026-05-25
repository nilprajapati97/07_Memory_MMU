# Spinlock — Conceptual Detailed Design

> **Scope:** Pure-concept treatment of the kernel spinlock. No code.
> Covers all execution contexts (process / softirq / hardirq / NMI /
> preempt-disabled / RT), why busy-waiting is correct, IRQ-disabling
> variants, the deadlock geometries that arise when contexts mix, and
> the interaction with the scheduler and the cache coherence fabric.

> **Companion:** [Mutex_Detailed_Design.md](Mutex_Detailed_Design.md).

---

## 1. The Conceptual Identity of a Spinlock

A spinlock is a **busy-wait, anonymous, single-holder mutual-exclusion**
primitive. Three words define its identity:

| Word | Meaning |
|------|---------|
| **Busy-wait** | A contender that cannot acquire stays on the CPU and re-checks the lock state in a tight loop. It does not yield, does not sleep, and is not scheduled away. |
| **Anonymous** | The lock does not record who holds it. There is no owner identity; there is only "free" or "held". |
| **Single-holder** | At most one execution stream may be inside the critical section. No counting, no reader/writer split (the rwlock is a separate primitive). |

The first property — busy-waiting — is the defining trade-off. The
spinlock burns CPU on contenders in exchange for the right to be
acquired in any context, including those where sleeping is impossible.

---

## 2. Why Busy-Wait Is the Right Choice (Conceptual)

A sleeping lock has a high constant cost on contention: putting a task
to sleep and waking it up later requires the scheduler to walk run-queue
data structures, save and restore register state, switch address spaces
(or not), and reload cache lines. On modern hardware this is on the
order of **microseconds**.

If the expected wait is **shorter than that cost**, sleeping is a
pessimization — you pay microseconds to avoid nanoseconds of spinning.

Spinlock-protected critical sections are deliberately designed to be
**short** (a handful of instructions to at most a microsecond). Under
this design rule, busy-waiting is always cheaper than sleeping.

The second reason is more fundamental: in atomic contexts — IRQ
handlers, softirqs, NMI, preempt-disabled regions — sleeping is not
even possible. A primitive usable in those contexts **must** busy-wait
because that is the only option.

---

## 3. The Execution-Context Map

The kernel partitions execution into contexts of increasing
restrictiveness. The spinlock is legal in **all** of them — but each
context has rules about *which variant* of the spinlock to use.

```
                       Execution Context Pyramid
                       ─────────────────────────

           ┌─────────────────────────────────┐
           │   NMI                           │   spinlock OK; raw variant preferred
           ├─────────────────────────────────┤
           │   Hardware IRQ (hardirq)        │   spinlock OK; plain inside ISR
           ├─────────────────────────────────┤
           │   Softirq / tasklet             │   spinlock OK; _bh from process side
           ├─────────────────────────────────┤
           │   preempt_disabled / atomic     │   spinlock OK
           ├─────────────────────────────────┤
           │   Process context (sleepable)   │   spinlock OK; _irqsave if shared with IRQ
           └─────────────────────────────────┘
```

| Context | Acceptable variant | Rationale |
|---------|--------------------|-----------|
| Process, lock NOT shared with IRQ/softirq | Plain `spin_lock` | Only needs to disable preemption. |
| Process, lock shared with softirq | `spin_lock_bh` | Must also disable softirqs on local CPU. |
| Process, lock shared with hardware IRQ | `spin_lock_irqsave` | Must also disable hardware IRQs on local CPU. |
| Inside a softirq handler | Plain `spin_lock` | Softirqs already disabled on this CPU. |
| Inside a hardware IRQ handler | Plain `spin_lock` | Hardware IRQs already disabled on this CPU. |
| Inside an NMI | Raw spinlock only | NMIs are not maskable; even disabling IRQs offers no protection. |

The general principle: **the lock acquirer must mask out every context
that could preempt it on the local CPU and also try to acquire the same
lock.** Failing to do so produces same-CPU deadlock — see §6.

---

## 4. The Two Foundational Invariants

### Invariant A — Mutual Exclusion

At any instant, at most one execution stream is inside the critical
section. The lock state word transitions from "free" to "held" via an
atomic operation; failed transitions retry. There is no second concurrent
holder.

### Invariant B — Causality

All writes performed by the previous holder while inside the critical
section are visible to the next holder when its acquire completes. This
is the standard release-acquire pair, identical in spirit to the mutex's
guarantee but with no sleeping involved.

These invariants are *all* the spinlock guarantees. Anything else —
fairness ordering, IRQ masking, preemption disabling, debug checks — is
implementation detail or policy on top.

---

## 5. The Three Things a Spinlock Acquire Does Locally

Independent of which variant is used, every spinlock acquire performs
three things on the **local CPU**:

1. **Disables kernel preemption.** While holding any spinlock, the
   holder cannot be context-switched off the CPU. This is essential: if
   another task were scheduled in, it might try to acquire the same
   lock, and since the holder is no longer running, the lock would
   never be released.

2. **Optionally disables a higher-priority context** that could preempt
   the holder on the same CPU and also try to take the lock:
   - `_bh` variant additionally disables softirqs.
   - `_irq` / `_irqsave` variant additionally disables hardware IRQs.

3. **Performs the atomic claim itself.** The shared lock state word is
   read-modified-written using a hardware atomic primitive (compare-and-
   swap, exchange, or similar). On success the contender owns the lock;
   on failure it busy-waits.

The first two are *local* effects (only this CPU is affected). The third
is the *cross-CPU* coordination — and it is the only step that involves
contention with other CPUs.

---

## 6. The Same-CPU Deadlock — Conceptual Core

The most important conceptual point about the spinlock is this:

> **A spinlock by itself does NOT protect against a higher-priority
> context on the same CPU.**

If process-context code on CPU 0 takes a plain `spin_lock`, and a
hardware IRQ fires on CPU 0 whose handler also tries to take the same
spinlock, the result is:

1. The IRQ pre-empts the holder mid-critical-section.
2. The handler tries to acquire the lock; it spins waiting for the
   holder to release.
3. The holder cannot release — it has been preempted by the very handler
   that is now spinning waiting for it.
4. The CPU is locked forever. Eventually a watchdog fires.

The fix is to **disable** the preempting context on the local CPU at
acquire time. That is the entire purpose of the `_bh`, `_irq`, and
`_irqsave` variants. They guarantee that no execution stream on the
local CPU can take the lock while the holder is in the critical section.

A spinlock between two CPUs only ever wastes CPU; a spinlock that
collides with itself on a single CPU is a hard deadlock. This asymmetry
is the source of nearly every spinlock bug in the kernel.

---

## 7. Cross-CPU Contention — What Happens on the Wire

When two CPUs try to take the same spinlock simultaneously, the
hardware's cache-coherence protocol arbitrates. Conceptually:

- The lock state word lives in a single cache line.
- The first CPU to issue a successful atomic op invalidates the line
  in all other caches and obtains exclusive ownership.
- Other CPUs read the now-changed value, see "held", and retry.
- Each retry is itself an atomic op that traffics on the inter-CPU
  bus.

A naive implementation has each spinner hammer the shared line on
every retry, producing O(N²) coherence traffic for N spinners — every
spinner invalidates every other spinner's cached copy. The kernel
avoids this by arranging spinners in a queue (a "ticket" or "MCS"
discipline), where each spinner waits on its **own** private cache line
and only the queue head reads the shared state. Coherence traffic drops
to O(1) per acquire/release regardless of contender count, and waiters
are served in strict arrival order.

This implementation detail matters conceptually because it determines:

- **Fairness:** queued spinlocks are FIFO; older designs allowed barging.
- **Scalability:** queued spinlocks remain efficient on hundreds of
  CPUs; older designs degraded badly past a few sockets.
- **NUMA behaviour:** spin location matters when the lock crosses
  socket boundaries; the queue discipline keeps spin reads local.

---

## 8. The Hold-Time Contract

Because every contender on every other CPU is **burning a CPU core**
while waiting, the spinlock makes an implicit contract with the rest of
the system:

> **The holder will release the lock in a bounded, short time.**

Concretely, "short" means **no operation inside the critical section
may be allowed to sleep**, and the total critical-section length should
be on the order of microseconds at most. Any of the following inside a
spinlock-held region is a contract violation:

- Sleeping memory allocation (any allocator flag that admits direct
  reclaim).
- Mutex acquisition.
- I/O waits or completions.
- Copy to or from user space (because of potential page faults).
- Filesystem operations.
- Voluntary scheduling calls.
- Anything that internally may sleep, including subsystem helpers that
  document themselves as "may sleep".

A violation does not always crash immediately — sometimes the call
returns quickly under low load and the bug surfaces only later, under
memory pressure or a busy filesystem. The kernel offers debug
infrastructure that flags every such call at the moment it happens, but
in production, violations manifest as system-wide hangs: the holder
sleeps, every contender on every other CPU spins forever, watchdogs
fire.

The hold-time contract is enforced by discipline, not by mechanism. It
is the single most important rule a driver author must internalize.

---

## 9. Interaction With ISR / Hard-IRQ Context

The spinlock's primary use case is **sharing data between an ISR and a
process-context thread**. The pattern is asymmetric:

- The **ISR side** uses a plain spinlock. Hardware IRQs are already
  disabled on this CPU (IRQ entry mask). No additional masking is
  needed.
- The **process side** uses the `_irqsave` variant. It must disable
  hardware IRQs on its local CPU to prevent the ISR from preempting it
  mid-critical-section and same-CPU-deadlocking on the lock.

The `_irqsave` choice is *required* even if the process-context code
believes it runs on a CPU different from the one handling the IRQ.
The kernel can migrate the process at any time (subject to affinity),
and even briefly running on the same CPU as the ISR is enough to
trigger the deadlock. The only safe assumption is "the ISR could fire
on my CPU right now".

The `_irqsave` variant also saves and restores the prior IRQ-enabled
state, so it composes correctly with already-IRQ-disabled regions.
Plain `_irq` (without save) assumes IRQs were enabled before the
acquire — incorrect composition under nested locking.

---

## 10. Interaction With Softirq / Tasklet Context

Softirqs and tasklets are atomic execution streams that run between
hardware IRQs and process context — typically on return from a hardware
IRQ or at scheduler boundaries. They cannot sleep.

If a lock is shared between a softirq (e.g. network RX processing) and
process context (e.g. configuration code), the pattern mirrors the IRQ
case but with `_bh` instead of `_irqsave`:

- The **softirq side** uses a plain spinlock. Softirqs are not
  reentrant on the same CPU — only one softirq runs at a time per CPU
  — so internal protection is not needed for same-context preemption.
- The **process side** uses `_bh` (bottom-half). This disables softirq
  processing on the local CPU for the duration of the critical section,
  preventing the same-CPU deadlock that would arise if a softirq
  fired and also tried to take the lock.

`_bh` is strictly cheaper than `_irqsave` because it leaves hardware
IRQs enabled. Use `_bh` when the lock is only shared with softirqs;
use `_irqsave` when an actual hardware IRQ handler is in the picture.

---

## 11. Interaction With NMI Context

The non-maskable interrupt cannot be disabled by any of the variants
above. An NMI handler that takes a regular spinlock is unsafe: if the
NMI fires while the same CPU holds the lock, the handler will deadlock
on the same-CPU pattern, and no masking discipline can prevent it.

For data shared with NMI handlers, only **raw spinlocks** combined with
extreme care are acceptable — or, more commonly, lock-free designs
(per-CPU buffers, RCU, atomic counters). NMI-safety is a specialist
topic and almost always solved by avoiding shared state with NMI rather
than locking it.

---

## 12. Interaction With the Scheduler

Holding a spinlock has direct scheduler consequences on the local CPU:

| Aspect | Effect |
|--------|--------|
| Preemption | Disabled. The holder cannot be context-switched off. |
| Migration | Disabled. The holder cannot move to another CPU. |
| Voluntary sleep | Forbidden by contract. Attempting it triggers a kernel BUG. |
| Higher-priority wake | Honored by the scheduler only after the lock is released. A real-time task waking up on this CPU must wait. |
| RCU read-side | Implicit RCU read-side critical section is active for many spinlock variants. Useful for combined RCU + spinlock patterns. |

These effects make the spinlock the right primitive when **latency
predictability** matters: the holder will not be perturbed while
inside the critical section.

---

## 13. The Spinlock Under PREEMPT_RT

Under PREEMPT_RT the picture changes radically. The `spinlock_t` type
is *reimplemented* as a sleeping mutex with priority inheritance.
Conceptually:

- `spin_lock` on PREEMPT_RT may sleep on contention.
- Hardware IRQ handlers run as threaded handlers; they no longer
  execute in true hardirq context except for a small entry stub.
- True atomic context survives only inside the `raw_spinlock_t` type
  and inside the small remaining set of code that runs with hardware
  IRQs explicitly disabled.

Code that obeys the strict non-RT rules — no sleeping under spinlock,
short critical sections, correct `_irqsave` / `_bh` variants — works
unchanged on PREEMPT_RT, but the operational character changes:

- Spinning is replaced by sleeping with priority inheritance.
- Throughput on heavily-contended spinlocks may drop slightly (sleep
  overhead) but latency for high-priority tasks improves dramatically.
- The `raw_spinlock_t` becomes the only true spinlock; use it
  sparingly and only where genuine atomic semantics are required
  (scheduler internals, true IRQ entry/exit, hardware register
  protection in critical paths).

A driver written with the standard discipline ports to PREEMPT_RT
without source changes — that is the entire point of the abstraction.

---

## 14. Failure Modes (Conceptual Catalog)

| Failure | Conceptual cause |
|---------|------------------|
| **Same-CPU deadlock** | Holder used a variant that did not mask the preempting context. ISR or softirq fired, tried to take the lock, and now waits for a holder that cannot run. |
| **System-wide lockup** | Holder sleeps (called a sleeping function inside the critical section). All contenders on all CPUs spin forever. |
| **Soft-lockup watchdog** | Holder runs too long inside the critical section. After ~20 seconds the watchdog fires. |
| **Lost ordering / corruption** | Caller forgot to use `_irqsave` / `_bh` and the shared data is touched by an ISR / softirq concurrently. Race undetected by the lock. |
| **Wrong-CPU release** | Not directly possible (spinlock has no owner identity), but flag-mismatched `irqrestore` after migration produces wrong IRQ state restoration. |
| **Cache-line bouncing** | High contention on a non-queued spinlock degrades throughput non-linearly. Modern queued implementation mitigates this; legacy code on older kernels can suffer. |
| **Priority inversion (non-RT)** | Plain spinlocks have no priority awareness. A low-priority CPU-bound thread holding the lock blocks a high-priority CPU-bound contender. RT or shorter critical sections are the only mitigation. |

---

## 15. Lifecycle of a Spinlock Acquisition (Narrative)

1. **Entry.** A caller asks to acquire the lock. The variant determines
   what local masking is performed first.
2. **Local masking.** Kernel preemption is disabled. Optionally,
   softirqs are disabled (`_bh`) or hardware IRQs are disabled
   (`_irq` / `_irqsave`, with `_irqsave` also recording the prior IRQ
   state).
3. **Atomic claim.** The lock state word is atomically tested. If free,
   it transitions to held; the caller now owns the lock.
4. **Contention path.** If held, the caller joins a queue (typically a
   per-CPU MCS node) and spins on its own private cache line until the
   node ahead signals it. The queue head spins on the shared lock
   state. When the lock becomes free, the queue head atomically claims
   it and signals the next node.
5. **Critical section.** The caller performs its short, non-sleeping
   work on the protected data. No scheduling can occur on this CPU.
6. **Release.** The lock state transitions back to free. If a queued
   waiter exists, ownership is handed off (with appropriate cache-line
   notification). Local masking is reversed in opposite order: IRQs
   re-enabled (or restored), softirqs re-enabled, preemption re-enabled.
7. **Preemption check.** Re-enabling preemption may immediately
   schedule a higher-priority task that became runnable while the
   lock was held. This is the only point at which the holder's CPU
   may yield.

---

## 16. Conceptual Contrast With the Mutex

| Property | Spinlock | Mutex |
|----------|----------|-------|
| Wait strategy | Busy-wait | Sleep (after optional bounded spin) |
| Legal contexts | All (with correct variant) | Process context only |
| Holder identity | Not tracked | Tracked |
| Priority inheritance | No (non-RT) / Yes (RT) | Yes (RT variant / PREEMPT_RT) |
| Sleep allowed in critical section | **Never** | Yes — that's the point |
| Appropriate critical-section length | Sub-microsecond to ~µs | µs to ms |
| Cost to contenders | High — CPU is fully consumed | Low — task is off-CPU |
| Shared with ISR / softirq | Yes (the canonical use case) | No (forbidden) |
| Watchdog risk if abused | Soft/hard lockup | Hung-task warning |

The two primitives are complementary, not interchangeable. Each is the
correct answer in the contexts where the other is wrong.

---

## 17. When To Choose a Spinlock (Decision Criteria)

Pick a spinlock when **any** of the following is true:

1. The data is touched by an ISR, softirq, tasklet, or any other
   atomic-context handler.
2. The critical section is genuinely brief — a handful of register or
   memory accesses, no sleeping calls.
3. Predictable, low-latency mutual exclusion is required and waking
   another task would dominate the critical-section cost.
4. The data is touched in a context where sleeping is not legal
   (preempt-disabled region, hard-IRQ context).

If **none** of those apply — the lock is only ever taken in process
context, the critical section is long or contains sleep-capable calls
— a mutex is the better answer.

---

## 18. Summary

- A spinlock is a **busy-wait, anonymous, single-holder** mutual-
  exclusion primitive.
- It is legal in **every** execution context, with the caveat that the
  caller must pick the variant that masks the preempting contexts
  capable of taking the same lock on the local CPU.
- Its single non-negotiable rule: **the holder must not sleep**.
- It guarantees mutual exclusion, release-acquire memory ordering, and
  local preemption-off semantics. Nothing else.
- It is the right answer when sharing state with ISRs or when the
  critical section is sub-microsecond. It is the wrong answer
  everywhere else — there, the mutex applies.

For the complementary primitive — what to use when sleeping is
acceptable, when ownership tracking matters, or when the critical
section is non-trivial — read [Mutex_Detailed_Design.md](Mutex_Detailed_Design.md).

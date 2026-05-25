# SpinLock Scenarios — Detailed Design Documents

This folder explains, **theory-first**, how the Linux kernel `spinlock_t` behaves in
multi-CPU systems when an **ISR**, a **lock-holding thread**, and a **second
contending thread** all compete for the same lock — and what happens when the
holder makes a *sleep-capable* allocation (`kmalloc(..., GFP_KERNEL)`) inside the
critical section.

The material is split into three progressive scenarios. Read them in order.

---

## Scenarios at a Glance

| # | Document | What it shows |
|---|----------|---------------|
| 1 | [01_Scenario_ISR_Thread_Contention.md](01_Scenario_ISR_Thread_Contention.md) | Baseline: ISR on CPU1, Thread-1 holds lock on CPU2, Thread-2 contends on CPU3. Buggy vs corrected (`spin_lock` vs `spin_lock_irqsave`). |
| 2 | [02_Scenario_Spinlock_with_GFP_KERNEL_Deadlock.md](02_Scenario_Spinlock_with_GFP_KERNEL_Deadlock.md) | The holder calls `kmalloc(100, GFP_KERNEL)` inside the critical section → `BUG: scheduling while atomic`, cascading lockup on CPU1 and CPU3. |
| 3 | [03_Scenario_Correct_Synchronization.md](03_Scenario_Correct_Synchronization.md) | Four corrective patterns (pre-allocate, `GFP_ATOMIC`, mutex swap, canonical `irqsave + ATOMIC`), with a decision matrix. |

---

## Shared Actors (used across all 3 docs)

| Actor | CPU | Role |
|-------|-----|------|
| **ISR-A** | CPU 1 | Hardware interrupt handler for the device. Needs to read/write the shared register state. |
| **Thread-1** | CPU 2 | Process-context kernel thread; **current holder** of `dev_lock`. |
| **Thread-2** | CPU 3 | Process-context kernel thread; **contender** wanting `dev_lock`. |
| **`dev_lock`** | — | `spinlock_t` guarding the shared device state / register shadow. |
| **Shared resource** | — | Device register window + driver-private structure. |

---

## Prerequisites — What is a SpinLock?

A `spinlock_t` is a **busy-wait** mutual-exclusion primitive used in the Linux
kernel:

- The contender **spins in a tight loop** (does not sleep) until the lock is free.
- Acquiring a spinlock **disables kernel preemption on the local CPU**.
- The `_irqsave` / `_bh` variants additionally **disable hardware IRQs / softirqs**
  on the local CPU.
- Therefore, while holding a spinlock you are in **atomic context** — you **must
  not sleep**, must not call `mutex_lock`, `msleep`, `wait_event`, `copy_from_user`,
  or any allocator that may block (e.g. `kmalloc(..., GFP_KERNEL)`).

### Why busy-wait?

Critical sections protected by spinlocks are **expected to be very short** (a few
instructions to a few µs). The cost of sleeping + context switch + wakeup would
dwarf the cost of a brief spin, so spinning wins.

### Spinlock variants used in these scenarios

| API | Disables preemption | Disables local IRQs | Disables softirqs | Safe vs ISR on same CPU? |
|-----|---------------------|---------------------|-------------------|--------------------------|
| `spin_lock` / `spin_unlock` | ✅ | ❌ | ❌ | **No** — deadlock if ISR runs on same CPU and also takes the lock |
| `spin_lock_bh` / `spin_unlock_bh` | ✅ | ❌ | ✅ | Only vs softirqs |
| `spin_lock_irqsave` / `spin_unlock_irqrestore` | ✅ | ✅ | ✅ | **Yes** — canonical pattern when sharing with an ISR |

---

## Quick Comparison: Spinlock vs Mutex vs Semaphore vs RW-lock

| Primitive | Context | Can sleep? | Owner tracked | Typical use |
|-----------|---------|------------|---------------|-------------|
| `spinlock_t` | Atomic OK (ISR, softirq, holder) | **No** | No | Very short critical sections; shared with IRQ/softirq |
| `struct mutex` | Process context only | **Yes** | Yes | Longer critical sections in process context |
| `struct semaphore` | Process context | Yes | No | Counting / signalling |
| `rwlock_t` | Atomic OK | No | No | Read-mostly data (largely superseded by RCU) |

> Detailed comparison beyond the table is **out of scope** here — focus is the
> spinlock + allocation interaction.

---

## How to Read the Scenario Documents

Each scenario doc follows the same template:

1. **Setup & Actors** — who runs where.
2. **Buggy Variant** (where applicable) — what goes wrong and why.
3. **Corrected Variant** — what to do instead.
4. **Mermaid sequence diagram** — message-passing view across CPUs.
5. **ASCII CPU timeline** — wall-clock view across CPU1 / CPU2 / CPU3.
6. **Key Takeaways**.
7. **Interview Q&A**.

---

## Navigation

➡ **Next:** [Scenario 1 — ISR + Thread Contention](01_Scenario_ISR_Thread_Contention.md)

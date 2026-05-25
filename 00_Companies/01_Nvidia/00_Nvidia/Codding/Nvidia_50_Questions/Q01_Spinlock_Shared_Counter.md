# Q01: Spinlock to Protect a Shared Counter (Multi-CPU)

**Section:** Linux Kernel Internals | **Difficulty:** Medium | **Topics:** `spinlock`, `atomic64_t`, SMP, IRQ context

---

## Question

Implement a custom kernel module that uses a spinlock to protect a shared counter incremented by multiple CPUs.

---

## Answer

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/spinlock.h>

static spinlock_t counter_lock;
static atomic64_t shared_counter;

static void increment_counter(void *info)
{
    unsigned long flags;
    spin_lock_irqsave(&counter_lock, flags);
    atomic64_inc(&shared_counter);
    spin_unlock_irqrestore(&counter_lock, flags);
}

static int __init counter_init(void)
{
    spin_lock_init(&counter_lock);
    atomic64_set(&shared_counter, 0);
    on_each_cpu(increment_counter, NULL, 1); /* run on all CPUs, wait=1 */
    pr_info("Counter value: %lld\n", atomic64_read(&shared_counter));
    return 0;
}

static void __exit counter_exit(void)
{
    pr_info("Module removed\n");
}

module_init(counter_init);
module_exit(counter_exit);
MODULE_LICENSE("GPL");
```

---

## Explanation

### Core Concept

A **spinlock** is a busy-wait lock — the acquiring thread spins in a tight loop until the lock is free. It is the go-to primitive for very short critical sections in the Linux kernel, especially when the code may run in interrupt context where sleeping is forbidden.

`spin_lock_irqsave` is the safe variant that:
1. **Disables local CPU interrupts** (prevents IRQ handler from racing with the current code)
2. **Saves the previous IRQ flags** so they can be restored on unlock

`on_each_cpu()` schedules the function on every online CPU and waits for all completions before returning.

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `spin_lock_init(&lock)` | Initialize a spinlock at runtime |
| `spin_lock_irqsave(&lock, flags)` | Lock + disable IRQs + save flags |
| `spin_unlock_irqrestore(&lock, flags)` | Unlock + restore saved IRQ flags |
| `atomic64_t` | 64-bit atomic integer — hardware-atomic operations |
| `atomic64_inc(&v)` | Atomically increment a 64-bit counter |
| `on_each_cpu(fn, info, wait)` | Execute `fn` on every online CPU |

### Trade-offs & Pitfalls

- **Never sleep while holding a spinlock.** Calling `kmalloc(GFP_KERNEL)`, `mutex_lock()`, or any schedule point inside a spinlock critical section causes a kernel BUG or deadlock.
- **Avoid long critical sections.** Spinning wastes CPU cycles on all other cores waiting for the lock. For counters, prefer `per_cpu` variables or `atomic64_inc` directly (which is already atomic on x86).
- **`spin_lock_irqsave` vs `spin_lock_bh`:** Use `irqsave` when the same lock is accessed from hard IRQ handlers. Use `spin_lock_bh` when only softirq/tasklet context accesses it.
- **Nested spinlocks** must always be acquired in a consistent order to avoid deadlocks — enforced by `CONFIG_LOCKDEP`.

### NVIDIA / GPU Context

NVIDIA GPU drivers use spinlocks extensively for:
- **IRQ handler → driver state** synchronization (e.g., acknowledging GPU fences from `irqreturn_t`)
- **Ring buffer head/tail** pointer updates shared between submit path and IRQ completion path
- **Per-channel state** protection in the command submission fast path where microsecond latency matters

---

## Cross Questions & Answers

**CQ1: Why use `spin_lock_irqsave` instead of plain `spin_lock`?**
> `spin_lock` only prevents other CPUs from acquiring the lock but does NOT disable interrupts on the current CPU. If an IRQ handler on the same CPU also tries to acquire the same spinlock, it will spin forever — a deadlock. `spin_lock_irqsave` disables interrupts first, eliminating this hazard.

**CQ2: When would you choose `atomic64_inc` over a spinlock-protected counter?**
> When the only operation needed is increment/decrement/read of a single integer. `atomic64_inc` compiles to a single locked `LOCK ADD` instruction on x86 — far faster than acquiring a spinlock. Spinlocks are needed when protecting *compound* state (multiple variables that must be updated together atomically).

**CQ3: What does `CONFIG_LOCKDEP` do and how does it help find spinlock bugs?**
> `CONFIG_LOCKDEP` is a kernel debugging feature that tracks every lock acquisition order at runtime. It maintains a directed graph of "lock A was acquired while holding lock B" and reports a warning (with stack trace) if a potential deadlock cycle is detected — even if the deadlock has not actually occurred yet. Essential for GPU driver development where locking hierarchies are complex.

**CQ4: What is the difference between a spinlock and a mutex in the Linux kernel?**
> A **spinlock** busy-waits (no sleeping), can be used in any context (IRQ, softirq, process), and has very low overhead for short sections. A **mutex** puts the waiting thread to sleep, can only be used in process context (where sleeping is allowed), but scales better when contention is high or the critical section is long. Trying to acquire a mutex in IRQ context will trigger a kernel BUG.

**CQ5: How does NVIDIA's driver handle the case where a spinlock-protected GPU register write must be ordered with respect to DMA operations?**
> After writing to a GPU register while holding a spinlock, the driver inserts a `wmb()` (write memory barrier) or uses `writel()` which includes an implicit barrier on x86. This ensures the write is visible to the GPU's DMA engine before the spinlock is released and another CPU potentially queues more DMA work. On weakly-ordered architectures (ARM), explicit barriers are critical.

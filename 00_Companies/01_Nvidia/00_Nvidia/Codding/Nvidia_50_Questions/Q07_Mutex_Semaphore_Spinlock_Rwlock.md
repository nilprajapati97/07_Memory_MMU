# Q07: mutex vs semaphore vs spinlock vs rwlock

**Section:** Linux Kernel Internals | **Difficulty:** Medium | **Topics:** synchronization primitives, locking, IRQ context, priority inversion, reader-writer locks

---

## Question

What is the difference between `mutex`, `semaphore`, `spinlock`, and `rwlock`? Write kernel code showing proper usage.

---

## Answer

```c
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/rwlock.h>

/* ─── SPINLOCK ────────────────────────────────────────────────────────────
 * Busy-wait. Zero overhead for uncontended case.
 * Use for: short critical sections, IRQ-safe operations.
 * Rule: NEVER sleep while holding a spinlock.
 */
static DEFINE_SPINLOCK(hw_lock);

void update_hw_reg(void __iomem *reg_base, u32 val)
{
    unsigned long flags;
    spin_lock_irqsave(&hw_lock, flags);   /* lock + disable local IRQs */
    writel(val, reg_base);                /* short: one register write */
    spin_unlock_irqrestore(&hw_lock, flags);
}

/* ─── MUTEX ───────────────────────────────────────────────────────────────
 * Sleepable lock. Puts the waiter to sleep (no CPU wasted spinning).
 * Use for: process context only, longer critical sections.
 * Supports priority inheritance (prevents priority inversion).
 */
static DEFINE_MUTEX(dev_mutex);

int open_device(struct inode *inode, struct file *filp)
{
    /* Returns -ERESTARTSYS if the process receives a signal while waiting */
    if (mutex_lock_interruptible(&dev_mutex))
        return -ERESTARTSYS;

    /* ... device open logic, may allocate memory, set up DMA ... */

    mutex_unlock(&dev_mutex);
    return 0;
}

/* ─── RWLOCK ──────────────────────────────────────────────────────────────
 * Multiple concurrent readers OR one exclusive writer.
 * Use for: read-heavy maps/tables where writes are rare.
 * IRQ-safe variants: read_lock_irqsave / write_lock_irqsave.
 */
static DEFINE_RWLOCK(map_lock);
static u64 address_map[1024];

u64 read_map(int key)
{
    unsigned long flags;
    u64 val;

    read_lock_irqsave(&map_lock, flags);
    val = address_map[key];
    read_unlock_irqrestore(&map_lock, flags);
    return val;
}

void write_map(int key, u64 addr)
{
    unsigned long flags;

    write_lock_irqsave(&map_lock, flags);
    address_map[key] = addr;
    write_unlock_irqrestore(&map_lock, flags);
}

/* ─── SEMAPHORE ───────────────────────────────────────────────────────────
 * Counting semaphore. Use for: resource pool (N identical resources).
 * down() decrements (acquire), up() increments (release).
 * NOT for mutual exclusion of shared state — use mutex for that.
 */
static DEFINE_SEMAPHORE(gpu_dma_channels, 4); /* 4 DMA channels available */

int acquire_dma_channel(void)
{
    /* Blocks until a channel is available, or returns -EINTR on signal */
    return down_interruptible(&gpu_dma_channels);
}

void release_dma_channel(void)
{
    up(&gpu_dma_channels);
}
```

### Comparison Table

| Primitive | Sleepable | IRQ Safe | Context | Use Case |
|-----------|-----------|----------|---------|----------|
| `spinlock` | No (busy-wait) | Yes | Any | Short sections, HW register access |
| `mutex` | Yes | No | Process only | Longer sections, driver open/close |
| `rwlock` | No (busy-wait) | Yes | Any | Read-heavy lookup tables |
| `semaphore` | Yes | No | Process only | Resource pools (N permits) |
| `rw_semaphore` | Yes | No | Process only | Read-heavy with sleep allowed |

---

## Explanation

### Core Concept

Choose your lock based on two dimensions:

**1. Can the code path sleep?**
- IRQ handlers, softirqs, tasklets → **cannot sleep** → use spinlock/rwlock
- Process context (ioctl, open, read) → **can sleep** → mutex/semaphore are preferred

**2. What is the access pattern?**
- Exclusive access → spinlock or mutex
- Concurrent reads + exclusive writes → rwlock or rw_semaphore
- Counting resource pool → semaphore

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `DEFINE_SPINLOCK(lock)` | Static init of spinlock |
| `spin_lock_irqsave(lock, flags)` | Lock + disable IRQs |
| `spin_unlock_irqrestore(lock, flags)` | Unlock + restore IRQs |
| `DEFINE_MUTEX(mutex)` | Static init of mutex |
| `mutex_lock(mutex)` | Acquire mutex (uninterruptible sleep) |
| `mutex_lock_interruptible(mutex)` | Acquire mutex (interruptible by signals) |
| `mutex_trylock(mutex)` | Non-blocking attempt, returns 0 if failed |
| `DEFINE_RWLOCK(lock)` | Static init of rwlock |
| `read_lock_irqsave(lock, flags)` | Shared read lock + disable IRQs |
| `write_lock_irqsave(lock, flags)` | Exclusive write lock + disable IRQs |
| `DEFINE_SEMAPHORE(sem, count)` | Static init with initial count |
| `down_interruptible(sem)` | Acquire (blocks, interruptible) |
| `up(sem)` | Release |

### Trade-offs & Pitfalls

- **rwlock writer starvation:** If readers are continuous, a writer may never acquire the lock. Consider `rw_semaphore` with fairness or use RCU for read-dominated cases.
- **Mutex ownership:** A mutex must be released by the same task that acquired it. Spinlocks have no such restriction.
- **Priority inversion with spinlocks:** A low-priority task holding a spinlock while a high-priority task spins wastes CPU cycles. Linux mutexes support priority inheritance to solve this.
- **Semaphore misuse:** Semaphores are often misused as mutexes (with initial count 1). Prefer `mutex` — it has ownership tracking, lockdep support, and priority inheritance.

### NVIDIA / GPU Context

| Lock Type | NVIDIA GPU Driver Usage |
|-----------|------------------------|
| `spinlock` | GPU ring buffer head/tail, IRQ handler ↔ submit path |
| `mutex` | Device open/close, large resource allocation, firmware load |
| `rwlock` | VA space range tree reads (hot) vs insertions (rare) |
| `semaphore` | DMA channel pool (4 CE engines), limiting concurrent DMA ops |
| `rw_semaphore` | GPU context table (many readers per command submit) |

---

## Cross Questions & Answers

**CQ1: What is priority inversion and how does Linux mutex prevent it?**
> Priority inversion: a high-priority task (H) waits for a mutex held by a low-priority task (L), but a medium-priority task (M) preempts L, preventing L from releasing the mutex. H is effectively blocked by M despite M having lower priority. Linux `mutex` supports **priority inheritance**: when H blocks on L's mutex, L temporarily inherits H's priority, preventing M from preempting it, so L finishes quickly and releases the mutex.

**CQ2: Why should you prefer `mutex_lock_interruptible` over `mutex_lock` in a driver's ioctl handler?**
> In an ioctl handler, if the user sends `CTRL+C` (SIGINT) while the driver is waiting on a mutex, `mutex_lock_interruptible` returns `-ERESTARTSYS` and the syscall returns to userspace cleanly. `mutex_lock` ignores signals and keeps the task sleeping indefinitely — the user's process becomes unkillable (`D` state in `ps`) until the lock is released.

**CQ3: Can you use a mutex inside a spinlock-protected section?**
> No. `mutex_lock` may put the current task to sleep (schedule). Sleeping while holding a spinlock is illegal: the CPU keeps the spinlock held while the scheduler runs other tasks, potentially causing deadlock (if another task tries to acquire the spinlock) or corrupted state. The kernel checks this with `might_sleep()` assertions inside mutex paths.

**CQ4: What is `rw_semaphore` and when does it outperform `rwlock`?**
> `rw_semaphore` is a sleepable reader-writer lock — blocked readers and writers sleep instead of spinning. It is preferred when the read or write section is long (e.g., walking a large tree, loading firmware). `rwlock` is better when the section is very short (just reading a pointer or integer). For GPU driver context table lookups (millions per second), `rwlock` or RCU is preferred over `rw_semaphore`.

**CQ5: What does `lockdep` track and how does it detect lock ordering violations?**
> `lockdep` (enabled via `CONFIG_LOCKDEP`) maintains a directed lock-dependency graph. Each time a lock is acquired while holding another lock, an edge `A → B` is recorded. If a cycle is ever detected (A→B and B→A), `lockdep` reports a "possible circular locking dependency" warning with a full stack trace. This catches potential deadlocks before they manifest in production. NVIDIA runs lockdep-enabled kernels during driver development to catch ordering bugs early.

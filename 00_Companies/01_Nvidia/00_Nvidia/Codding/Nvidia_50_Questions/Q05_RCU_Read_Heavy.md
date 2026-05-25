# Q05: RCU (Read-Copy-Update) for Read-Heavy Data Structures

**Section:** Linux Kernel Internals | **Difficulty:** Hard | **Topics:** RCU, `rcu_read_lock`, `kfree_rcu`, grace period, lock-free reads, memory ordering

---

## Question

Explain and implement RCU (Read-Copy-Update) for a read-heavy data structure.

---

## Answer

```c
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

struct config {
    int  threshold;
    char name[64];
    struct rcu_head rcu;  /* required for kfree_rcu deferred free */
};

/* RCU-protected global pointer */
static struct config __rcu *global_cfg;

/* Writer lock: only one writer at a time */
static DEFINE_SPINLOCK(update_lock);

/* ─── Reader ──────────────────────────────────────────────────────────────
 * Extremely fast: just disables preemption on PREEMPT kernels.
 * Zero lock acquisition — scales to any number of concurrent readers.
 */
int read_threshold(void)
{
    struct config *cfg;
    int val;

    rcu_read_lock();                        /* enter RCU read-side critical section */
    cfg = rcu_dereference(global_cfg);      /* safely load the pointer */
    val = cfg ? cfg->threshold : 0;
    rcu_read_unlock();                      /* exit — may not sleep between lock/unlock */

    return val;
}

/* ─── Writer ──────────────────────────────────────────────────────────────
 * Allocates a NEW config, copies old values, updates the desired field,
 * atomically swaps the pointer, then frees the OLD config after a grace period.
 */
int update_threshold(int new_val)
{
    struct config *new_cfg, *old_cfg;

    new_cfg = kmalloc(sizeof(*new_cfg), GFP_KERNEL);
    if (!new_cfg)
        return -ENOMEM;

    /* Copy current config under a read lock */
    rcu_read_lock();
    old_cfg = rcu_dereference(global_cfg);
    if (old_cfg)
        memcpy(new_cfg, old_cfg, sizeof(*new_cfg));
    else
        memset(new_cfg, 0, sizeof(*new_cfg));
    rcu_read_unlock();

    /* Apply the update */
    new_cfg->threshold = new_val;

    /* Atomically replace the pointer — all NEW readers see new_cfg */
    spin_lock(&update_lock);
    old_cfg = rcu_replace_pointer(global_cfg, new_cfg,
                                   lockdep_is_held(&update_lock));
    spin_unlock(&update_lock);

    /*
     * kfree_rcu: free old_cfg AFTER the next grace period
     * (after all pre-existing readers finish their critical sections).
     * 'rcu' is the name of the struct rcu_head field inside struct config.
     */
    if (old_cfg)
        kfree_rcu(old_cfg, rcu);

    return 0;
}

/* Initialize global config */
static int __init rcu_demo_init(void)
{
    struct config *cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
    if (!cfg)
        return -ENOMEM;
    cfg->threshold = 100;
    rcu_assign_pointer(global_cfg, cfg);
    return 0;
}

static void __exit rcu_demo_exit(void)
{
    struct config *cfg;
    spin_lock(&update_lock);
    cfg = rcu_dereference_protected(global_cfg,
                                     lockdep_is_held(&update_lock));
    rcu_assign_pointer(global_cfg, NULL);
    spin_unlock(&update_lock);
    if (cfg)
        kfree_rcu(cfg, rcu);
    rcu_barrier(); /* wait for all kfree_rcu callbacks to complete */
}
```

---

## Explanation

### Core Concept

**RCU (Read-Copy-Update)** is a synchronization mechanism optimized for the common case of **many concurrent readers, rare writers**. The key insight:

- **Readers** pay almost zero cost (no atomic operations, no lock acquisition)
- **Writers** pay a higher cost: allocate new version → update → swap pointer → wait for old readers to finish → free old version

The "grace period" is the interval during which all pre-existing read-side critical sections complete. After the grace period, no reader can hold a reference to the old pointer.

```
Time →
Reader A:  [rcu_read_lock ... read old_cfg ... rcu_read_unlock]
Reader B:                       [rcu_read_lock ... read new_cfg ...]
Writer:                  swap pointer
                                        ←grace period→  kfree(old_cfg)
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `rcu_read_lock()` | Enter RCU read-side critical section (disables preemption) |
| `rcu_read_unlock()` | Exit read-side critical section |
| `rcu_dereference(ptr)` | Read RCU-protected pointer with acquire semantics |
| `rcu_assign_pointer(ptr, val)` | Assign to RCU pointer with release semantics |
| `rcu_replace_pointer(ptr, new, cond)` | Atomically replace pointer, return old |
| `kfree_rcu(ptr, rcu_field)` | Free after next grace period |
| `synchronize_rcu()` | Block writer until grace period elapses |
| `rcu_barrier()` | Wait for all pending `kfree_rcu` callbacks to complete |
| `rcu_dereference_protected(ptr, cond)` | Read RCU pointer when holding a write-side lock |

### Trade-offs & Pitfalls

- **Cannot sleep inside `rcu_read_lock`/`rcu_read_unlock`.** On non-preemptible kernels, the read-side critical section disables preemption. Sleeping inside violates RCU's grace period assumptions.
- **`kfree_rcu` is asynchronous.** The free happens in the future. Do not access `old_cfg` after calling `kfree_rcu`. Always call `rcu_barrier()` before module unload to flush pending callbacks.
- **RCU vs Seqlock:** Use RCU for pointer-based data (structs, linked lists). Use seqlocks for small integer data that writers update atomically without pointer indirection.
- **`rcu_dereference` vs plain dereference:** Plain dereference of an RCU pointer compiles to a load without memory barrier — on weakly ordered CPUs (ARM), you may read stale data. Always use `rcu_dereference`.

### NVIDIA / GPU Context

NVIDIA GPU driver uses RCU for:
- **Per-process GPU context table:** lookup by PID — read on every GPU command submission (hot path), written only on `open()`/`close()`
- **Driver configuration / feature flags:** read by many GPU execution paths simultaneously, updated rarely via `ioctl`
- **Device list:** list of available GPUs — read constantly by userspace threads, modified only on hotplug events

---

## Cross Questions & Answers

**CQ1: What is a "grace period" in RCU and how does the kernel detect it has elapsed?**
> A grace period is a time window during which every CPU that was executing a read-side critical section at the *start* of the period has completed it. The kernel detects this by observing a "quiescent state" on each CPU — a context switch, idle entry, or user-mode execution. On a non-preemptible kernel, any time a CPU is not executing the read-side critical section is a quiescent state. `synchronize_rcu()` polls for quiescent states on all CPUs.

**CQ2: What is the difference between `call_rcu` and `kfree_rcu`?**
> `call_rcu(rcu_head, callback_fn)` registers an arbitrary callback function to be called after the grace period. `kfree_rcu(ptr, field)` is a convenience wrapper that registers a `kfree` callback — it frees the object containing `rcu_head` at the appropriate offset. `kfree_rcu` is preferred for simple freeing because it allows the kernel to batch multiple `kfree` callbacks together for efficiency.

**CQ3: Can you use RCU inside an interrupt handler?**
> Yes. `rcu_read_lock`/`rcu_read_unlock` are safe in interrupt context on non-preemptible kernels because they only toggle the preemption counter. However, you cannot use `synchronize_rcu()` in IRQ context (it blocks). For IRQ-to-process synchronization, use `call_rcu` or `kfree_rcu` on the writer side, and `rcu_read_lock` in the IRQ handler.

**CQ4: How does TREE RCU differ from TINY RCU in the Linux kernel?**
> `TINY_RCU` is for uniprocessor (UP) kernels — a trivial implementation where there is only one CPU, so grace periods are instantaneous. `TREE_RCU` (default on SMP) uses a hierarchical tree of RCU nodes to efficiently track quiescent states across hundreds or thousands of CPUs without every CPU contacting a central node. NVIDIA servers with 64–256 CPUs require `TREE_RCU` for scalability.

**CQ5: Why is `rcu_assign_pointer` needed instead of a plain assignment?**
> `rcu_assign_pointer` includes a **store-release** memory barrier. This ensures all prior writes to the new object (e.g., initializing its fields) are visible to other CPUs *before* the pointer swap becomes visible. Without this barrier, a reader on another CPU might see the new pointer but still read uninitialized fields of the new object — a data race. `rcu_dereference` on the reader side provides the matching load-acquire barrier.

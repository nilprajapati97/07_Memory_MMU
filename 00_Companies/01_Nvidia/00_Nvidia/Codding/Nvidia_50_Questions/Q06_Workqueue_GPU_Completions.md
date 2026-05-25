# Q06: Work Queue for Batching GPU Command Completions

**Section:** Linux Kernel Internals | **Difficulty:** Medium | **Topics:** `workqueue`, `INIT_WORK`, IRQ bottom half, `list_splice_init`, deferred processing

---

## Question

Implement a kernel work queue that batches GPU command completions.

---

## Answer

```c
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

struct gpu_completion {
    u64              fence_id;
    struct list_head list;
};

/* Dedicated high-priority unbound workqueue */
static struct workqueue_struct *gpu_wq;
static struct work_struct       completion_work;

/* Shared list between IRQ top-half and worker */
static LIST_HEAD(pending_completions);
static DEFINE_SPINLOCK(comp_lock);

/* ─── Bottom Half (Worker) ────────────────────────────────────────────────
 * Runs in process context — can sleep, allocate, call blocking APIs.
 * Uses list_splice_init to drain the shared list under minimal lock hold.
 */
static void process_completions(struct work_struct *work)
{
    struct gpu_completion *comp, *tmp;
    LIST_HEAD(local_list);

    /*
     * Atomically steal the entire pending list.
     * Hold the spinlock only for this O(1) pointer swap —
     * not during the entire processing loop.
     */
    spin_lock(&comp_lock);
    list_splice_init(&pending_completions, &local_list);
    spin_unlock(&comp_lock);

    list_for_each_entry_safe(comp, tmp, &local_list, list) {
        pr_info("GPU: fence %llu completed\n", comp->fence_id);
        /* signal waiting tasks, update timeline, unmap DMA buffers... */
        list_del(&comp->list);
        kfree(comp);
    }
}

/* ─── Top Half (IRQ Handler) ──────────────────────────────────────────────
 * Runs in hard IRQ context — must be extremely fast.
 * Only reads the GPU register, enqueues the completion, schedules work.
 */
irqreturn_t gpu_irq_handler(int irq, void *dev_id)
{
    struct gpu_completion *comp;
    u64 fence_id = readq(dev_id + FENCE_COMPLETE_REG); /* MMIO read */

    /* GFP_ATOMIC: cannot sleep in IRQ context */
    comp = kmalloc(sizeof(*comp), GFP_ATOMIC);
    if (!comp)
        return IRQ_HANDLED; /* drop on OOM — fence timeout will recover */

    comp->fence_id = fence_id;

    spin_lock(&comp_lock);
    list_add_tail(&comp->list, &pending_completions);
    spin_unlock(&comp_lock);

    /* Schedule worker — queue_work is safe from IRQ context */
    queue_work(gpu_wq, &completion_work);
    return IRQ_HANDLED;
}

/* ─── Init / Exit ─────────────────────────────────────────────────────────*/
static int __init gpu_wq_init(void)
{
    /*
     * WQ_UNBOUND: not bound to a specific CPU — can run on any CPU.
     * WQ_HIGHPRI: use high-priority worker threads.
     * max_active=0: use default (per-CPU concurrency).
     */
    gpu_wq = alloc_workqueue("gpu_completion_wq",
                              WQ_UNBOUND | WQ_HIGHPRI, 0);
    if (!gpu_wq)
        return -ENOMEM;

    INIT_WORK(&completion_work, process_completions);

    return request_irq(GPU_IRQ_NUM, gpu_irq_handler,
                        IRQF_SHARED, "gpu", reg_base);
}

static void __exit gpu_wq_exit(void)
{
    free_irq(GPU_IRQ_NUM, reg_base);
    flush_workqueue(gpu_wq);
    destroy_workqueue(gpu_wq);
}
```

---

## Explanation

### Core Concept

**Top half vs Bottom half** is the fundamental Linux IRQ design pattern:

```
GPU raises IRQ
     │
     ▼
Top Half (irqreturn_t)   ← hard IRQ context
  - ACK the interrupt
  - Read status register
  - Enqueue completion
  - Schedule workqueue
     │
     ▼
Bottom Half (work_struct) ← process context (can sleep)
  - Process completions
  - Signal waiters
  - Free DMA mappings
  - Call blocking APIs
```

The critical `list_splice_init` pattern minimizes spinlock hold time: instead of processing each completion under the lock, we atomically steal the entire list in O(1) and process it lock-free.

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `alloc_workqueue(name, flags, max_active)` | Create a custom workqueue |
| `INIT_WORK(&work, fn)` | Initialize a `work_struct` with its handler |
| `queue_work(wq, work)` | Schedule work on the workqueue (safe from any context) |
| `flush_workqueue(wq)` | Wait for all pending work to complete |
| `destroy_workqueue(wq)` | Drain and destroy a workqueue |
| `WQ_UNBOUND` | Worker not bound to a CPU — better for latency-sensitive tasks |
| `WQ_HIGHPRI` | Worker threads run with elevated priority |
| `INIT_DELAYED_WORK(&work, fn)` | For work that should run after a delay |
| `schedule_delayed_work(&work, delay)` | Schedule delayed work |

### Trade-offs & Pitfalls

- **`kmalloc(GFP_ATOMIC)` in IRQ.** Must use `GFP_ATOMIC` — `GFP_KERNEL` will BUG because it tries to sleep. If `GFP_ATOMIC` fails (OOM), the completion is dropped — have a timeout recovery path.
- **`flush_workqueue` before destroy.** Failing to flush before `destroy_workqueue` during module unload can result in work executing after the module's code is unloaded — a guaranteed crash.
- **`queue_work` is idempotent for `work_struct`.** If the work is already queued, a second `queue_work` call is a no-op. This is fine for our pattern — multiple IRQs queue one work item, and the worker drains all completions at once.
- **Tasklet vs Workqueue:** Tasklets run in softirq context (cannot sleep). Workqueues run in process context (can sleep, alloc with `GFP_KERNEL`, call `mutex_lock`). Prefer workqueues for GPU drivers where completion processing may involve DMA unmapping and waking sleeping tasks.

### NVIDIA / GPU Context

NVIDIA GPU drivers use this exact pattern (IRQ top-half + workqueue bottom-half) for:
- **Fence completion signaling:** GPU raises interrupt → driver reads seqno → wakes tasks waiting on fence
- **Channel recovery:** GPU errors trigger IRQ → driver schedules recovery workqueue to reset the channel
- **DMA buffer cleanup:** GPU completes DMA → IRQ schedules unmap/unpin work on CPU memory
- **ECC error handling:** Memory error IRQ → defer to workqueue for page retirement (slow, blocking)

---

## Cross Questions & Answers

**CQ1: What is the difference between `schedule_work` (system workqueue) and `alloc_workqueue` + `queue_work`?**
> `schedule_work` uses the kernel's shared `system_wq` workqueue, which is convenient but shared with all drivers in the system. A misbehaving or slow handler in `system_wq` can starve other users. For GPU drivers, `alloc_workqueue` creates a dedicated workqueue with controlled priority and concurrency, ensuring GPU completions are not delayed by unrelated kernel subsystems.

**CQ2: What is `WQ_MEM_RECLAIM` and why does a GPU driver need it?**
> `WQ_MEM_RECLAIM` guarantees that at least one worker thread is always available even under severe memory pressure, by pre-allocating a rescue thread at workqueue creation time. GPU drivers should use this flag if their workqueue processes callbacks that might be called during the memory reclaim path — otherwise, a deadlock can occur where the memory reclaim path is waiting for the workqueue, but the workqueue can't start because the system is OOM.

**CQ3: How would you implement a "high-water mark" batching strategy — only process completions when 16 have accumulated?**
> Use `INIT_DELAYED_WORK` and schedule with a short timeout. In the IRQ handler, increment an atomic counter. When the counter reaches 16, `queue_work` immediately. Otherwise, `schedule_delayed_work` with a 1ms timeout as a flush. This amortizes per-completion overhead while bounding latency.

**CQ4: Can a work item reschedule itself? Is there a risk of live-locking the system?**
> Yes, a work item can call `queue_work(wq, work)` on itself before returning. The kernel handles this safely — the work is re-queued after the current execution completes. There is no live-lock risk because the workqueue infrastructure ensures the worker thread yields control between executions, allowing the scheduler to run other threads.

**CQ5: How does NVIDIA's driver ensure that GPU fence completion callbacks are called in fence order (not interrupt order)?**
> The driver maintains a sorted list of pending fences ordered by seqno. The worker processes completions in order: it checks the head of the sorted list, signals it only if `gpu_seqno >= fence_seqno`, advances to the next, and repeats. Even if interrupts arrive out of order (possible on multi-queue GPUs), the software layer serializes signaling. This is similar to how `drm/fence` ordering works in the kernel DRM subsystem.

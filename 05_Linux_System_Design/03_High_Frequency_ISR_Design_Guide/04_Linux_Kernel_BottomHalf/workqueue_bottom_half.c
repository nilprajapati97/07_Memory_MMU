// SPDX-License-Identifier: GPL-2.0
/*
 * workqueue_bottom_half.c — Linux Kernel Workqueue Implementation
 *
 * WORKQUEUE: deferred work running in PROCESS context (kworker kernel thread).
 *
 * Process context properties (vs tasklet's softirq context):
 *   ✅ CAN sleep: mutex_lock(), msleep(), wait_event(), down()
 *   ✅ CAN allocate: kmalloc(GFP_KERNEL), vmalloc()
 *   ✅ CAN do I/O: filp_open(), vfs_write(), sendmsg()
 *   ✅ CAN access: per-process resources (via kthread context)
 *   ✅ CAN use: complete(), wait_for_completion()
 *   ❌ CANNOT access user-space memory directly (no user mm_struct)
 *      Use get_user_pages() or copy_to_user() with set_fs() (deprecated in 5.10)
 *   ❌ CANNOT call kernel_fpu_begin() without matching kernel_fpu_end()
 *
 * Three workqueue types (choose based on requirements):
 *
 *   1. system_wq (global shared):
 *      schedule_work(&work);
 *      Pros: no allocation, simple
 *      Cons: shared with all drivers, may be delayed, no priority
 *      Use for: rare events, non-latency-sensitive work
 *
 *   2. alloc_workqueue("name", flags, max_active):
 *      queue_work(wq, &work);
 *      Pros: dedicated thread pool, configurable priority, isolation
 *      Cons: consumes kernel thread(s)
 *      Use for: high-rate, latency-sensitive, isolated from other drivers
 *      Flags:
 *        WQ_HIGHPRI: runs before normal-priority work
 *        WQ_UNBOUND: not bound to a CPU (can run on any CPU)
 *        WQ_CPU_INTENSIVE: long-running; yields to other work sooner
 *        WQ_FREEZABLE: suspends during system sleep
 *
 *   3. alloc_ordered_workqueue("name", flags):
 *      Equivalent to alloc_workqueue with max_active=1.
 *      Work items execute one at a time, in submission order.
 *      Use when work items must not run concurrently (stateful processing).
 *
 * Threaded IRQ (alternative — often better than workqueue for ISR deferral):
 *   request_threaded_irq(irq, top_half, thread_fn, flags, name, dev);
 *   thread_fn runs in a dedicated kernel thread (irq/%s/%d naming).
 *   Simplest model: top half checks/clears HW, thread_fn does everything else.
 *   PREEMPT_RT friendly: thread can be assigned RT scheduling policy.
 */

#include <linux/workqueue.h>    /* work_struct, alloc_workqueue, queue_work  */
#include <linux/kfifo.h>        /* kfifo_out                                 */
#include <linux/slab.h>         /* kmalloc, kfree, GFP_KERNEL               */
#include <linux/mutex.h>        /* mutex_lock, mutex_unlock                  */
#include <linux/wait.h>         /* wake_up_interruptible                     */
#include <linux/atomic.h>       /* atomic_inc, atomic_add                    */

/* Forward declaration */
struct hf_dev;
extern struct hf_dev *g_hf_dev;

/*
 * Batch drain size for workqueue handler.
 * Larger than tasklet batch: workqueue runs in process context,
 * can hold CPU longer without starving other work (scheduler handles it).
 */
#define WQ_DRAIN_BATCH  512u

/* Optional mutex protecting the output data structure */
static DEFINE_MUTEX(output_mutex);

/* ---- Sample processing in process context -------------------------------- */

/*
 * process_samples_block() — runs in kworker thread, full kernel capabilities
 *
 * Can use:
 *   - mutex_lock() for complex shared data
 *   - kmalloc(GFP_KERNEL) for dynamic output buffers
 *   - wait_event_interruptible() for upstream flow control
 *   - filp_write() to write output to a file
 *   - Netlink socket to send data to userspace
 *   - Completion variables for synchronization
 *   - Floating point: kernel_fpu_begin() / kernel_fpu_end() on x86
 */
static void process_samples_block(const u32 *buf, unsigned int count)
{
    unsigned int i;

    mutex_lock(&output_mutex);

    for (i = 0u; i < count; i++) {
        /*
         * Example operations in process context:
         *
         * 1. Protocol decode (can use complex state machine, alloc intermediate buf)
         * 2. Write to circular log in procfs/sysfs output buffer
         * 3. Accumulate statistics with mutex-protected shared counter
         * 4. Signal userspace reader via poll/select:
         *    wake_up_interruptible(&dev->read_wq);
         */
        (void)buf[i];
    }

    mutex_unlock(&output_mutex);
}

/* ---- Workqueue work handler ---------------------------------------------- */

/*
 * hf_work_handler() — workqueue bottom half
 *
 * container_of(work, struct hf_dev, work):
 *   Recovers hf_dev from the embedded work_struct.
 *   This is the standard Linux pattern for embedding work_structs in
 *   driver private data — no global state, no void* casts.
 *
 * Drain strategy: loop until kfifo is empty.
 *   Unlike tasklet (bounded by budget), workqueue can run as long as needed.
 *   The scheduler will preempt it if higher-priority tasks become runnable.
 *
 * Re-queue pattern:
 *   If new data arrives while we're processing (ISR pushes to kfifo),
 *   queue_work() at the end is idempotent and safe — ensures no data is
 *   left unprocessed if ISR doesn't re-schedule us.
 */
void hf_work_handler(struct work_struct *work)
{
    /*
     * Recover hf_dev from embedded work_struct.
     * This is why we use INIT_WORK(&dev->work, handler) not a global work_struct.
     */
    struct hf_dev *dev = container_of(work, struct hf_dev, work);

    u32         *batch;
    unsigned int count;
    unsigned int total = 0u;

    /*
     * Allocate batch buffer from kernel heap (GFP_KERNEL: can sleep = OK here).
     * This would be ILLEGAL in tasklet (softirq) context.
     */
    batch = kmalloc(WQ_DRAIN_BATCH * sizeof(u32), GFP_KERNEL);
    if (!batch) {
        pr_err("%s: failed to allocate batch buffer\n", __func__);
        return;
    }

    /*
     * Drain the entire kfifo.
     * Loop: pop WQ_DRAIN_BATCH entries, process, repeat until empty.
     *
     * kfifo_out() is lock-free for SPSC: ISR = producer (writes head),
     * this handler = consumer (writes tail). Safe without spinlock.
     */
    do {
        count = kfifo_out(&dev->data_fifo, batch, WQ_DRAIN_BATCH);
        if (count > 0u) {
            process_samples_block(batch, count);
            atomic_add((int)count, &dev->processed_count);
            total += count;
        }
    } while (count == WQ_DRAIN_BATCH);  /* If we got a full batch, there may be more */

    kfree(batch);

    /*
     * Re-queue if fifo still has data.
     * This handles the race: ISR pushed data between our last kfifo_out
     * and here. Without this, that data would sit in the fifo until the
     * next ISR fires and calls queue_work() again.
     *
     * queue_work() is idempotent: if work is already queued, it's a no-op.
     * Safe to call from process context.
     */
    if (!kfifo_is_empty(&dev->data_fifo)) {
        queue_work(dev->wq, &dev->work);
    }

    /*
     * Wake up any userspace reader waiting in poll()/select()/read().
     * The driver's read() implementation would call wait_event_interruptible()
     * on this wait queue.
     */
    wake_up_interruptible(&dev->read_wq);
}

/*
 * ============================================================================
 * THREADED IRQ — The Modern Alternative to Tasklet + Workqueue
 * ============================================================================
 *
 * request_threaded_irq(irq, top_half, thread_fn, flags, name, dev_id):
 *
 *   top_half:  runs in hardirq context (fast, like hf_isr above)
 *              Returns: IRQ_HANDLED (done) or IRQ_WAKE_THREAD (wake thread_fn)
 *
 *   thread_fn: runs in a dedicated kernel thread (irq/17-hf_isr_dev)
 *              Has its own task_struct, can sleep, full process context
 *              Returns: IRQ_HANDLED
 *
 * Example:
 *
 *   static irqreturn_t hf_top_half(int irq, void *dev_id) {
 *       struct hf_dev *dev = dev_id;
 *       u32 status = readl(dev->base + REG_STATUS_OFFSET);
 *       if (!(status & REG_STATUS_IRQ_MASK))
 *           return IRQ_NONE;
 *       // Minimal: just clear hardware IRQ and wake thread
 *       writel(REG_STATUS_IRQ_MASK, dev->base + REG_STATUS_OFFSET);
 *       return IRQ_WAKE_THREAD;
 *   }
 *
 *   static irqreturn_t hf_thread_fn(int irq, void *dev_id) {
 *       struct hf_dev *dev = dev_id;
 *       // Full process context: can sleep, allocate, I/O
 *       u32 data = readl(dev->base + REG_DATA_OFFSET);
 *       process_complex(dev, data);
 *       return IRQ_HANDLED;
 *   }
 *
 *   // Registration:
 *   request_threaded_irq(irq, hf_top_half, hf_thread_fn,
 *                        IRQF_SHARED | IRQF_ONESHOT,
 *                        "hf_isr_dev", dev);
 *
 * IRQF_ONESHOT: keeps IRQ disabled until thread_fn returns.
 *   Required when top_half doesn't re-enable the IRQ (no ACK in top half).
 *   Prevents re-entry of top_half while thread_fn runs.
 *
 * Advantages over tasklet/workqueue:
 *   1. Simpler code: no explicit scheduling, one function does everything
 *   2. PREEMPT_RT compatible: thread can be given SCHED_FIFO/RR priority
 *   3. Per-IRQ thread: isolation, no shared pool (vs system_wq)
 *   4. Debuggable: appears in ps/top as "irq/17-hf_isr_dev"
 *
 * Disadvantages:
 *   1. One kernel thread per threaded IRQ (memory overhead)
 *   2. Higher latency than tasklet (thread scheduling overhead)
 *   3. Cannot batch multiple IRQs as efficiently as workqueue drain loop
 */

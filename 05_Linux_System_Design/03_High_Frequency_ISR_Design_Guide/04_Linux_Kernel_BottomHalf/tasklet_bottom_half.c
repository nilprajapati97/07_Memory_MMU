// SPDX-License-Identifier: GPL-2.0
/*
 * tasklet_bottom_half.c — Linux Kernel Tasklet Implementation
 *
 * TASKLET: a deferred interrupt handler running in softirq context.
 *
 * Softirq context properties:
 *   - NOT a process: no mm, no files, no signal handling
 *   - Runs after hardware IRQ handler returns
 *   - Local IRQs are ENABLED (unlike top half, which ran with IRQs disabled)
 *   - Preemption is DISABLED (cannot be scheduled out by another task)
 *   - Cannot sleep: no mutex_lock, kmalloc(GFP_KERNEL), msleep
 *   - Cannot access user-space memory
 *   - Runs on the SAME CPU that scheduled it (cache-warm data)
 *   - Two tasklets of the same instance CANNOT run concurrently (serialized)
 *   - Different tasklet instances CAN run on different CPUs concurrently
 *
 * Softirq budget:
 *   Linux processes up to 10 softirqs per loop, or up to 2 ms.
 *   After that, remaining softirqs are handed to ksoftirqd kernel thread.
 *   At 100 kHz, 100,000 tasklet schedules/sec → ~100 µs/cycle avg processing.
 *
 * Deprecation note (kernel 5.10+):
 *   DECLARE_TASKLET() and DECLARE_TASKLET_OLD() are deprecated.
 *   Use tasklet_setup() with a function taking struct tasklet_struct *.
 *   Old signature: void func(unsigned long data)
 *   New signature: void func(struct tasklet_struct *t)
 *   Use from_tasklet(var, t, member) to get container struct.
 *
 * When to use tasklet vs workqueue:
 *   Tasklet: < 100 µs work, no sleeping, latency-sensitive (network RX, timer)
 *   Workqueue: > 100 µs, needs sleep, allocations, or I/O
 */

#include <linux/interrupt.h>    /* tasklet_struct, tasklet_schedule         */
#include <linux/kfifo.h>        /* kfifo_out                                */
#include <linux/spinlock.h>     /* spin_lock, spin_unlock                   */

/* Forward declaration: hf_dev defined in top_half_isr.c */
struct hf_dev;
extern struct hf_dev *g_hf_dev;

/*
 * Batch size: drain up to this many samples per tasklet invocation.
 *
 * Trade-off:
 *   Too small: tasklet re-schedules many times → more softirq overhead
 *   Too large: tasklet holds CPU too long → starves other softirqs
 *              (network RX, timer callbacks, etc.)
 *
 * Rule of thumb: keep each tasklet invocation < 100 µs of CPU time.
 * At 100 kHz: 100 samples × 1 µs/sample = 100 µs → reasonable batch size.
 */
#define TASKLET_BATCH_SIZE   128u

/* ---- Lightweight sample processing --------------------------------------- */

/*
 * process_sample_atomic() — must complete in softirq context
 *
 * Allowed:
 *   - Arithmetic, bitwise ops, array indexing
 *   - spin_lock() (NOT mutex_lock)
 *   - Accessing per-CPU variables (__this_cpu_add, etc.)
 *   - Incrementing atomic_t counters
 *   - Writing to a pre-allocated output buffer
 *   - Signaling via wake_up() (non-blocking form)
 *
 * Forbidden:
 *   - mutex_lock() / down() — may sleep
 *   - kmalloc(GFP_KERNEL) — may sleep (use GFP_ATOMIC if allocation needed)
 *   - copy_to_user() — may page-fault
 *   - msleep(), schedule(), wait_event() — sleep
 *   - Any function that calls schedule() internally
 */
static void process_sample_atomic(u32 data)
{
    /*
     * Example: running sum for diagnostics (per-CPU to avoid contention)
     * Real use: lightweight decode, threshold check, staging to output ring
     */
    (void)data;
}

/* ---- Tasklet handler ------------------------------------------------------ */

/*
 * hf_tasklet_handler() — softirq bottom half
 *
 * Called after hf_isr() (top half) returns IRQ_HANDLED.
 * Runs on same CPU as the ISR (CPU affinity → cache warm kfifo data).
 *
 * Design: drain up to TASKLET_BATCH_SIZE samples, then:
 *   - If more remain AND we haven't starved other softirqs: continue
 *   - If softirq budget exhausted OR we hit batch limit: re-schedule tasklet
 *     (tasklet_schedule is idempotent: safe to call if already pending)
 *
 * This is the "NAPI-style" approach for softirq drivers: process in bounded
 * batches, yield to other softirqs, reschedule if more work remains.
 */
void hf_tasklet_handler(struct tasklet_struct *t)
{
    /*
     * from_tasklet(var, t, member):
     *   Recovers the containing hf_dev struct from the embedded tasklet field.
     *   Equivalent to container_of(t, struct hf_dev, tasklet).
     *   Kernel 5.10+ API.
     */
    struct hf_dev *dev = from_tasklet(dev, t, tasklet);

    u32      batch[TASKLET_BATCH_SIZE];
    unsigned int count;
    unsigned int i;

    /*
     * kfifo_out(): pop up to TASKLET_BATCH_SIZE elements.
     * Thread-safe for single consumer (SPSC): no locking needed.
     * Returns actual count popped (may be less than requested if fifo near-empty).
     */
    count = kfifo_out(&dev->data_fifo, batch, TASKLET_BATCH_SIZE);

    for (i = 0u; i < count; i++) {
        process_sample_atomic(batch[i]);
    }

    atomic_add((int)count, &dev->processed_count);

    /*
     * If fifo still has data, reschedule this tasklet.
     *
     * kfifo_is_empty() is safe here (single consumer reads tail).
     * tasklet_schedule() is idempotent: if already scheduled, does nothing.
     *
     * By rescheduling instead of looping, we:
     * 1. Yield the CPU to other softirqs (network, timers) between batches
     * 2. Allow other CPUs' softirqs to run
     * 3. Prevent one high-rate IRQ from starving the entire softirq subsystem
     */
    if (!kfifo_is_empty(&dev->data_fifo)) {
        tasklet_schedule(&dev->tasklet);
    }
}

/*
 * ============================================================================
 * NOTES ON TASKLET DEPRECATION (kernel 5.10 → 6.x migration path)
 * ============================================================================
 *
 * The tasklet mechanism has been deprecated since kernel 5.10. Reasons:
 *
 * 1. Runs in atomic context (softirq) — severely limits what code can do
 * 2. Serialized per-instance but not globally — complex locking analysis
 * 3. Poor PREEMPT_RT compatibility (softirqs run in interrupt context on RT)
 * 4. Equivalent functionality achievable with threaded IRQs (cleaner model)
 *
 * Migration path:
 *   Old (tasklet): tasklet_schedule() → void handler(struct tasklet_struct *)
 *   New (workqueue): queue_work() → void handler(struct work_struct *)
 *   New (threaded IRQ): request_threaded_irq() → irqreturn_t thread_fn()
 *
 * Tasklets are still widely present in production kernels (drivers/net, etc.)
 * and still asked about in interviews because:
 * - Millions of lines of production code use them
 * - They appear in old codebases candidates maintain
 * - They demonstrate understanding of softirq context constraints
 *
 * Know both: old DECLARE_TASKLET pattern + new tasklet_setup() pattern.
 *
 * DECLARE_TASKLET (old, < kernel 5.10):
 *   DECLARE_TASKLET(my_tasklet, my_handler_fn);
 *   // handler: void my_handler_fn(unsigned long data)
 *   // data: unsigned long passed to handler (often a pointer cast)
 *
 * tasklet_setup (new, >= kernel 5.10):
 *   tasklet_setup(&dev->tasklet, hf_tasklet_handler);
 *   // handler: void hf_tasklet_handler(struct tasklet_struct *t)
 *   // use from_tasklet() to get container struct
 */

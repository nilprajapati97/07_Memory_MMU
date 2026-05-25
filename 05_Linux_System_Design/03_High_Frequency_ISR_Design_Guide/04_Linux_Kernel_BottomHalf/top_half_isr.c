// SPDX-License-Identifier: GPL-2.0
/*
 * top_half_isr.c — Linux Kernel Interrupt Top Half
 *
 * The Linux interrupt model splits interrupt handling into two halves:
 *
 *   TOP HALF (this file):
 *     - Hardware interrupt handler registered via request_irq()
 *     - Runs with CPU local IRQs disabled (non-threaded) or enabled (threaded)
 *     - MUST be fast: ACK hardware, read minimal data, schedule bottom half
 *     - Returns IRQ_HANDLED or IRQ_NONE (for shared IRQs)
 *
 *   BOTTOM HALF (tasklet_bottom_half.c / workqueue_bottom_half.c):
 *     - Deferred processing in softer context
 *     - Tasklet: softirq context (no sleep allowed)
 *     - Workqueue: process context (can sleep, allocate, I/O)
 *     - Threaded IRQ: kernel thread per IRQ (can sleep, simplified model)
 *
 * Linux interrupt context rules (HARD CONSTRAINTS):
 *   ✅ can_spin_lock_irqsave()  — spinlocks OK (non-sleeping)
 *   ✅ kfifo_in()               — lock-free SPSC FIFO, ISR-safe
 *   ✅ tasklet_schedule()        — non-blocking, schedules softirq
 *   ✅ queue_work()             — non-blocking, schedules workqueue item
 *   ✅ wake_up_interruptible()   — wake sleeping waiter (non-blocking)
 *   ❌ mutex_lock()             — may sleep → kernel BUG() in atomic context
 *   ❌ kmalloc(GFP_KERNEL)      — may sleep → BUG()
 *   ❌ msleep() / schedule()    — explicit sleep → BUG()
 *   ❌ copy_to_user()           — page fault → BUG()
 *   ❌ printk() with KERN_DEBUG — too slow at high rate; use trace_printk()
 *
 * Kernel version: 5.10+ (uses tasklet_setup instead of deprecated DECLARE_TASKLET)
 */

#include <linux/interrupt.h>   /* request_irq, IRQ_HANDLED, tasklet_struct */
#include <linux/workqueue.h>   /* alloc_workqueue, INIT_WORK, queue_work   */
#include <linux/kfifo.h>       /* DECLARE_KFIFO, kfifo_in, kfifo_out       */
#include <linux/spinlock.h>    /* spinlock_t, spin_lock_irqsave             */
#include <linux/slab.h>        /* kzalloc, kfree                            */
#include <linux/io.h>          /* readl, writel (MMIO access)               */
#include <linux/module.h>      /* MODULE_LICENSE, module_init/exit          */
#include <linux/platform_device.h>

/* ---- Device constants ----------------------------------------------------- */

#define HF_IRQ_NUM          17          /* Platform IRQ number              */
#define HF_KFIFO_SIZE       4096u       /* Entries — must be power of 2     */
#define HF_DEVICE_NAME      "hf_isr_dev"

/* MMIO register offsets (platform-specific) */
#define REG_DATA_OFFSET     0x04u       /* Data register offset             */
#define REG_STATUS_OFFSET   0x08u       /* Status / IRQ clear register      */
#define REG_STATUS_IRQ_MASK 0x00000001u /* Bit 0: interrupt pending         */

/* ---- Driver private data -------------------------------------------------- */

struct hf_dev {
    int                      irq;
    void __iomem            *base;          /* MMIO base (ioremap'd)         */
    spinlock_t               lock;          /* Protects shared state         */

    /*
     * kfifo: Linux's built-in lock-free SPSC circular buffer.
     * kfifo_in() is safe from IRQ context (no locking internally for SPSC).
     * kfifo_out() is safe from any single-consumer context.
     *
     * For multi-producer/consumer: use kfifo_in_spinlocked() / kfifo_out_spinlocked()
     * with an external spinlock.
     *
     * Size: HF_KFIFO_SIZE entries of type u32.
     * Memory: 4096 × 4 = 16 KB (kernel heap, not stack).
     */
    DECLARE_KFIFO(data_fifo, u32, HF_KFIFO_SIZE);

    /* Bottom half: choose ONE of tasklet or workqueue */
    struct tasklet_struct    tasklet;
    struct work_struct       work;
    struct workqueue_struct *wq;

    /* Stats (use atomic_t for lock-free increment from IRQ and read from userspace) */
    atomic_t                 total_irqs;
    atomic_t                 overflow_count;
    atomic_t                 processed_count;

    /* Wait queue: block userspace read() until data available */
    wait_queue_head_t        read_wq;
};

/* Module-level device pointer (single device for simplicity) */
static struct hf_dev *g_hf_dev;

/* Forward declarations for bottom halves */
static void hf_tasklet_handler(struct tasklet_struct *t);
static void hf_work_handler(struct work_struct *work);

/* ---- ISR: Top Half --------------------------------------------------------
 *
 * hf_isr() — registered with request_irq()
 *
 * Called with local IRQs disabled on this CPU (for non-IRQF_SHARED non-threaded).
 * Execution path: hardware asserts IRQ line → GIC/NVIC → CPU exception →
 *                 Linux IRQ subsystem → do_IRQ() → hf_isr()
 *
 * Time budget: as low as possible. 100 kHz = 10 µs period.
 * Target: < 2 µs (readl + kfifo_in + tasklet_schedule = ~500-1000 ns on Cortex-A)
 *
 * IRQ type: IRQF_SHARED allows sharing the IRQ line with other devices.
 *           Each handler checks if its device triggered the IRQ.
 */
static irqreturn_t hf_isr(int irq, void *dev_id)
{
    struct hf_dev *dev = dev_id;
    u32 status;
    u32 data;
    int pushed;

    /*
     * Step 1: Check if OUR device triggered this IRQ.
     * Critical for IRQF_SHARED: if we return IRQ_NONE for non-our IRQ,
     * Linux will route to the next handler on this shared line.
     *
     * readl(): memory-mapped I/O read with implicit full memory barrier.
     * Equivalent to: volatile read + DSB + ISB (on ARM).
     */
    status = readl(dev->base + REG_STATUS_OFFSET);
    if (!(status & REG_STATUS_IRQ_MASK)) {
        return IRQ_NONE;    /* Not our device — return so next handler runs */
    }

    /*
     * Step 2: Read data register FIRST (before clearing IRQ).
     * On some peripherals: reading data register automatically clears the IRQ.
     * On others: must clear separately (step 3).
     * Platform-specific — check datasheet.
     */
    data = readl(dev->base + REG_DATA_OFFSET);

    /*
     * Step 3: Clear interrupt pending bit in hardware.
     * MUST be done before returning IRQ_HANDLED.
     * If not cleared: IRQ fires again immediately → interrupt storm → system hang.
     */
    writel(REG_STATUS_IRQ_MASK, dev->base + REG_STATUS_OFFSET);

    atomic_inc(&dev->total_irqs);

    /*
     * Step 4: Push data to kfifo.
     *
     * kfifo_in() is lock-free for SPSC (single producer = this ISR,
     * single consumer = bottom half). Returns number of elements pushed
     * (0 if fifo full).
     *
     * SPSC guarantee: head written only by ISR, tail written only by consumer.
     * Same invariant as approach 01's ring_buffer.
     */
    pushed = kfifo_in(&dev->data_fifo, &data, 1);
    if (pushed == 0) {
        /* kfifo full: bottom half not draining fast enough */
        atomic_inc(&dev->overflow_count);
        return IRQ_HANDLED;
    }

    /*
     * Step 5: Schedule bottom half.
     *
     * OPTION A: Tasklet (softirq context, cannot sleep)
     *   tasklet_schedule() is safe from IRQ context.
     *   If tasklet is already pending: no-op (idempotent, no double-scheduling).
     *   Tasklet runs on same CPU as this IRQ → cache warm data.
     */
    tasklet_schedule(&dev->tasklet);

    /*
     * OPTION B: Workqueue (process context, can sleep)
     *   queue_work() is safe from IRQ context.
     *   If work is already queued: no-op.
     *   Runs in kworker/N:M kernel thread → different CPU possible.
     */
    /* queue_work(dev->wq, &dev->work); */

    /*
     * OPTION C: Wake up read() waiter directly (for character device driver)
     *   wake_up_interruptible(&dev->read_wq);
     */

    return IRQ_HANDLED;
}

/* ---- Device probe (called by platform driver framework) ------------------ */

int hf_dev_probe(struct platform_device *pdev)
{
    struct hf_dev *dev;
    struct resource *res;
    int ret;

    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    /* Get MMIO resource from device tree / ACPI */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    dev->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(dev->base))
        return PTR_ERR(dev->base);

    spin_lock_init(&dev->lock);
    INIT_KFIFO(dev->data_fifo);
    atomic_set(&dev->total_irqs, 0);
    atomic_set(&dev->overflow_count, 0);
    atomic_set(&dev->processed_count, 0);
    init_waitqueue_head(&dev->read_wq);

    /* Initialize tasklet (kernel 5.10+: tasklet_setup replaces DECLARE_TASKLET) */
    tasklet_setup(&dev->tasklet, hf_tasklet_handler);

    /* Initialize workqueue (high-priority, unbounded, max 1 thread) */
    dev->wq = alloc_workqueue("hf_wq",
                              WQ_HIGHPRI | WQ_UNBOUND,
                              1 /* max_active */);
    if (!dev->wq)
        return -ENOMEM;

    INIT_WORK(&dev->work, hf_work_handler);

    /* Get IRQ number from platform resource */
    dev->irq = platform_get_irq(pdev, 0);
    if (dev->irq < 0) {
        destroy_workqueue(dev->wq);
        return dev->irq;
    }

    /*
     * register_irq():
     *   IRQF_SHARED: line may be shared (each handler must check own device)
     *   IRQF_TRIGGER_RISING: edge-triggered (level vs edge: platform-specific)
     *
     * devm_request_irq(): auto-freed on device remove (managed resource).
     */
    ret = devm_request_irq(&pdev->dev,
                           dev->irq,
                           hf_isr,
                           IRQF_SHARED | IRQF_TRIGGER_RISING,
                           HF_DEVICE_NAME,
                           dev);
    if (ret) {
        destroy_workqueue(dev->wq);
        return ret;
    }

    platform_set_drvdata(pdev, dev);
    g_hf_dev = dev;

    dev_info(&pdev->dev, "%s: IRQ %d registered, kfifo %u entries\n",
             HF_DEVICE_NAME, dev->irq, HF_KFIFO_SIZE);
    return 0;
}

int hf_dev_remove(struct platform_device *pdev)
{
    struct hf_dev *dev = platform_get_drvdata(pdev);

    /*
     * Order matters for cleanup:
     * 1. free_irq: stop ISR from firing (devm handles this automatically)
     * 2. tasklet_kill: wait for in-progress tasklet to complete
     * 3. flush + destroy workqueue: drain pending work items
     */
    tasklet_kill(&dev->tasklet);
    flush_workqueue(dev->wq);
    destroy_workqueue(dev->wq);
    g_hf_dev = NULL;

    return 0;
}

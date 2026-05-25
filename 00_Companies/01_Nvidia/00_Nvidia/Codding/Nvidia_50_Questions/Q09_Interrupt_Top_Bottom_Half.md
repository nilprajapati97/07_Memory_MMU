# Q09: Interrupt Handling — Top Half vs Bottom Half / Tasklet

**Section:** Linux Kernel Internals | **Difficulty:** Medium | **Topics:** IRQ, `request_irq`, `tasklet`, softirq, top half, bottom half, `irqreturn_t`

---

## Question

Explain interrupt handling — top half vs bottom half. Implement a tasklet.

---

## Answer

```c
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/io.h>

#define GPU_IRQ          45
#define IRQ_STATUS_REG   0x00
#define IRQ_CLEAR_REG    0x04
#define GPU_FRAME_DONE   BIT(0)
#define GPU_DMA_DONE     BIT(1)
#define GPU_ERROR        BIT(2)

static void __iomem *gpu_regs;
static struct tasklet_struct gpu_tasklet;

/* ─── Bottom Half (Tasklet) ───────────────────────────────────────────────
 * Runs in softirq context — between HW IRQs, after the top half completes.
 * CAN: disable/enable IRQs, use spinlocks, access HW.
 * CANNOT: sleep, allocate with GFP_KERNEL, call blocking APIs.
 *
 * 'data' carries the saved GPU status register value.
 */
static void gpu_tasklet_fn(unsigned long data)
{
    u32 status = (u32)data;

    if (status & GPU_FRAME_DONE) {
        /* Advance frame timeline, wake up waiter tasks */
        pr_debug("GPU: frame render complete\n");
        signal_frame_complete();
    }

    if (status & GPU_DMA_DONE) {
        /* Safe to unmap DMA buffers (no sleep needed for dma_unmap_single) */
        pr_debug("GPU: DMA transfer complete\n");
        unmap_dma_buffer();
    }

    if (status & GPU_ERROR) {
        /* Schedule a workqueue for error recovery (needs sleeping) */
        pr_warn("GPU: hardware error detected, scheduling recovery\n");
        schedule_work(&gpu_recovery_work);
    }
}

/* ─── Top Half (Hard IRQ Handler) ────────────────────────────────────────
 * MUST complete in microseconds.
 * Rules: no sleeping, no blocking, no heavy computation.
 * Goal: ACK the interrupt, read status, defer work.
 */
static irqreturn_t gpu_irq_top(int irq, void *dev_id)
{
    u32 status;

    /* 1. Read the hardware status register */
    status = readl(gpu_regs + IRQ_STATUS_REG);

    /* 2. ACK the interrupt immediately (clear in hardware) */
    writel(status, gpu_regs + IRQ_CLEAR_REG);

    /* 3. Is this our interrupt? Return IRQ_NONE if not (shared IRQ line) */
    if (!status)
        return IRQ_NONE;

    /* 4. Pass status to bottom half and schedule it */
    tasklet_hi_schedule_first(&gpu_tasklet); /* high-priority tasklet */
    /* Alternative: tasklet_schedule(&gpu_tasklet) — normal priority */

    /* Alternatively pass data via tasklet data field: */
    gpu_tasklet.data = (unsigned long)status;
    tasklet_schedule(&gpu_tasklet);

    return IRQ_HANDLED;
}

/* ─── Init / Exit ─────────────────────────────────────────────────────────*/
static int __init gpu_irq_init(void)
{
    int ret;

    gpu_regs = ioremap(GPU_REGS_BASE, GPU_REGS_SIZE);
    if (!gpu_regs)
        return -ENOMEM;

    /* Initialize tasklet with handler function and initial data */
    tasklet_init(&gpu_tasklet, gpu_tasklet_fn, 0);

    /*
     * IRQF_SHARED: IRQ line is shared with other devices.
     * dev_id (gpu_regs) must be unique and passed to free_irq().
     */
    ret = request_irq(GPU_IRQ, gpu_irq_top,
                       IRQF_SHARED, "gpu_irq", gpu_regs);
    if (ret) {
        iounmap(gpu_regs);
        return ret;
    }

    pr_info("GPU IRQ %d registered\n", GPU_IRQ);
    return 0;
}

static void __exit gpu_irq_exit(void)
{
    free_irq(GPU_IRQ, gpu_regs);
    tasklet_kill(&gpu_tasklet);  /* wait for any running tasklet to finish */
    iounmap(gpu_regs);
}

module_init(gpu_irq_init);
module_exit(gpu_irq_exit);
MODULE_LICENSE("GPL");
```

---

## Explanation

### Core Concept

Linux interrupt handling is split into two halves to keep hardware IRQ latency minimal:

```
Hardware IRQ fires
       │
       ▼
┌──────────────────────────────────────────────────────────┐
│  TOP HALF (Hard IRQ Context)           Time: < 10μs      │
│  - Disable/acknowledge hardware interrupt                 │
│  - Read status registers                                  │
│  - Schedule bottom half (tasklet or workqueue)            │
│  - Return IRQ_HANDLED                                     │
└──────────────────────────────────────────────────────────┘
       │  (hardware IRQs re-enabled)
       ▼
┌──────────────────────────────────────────────────────────┐
│  BOTTOM HALF (Softirq / Tasklet Context) Time: < 1ms     │
│  - Process completion data                                │
│  - Signal waiters (wake_up)                               │
│  - Unmap DMA buffers                                      │
│  - Schedule recovery work if needed                       │
└──────────────────────────────────────────────────────────┘
```

**Three bottom-half mechanisms** (in order of decreasing priority):

| Mechanism | Context | Can Sleep | Priority |
|-----------|---------|-----------|----------|
| Softirq | Softirq | No | Highest |
| Tasklet | Softirq | No | High |
| Workqueue | Process | Yes | Normal |

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `request_irq(irq, handler, flags, name, dev)` | Register IRQ handler |
| `free_irq(irq, dev)` | Unregister and wait for handler completion |
| `IRQF_SHARED` | Allow sharing IRQ line with other devices |
| `IRQF_TRIGGER_RISING` | Set trigger type |
| `tasklet_init(&t, fn, data)` | Initialize tasklet |
| `tasklet_schedule(&t)` | Schedule tasklet (runs in softirq after top half) |
| `tasklet_hi_schedule(&t)` | Schedule at high-priority softirq level |
| `tasklet_kill(&t)` | Wait for tasklet to finish and disable it |
| `IRQ_HANDLED` | Return value: this driver handled the interrupt |
| `IRQ_NONE` | Return value: not our interrupt (for shared IRQs) |
| `disable_irq(irq)` | Disable IRQ line and wait for running handler to finish |
| `enable_irq(irq)` | Re-enable IRQ line |

### Trade-offs & Pitfalls

- **Sleeping in a tasklet = BUG.** Tasklets run in softirq context where sleeping is forbidden. For work that needs to allocate memory with `GFP_KERNEL` or acquire a mutex, use a workqueue.
- **`tasklet_kill` in cleanup.** Always call `tasklet_kill` before freeing resources the tasklet uses. It waits for any currently-running instance to complete.
- **Shared IRQ lines (IRQF_SHARED).** The handler is called for all devices sharing the line. Always check if the interrupt belongs to your device (read your status register) and return `IRQ_NONE` if not — otherwise you corrupt another driver's interrupt handling.
- **MSI/MSI-X.** Modern PCIe devices use MSI (Message Signaled Interrupts) which are not shared and support multiple vectors (one per GPU engine). Use `pci_alloc_irq_vectors(pdev, min, max, PCI_IRQ_MSI)`.

### NVIDIA / GPU Context

NVIDIA GPU drivers use a multi-level interrupt architecture:
- **Top half:** Reads GPU interrupt status register (INTR_TOP), ACKs the interrupt, reads seqno from completion registers
- **Tasklet:** Signals GPU fence completions, wakes tasks waiting on `wait_event_timeout`
- **Workqueue:** Handles GPU errors (TDR — Timeout Detection and Recovery), channel teardown, memory retirement
- **MSI-X:** NVIDIA A100/H100 uses multiple MSI-X vectors — separate vectors for different GPU engines (Copy Engine, Compute, Video Decode), avoiding serialization

---

## Cross Questions & Answers

**CQ1: What is the difference between a tasklet and a softirq? When would you implement a new softirq?**
> A **softirq** is a statically-defined, fixed set of software interrupt types (like `NET_RX_SOFTIRQ`, `TASKLET_SOFTIRQ`). Softirqs can run concurrently on multiple CPUs simultaneously. A **tasklet** is built on top of `TASKLET_SOFTIRQ` and guarantees that the same tasklet does NOT run concurrently on two CPUs at once (serialized). You should almost never add a new softirq — it requires modifying core kernel code. Tasklets and workqueues are the correct abstraction for driver-level deferred work.

**CQ2: Why is it important to ACK the interrupt in the top half before deferring work to a tasklet?**
> If you don't ACK (clear) the interrupt in hardware before returning from the top half, the hardware will re-assert the interrupt line immediately. On level-triggered interrupts, this causes an interrupt storm — the CPU is continuously interrupted, starving all other tasks. Always ACK the hardware interrupt register in the top half, then do all processing in the bottom half.

**CQ3: What is `threaded IRQ` and when is it better than tasklets?**
> `request_threaded_irq(irq, hard_handler, thread_fn, flags, name, dev)` creates a dedicated kernel thread for the interrupt handler. `hard_handler` runs in hard IRQ context and wakes the thread; `thread_fn` runs in the kernel thread context and can sleep. Threaded IRQs are better when the interrupt processing needs to sleep (e.g., acquire a mutex, do I2C register reads). They also improve real-time latency because PREEMPT_RT kernels can preempt threaded IRQ handlers.

**CQ4: How does NVIDIA handle the case where a GPU raises an interrupt but the driver's handler takes too long?**
> NVIDIA uses a watchdog timer that detects if a GPU channel hasn't made progress within a threshold (e.g., 10 seconds — "TDR"). If the watchdog fires, it schedules a recovery workqueue that: (1) saves GPU state, (2) sends a channel reset command, (3) signals all waiting fences with an error status, (4) notifies userspace via the event mechanism. The GPU is recovered without requiring a system reboot.

**CQ5: What happens if `free_irq` is called while the interrupt handler is running on another CPU?**
> `free_irq` internally calls `synchronize_irq(irq)` which blocks until all currently-executing instances of the handler on all CPUs have returned. Only after synchronization does `free_irq` remove the handler registration. This guarantees that after `free_irq` returns, the handler will never be called again — safe to free the resources it uses. This is why proper cleanup order matters: `free_irq` must come before `iounmap` or `kfree` of the handler's data.

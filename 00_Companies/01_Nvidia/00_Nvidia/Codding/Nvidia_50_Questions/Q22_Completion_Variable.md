# Q22: Completion Variable for GPU Hardware Initialization

**Section:** Concurrency & Synchronization | **Difficulty:** Medium | **Topics:** `completion`, `DECLARE_COMPLETION`, `wait_for_completion_timeout`, `complete`, `kthread_run`, one-time synchronization

---

## Question

Implement a completion variable to synchronize GPU hardware initialization from a kernel thread.

---

## Answer

```c
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>

/* ─── GPU device structure with completion ────────────────────────────────*/
struct gpu_device {
    /* Static (stack/global) completion */
    struct completion hw_init_done;

    /* Dynamic (heap-allocated) completion for per-context sync */
    struct completion *ctx_ready;

    void __iomem *regs;
    bool          hw_ok;
};

/* ─── GPU hardware init thread ────────────────────────────────────────────
 * Runs in a kthread. Performs slow hardware initialization,
 * then signals the main thread that it's safe to proceed.
 */
static int gpu_hw_init_thread(void *data)
{
    struct gpu_device *gpu = data;

    pr_info("GPU: starting hardware initialization...\n");

    /* Simulate slow hardware init: bring up power domains,
     * load firmware, run POST, calibrate clocks */
    msleep(500);

    /* Program GPU registers */
    writel(GPU_INIT_CMD, gpu->regs + GPU_CTRL_REG);

    /* Wait for GPU ready bit (hardware polling, max 1s) */
    if (readl_poll_timeout(gpu->regs + GPU_STATUS_REG,
                            val, val & GPU_READY_BIT,
                            100, 1000000)) {
        pr_err("GPU: hardware init timeout!\n");
        gpu->hw_ok = false;
    } else {
        pr_info("GPU: hardware ready.\n");
        gpu->hw_ok = true;
    }

    /*
     * complete(&hw_init_done): wake the main thread.
     * If multiple waiters exist, complete_all() wakes all of them.
     * complete() wakes only one.
     */
    complete(&gpu->hw_init_done);

    return 0;
}

/* ─── GPU driver probe: start init thread, wait for it ───────────────────*/
int gpu_driver_probe(struct pci_dev *pdev)
{
    struct gpu_device *gpu;
    struct task_struct *init_task;
    int ret;

    gpu = devm_kzalloc(&pdev->dev, sizeof(*gpu), GFP_KERNEL);
    if (!gpu)
        return -ENOMEM;

    gpu->regs = pcim_iomap(pdev, 0, 0);
    if (!gpu->regs)
        return -ENOMEM;

    /*
     * init_completion: initialize to "not completed" state (count=0).
     * DECLARE_COMPLETION(name) can be used for static/global completions.
     */
    init_completion(&gpu->hw_init_done);
    gpu->hw_ok = false;

    /* Spawn hardware init thread */
    init_task = kthread_run(gpu_hw_init_thread, gpu, "gpu_hw_init/%s",
                             pci_name(pdev));
    if (IS_ERR(init_task)) {
        pr_err("GPU: failed to create init thread\n");
        return PTR_ERR(init_task);
    }

    /*
     * wait_for_completion_timeout: sleep until:
     *   - complete() is called (returns remaining jiffies > 0)
     *   - timeout expires (returns 0)
     * Does NOT return early for signals (use _interruptible variant if needed).
     */
    ret = wait_for_completion_timeout(&gpu->hw_init_done,
                                       msecs_to_jiffies(2000));
    if (!ret) {
        pr_err("GPU: hw init timed out after 2 seconds!\n");
        /* kthread_stop would be needed to kill the thread */
        return -ETIMEDOUT;
    }

    if (!gpu->hw_ok)
        return -EIO;

    pr_info("GPU: probe complete, hw_init took %lu ms\n",
            2000 - jiffies_to_msecs(ret));
    return 0;
}

/* ─── Dynamic completion for per-CUDA-context ready signaling ─────────────*/
struct gpu_context {
    struct completion *ready;
    atomic_t           refcount;
    /* ... */
};

struct gpu_context *gpu_ctx_create(void)
{
    struct gpu_context *ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    ctx->ready = kmalloc(sizeof(*ctx->ready), GFP_KERNEL);
    if (!ctx->ready) {
        kfree(ctx);
        return NULL;
    }

    init_completion(ctx->ready);
    atomic_set(&ctx->refcount, 1);
    return ctx;
}

/* Signal from GPU interrupt when context setup is complete */
void gpu_ctx_signal_ready(struct gpu_context *ctx)
{
    complete(ctx->ready);
}

/* Wait for context to be ready before submitting work */
int gpu_ctx_wait_ready(struct gpu_context *ctx, long timeout_ms)
{
    long ret = wait_for_completion_interruptible_timeout(
                    ctx->ready, msecs_to_jiffies(timeout_ms));
    if (ret > 0) return 0;
    if (ret == 0) return -ETIMEDOUT;
    return -ERESTARTSYS;
}

/* ─── Reinitializable completion (rearm after use) ────────────────────────*/
void gpu_reinit_completion_example(struct gpu_device *gpu)
{
    /* DECLARE_COMPLETION_ONSTACK: for on-stack completions in functions */
    DECLARE_COMPLETION_ONSTACK(local_done);

    /* After waiting, reinitialize for reuse */
    reinit_completion(&gpu->hw_init_done);

    /* Now hw_init_done can be wait/complete'd again */
}
```

---

## Explanation

### Core Concept

`completion` is a one-shot (or reinitializable) synchronization primitive for the pattern: "thread A waits for thread B to finish a task." It is simpler and more correct than using a mutex+condvar pair for this pattern.

```
Main Thread (probe)                 Init kthread
      │                                 │
      │─ init_completion(done) ─────────│
      │─ kthread_run(init_thread) ──────►
      │                                 │ (doing slow init)
      │─ wait_for_completion() ───►SLEEP │
      │                                 │ init complete
      │                                 │─ complete(done)
      │◄── woken ───────────────────────│
      │─ continue probe                 │ thread exits
```

Internal mechanism: `completion.done` (counter) starts at 0. `complete()` increments it and wakes waiters. `wait_for_completion()` decrements it (or sleeps if 0).

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `DECLARE_COMPLETION(name)` | Static completion at file/global scope |
| `DECLARE_COMPLETION_ONSTACK(name)` | On-stack completion (avoids heap alloc) |
| `init_completion(comp)` | Runtime initialization |
| `reinit_completion(comp)` | Reset completion for reuse (race-free) |
| `complete(comp)` | Signal one waiter |
| `complete_all(comp)` | Signal all waiters (sets done to UINT_MAX) |
| `wait_for_completion(comp)` | Wait (uninterruptible, no timeout) |
| `wait_for_completion_timeout(comp, jiffies)` | Wait with timeout (returns remaining jiffies or 0) |
| `wait_for_completion_interruptible(comp)` | Wait, woken by signal |
| `wait_for_completion_interruptible_timeout(comp, j)` | Signal-aware + timeout |
| `completion_done(comp)` | Non-blocking check if completion is done |

### Trade-offs & Pitfalls

- **`complete` vs `complete_all`.** After `complete_all`, `wait_for_completion` returns immediately for all future callers — the completion stays "done" permanently. After `complete` (one waiter), the counter decrements back to 0. Use `complete_all` for broadcast "everyone can proceed" events (e.g., module initialization done, GPU ready for all CUDA streams).
- **`reinit_completion` not `init_completion` for reuse.** `init_completion` sets `done=0` unconditionally — if a waiter is in the middle of `wait_for_completion`, `init_completion` would corrupt its state. `reinit_completion` uses `WRITE_ONCE(comp->done, 0)` with proper barriers.
- **Stack lifetime.** `DECLARE_COMPLETION_ONSTACK` must not be used if the completion outlives the function (e.g., stored in a kthread that persists after the function returns).

### NVIDIA / GPU Context

NVIDIA GPU driver uses completions for:
- **Firmware load:** `wait_for_completion_timeout(&fw_load_done, 30*HZ)` — firmware loading is done by a separate kthread while probe waits
- **GPU FECS (Front End Command Scheduler) ready:** completion signaled from GPU interrupt when FECS reports ready after reset
- **PM (power management) transitions:** GPU suspend/resume uses completions to ensure the irq handler does not run while the GPU is powering down

---

## Cross Questions & Answers

**CQ1: What is the difference between `completion` and a `wait_queue` for one-time synchronization?**
> A `wait_queue` requires the waker to signal AFTER ensuring the waiter has entered the queue — otherwise the signal is lost. `completion` solves this: if `complete()` is called before `wait_for_completion()`, the completion count is > 0 and the waiter returns immediately without sleeping. This "remembered" signal makes completions safer for the pattern "init thread may finish before the main thread even checks."

**CQ2: Can `complete_all` be used for a GPU ready broadcast to multiple CUDA contexts?**
> Yes, exactly. When GPU hardware initialization finishes, call `complete_all(&gpu->hw_init_done)`. All waiting CUDA context creation threads (waiting on `wait_for_completion`) wake simultaneously. Future calls to `wait_for_completion` also return immediately since `done` is set to `UINT_MAX/2` (a large value that decrements back but never reaches 0 for reasonable call counts). This is the correct pattern for one-time "system ready" broadcasts.

**CQ3: How would you implement a countdown completion (wait until N tasks finish)?**
> Use `atomic_t pending_count`. Initialize to N. Each task decrements with `atomic_dec_and_test` — when the last task brings it to 0, call `complete()`. The waiter calls `wait_for_completion()`. Alternatively, use `kref` with a custom release function that calls `complete()`. This pattern is used in GPU barrier synchronization: wait for all N GPU engines to finish before proceeding.

**CQ4: What is `try_wait_for_completion` and when is it useful?**
> `try_wait_for_completion(comp)` is non-blocking: returns 1 if the completion is done (and decrements done), 0 if still pending. Useful in polling loops where you want to check completion status without sleeping: GPU submit path can poll with `try_wait_for_completion` for a few microseconds before falling back to `wait_for_completion_timeout` to avoid unnecessary context switches for fast GPU operations.

**CQ5: What is `swait_completion` (simple wait queue completion) in RT-Linux?**
> `swait_completion` is a variant using `swait_queue` instead of `wait_queue`. On `PREEMPT_RT` kernels, `wait_queue`'s spinlock becomes a sleeping lock, breaking the assumption that completions can be signaled from IRQ context. `swait_completion` uses a simpler, RT-compatible wait queue that avoids this issue. NVIDIA's GPU driver on RT-Linux (used in automotive/robotics) uses `swait_completion` for fence signaling from the GPU interrupt handler.

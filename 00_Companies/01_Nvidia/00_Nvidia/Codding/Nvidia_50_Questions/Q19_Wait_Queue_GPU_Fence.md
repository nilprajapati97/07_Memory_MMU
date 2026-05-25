# Q19: Wait Queue for GPU Fence Completion

**Section:** Concurrency & Synchronization | **Difficulty:** Hard | **Topics:** `wait_queue_head_t`, `wait_event_interruptible_timeout`, GPU fence, seqno, `wake_up_all`

---

## Question

Implement wait queue usage for GPU fence completion using `wait_event_interruptible_timeout`.

---

## Answer

```c
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/errno.h>

/* ─── GPU Fence Structure ─────────────────────────────────────────────────
 * A GPU fence tracks completion of a submitted GPU command batch.
 * The GPU hardware increments seqno (via interrupt) when work completes.
 */
struct gpu_fence {
    atomic64_t          seqno;     /* completed sequence number (GPU writes) */
    wait_queue_head_t   wq;        /* waiters for fence completion           */
    u64                 context;   /* fence context identifier               */
};

/* ─── Fence initialization ────────────────────────────────────────────────*/
void gpu_fence_init(struct gpu_fence *fence, u64 context)
{
    atomic64_set(&fence->seqno, 0);
    init_waitqueue_head(&fence->wq);
    fence->context = context;
}

/* ─── Signal fence (called from GPU interrupt handler) ───────────────────*/
void gpu_fence_signal(struct gpu_fence *fence, u64 seqno)
{
    /*
     * Atomically update the completed seqno.
     * GPU irq handler calls this after reading fence value from GPU register.
     */
    atomic64_set(&fence->seqno, seqno);

    /*
     * wake_up_all: wake every process waiting on this wait queue.
     * The condition in wait_event_* is re-evaluated after wakeup.
     */
    wake_up_all(&fence->wq);
}

/* ─── Wait for fence (called from CUDA runtime / user ioctl) ─────────────*/
int gpu_fence_wait(struct gpu_fence *fence, u64 target_seqno,
                    long timeout_ms)
{
    long timeout_jiffies;
    long ret;

    /* Already signaled — fast path, no sleep */
    if (atomic64_read(&fence->seqno) >= target_seqno)
        return 0;

    if (timeout_ms < 0)
        timeout_jiffies = MAX_SCHEDULE_TIMEOUT;  /* wait forever */
    else
        timeout_jiffies = msecs_to_jiffies(timeout_ms);

    /*
     * wait_event_interruptible_timeout:
     *   - Puts process to sleep (TASK_INTERRUPTIBLE)
     *   - Wakes when condition is true OR signal arrives OR timeout
     *   - Returns: > 0 = woke by condition (remaining jiffies)
     *              = 0 = timeout expired
     *              < 0 = interrupted by signal (−ERESTARTSYS)
     *
     * The condition is checked atomically with adding to the wait queue,
     * preventing the race where the GPU signals between our check and sleep.
     */
    ret = wait_event_interruptible_timeout(
            fence->wq,
            atomic64_read(&fence->seqno) >= target_seqno,
            timeout_jiffies);

    if (ret > 0)
        return 0;   /* condition met */
    if (ret == 0)
        return -ETIMEDOUT;
    return -ERESTARTSYS; /* signal interrupted */
}

/* ─── Non-blocking poll (for CUDA stream queries) ─────────────────────────*/
bool gpu_fence_is_signaled(struct gpu_fence *fence, u64 target_seqno)
{
    return atomic64_read(&fence->seqno) >= target_seqno;
}

/* ─── Wait without signal interruption ───────────────────────────────────*/
int gpu_fence_wait_uninterruptible(struct gpu_fence *fence,
                                    u64 target_seqno, long timeout_ms)
{
    long ret = wait_event_timeout(
        fence->wq,
        atomic64_read(&fence->seqno) >= target_seqno,
        msecs_to_jiffies(timeout_ms));

    return (ret > 0) ? 0 : -ETIMEDOUT;
}

/* ─── Usage: submit GPU work and wait for its fence ──────────────────────*/
int submit_and_wait_example(struct gpu_fence *fence)
{
    u64 target;
    int ret;

    /* Submit GPU command — GPU will signal fence->seqno = target when done */
    target = gpu_submit_command(fence, some_gpu_cmd_buffer);

    /* Wait up to 5 seconds for GPU to complete */
    ret = gpu_fence_wait(fence, target, 5000);
    if (ret == -ETIMEDOUT) {
        pr_err("GPU timeout! fence seqno=%lld target=%lld\n",
               atomic64_read(&fence->seqno), target);
        /* Trigger GPU TDR recovery */
    }
    return ret;
}
```

---

## Explanation

### Core Concept

A **GPU fence** is a synchronization primitive: the CPU submits work to the GPU and then needs to know when the GPU has finished. The wait queue mechanism allows the CPU thread to sleep (without burning CPU cycles) while waiting for the GPU interrupt.

```
CPU Thread                     GPU Hardware           GPU IRQ Handler
    │                               │                       │
    │─ submit command ──────────────►                       │
    │                               │ (executing)           │
    │─ gpu_fence_wait() ──►  SLEEP  │                       │
    │    (TASK_INTERRUPTIBLE)        │ work done ────────────►
    │                               │               atomic64_set(seqno)
    │                               │               wake_up_all(wq)
    │◄── woken ──────────────────────────────────────────── │
    │─ fence signaled, continue     │                       │
```

**The race condition is handled by the kernel**: `wait_event_*` adds the task to the wait queue and checks the condition atomically — if the GPU signals between our condition check and sleep entry, the kernel guarantees we are woken immediately.

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `init_waitqueue_head(wq)` | Initialize a wait queue head |
| `DECLARE_WAITQUEUE(name, tsk)` | Statically declare a wait queue entry |
| `wait_event_interruptible_timeout(wq, cond, timeout)` | Sleep until condition, signal, or timeout |
| `wait_event_timeout(wq, cond, timeout)` | Same but uninterruptible (ignores signals) |
| `wait_event_interruptible(wq, cond)` | Sleep until condition or signal (no timeout) |
| `wake_up_all(wq)` | Wake all tasks waiting on the queue |
| `wake_up(wq)` | Wake exactly one task (not ideal for fences) |
| `wake_up_interruptible_all(wq)` | Wake only TASK_INTERRUPTIBLE tasks |
| `msecs_to_jiffies(ms)` | Convert milliseconds to jiffies |
| `MAX_SCHEDULE_TIMEOUT` | Infinite timeout value for `wait_event_timeout` |

### Trade-offs & Pitfalls

- **`wake_up` vs `wake_up_all`.** GPU fences with multiple waiters must use `wake_up_all` — multiple CUDA streams can wait on the same fence. `wake_up` only wakes one waiter (thundering herd avoidance) — wrong for broadcast events like fence signals.
- **Don't busy-wait without `cpu_relax`.** For very short GPU operations (< 10µs), a spin with `cpu_relax()` has lower latency than a sleep-wake cycle. CUDA uses adaptive waiting: spin first (up to ~50µs), then sleep.
- **`-ERESTARTSYS` vs `-EINTR`.** When a signal interrupts the wait, `wait_event_interruptible_timeout` returns `-ERESTARTSYS`. The ioctl handler should propagate this as-is — the kernel will restart the syscall if `SA_RESTART` is set. Converting to `-EINTR` prevents restartable syscalls.

### NVIDIA / GPU Context

NVIDIA GPU fences track:
- **Command stream seqno:** Each submitted command batch gets a seqno; the GPU increments a memory-mapped counter as batches complete
- **Multiple fence types:** binary (on/off), timeline (seqno), timeline_semaphore (dma_fence compatible for Vulkan/D3D12)
- **CUDA synchronization:** `cudaStreamSynchronize` → `gpu_fence_wait(fence, stream_seqno, timeout)` — the CUDA thread sleeps until the GPU completes all work in that stream

---

## Cross Questions & Answers

**CQ1: What is the difference between `wait_event_interruptible` and `wait_event`?**
> `wait_event` puts the process in `TASK_UNINTERRUPTIBLE` state — it cannot be interrupted by signals, only woken by the condition. `wait_event_interruptible` uses `TASK_INTERRUPTIBLE` — signals (like `SIGKILL`, `CTRL+C`) can wake the process and return `-ERESTARTSYS`. For GPU driver ioctl waits, use `interruptible` so users can cancel hung GPU operations with `CTRL+C`. Use `TASK_UNINTERRUPTIBLE` only for very brief, guaranteed-to-complete critical sections.

**CQ2: How do you wake only one specific waiter from a wait queue?**
> Use a custom `wake_function` in the wait queue entry. Define a `wait_queue_entry_t` with a `wake_function` that returns 1 (wake this one) or 0 (skip). Use `__wake_up_locked_key` with a key to match specific waiters. Alternatively, use a per-waiter `completion` variable — more idiomatic for one-to-one wakeups (see Q22).

**CQ3: How does `pollwait` integrate with GPU fence wait queues for `poll()`/`select()` support?**
> In the `file_operations.poll` callback, call `poll_wait(file, &fence->wq, wait)`. This adds the file's wait queue to the poll table so `poll()`/`select()` wakes when the fence is signaled. Return `POLLIN | POLLRDNORM` if already signaled, 0 if pending. NVIDIA's GPU driver exposes fence file descriptors (`sync_file`) via this mechanism, enabling Vulkan/OpenGL drivers to use `poll()` on GPU fences without blocking.

**CQ4: What is the `dma_fence` framework and how does it extend the basic wait queue fence?**
> `dma_fence` (kernel DMA fence) is a standardized fence interface for cross-driver synchronization. It wraps a callback mechanism: `dma_fence_add_callback(fence, cb, fn)` invokes `fn` when the fence signals, without sleeping. It also supports `dma_fence_wait()` which internally uses wait queues. The `sync_file` mechanism wraps a `dma_fence` as a file descriptor, enabling user-space fence passing (e.g., Vulkan `VkSemaphore` backed by GPU fence FD for CPU-GPU and inter-process synchronization).

**CQ5: What happens if a GPU hangs and the fence is never signaled — how should a driver handle it?**
> The `gpu_fence_wait` will return `-ETIMEDOUT` after the specified timeout. The driver should: (1) read GPU fault/status registers to diagnose the hang, (2) issue a GPU reset (hard or soft reset via MMIO), (3) re-initialize GPU state (re-submit lost work or notify CUDA runtime of the loss), (4) signal the timed-out fence with an error flag so other waiters are also unblocked. Linux TDR (Timeout Detection and Recovery) is the standard pattern; NVIDIA implements `nvgpu_rc_gpu_sm_fault()` for this.

# Q44: hrtimer GPU Watchdog

**Section:** Performance & Debugging | **Difficulty:** Hard | **Topics:** `hrtimer`, GPU watchdog, `HRTIMER_RESTART`, `hrtimer_forward_now`, seqno comparison, recovery work

---

## Question

Implement a GPU watchdog using `hrtimer` to detect stuck GPU workloads and trigger recovery.

---

## Answer

```c
#include <linux/hrtimer.h>
#include <linux/atomic.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/sched.h>

/* ─── Watchdog Parameters ─────────────────────────────────────────────────*/
#define GPU_WATCHDOG_PERIOD_MS  2000   /* check every 2 seconds             */
#define GPU_WATCHDOG_TIMEOUT_MS 10000  /* GPU is stuck if no progress in 10s */

/* ─── GPU Watchdog State ──────────────────────────────────────────────────*/
struct gpu_watchdog {
    struct hrtimer      timer;              /* high-resolution timer         */
    atomic64_t          last_completed_seq; /* last seqno completed by GPU   */
    atomic64_t          last_seen_seq;      /* last seqno seen at last check  */
    atomic_t            stuck_count;        /* consecutive stuck checks       */
    struct work_struct  recovery_work;      /* runs GPU recovery in process ctx */
    struct gpu_device  *gpu;
    bool                enabled;
};

/* ─── GPU Recovery Work ───────────────────────────────────────────────────
 * Runs in a workqueue (process context) after watchdog detects a hung GPU.
 * Cannot run directly in hrtimer callback (interrupt context).
 */
static void gpu_recovery_work_fn(struct work_struct *work)
{
    struct gpu_watchdog *wdog =
        container_of(work, struct gpu_watchdog, recovery_work);
    struct gpu_device *gpu = wdog->gpu;

    pr_crit("GPU Watchdog: initiating GPU recovery (stuck_count=%d)\n",
            atomic_read(&wdog->stuck_count));

    /* ── Step 1: Stop the GPU from processing new commands ─────────────── */
    gpu_disable_command_submission(gpu);

    /* ── Step 2: Wait for any in-flight DMA to complete (with timeout) ─── */
    if (gpu_wait_dma_idle(gpu, 100 /* ms */))
        pr_warn("GPU: DMA did not drain cleanly, forcing reset\n");

    /* ── Step 3: Hardware GPU reset via PCIe FLR ─────────────────────────
     * pci_reset_function(): triggers PCIe Function Level Reset.
     * Resets GPU registers to power-on state.
     */
    if (pci_reset_function(gpu->pdev))
        pr_err("GPU: FLR failed, system may need reboot\n");

    /* ── Step 4: Reinitialize GPU firmware and hardware ─────────────────── */
    gpu_fw_load(gpu);
    gpu_hw_init(gpu);

    /* ── Step 5: Fail all pending GPU work (return error to waiting tasks) */
    gpu_fail_pending_commands(gpu, -EIO);

    /* ── Step 6: Re-enable command submission ────────────────────────────── */
    gpu_enable_command_submission(gpu);
    atomic_set(&wdog->stuck_count, 0);

    pr_info("GPU Watchdog: recovery complete\n");
}

/* ─── hrtimer Callback ────────────────────────────────────────────────────
 * Runs in hardirq context. Must be fast, non-sleeping.
 * Checks GPU progress; if stuck, schedules recovery workqueue.
 */
static enum hrtimer_restart gpu_watchdog_fn(struct hrtimer *timer)
{
    struct gpu_watchdog *wdog =
        container_of(timer, struct gpu_watchdog, timer);

    u64 current_completed = atomic64_read(&wdog->last_completed_seq);
    u64 last_seen         = atomic64_read(&wdog->last_seen_seq);

    /* ── Check GPU progress ──────────────────────────────────────────────── */
    if (current_completed == last_seen && current_completed > 0) {
        /*
         * GPU has not made progress since last check.
         * Increment stuck counter. After enough consecutive misses, recover.
         */
        int count = atomic_inc_return(&wdog->stuck_count);
        int max_stuck = GPU_WATCHDOG_TIMEOUT_MS / GPU_WATCHDOG_PERIOD_MS;

        pr_warn("GPU Watchdog: GPU stuck! seqno=%llu stuck_count=%d/%d\n",
                current_completed, count, max_stuck);

        if (count >= max_stuck) {
            /* GPU is definitively stuck — schedule recovery */
            schedule_work(&wdog->recovery_work);
        }
    } else {
        /* GPU made progress — reset stuck counter */
        atomic_set(&wdog->stuck_count, 0);
        atomic64_set(&wdog->last_seen_seq, current_completed);
    }

    /*
     * hrtimer_forward_now: advance the timer expiration to
     * "now + period". If the timer fired late (overrun), this
     * ensures the NEXT expiry is exactly one period from NOW,
     * not from the original scheduled time (which prevents
     * a burst of callbacks catching up after a late fire).
     */
    hrtimer_forward_now(timer,
                         ms_to_ktime(GPU_WATCHDOG_PERIOD_MS));

    /*
     * HRTIMER_RESTART: reschedule the timer for another period.
     * HRTIMER_NORESTART: do not reschedule (one-shot timer).
     */
    return HRTIMER_RESTART;
}

/* ─── GPU completion interrupt: update seqno ─────────────────────────────
 * Called from GPU completion IRQ handler when a GPU workload finishes.
 */
void gpu_watchdog_heartbeat(struct gpu_watchdog *wdog, u64 completed_seqno)
{
    /*
     * Update the completed seqno. The watchdog timer reads this
     * to determine if the GPU is making progress.
     */
    atomic64_set(&wdog->last_completed_seq, completed_seqno);
}

/* ─── Initialize the watchdog ────────────────────────────────────────────*/
int gpu_watchdog_init(struct gpu_watchdog *wdog, struct gpu_device *gpu)
{
    wdog->gpu = gpu;
    atomic64_set(&wdog->last_completed_seq, 0);
    atomic64_set(&wdog->last_seen_seq, 0);
    atomic_set(&wdog->stuck_count, 0);
    INIT_WORK(&wdog->recovery_work, gpu_recovery_work_fn);

    /*
     * hrtimer_init: initialize the hrtimer.
     * CLOCK_MONOTONIC: monotonic clock (not affected by wall-clock adjustments).
     * HRTIMER_MODE_REL: expiry is relative to NOW (not absolute time).
     */
    hrtimer_init(&wdog->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    wdog->timer.function = gpu_watchdog_fn;

    wdog->enabled = true;

    /* Start the watchdog timer */
    hrtimer_start(&wdog->timer,
                   ms_to_ktime(GPU_WATCHDOG_PERIOD_MS),
                   HRTIMER_MODE_REL);

    pr_info("GPU Watchdog: started (period=%dms, timeout=%dms)\n",
            GPU_WATCHDOG_PERIOD_MS, GPU_WATCHDOG_TIMEOUT_MS);
    return 0;
}

/* ─── Stop the watchdog (during driver remove) ───────────────────────────*/
void gpu_watchdog_stop(struct gpu_watchdog *wdog)
{
    wdog->enabled = false;
    /*
     * hrtimer_cancel: cancel the timer. If the callback is currently
     * running, waits for it to complete before returning.
     * Safe to call from any context (process, softirq, but not hardirq
     * for the same timer).
     */
    hrtimer_cancel(&wdog->timer);
    cancel_work_sync(&wdog->recovery_work);
}
```

---

## Explanation

### Core Concept

```
GPU Watchdog Architecture:

  hrtimer (2s period)
       │
       ▼  [hardirq context]
  gpu_watchdog_fn()
       │
       ├── GPU made progress? → reset stuck_count, reschedule timer
       │
       └── GPU stuck? → stuck_count++
                           │
                           └── stuck_count >= 5? → schedule_work(recovery)
                                                          │
                                                          ▼ [process context]
                                                   gpu_recovery_work_fn()
                                                   FLR → reinit → notify apps
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `hrtimer_init(timer, clock, mode)` | Initialize hrtimer |
| `hrtimer_start(timer, time, mode)` | Arm the timer |
| `hrtimer_cancel(timer)` | Cancel and wait for callback to finish |
| `hrtimer_forward_now(timer, period)` | Advance expiry (handles late firings) |
| `HRTIMER_RESTART` | Reschedule timer from callback |
| `HRTIMER_NORESTART` | One-shot: do not reschedule |
| `ms_to_ktime(ms)` | Convert milliseconds to `ktime_t` |
| `CLOCK_MONOTONIC` | Not affected by wall-clock adjustments |
| `HRTIMER_MODE_REL` | Relative (from now) expiry time |
| `atomic64_read/set` | Atomic seqno read/write |
| `schedule_work(work)` | Queue work in system workqueue |

### Trade-offs & Pitfalls

- **hrtimer callback is in hardirq context.** Cannot sleep, cannot acquire mutexes, cannot call most kernel APIs. Defer all real work to `schedule_work`. The callback only checks state and schedules.
- **`hrtimer_cancel` in driver remove.** Must call `hrtimer_cancel` before the `gpu_device` struct is freed. Otherwise the timer callback runs after the struct is freed — use-after-free. Also cancel the recovery work with `cancel_work_sync`.

### NVIDIA / GPU Context

NVIDIA's GPU watchdog fires at 2–4 second intervals. If a GPU doesn't complete a command within the watchdog timeout, Xorg/compositor receives a signal and displays "GPU has stopped responding". The watchdog triggers a GPU reset (similar to `pci_reset_function`) which terminates all running CUDA kernels and notifies applications via CUDA error codes (`cudaErrorDevicesUnavailable`).

---

## Cross Questions & Answers

**CQ1: What is the difference between `hrtimer` and the regular `timer_list` in the Linux kernel?**
> `timer_list` (jiffies-based): resolution limited to HZ (1ms–10ms). Built on the timer wheel — O(1) insert, efficient for large numbers of timers. `hrtimer` (high-resolution): nanosecond resolution backed by hardware timer (HPET, TSC-deadline, ARM architecture counter). Uses a red-black tree ordered by expiry — O(log n) insert. Use `hrtimer` when precision matters (GPU timeout detection, media timing). Use `timer_list` for coarse timeouts (TCP keepalive, network retransmission) where O(1) is more important than precision.

**CQ2: What happens if the GPU recovery takes longer than the watchdog period?**
> The hrtimer fires again during recovery. The callback checks `wdog->stuck_count` and sees it's already >= max_stuck. It calls `schedule_work(&wdog->recovery_work)` again. `schedule_work` is idempotent if the work is already pending — it won't queue a second instance if one is already running. `INIT_WORK` (not `INIT_DELAYED_WORK`) with `schedule_work` ensures at most one recovery instance runs at a time. After recovery completes, `stuck_count` is reset to 0, so subsequent checks won't re-trigger until the GPU is stuck again.

**CQ3: How does `hrtimer_forward_now` prevent timer burst after a delayed callback?**
> Without `hrtimer_forward_now`: if the callback fires 500ms late (CPU was busy), and the timer was programmed for T+2000ms, after callback the next expiry is T+4000ms — still 1500ms in the future. So only one callback fires for the late period. With `hrtimer_forward_now(timer, 2000ms)`: sets expiry to "now + 2000ms" regardless of the original schedule. If there were N overruns (timer should have fired N times), `hrtimer_forward_now` advances past all of them in one call and returns the number of overruns. Prevents callback burst while maintaining the correct period going forward.

**CQ4: What is a GPU "hang" and what are the common causes in production CUDA workloads?**
> GPU hang: the GPU stops processing commands and the watchdog timeout expires. Common causes: (1) **Infinite loop in GPU kernel** — a CUDA kernel with a conditional loop that never terminates (e.g., convergence check that never converges), (2) **GPU memory fault** — an unhandled GPU page fault that halts the SM (corrected in newer architectures with fault replay), (3) **PCIe link error** — loss of PCIe connectivity causing commands to be lost, (4) **Firmware deadlock** — GPU microcontroller (SEC2, GSP) in a deadlock, (5) **Thermal emergency** — GPU powered off automatically due to overtemperature.

**CQ5: How would you implement a "heartbeat" mechanism where GPU firmware reports liveness?**
> GPU firmware writes a monotonically increasing sequence number to a shared DRAM location (mapped into both GPU and CPU address spaces) at a regular interval (e.g., every 100ms). The watchdog reads this location and compares it to the last seen value. If the value hasn't changed for 2 watchdog intervals: firmware is dead (not just GPU workload stuck). This distinction matters: a stuck user CUDA kernel doesn't affect firmware liveness. Implementation: allocate a `dma_coherent` 8-byte buffer, map it into GPU firmware address space via MMIO register, firmware writes a counter, driver reads the counter in the watchdog timer.

# Q43: Ftrace GPU Submission Latency Measurement

**Section:** Performance & Debugging | **Difficulty:** Hard | **Topics:** `ktime_get`, `ktime_sub`, per-CPU latency, `debugfs`, `seq_file`, latency histogram, `DEFINE_PER_CPU`

---

## Question

Implement a GPU command submission latency measurement system using per-CPU statistics and debugfs.

---

## Answer

```c
#include <linux/ktime.h>
#include <linux/percpu.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>
#include <linux/minmax.h>

/* ─── Per-CPU Latency Statistics ─────────────────────────────────────────*/
struct latency_stat {
    u64 min_ns;      /* minimum submission latency (nanoseconds)    */
    u64 max_ns;      /* maximum submission latency                  */
    u64 sum_ns;      /* sum for computing average                   */
    u64 count;       /* number of submissions sampled               */

    /* Histogram buckets: [0–1µs, 1–10µs, 10–100µs, 100µs–1ms, >1ms] */
    u64 hist[5];

    /* ktime of last submission start (per-CPU, no lock needed) */
    ktime_t submit_start;
} ____cacheline_aligned; /* pad to cache line to prevent false sharing */

DEFINE_PER_CPU(struct latency_stat, submit_latency);

/* ─── Histogram bucket index ─────────────────────────────────────────────*/
static int latency_bucket(u64 ns)
{
    if (ns < 1000)           return 0; /* < 1µs   */
    if (ns < 10000)          return 1; /* < 10µs  */
    if (ns < 100000)         return 2; /* < 100µs */
    if (ns < 1000000)        return 3; /* < 1ms   */
    return 4;                          /* >= 1ms  */
}

/* ─── Record start of GPU submission ─────────────────────────────────────
 * Called at the beginning of gpu_channel_submit().
 * Runs on the current CPU (no migration during submission).
 */
void record_submit_start(void)
{
    /*
     * this_cpu_ptr: fast per-CPU access — no lock, no atomic, no preemption
     * needed (caller must ensure no CPU migration between start and end).
     * Use get_cpu / put_cpu to prevent migration, or disable preemption.
     */
    struct latency_stat *stat = this_cpu_ptr(&submit_latency);

    preempt_disable(); /* prevent CPU migration between start/end */
    stat->submit_start = ktime_get(); /* nanosecond-resolution monotonic clock */
}

/* ─── Record end of GPU submission ───────────────────────────────────────
 * Called after command is written to ring buffer and doorbell is rung.
 */
void record_submit_latency(void)
{
    struct latency_stat *stat = this_cpu_ptr(&submit_latency);
    ktime_t now     = ktime_get();
    ktime_t start   = stat->submit_start;
    u64     lat_ns;

    preempt_enable(); /* re-enable preemption now that we've read submit_start */

    lat_ns = ktime_to_ns(ktime_sub(now, start));

    /* Update per-CPU statistics (no lock — single CPU writes these) */
    if (stat->count == 0 || lat_ns < stat->min_ns)
        stat->min_ns = lat_ns;
    if (lat_ns > stat->max_ns)
        stat->max_ns = lat_ns;

    stat->sum_ns += lat_ns;
    stat->count++;
    stat->hist[latency_bucket(lat_ns)]++;
}

/* ─── debugfs: display aggregated latency stats ─────────────────────────*/
static const char * const bucket_labels[] = {
    "<1µs", "1–10µs", "10–100µs", "100µs–1ms", ">1ms"
};

static int latency_show(struct seq_file *m, void *v)
{
    struct latency_stat total = {
        .min_ns = U64_MAX,
        .max_ns = 0,
    };
    int cpu, i;

    /* ── Aggregate per-CPU stats ───────────────────────────────────────── */
    for_each_possible_cpu(cpu) {
        const struct latency_stat *s = &per_cpu(submit_latency, cpu);

        if (s->count == 0)
            continue;

        total.sum_ns += s->sum_ns;
        total.count  += s->count;
        if (s->min_ns < total.min_ns) total.min_ns = s->min_ns;
        if (s->max_ns > total.max_ns) total.max_ns = s->max_ns;
        for (i = 0; i < 5; i++)
            total.hist[i] += s->hist[i];
    }

    /* ── Print summary ──────────────────────────────────────────────────── */
    if (total.count == 0) {
        seq_puts(m, "No submissions recorded.\n");
        return 0;
    }

    seq_printf(m, "GPU Submission Latency Statistics\n");
    seq_printf(m, "==================================\n");
    seq_printf(m, "Samples:  %llu\n",         total.count);
    seq_printf(m, "Min:      %llu ns\n",       total.min_ns);
    seq_printf(m, "Max:      %llu ns\n",       total.max_ns);
    seq_printf(m, "Avg:      %llu ns\n",       total.sum_ns / total.count);

    seq_printf(m, "\nLatency Histogram:\n");
    for (i = 0; i < 5; i++) {
        u64 pct = total.hist[i] * 100 / total.count;
        seq_printf(m, "  %-12s: %8llu  (%3llu%%)\n",
                   bucket_labels[i], total.hist[i], pct);
    }

    return 0;
}

DEFINE_SHOW_ATTRIBUTE(latency); /* defines latency_open using latency_show */

/* ─── Reset per-CPU counters via write to debugfs ────────────────────────*/
static ssize_t latency_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *ppos)
{
    int cpu;
    for_each_possible_cpu(cpu)
        memset(&per_cpu(submit_latency, cpu), 0, sizeof(struct latency_stat));
    return count;
}

static const struct file_operations latency_reset_fops = {
    .open    = latency_open,
    .read    = seq_read,
    .write   = latency_write,
    .release = single_release,
    .llseek  = seq_lseek,
};

/* ─── debugfs initialization ─────────────────────────────────────────────*/
int gpu_latency_debugfs_init(struct dentry *parent)
{
    debugfs_create_file("submit_latency", 0644, parent, NULL,
                         &latency_reset_fops);
    return 0;
}

/* ─── Usage in submission hot path ───────────────────────────────────────
 *
 * void gpu_channel_submit(struct gpu_channel *ch, struct gpu_cmd *cmd)
 * {
 *     record_submit_start();
 *
 *     spin_lock(&ch->lock);
 *     gpu_write_cmd_to_ring(ch, cmd);
 *     gpu_ring_doorbell(ch);
 *     spin_unlock(&ch->lock);
 *
 *     record_submit_latency();
 * }
 *
 * Read stats:
 *   cat /sys/kernel/debug/gpu/submit_latency
 *
 * Reset stats:
 *   echo 1 > /sys/kernel/debug/gpu/submit_latency
 */
```

---

## Explanation

### Core Concept

```
CPU0                     CPU1
submit_latency[0]        submit_latency[1]
  min=450ns                min=480ns
  max=12µs                 max=9.8µs
  count=50000              count=48000
  hist[1]=45000            hist[1]=43000

→ latency_show() aggregates all CPUs → single view:
  Avg: 465ns, Min: 450ns, Max: 12µs
  1–10µs: 93k/98k = 95%   ← most submissions fast
  >1ms: 3                  ← 3 outliers (contention spikes)
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `DEFINE_PER_CPU(type, name)` | Declare per-CPU variable |
| `this_cpu_ptr(&var)` | Get pointer to current CPU's instance |
| `per_cpu(var, cpu)` | Access specific CPU's instance |
| `for_each_possible_cpu(cpu)` | Iterate all CPUs |
| `ktime_get()` | Monotonic clock (nanosecond resolution) |
| `ktime_sub(a, b)` | Compute time difference |
| `ktime_to_ns(kt)` | Convert `ktime_t` to nanoseconds |
| `____cacheline_aligned` | Pad struct to cache line to prevent false sharing |
| `preempt_disable()` / `preempt_enable()` | Prevent CPU migration |
| `DEFINE_SHOW_ATTRIBUTE(name)` | Define debugfs `file_operations` from show fn |
| `seq_printf(m, fmt, ...)` | Write to seq_file output |

### Trade-offs & Pitfalls

- **False sharing without `____cacheline_aligned`.** Without cache line padding, two adjacent CPUs' `submit_latency` structs share a cache line. CPU0 writing its counters invalidates CPU1's cache line — both CPUs experience cache misses for every update. `____cacheline_aligned` pads to 64 bytes, putting each CPU's struct on its own cache line.
- **preempt_disable/enable overhead.** Calling `preempt_disable()` and `preempt_enable()` on every submission adds ~2ns. For 10M submissions/second: 20ms/second overhead. Use `this_cpu_inc` instead of `this_cpu_ptr` for simple counters — it's preemption-safe by design without explicit preempt_disable.

### NVIDIA / GPU Context

NVIDIA's kernel driver uses identical per-CPU latency tracking for `RM_CTRL_MEMORY_MAP` and submission paths. Exposed via `nvidia-smi dmon` (device monitor) which reads from the RM's stats. `Nsight Systems` timeline view gets GPU submission timestamps by attaching to these same per-CPU stats via ftrace tracepoints.

---

## Cross Questions & Answers

**CQ1: Why use per-CPU statistics instead of a global atomic counter?**
> Global `atomic_t counter`: every `atomic_inc` requires a cache-line exclusive lock (LOCK XADD on x86) — bounces the cache line between all CPUs. At 64 CPUs × 1M submissions/second = 64M atomic operations/second. Each operation: ~20ns (inter-CPU cache coherence latency). Total: 1.28 seconds of overhead per second — 128% overhead. Per-CPU: each CPU writes its own cache line (no contention). Aggregation reads all CPU instances once per report (cheap, infrequent). Per-CPU is 10–100× faster for write-heavy statistics.

**CQ2: What is `ktime_t` and why is it preferred over `jiffies` for latency measurement?**
> `jiffies`: kernel tick counter, increments at HZ (100–1000 Hz). Resolution: 1ms–10ms. Useless for sub-millisecond GPU submission latency. `ktime_t`: nanosecond-resolution monotonic clock (backed by `rdtsc` on x86 / ARM counter). `ktime_get()` reads the hardware time counter via `vDSO` — no syscall, ~5ns overhead. For GPU submission latency measurement (target: < 1µs), only `ktime_t` provides sufficient resolution.

**CQ3: How would you implement a sliding window average (moving average) of latency?**
> Circular buffer of last N samples: `u64 window[WINDOW_SIZE]; u32 head; u64 window_sum;`. On new sample: `window_sum -= window[head]; window[head] = new_lat; window_sum += new_lat; head = (head+1)%WINDOW_SIZE;`. Average = `window_sum / WINDOW_SIZE`. This gives "last N submissions" average instead of "all-time" average. For GPU latency: N=1000 gives a 1-second window at 1000 submissions/second. Useful for detecting gradual degradation vs total average obscuring recent spikes.

**CQ4: What is the overhead of `ktime_get()` and can it be reduced for extremely hot paths?**
> `ktime_get()` on x86: typically 10–20ns (reads `rdtsc` and multiplies by nanosecond multiplier). For a submission path that itself takes 200ns, 20ns is 10% overhead — acceptable. For paths < 100ns: use `local_clock()` (reads raw TSC, ~3ns, not converted to ns) or `sched_clock()`. Alternatively: sample-based latency — only record every Nth submission (`if (!(count % 1000)) { measure(); }`). 1:1000 sampling reduces overhead to < 0.01% while still providing statistical accuracy.

**CQ5: How do you expose GPU driver statistics to user space securely via debugfs?**
> Security considerations for debugfs: (1) mount with `mode=700` or restrict via DAC (`debugfs_create_file(name, 0400, ...)`), (2) validate user writes in the `write` fop (the reset handler) — copy from user space with `copy_from_user` + bounds check, (3) never expose sensitive data (MMIO addresses, cryptographic material, other processes' context IDs) via debugfs, (4) for production: use ioctl (audited, with capability checks) instead of debugfs for sensitive statistics. `nvidia-smi` uses ioctl via `/dev/nvidiactl`, not debugfs.

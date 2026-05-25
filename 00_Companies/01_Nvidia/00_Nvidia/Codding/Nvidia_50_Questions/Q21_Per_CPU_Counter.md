# Q21: Per-CPU Counter for GPU Submit Tracking

**Section:** Concurrency & Synchronization | **Difficulty:** Medium | **Topics:** `DEFINE_PER_CPU`, `this_cpu_inc`, `for_each_possible_cpu`, per-CPU variables, cache-line isolation, performance counters

---

## Question

Implement a per-CPU counter for tracking GPU command submissions and aggregating across CPUs.

---

## Answer

```c
#include <linux/percpu.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/atomic.h>

/* ─── Per-CPU counter definition ─────────────────────────────────────────
 * Each CPU has its own copy of this counter in CPU-local memory.
 * No locks needed — each CPU only modifies its own copy.
 * Eliminates cache-line bouncing of a shared atomic counter.
 */
DEFINE_PER_CPU(u64, gpu_submit_count);     /* commands submitted */
DEFINE_PER_CPU(u64, gpu_complete_count);   /* commands completed */
DEFINE_PER_CPU(u64, gpu_error_count);      /* error events       */

/* Per-CPU structure for richer stats (cache-line aligned) */
struct gpu_cpu_stats {
    u64 submits;
    u64 completions;
    u64 bytes_transferred;
    u64 fence_waits;
} ____cacheline_aligned;

DEFINE_PER_CPU(struct gpu_cpu_stats, gpu_stats);

/* ─── Increment on the hot path (submission) ─────────────────────────────
 * this_cpu_inc: lock-free, preemption-safe increment of per-CPU var.
 * Must NOT be preempted between reading the per-CPU address and writing.
 * this_cpu_inc handles this internally by disabling preemption momentarily.
 */
void gpu_command_submit(struct gpu_ring *ring, u64 cmd)
{
    /* Increment the per-CPU counter — no lock, no atomic overhead */
    this_cpu_inc(gpu_submit_count);

    /* Richer stats structure */
    this_cpu_inc(gpu_stats.submits);

    /* submit command to ring... */
    gpu_ring_write(ring, cmd);
}

/* ─── GPU IRQ completion path ─────────────────────────────────────────────*/
irqreturn_t gpu_completion_irq(int irq, void *dev)
{
    this_cpu_inc(gpu_complete_count);
    this_cpu_inc(gpu_stats.completions);
    return IRQ_HANDLED;
}

/* ─── Aggregate all CPUs into a global sum ────────────────────────────────
 * Iterate all possible CPUs and sum their per-CPU counters.
 * Returns a snapshot — not instantaneously consistent, but good enough
 * for monitoring/statistics.
 */
u64 gpu_total_submits(void)
{
    u64 total = 0;
    int cpu;

    /*
     * for_each_possible_cpu: iterates all CPUs that could ever be present,
     * including hot-plugged CPUs. Use for_each_online_cpu for only
     * currently active CPUs.
     */
    for_each_possible_cpu(cpu)
        total += per_cpu(gpu_submit_count, cpu);

    return total;
}

/* ─── Aggregate rich stats structure ─────────────────────────────────────*/
void gpu_get_global_stats(struct gpu_cpu_stats *out)
{
    int cpu;

    memset(out, 0, sizeof(*out));
    for_each_possible_cpu(cpu) {
        const struct gpu_cpu_stats *s = &per_cpu(gpu_stats, cpu);
        out->submits           += s->submits;
        out->completions       += s->completions;
        out->bytes_transferred += s->bytes_transferred;
        out->fence_waits       += s->fence_waits;
    }
}

/* ─── Reset all per-CPU counters ─────────────────────────────────────────*/
void gpu_reset_stats(void)
{
    int cpu;
    for_each_possible_cpu(cpu) {
        per_cpu(gpu_submit_count, cpu)   = 0;
        per_cpu(gpu_complete_count, cpu) = 0;
        per_cpu(gpu_error_count, cpu)    = 0;
        memset(&per_cpu(gpu_stats, cpu), 0, sizeof(struct gpu_cpu_stats));
    }
}

/* ─── debugfs read callback ───────────────────────────────────────────────*/
static ssize_t gpu_stats_read(struct file *f, char __user *buf,
                               size_t count, loff_t *pos)
{
    char tmp[256];
    int len;
    u64 total_sub, total_cmp;
    int cpu;

    total_sub = total_cmp = 0;
    for_each_possible_cpu(cpu) {
        total_sub += per_cpu(gpu_submit_count, cpu);
        total_cmp += per_cpu(gpu_complete_count, cpu);
    }

    len = snprintf(tmp, sizeof(tmp),
                   "submit_total:    %llu\n"
                   "complete_total:  %llu\n"
                   "in_flight:       %llu\n",
                   total_sub, total_cmp,
                   total_sub - total_cmp);

    return simple_read_from_buffer(buf, count, pos, tmp, len);
}
```

---

## Explanation

### Core Concept

Per-CPU variables eliminate lock contention and cache-line bouncing on hot counters:

```
Shared atomic counter (bad on many CPUs):
  CPU0: atomic_inc(count) → acquires cache line → increments → releases
  CPU1: waits for CPU0 to release cache line
  CPU2: waits for CPU1 ...
  → O(N) serialization on the cache line

Per-CPU counter (good):
  CPU0: this_cpu_inc(count) → increments CPU0's copy (always in CPU0's L1)
  CPU1: this_cpu_inc(count) → increments CPU1's copy (always in CPU1's L1)
  → zero contention, O(1) parallel increments
```

**Reading aggregate** requires iterating all CPUs, but reads are rare (stats polling) compared to writes (every GPU submission). This is an asymmetric trade-off optimal for high-frequency writes with infrequent reads.

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `DEFINE_PER_CPU(type, name)` | Declare per-CPU variable |
| `DECLARE_PER_CPU(type, name)` | Declare extern per-CPU variable |
| `this_cpu_inc(var)` | Increment current CPU's copy (preempt-safe) |
| `this_cpu_add(var, val)` | Add value to current CPU's copy |
| `this_cpu_read(var)` | Read current CPU's copy |
| `this_cpu_write(var, val)` | Write current CPU's copy |
| `per_cpu(var, cpu)` | Access specific CPU's copy by CPU number |
| `get_cpu_var(var)` | Disable preemption + return current CPU's var |
| `put_cpu_var(var)` | Re-enable preemption (pair with `get_cpu_var`) |
| `for_each_possible_cpu(cpu)` | Iterate all possible CPUs |
| `for_each_online_cpu(cpu)` | Iterate only currently online CPUs |
| `____cacheline_aligned` | Align struct to cache line (prevent false sharing) |

### Trade-offs & Pitfalls

- **Per-CPU reads are not consistent snapshots.** While aggregating with `for_each_possible_cpu`, other CPUs continue incrementing their counters. The total is approximately correct for monitoring but not transactionally consistent.
- **`get_cpu_var` vs `this_cpu_inc`.** `this_cpu_*` macros disable preemption internally and are optimized for single operations. For multi-step read-modify-write sequences, use `get_cpu_var(v)` / `put_cpu_var(v)` to hold preemption disabled across all steps.
- **Memory consumption.** Each per-CPU variable allocates one copy per possible CPU. On a 256-CPU system, a 64-byte struct becomes 16KB. Keep per-CPU structs small for hot-path variables.

### NVIDIA / GPU Context

NVIDIA GPU driver per-CPU usage:
- **Submission fast path:** Each CPU that can submit GPU commands has its own per-CPU submission counter and small ring buffer segment — eliminates submission lock contention across CPU cores in a multi-threaded CUDA application
- **IRQ affinity + per-CPU IRQ:** NVIDIA GPUs use `irq_set_affinity` to pin GPU completion interrupts to specific CPUs; per-CPU counters naturally track which CPU is processing completions
- **Perf counters in nvperf:** `this_cpu_add(bytes_transferred, size)` tracks DMA byte counts without atomic overhead — aggregated for user-visible `nvidia-smi` statistics

---

## Cross Questions & Answers

**CQ1: What is false sharing and how does `____cacheline_aligned` prevent it?**
> False sharing occurs when two unrelated variables sit on the same CPU cache line, causing unnecessary cache invalidations. If CPU0 increments `var_a` and CPU1 increments `var_b`, but both are on the same 64-byte cache line, each increment invalidates the other CPU's cache line — even though they're logically independent. `____cacheline_aligned` pads the struct/variable to start on a cache-line boundary, ensuring each CPU's per-CPU struct occupies its own exclusive cache lines.

**CQ2: What is the difference between `this_cpu_inc` and `__this_cpu_inc`?**
> `this_cpu_inc(var)` is the safe variant — it disables preemption internally to ensure the read-modify-write is not interrupted mid-way by preemption (which would move the task to a different CPU between the read and write). `__this_cpu_inc(var)` (double underscore) is the unsafe variant — caller must guarantee preemption is already disabled (e.g., inside a `preempt_disable()` region, in IRQ context). The unsafe variant is slightly faster as it skips the preemption disable/enable overhead.

**CQ3: How do you implement a per-CPU seqlock for consistent reads without a lock?**
> Combine a per-CPU write path (fast, uncontended) with a seqlock for readers who need consistency. Writer: `write_seqcount_begin`, update per-CPU data, `write_seqcount_end`. Reader: `do { seq = read_seqcount_begin(&sc); aggregate all CPUs; } while(read_seqcount_retry(&sc, seq))`. This gives consistent reads without taking a lock on the write path, only on the rare consistent-read path.

**CQ4: What happens to per-CPU data when a CPU goes offline (hot-unplug)?**
> When a CPU goes offline, its per-CPU data remains allocated and accessible via `per_cpu(var, cpu)` — it's just not actively modified by any running thread. `for_each_possible_cpu` still includes it. When aggregating stats, offline CPUs contribute their last-written values. If the CPU comes back online, it resumes writing to the same per-CPU memory region. Drivers typically don't need to handle CPU hotplug specially for per-CPU counters.

**CQ5: For GPU bandwidth measurement, why is per-CPU better than a single `atomic64_t`?**
> GPU bandwidth measurement calls `bytes_transferred += dma_size` on every DMA transfer. On a 64-CPU system with CUDA multi-stream (many CPUs submitting concurrently), a single `atomic64_add` creates a memory bus bottleneck — all 64 CPUs contend for the same atomic variable. Per-CPU: each CPU's `bytes_transferred` update is purely local (L1 cache hit, ~1 cycle). Aggregation for `nvidia-smi` polling (once per second) costs only 64 L3 cache reads — amortized over millions of DMA transfers, per-CPU is ~100x faster.

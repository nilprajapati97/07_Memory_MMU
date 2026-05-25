# Q23: Seqlock vs RCU for GPU Clock Synchronization

**Section:** Concurrency & Synchronization | **Difficulty:** Hard | **Topics:** `seqlock_t`, `read_seqbegin/retry`, `write_seqlock`, RCU, read-heavy workloads, timestamp synchronization

---

## Question

Implement a seqlock for GPU clock synchronization and compare with RCU read-side performance.

---

## Answer

```c
#include <linux/seqlock.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

/* ─── Seqlock: GPU clock base synchronization ─────────────────────────────
 * GPU and CPU clocks must be synchronized for profiling timestamps.
 * Writer: rare (system time update, GPU clock calibration)
 * Readers: extremely frequent (every GPU event must be timestamped)
 */
struct gpu_clock {
    seqlock_t   lock;
    u64         clock_base;     /* GPU clock base in nanoseconds */
    u32         clock_mult;     /* frequency multiplier          */
    u32         clock_shift;    /* frequency shift               */
};

static struct gpu_clock gpu_clk;

void gpu_clock_init(void)
{
    seqlock_init(&gpu_clk.lock);
    gpu_clk.clock_base  = ktime_get_ns();
    gpu_clk.clock_mult  = 1000000; /* example values */
    gpu_clk.clock_shift = 20;
}

/* ─── Writer: rare clock recalibration ────────────────────────────────────
 * write_seqlock increments the sequence number (makes it odd = update in progress).
 * write_sequnlock increments again (makes it even = update complete).
 */
void gpu_clock_update(u64 new_base, u32 new_mult, u32 new_shift)
{
    write_seqlock(&gpu_clk.lock);
    /* Between seqlock acquire and release, seq is ODD — readers retry */
    gpu_clk.clock_base  = new_base;
    gpu_clk.clock_mult  = new_mult;
    gpu_clk.clock_shift = new_shift;
    write_sequnlock(&gpu_clk.lock);
}

/* ─── Reader: frequent timestamp conversion ───────────────────────────────
 * Readers never block writers (no lock acquisition).
 * If a writer modifies data while we read, read_seqretry returns true
 * and we retry — ensures we never see a torn (inconsistent) read.
 *
 * No locks held, no cache line bouncing. O(1) with rare retries.
 */
u64 gpu_clock_read(u64 gpu_ticks)
{
    u64 base, result;
    u32 mult, shift;
    unsigned int seq;

    do {
        /*
         * read_seqbegin: reads current sequence number.
         * If odd (writer active), spin here until even.
         * Returns the even sequence number.
         */
        seq = read_seqbegin(&gpu_clk.lock);

        /* Read the protected data */
        base  = gpu_clk.clock_base;
        mult  = gpu_clk.clock_mult;
        shift = gpu_clk.clock_shift;

        /*
         * read_seqretry: returns true if sequence number changed
         * since read_seqbegin (i.e., a writer updated between our reads).
         * If true, we retry the entire read.
         */
    } while (read_seqretry(&gpu_clk.lock, seq));

    /* Safe to use base/mult/shift now — no concurrent write observed */
    result = base + ((gpu_ticks * mult) >> shift);
    return result;
}

/* ─── Seqcount variant (for protected-by-other-lock writers) ─────────────*/
struct gpu_stats_snapshot {
    seqcount_spinlock_t seq;
    spinlock_t          lock;
    u64                 last_submit_ns;
    u64                 last_complete_ns;
    u64                 avg_latency_ns;
};

static struct gpu_stats_snapshot gpu_snap;

void gpu_stats_update(u64 submit_ns, u64 complete_ns)
{
    spin_lock(&gpu_snap.lock);
    write_seqcount_begin(&gpu_snap.seq);  /* mark update start */
    gpu_snap.last_submit_ns   = submit_ns;
    gpu_snap.last_complete_ns = complete_ns;
    gpu_snap.avg_latency_ns   = (complete_ns - submit_ns);
    write_seqcount_end(&gpu_snap.seq);    /* mark update end */
    spin_unlock(&gpu_snap.lock);
}

u64 gpu_stats_read_latency(void)
{
    u64 lat;
    unsigned int seq;

    do {
        seq = read_seqcount_begin(&gpu_snap.seq);
        lat = gpu_snap.avg_latency_ns;
    } while (read_seqcount_retry(&gpu_snap.seq, seq));

    return lat;
}

/* ─── RCU alternative for pointer-sized protected data ───────────────────
 * When the "data" is a pointer to a heap-allocated structure (not scalars),
 * RCU is more natural than seqlock.
 */
struct gpu_config {
    u32 max_freq_mhz;
    u32 power_limit_w;
    u32 fan_speed_pct;
    struct rcu_head rcu;
};

static struct gpu_config __rcu *current_config;

/* RCU reader — zero overhead if no writer active */
u32 gpu_get_max_freq(void)
{
    struct gpu_config *cfg;
    u32 freq;

    rcu_read_lock();
    cfg  = rcu_dereference(current_config);
    freq = cfg ? cfg->max_freq_mhz : 0;
    rcu_read_unlock();

    return freq;
}

/* RCU writer — allocates new config, publishes atomically */
int gpu_set_config(u32 freq, u32 power, u32 fan)
{
    struct gpu_config *new_cfg, *old_cfg;

    new_cfg = kmalloc(sizeof(*new_cfg), GFP_KERNEL);
    if (!new_cfg)
        return -ENOMEM;

    new_cfg->max_freq_mhz  = freq;
    new_cfg->power_limit_w = power;
    new_cfg->fan_speed_pct = fan;

    old_cfg = rcu_replace_pointer(current_config, new_cfg,
                                   lockdep_is_held(&some_writer_mutex));
    if (old_cfg)
        kfree_rcu(old_cfg, rcu);

    return 0;
}
```

### Seqlock vs RCU Comparison

| Property | Seqlock | RCU |
|----------|---------|-----|
| Reader cost | ~2 reads (seq check) | ~0 (just a barrier) |
| Writer cost | 2 seq increments | Grace period wait (ms) |
| Reader blocks writer | Never | Never |
| Writer blocks reader | Causes retry | Never |
| Data type | Scalar (int, u64) | Pointers to structs |
| Reader retry needed | Yes (on write collision) | No |
| Memory overhead | None | Old pointer until grace period |

---

## Explanation

### Core Concept

A seqlock solves the read-write synchronization problem where:
- Reads are **extremely frequent** (timestamp conversion per GPU event)
- Writes are **rare** (clock calibration, config changes)
- Data is **small scalars** (not pointers)

```
Seqlock state machine:
  seq=0 (even)  → no write in progress → readers proceed
  seq=1 (odd)   → write in progress    → readers spin then retry
  seq=2 (even)  → write complete       → readers proceed with new data
  seq=3 (odd)   → next write           → readers retry again
```

**Why not a rwlock?** `rwlock` readers increment a shared counter — still causes cache line bouncing. Seqlock readers touch NO shared state (just read the sequence number) — pure reads, zero contention.

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `seqlock_init(sl)` | Initialize seqlock |
| `write_seqlock(sl)` | Writer acquires (spinlock + seq odd) |
| `write_sequnlock(sl)` | Writer releases (seq even) |
| `read_seqbegin(sl)` | Reader reads seq (spins if odd) |
| `read_seqretry(sl, seq)` | Returns true if seq changed → retry |
| `write_seqlock_irqsave(sl, flags)` | IRQ-safe writer lock |
| `seqcount_spinlock_t` | Seqcount embedded in a spinlock-protected struct |
| `write_seqcount_begin/end(sc)` | Seqcount update markers |
| `read_seqcount_begin/retry(sc, seq)` | Seqcount reader path |

### Trade-offs & Pitfalls

- **Seqlock readers can starve.** If a writer holds the lock continuously (malicious or buggy), readers retry forever. In practice, writers are rare and brief — this is not an issue.
- **Cannot protect pointer-sized data that requires deref.** If the seqlock-protected data contains a pointer that is dereferenced inside the `do/while` loop, the pointer might be freed by the writer between reading and dereferencing, causing a UAF. Use RCU for pointer-protected data.
- **Seqcount vs seqlock.** Seqcount embeds in an existing lock structure. Use `seqcount_spinlock_t` when the writer is already protected by a spinlock — prevents double-spinlock acquisition.

### NVIDIA / GPU Context

- **GPU clock synchronization:** GPU perf timestamps require converting GPU cycle counts to nanoseconds using a calibrated (mult, shift, base) triple — updated rarely but read on every GPU event (hundreds of thousands per second) — perfect seqlock use case
- **GPU thermal throttle table:** current freq/power limits read by every dispatch to check headroom, updated by thermal kthread — seqlock protected
- **RCU for GPU driver config:** kernel module parameters (ECC mode, persistence mode, power cap) are pointer-to-struct, updated by `nvidia-smi`, read by CUDA runtime — RCU natural fit

---

## Cross Questions & Answers

**CQ1: Can a seqlock-protected read section call `kmalloc` or `copy_to_user`?**
> No. The seqlock read section (between `read_seqbegin` and `read_seqretry`) must be non-blocking and non-sleeping. Calling `kmalloc(GFP_KERNEL)` or `copy_to_user` inside the loop would be incorrect because: (1) the loop retries on concurrent write, meaning work is repeated — side effects must be idempotent; (2) sleeping inside the read section is unexpected. Keep the seqlock read section limited to plain memory reads of scalar values.

**CQ2: How does seqlock handle the case where a reader reads data that is being modified word-by-word?**
> On architectures where reading a 64-bit value is not atomic (e.g., 32-bit ARM reading `u64`), a writer could update the high 32 bits of `clock_base` while the reader reads the low 32 bits — a torn read. However, `read_seqretry` detects this: if any write occurred between `read_seqbegin` and `read_seqretry`, the sequence number changed and the reader retries. The reader never acts on a torn value — it retries until it gets a clean snapshot.

**CQ3: Why is RCU read overhead described as "nearly zero"?**
> `rcu_read_lock()` on non-PREEMPT kernels is just `preempt_disable()` (a flag write to the task struct's `preempt_count`). On PREEMPT kernels, it's a bit more complex but still involves no cache-line-shared data. The `rcu_dereference()` is just `READ_ONCE(ptr)` plus a `data_read_depends` barrier. There is no shared counter, no lock acquisition, no bus transaction — making RCU read-side essentially free compared to any lock-based synchronization.

**CQ4: What is the "big reader" RCU (`SRCU`) and when is it needed?**
> `srcu` (Sleepable RCU) allows RCU read-side critical sections to sleep (`schedule()`, `mutex_lock()`, etc.). Regular RCU requires read sections to be non-preemptible. SRCU uses a per-CPU completion counter array so sleeping is safe. GPU drivers use SRCU when the "reader" is a CUDA kernel launch that takes arbitrary time — the SRCU grace period waits for all ongoing kernel launches to finish before the protected pointer can be freed. Cost: higher overhead than regular RCU.

**CQ5: When should you choose seqlock over RCU for GPU driver synchronization?**
> Choose seqlock when: (1) data is scalar (integers, structs with no pointers), (2) data must be read atomically as a consistent snapshot (e.g., (base, mult, shift) triple), (3) you need the update to be immediately visible to readers (no grace period delay). Choose RCU when: (1) data is a pointer to a heap-allocated structure, (2) the old data can be safely freed after a grace period, (3) reader code complexity makes retry loops impractical, (4) read sections might sleep (use SRCU).

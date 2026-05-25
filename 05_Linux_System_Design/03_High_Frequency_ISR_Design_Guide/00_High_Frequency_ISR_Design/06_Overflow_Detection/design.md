# 06 — Overflow Detection and Monitoring

## Why Monitoring Matters

> "A system that silently drops data is worse than one that crashes — it continues running and appears healthy while producing incorrect output."

Overflow detection gives you:
1. **Immediate diagnosis**: know exactly when data loss started
2. **Buffer sizing**: peak fill level tells you how close to the edge you are
3. **Worker performance**: if overflow_count > 0, worker is too slow
4. **System health reporting**: telemetry, debug logs, fault LEDs

---

## What Overflow Means

Overflow occurs when the **ISR produces faster than the worker consumes** — the buffer fills and the ISR has nowhere to put new data.

```
Causes:
  ① Buffer too small:
        min_size = rate × worst_case_latency
        100,000/sec × 1ms = 100 entries minimum
        Use 4096 (40× margin)

  ② Worker too slow:
        Worker processing algorithm is too expensive for the available time budget
        Fix: optimize, use SIMD/DSP, move work to DMA, reduce sample rate

  ③ Interrupt rate higher than expected:
        Clock source drift, misconfigured timer/prescaler
        Fix: verify hardware configuration, add rate measurement ISR
```

---

## The `isr_stats_t` Structure

```c
typedef struct {
    volatile uint32_t overflow_count;    /* ← non-zero means DATA LOSS        */
    volatile uint32_t total_received;    /* total ISR events (with overflow)   */
    volatile uint32_t total_processed;   /* total worker consumes              */
    volatile uint32_t peak_fill_level;   /* max buffer occupancy ever seen     */
    volatile uint32_t total_resets;      /* measurement window changes         */
} isr_stats_t;
```

### Derived metrics

```
Drop rate (PPM) = (overflow_count / total_received) × 1,000,000

Lost samples = total_received - total_processed

Buffer headroom (%) = 100 × (1 - peak_fill_level / RING_BUF_SIZE)
```

---

## Integration Pattern

```c
/* In ring buffer push: */
bool rb_push_from_isr(ring_buf_t *rb, rb_data_t data, isr_stats_t *stats)
{
    uint32_t next = (rb->head + 1u) & RING_BUF_MASK;

    if (next == rb->tail) {
        stats_on_overflow(stats);           /* ← count the drop */
        return false;
    }

    rb->buf[rb->head] = data;
    __DMB();
    rb->head = next;

    stats_on_push(stats, rb_used(rb));     /* ← count the push + track fill */
    return true;
}

/* In worker task: */
void worker_task(void)
{
    rb_data_t data;
    while (rb_pop(&g_rb, &data)) {
        process(data);
        stats_on_pop(&g_stats);             /* ← count the pop */
    }

    /* Periodic health check (every 1 second): */
    if (get_tick_ms() - last_report_ms >= 1000u) {
        stats_report(&g_stats);
        stats_reset(&g_stats);              /* ← new measurement window */
        last_report_ms = get_tick_ms();
    }
}
```

---

## Buffer Sizing Formula (Detailed)

```
Required buffer size = interrupt_rate × worst_case_worker_latency

Factors to consider for worst_case_worker_latency:
  - RTOS scheduler latency (time from sem_post to task running): typically 10–50 µs
  - Task blocking on other resources (I2C, SPI, logging): 0–10 ms
  - ISR disabled period (inside safe_read_u64 or critical sections): 1–10 µs
  - Cache miss penalties on first buffer access: 1–10 µs
  - Peak processing time (worst-case input data): algorithm-dependent

Example calculation:
  interrupt_rate          = 100,000/sec
  RTOS scheduler latency  = 50 µs
  max processing time     = 2 ms (FIR filter over 512 samples)
  worst_case_latency      = 2.05 ms = 0.00205 sec

  min_entries = 100,000 × 0.00205 = 205
  With 4× safety margin: 820
  Round up to power-of-2: 1024

  Recommended: 1024 entries = 4 KB (uint32_t)
```

### Peak fill level interpretation

| Peak fill % | Action |
|---|---|
| 0–25% | Healthy — plenty of headroom |
| 25–50% | Normal — monitor but no action needed |
| 50–75% | Warning — consider buffer increase or worker optimization |
| 75–90% | Critical — overflow likely under load spikes |
| > 90% | Imminent overflow — must increase buffer or reduce rate |

---

## Atomic Considerations on Multi-Core

On single-core (Cortex-M): 32-bit aligned writes are atomic (single STR instruction). The `volatile` qualifier ensures the write isn't optimized away.

On multi-core (Cortex-A, x86, Qualcomm Kryo, NVIDIA Denver):

```c
/* Multi-core safe version of stats_on_overflow(): */
#include <stdatomic.h>

typedef struct {
    _Atomic uint32_t overflow_count;
    _Atomic uint32_t total_received;
    _Atomic uint32_t total_processed;
    _Atomic uint32_t peak_fill_level;
} isr_stats_atomic_t;

void stats_on_overflow_atomic(isr_stats_atomic_t *s)
{
    /* memory_order_relaxed: no ordering needed — just counting */
    atomic_fetch_add_explicit(&s->overflow_count, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(&s->total_received, 1u, memory_order_relaxed);
}
```

---

## Example Health Report Output

```
=== ISR Health Report ===
Total received   : 100000
Total processed  : 100000
Overflow (drops) : 0
Drop rate        : 0 ppm
Peak fill level  : 47        ← used at most 47/4096 = 1.1% of buffer
Status           : HEALTHY

=== After doubling interrupt rate ===
Total received   : 200000
Total processed  : 199437
Overflow (drops) : 563
Drop rate        : 2815 ppm  ← 0.28% drop rate — PROBLEM
Peak fill level  : 4094      ← buffer was nearly full!
Status           : OVERFLOW DETECTED — action needed
```

---

## Common Interview Questions

**Q: overflow_count is volatile uint32_t. Is `overflow_count++` atomic?**  
A: On single-core ARM Cortex-M: yes, if this ISR is the ONLY writer of `overflow_count`. A 32-bit aligned increment (LDR + ADD + STR) cannot be interrupted mid-instruction on Cortex-M because the ISR runs with a fixed priority and cannot be preempted by another ISR of the same/lower priority. On multi-core or if two ISRs both write: use `atomic_fetch_add()`.

**Q: total_received != total_processed — does this mean data is lost?**  
A: If `overflow_count > 0`: yes, exactly `overflow_count` samples are lost. If `overflow_count == 0` but the counts differ: the worker is still processing (lagging behind). The difference `(total_received - total_processed)` is the current queue depth, not loss.

**Q: How do you monitor overflow in production without printf?**  
A: Three techniques: (a) Write `overflow_count` to a dedicated hardware counter register or set a GPIO fault pin that a monitoring system detects. (b) Write to a non-volatile memory location (SRAM2 on STM32) that survives a watchdog reset — on reboot you can inspect the last run's stats. (c) Expose via a debug register read by a JTAG probe or RTT (SEGGER Real-Time Transfer) without stopping the CPU.

**Q: How would you reduce the drop rate from 2815 ppm to 0?**  
A: Root cause: worker too slow relative to interrupt rate. Options in order: (1) Increase buffer size (4096 → 8192) — absorbs more burst. (2) Optimize worker algorithm (SIMD, LUT, reduce complexity). (3) Switch to DMA (reduces ISR overhead). (4) Reduce sample rate. (5) Move to a faster CPU. Measure peak_fill_level after each change to verify improvement.

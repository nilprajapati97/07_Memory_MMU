/*
 * overflow_monitor.h — ISR Statistics and Overflow Health Monitoring
 *
 * Guide §6: Never silently drop data.
 *
 * This module answers:
 *   - How many samples were dropped (overflow_count > 0 = problem)?
 *   - What fraction were dropped (drop rate in PPM)?
 *   - What was the peak buffer fill level (tells you headroom)?
 *   - Is total_received == total_processed (no silent loss)?
 *
 * Buffer sizing formula:
 *   min_entries = interrupt_rate × worst_case_worker_latency_sec
 *   Example: 100,000/sec × 0.001 sec = 100 entries minimum
 *   Recommended: add 4× safety margin → 400 → round up to 512 (power-of-2)
 *
 * If overflow_count > 0, either:
 *   a) Buffer too small  → increase RING_BUF_SIZE / DB_BUF_LEN
 *   b) Worker too slow   → optimize processing algorithm
 *   c) ISR rate higher   → reduce sample rate or switch to DMA
 */

#ifndef OVERFLOW_MONITOR_H
#define OVERFLOW_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

/* ---- Statistics structure ------------------------------------------------ */

typedef struct {
    volatile uint32_t overflow_count;   /* data dropped (loss)                */
    volatile uint32_t total_received;   /* total ISR firings                  */
    volatile uint32_t total_processed;  /* total worker consumes              */
    volatile uint32_t peak_fill_level;  /* highest observed buffer occupancy  */
    volatile uint32_t total_resets;     /* times stats were cleared           */
} isr_stats_t;

/* ---- API ----------------------------------------------------------------- */

/** stats_init() — Zero all counters (preserves total_resets). */
void stats_init(isr_stats_t *s);

/**
 * stats_on_push() — Call from ISR when a sample is successfully stored.
 * @param fill_level  Current buffer occupancy (for peak tracking).
 */
void stats_on_push(isr_stats_t *s, uint32_t fill_level);

/** stats_on_overflow() — Call from ISR when buffer full and data dropped. */
void stats_on_overflow(isr_stats_t *s);

/** stats_on_pop() — Call from worker when a sample is consumed. */
void stats_on_pop(isr_stats_t *s);

/** stats_is_healthy() — Returns true if overflow_count == 0. */
bool stats_is_healthy(const isr_stats_t *s);

/** stats_report() — Print formatted health report to stdout. */
void stats_report(const isr_stats_t *s);

/** stats_reset() — Clear all counters for a new measurement window. */
void stats_reset(isr_stats_t *s);

#endif /* OVERFLOW_MONITOR_H */

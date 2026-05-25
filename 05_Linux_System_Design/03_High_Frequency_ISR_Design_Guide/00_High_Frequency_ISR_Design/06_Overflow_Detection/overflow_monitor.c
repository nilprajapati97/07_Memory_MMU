/*
 * overflow_monitor.c — ISR Statistics and Health Monitoring
 */

#include "overflow_monitor.h"
#include <stdio.h>

void stats_init(isr_stats_t *s)
{
    s->overflow_count  = 0u;
    s->total_received  = 0u;
    s->total_processed = 0u;
    s->peak_fill_level = 0u;
    /* total_resets intentionally NOT cleared — accumulates across init calls */
}

void stats_on_push(isr_stats_t *s, uint32_t fill_level)
{
    /*
     * Called from ISR context — must complete in < 5 cycles.
     *
     * 32-bit aligned increment (total_received++) is atomic on ARM Cortex-M.
     * Single STR instruction — no race between ISR and task for this field.
     *
     * On multi-core (Cortex-A, Qualcomm Kryo, NVIDIA Denver):
     *   Use atomic_fetch_add(&s->total_received, 1, memory_order_relaxed)
     *   to prevent races when multiple cores can fire IRQs simultaneously.
     */
    s->total_received++;

    /* Track peak fill level (non-atomic max — benign race: worst case we
     * miss one peak sample, not a correctness issue for monitoring) */
    if (fill_level > s->peak_fill_level) {
        s->peak_fill_level = fill_level;
    }
}

void stats_on_overflow(isr_stats_t *s)
{
    s->overflow_count++;
    s->total_received++;    /* Count arrival even though we dropped the data */
}

void stats_on_pop(isr_stats_t *s)
{
    /* Called from worker task context — no ISR access here */
    s->total_processed++;
}

bool stats_is_healthy(const isr_stats_t *s)
{
    return s->overflow_count == 0u;
}

void stats_report(const isr_stats_t *s)
{
    uint32_t drop_rate_ppm = 0u;

    if (s->total_received > 0u) {
        /* Parts-per-million: (dropped / total) × 1,000,000
         * Use 64-bit intermediate to avoid overflow when overflow_count is large */
        drop_rate_ppm = (uint32_t)(
            ((uint64_t)s->overflow_count * 1000000ULL) / s->total_received
        );
    }

    printf("=== ISR Health Report ===\n");
    printf("Total received   : %u\n",     s->total_received);
    printf("Total processed  : %u\n",     s->total_processed);
    printf("Overflow (drops) : %u\n",     s->overflow_count);
    printf("Drop rate        : %u ppm\n", drop_rate_ppm);
    printf("Peak fill level  : %u\n",     s->peak_fill_level);
    printf("Status           : %s\n",
           stats_is_healthy(s) ? "HEALTHY" : "OVERFLOW DETECTED — action needed");
}

void stats_reset(isr_stats_t *s)
{
    s->overflow_count  = 0u;
    s->total_received  = 0u;
    s->total_processed = 0u;
    s->peak_fill_level = 0u;
    s->total_resets++;
}

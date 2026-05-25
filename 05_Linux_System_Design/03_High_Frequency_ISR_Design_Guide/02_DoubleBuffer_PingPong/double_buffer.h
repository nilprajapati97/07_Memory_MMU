#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

/*
 * double_buffer.h — Ping-Pong (Double Buffer) for ISR Data Capture
 *
 * Concept:
 *   Two equal-sized buffers A (index 0) and B (index 1).
 *   ISR fills whichever is "active". When the active buffer fills up:
 *     1. ISR marks it "ready" for the worker.
 *     2. ISR atomically swaps to the OTHER buffer and resets the write index.
 *     3. Worker processes the full buffer independently.
 *
 * Key guarantee:
 *   After the swap, ISR writes ONLY to the new active buffer.
 *   Worker reads ONLY from the ready buffer (the previously active one).
 *   → No simultaneous read/write to the same buffer. Ever.
 *   → Worker gets a direct pointer — zero-copy.
 *
 * When to prefer over SPSC ring buffer:
 *   - Data is naturally block-structured (ADC frame, SPI packet)
 *   - Processing is more efficient in bulk (DSP algorithms, memcpy, DMA)
 *   - Zero-copy is critical (large buffers, high throughput)
 *   - Sample latency up to N/2 samples is acceptable
 *
 * Limitation vs ring buffer:
 *   - Higher latency: worker gets data only after N samples (not immediately)
 *   - If worker is still processing when ISR fills next buffer → overflow
 *     (only 2 buffers; ring buffer has 4096 slots of headroom)
 */

#include <stdint.h>
#include <stdbool.h>

/* ---- Configuration -------------------------------------------------------- */

#define DB_HALF_SIZE    512u        /* samples per single buffer              */
#define DB_NUM_BUFS     2u          /* always 2 for ping-pong                 */

_Static_assert(DB_NUM_BUFS == 2u, "Ping-pong uses exactly 2 buffers");
_Static_assert(DB_HALF_SIZE >= 1u, "Buffer must hold at least 1 sample");

/* ---- Types ---------------------------------------------------------------- */

typedef uint32_t db_data_t;

typedef struct {
    /*
     * Two equal buffers. ISR fills one, worker processes the other.
     * Placed first for natural alignment (large array).
     */
    db_data_t        buf[DB_NUM_BUFS][DB_HALF_SIZE];

    /*
     * active: which buffer index (0 or 1) ISR is currently writing to.
     * Written ONLY by ISR. Worker reads it to know which one NOT to touch.
     */
    volatile uint8_t  active;

    /*
     * write_pos: current fill position in the active buffer.
     * Written ONLY by ISR. Never accessed by worker.
     */
    volatile uint32_t write_pos;

    /*
     * ready: 1 = a full buffer is available for worker.
     * Written by ISR (set to 1 on buffer-full), worker (set to 0 on release).
     * The only shared-write variable — protected by ISR atomicity on Cortex-M.
     */
    volatile uint8_t  ready;

    /*
     * ready_buf: index of the buffer that is "ready".
     * Written by ISR before setting ready=1.
     * Read by worker after seeing ready=1.
     * ISR sets this BEFORE ready (MEM_BARRIER ensures ordering).
     */
    volatile uint8_t  ready_buf;

    /* Stats */
    volatile uint32_t overflow_cnt;     /* ISR dropped buffers (worker too slow) */
    volatile uint32_t total_handoffs;   /* successful buffer handoffs to worker  */
} double_buf_t;

/* ---- API ------------------------------------------------------------------ */

void             db_init(double_buf_t *db);

/*
 * db_push_from_isr() — write one sample; returns true on successful handoff
 * (i.e., buffer just became full and was handed to worker).
 * Returns false if overflow (worker hasn't consumed previous buffer yet).
 * MUST be called ONLY from ISR context (single producer).
 */
bool             db_push_from_isr(double_buf_t *db, db_data_t data);

/*
 * db_get_ready() — get pointer to full buffer ready for processing.
 * Returns NULL if no buffer is ready. Sets *count to DB_HALF_SIZE.
 * MUST be called ONLY from worker context (single consumer).
 * Caller MUST call db_release() after processing.
 */
const db_data_t *db_get_ready(double_buf_t *db, uint32_t *count);

/*
 * db_release() — release the processed buffer back to ISR.
 * MUST be called after db_get_ready() returns non-NULL.
 */
void             db_release(double_buf_t *db);

bool             db_is_ready(const double_buf_t *db);

#endif /* DOUBLE_BUFFER_H */

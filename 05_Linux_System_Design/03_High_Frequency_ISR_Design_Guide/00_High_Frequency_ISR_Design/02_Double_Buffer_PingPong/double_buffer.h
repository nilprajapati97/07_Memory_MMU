/*
 * double_buffer.h — Ping-Pong Double Buffer for burst/block ISR data
 *
 * Design: Two fixed-size buffers (ping / pong).
 *   ISR always fills the "active" buffer.
 *   When active buffer is full → swap active/inactive → signal worker.
 *   Worker processes the completed buffer while ISR fills the new active one.
 *
 * Advantage over ring buffer:
 *   Worker wakes only when a full block (N samples) is ready.
 *   At 100 kHz with N=256: worker wakes ~390 times/sec vs 100,000 times/sec.
 *   → 256× fewer wake-ups, zero per-sample worker overhead.
 *
 * Constraint:
 *   Worker MUST finish before the next buffer fills.
 *   Deadline = DB_BUF_LEN / interrupt_rate = 256 / 100,000 = 2.56 ms.
 */

#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---- Configuration ------------------------------------------------------- */

#define DB_BUF_LEN   256u           /* samples per single buffer              */
#define DB_NUM_BUFS  2u

typedef uint32_t db_data_t;

/* ---- Double buffer state ------------------------------------------------- */

typedef struct {
    db_data_t        buf[DB_NUM_BUFS][DB_BUF_LEN];
    volatile uint8_t  active;           /* ISR writes to buf[active]          */
    volatile uint32_t idx;              /* next write position in active buf  */
    volatile uint8_t  ready;            /* 1 = a complete buffer awaits worker*/
    volatile uint8_t  ready_buf;        /* which buffer index is ready        */
    volatile uint32_t overflow_cnt;     /* worker too slow — buffers dropped  */
    volatile uint32_t total_handoffs;   /* total completed buffer swaps       */
} double_buf_t;

/* ---- API ----------------------------------------------------------------- */

void db_init(double_buf_t *db);

/**
 * db_push_from_isr() — Store one sample. Call from ISR context only.
 * @return true when a complete buffer just became ready (worker should run).
 *         false when buffer not yet full (fast path, ~99.6% of calls).
 */
bool db_push_from_isr(double_buf_t *db, db_data_t data);

/**
 * db_get_ready() — Get pointer to the ready buffer. Worker context only.
 * @param count  Set to number of samples in the buffer (DB_BUF_LEN).
 * @return Pointer to buffer data, or NULL if no buffer is ready.
 */
const db_data_t *db_get_ready(double_buf_t *db, uint32_t *count);

/**
 * db_release() — Release the ready buffer after worker is done.
 * MUST be called after processing — allows ISR to reuse the slot.
 */
void db_release(double_buf_t *db);

#endif /* DOUBLE_BUFFER_H */

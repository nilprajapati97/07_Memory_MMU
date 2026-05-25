/*
 * double_buffer.c — Ping-Pong Double Buffer Implementation
 *
 * Buffer swap protocol:
 *
 *   ISR fast path (idx < DB_BUF_LEN):
 *     buf[active][idx++] = data   →  ~3 cycles, no signal
 *
 *   ISR swap path (idx == DB_BUF_LEN):
 *     1. Check worker still busy (overflow path)
 *     2. Record which buffer just filled: ready_buf = active
 *     3. Swap:  active ^= 1   (XOR swap: 0→1, 1→0)
 *     4. Reset: idx = 0
 *     5. DMB (RELEASE) — data in buf[ready_buf] visible before ready=1
 *     6. ready = 1            — wake worker
 *
 *   Worker:
 *     1. Poll ready flag
 *     2. DMB (ACQUIRE)
 *     3. process buf[ready_buf]
 *     4. db_release() → ready = 0
 */

#include "double_buffer.h"
#include <stddef.h>

#if defined(__ARM_ARCH)
    #define MEM_BARRIER() __asm__ volatile ("dmb sy" ::: "memory")
#else
    #define MEM_BARRIER() __asm__ volatile ("" ::: "memory")
#endif

void db_init(double_buf_t *db)
{
    db->active         = 0u;
    db->idx            = 0u;
    db->ready          = 0u;
    db->ready_buf      = 0u;
    db->overflow_cnt   = 0u;
    db->total_handoffs = 0u;
}

bool db_push_from_isr(double_buf_t *db, db_data_t data)
{
    /* FAST PATH: store sample and return — called ~99.6% of the time */
    db->buf[db->active][db->idx] = data;
    db->idx++;

    if (db->idx < DB_BUF_LEN) {
        return false;
    }

    /* SLOW PATH: active buffer just filled — swap every DB_BUF_LEN samples */

    if (db->ready) {
        /* Worker hasn't released the previous buffer yet.
         * Options: (a) drop this buffer  (b) overwrite oldest  (c) assert
         * Here: reset write pointer and discard (simplest for embedded). */
        db->overflow_cnt++;
        db->idx = 0u;
        return false;
    }

    /* Record which buffer filled, then swap active to the other one */
    uint8_t filled = db->active;
    db->active     = db->active ^ 1u;   /* 0→1 or 1→0 */
    db->idx        = 0u;

    db->total_handoffs++;

    /* RELEASE: all writes to buf[filled] must be visible BEFORE ready=1.
     * The worker reads buf[ready_buf] after seeing ready=1 — without this
     * barrier, the CPU could reorder ready=1 before the buf writes. */
    MEM_BARRIER();

    db->ready_buf = filled;
    db->ready     = 1u;

    return true;
}

const db_data_t *db_get_ready(double_buf_t *db, uint32_t *count)
{
    if (!db->ready) {
        *count = 0u;
        return NULL;
    }

    /* ACQUIRE: pairs with RELEASE in db_push_from_isr.
     * Guarantees we see all data written to buf[ready_buf]. */
    MEM_BARRIER();

    *count = DB_BUF_LEN;
    return db->buf[db->ready_buf];
}

void db_release(double_buf_t *db)
{
    /* RELEASE: write processing results before clearing the flag */
    MEM_BARRIER();
    db->ready = 0u;
}

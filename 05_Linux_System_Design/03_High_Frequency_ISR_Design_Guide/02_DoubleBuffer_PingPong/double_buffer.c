/*
 * double_buffer.c — Ping-Pong Double Buffer Implementation
 *
 * Memory barrier strategy:
 *   Same as ring_buffer.c: DMB on ARM, compiler fence on x86.
 *
 * Critical ordering requirements:
 *
 * ISR (producer) — on buffer full:
 *   1. All buf[active][0..N-1] writes must be visible
 *   2. BEFORE ready_buf is written
 *   3. BEFORE ready = 1 is written (the "publication" store)
 *
 * Worker (consumer) — on buffer ready:
 *   1. Acquire barrier AFTER reading ready = 1
 *   2. Guarantees all ISR writes to buf[] are visible
 *   3. AFTER done processing: release barrier before ready = 0
 */

#include "double_buffer.h"
#include <stddef.h>        /* NULL */

#if defined(__ARM_ARCH)
    #define MEM_BARRIER_RELEASE() __asm__ __volatile__("dmb sy" ::: "memory")
    #define MEM_BARRIER_ACQUIRE() __asm__ __volatile__("dmb sy" ::: "memory")
#else
    #define MEM_BARRIER_RELEASE() __asm__ __volatile__("" ::: "memory")
    #define MEM_BARRIER_ACQUIRE() __asm__ __volatile__("" ::: "memory")
#endif

/* ---- Init ----------------------------------------------------------------- */

void db_init(double_buf_t *db)
{
    db->active        = 0u;
    db->write_pos     = 0u;
    db->ready         = 0u;
    db->ready_buf     = 0u;
    db->overflow_cnt  = 0u;
    db->total_handoffs = 0u;
}

/* ---- Producer (ISR) ------------------------------------------------------- */

/*
 * db_push_from_isr() — single-sample push
 *
 * Fast path (most calls): just write one sample and increment write_pos.
 * Slow path (every DB_HALF_SIZE calls): buffer-full → try to hand off.
 *
 * ISR execution time:
 *   Fast path: ~5 cycles (store + increment + compare + branch) = ~30 ns @ 168 MHz
 *   Slow path: ~30 cycles (DMB + assignments + XOR swap) = ~180 ns @ 168 MHz
 *
 * Slow path frequency: 100,000 / 512 = ~195 times/sec (once per full buffer)
 * Fast path frequency: ~99,805 times/sec
 *
 * ISR average overhead: mostly fast path — excellent for high-frequency use.
 *
 * Safety analysis:
 *   - `active` is written only by ISR → no race on active index
 *   - `write_pos` is written only by ISR → no race on write position
 *   - `ready` is shared: ISR sets to 1, worker sets to 0
 *     On Cortex-M (single core): ISR is atomic relative to worker
 *     (ISR cannot be preempted by worker; worker cannot run while ISR runs)
 *     On multi-core: need atomic store to ready (see notes below)
 *   - buf[active] and buf[ready_buf] are DIFFERENT buffers after swap
 *     → no simultaneous access to the same buffer
 */
bool db_push_from_isr(double_buf_t *db, db_data_t data)
{
    /* ---- Fast path: write sample to current active buffer ---- */
    db->buf[db->active][db->write_pos] = data;
    db->write_pos++;

    if (db->write_pos < DB_HALF_SIZE) {
        return false;  /* Buffer not yet full; nothing to hand off */
    }

    /* ---- Slow path: buffer full, attempt handoff to worker ---- */

    if (db->ready != 0u) {
        /*
         * Worker hasn't released the previous buffer yet.
         * We CANNOT overwrite the ready buffer (worker is reading it).
         * Two choices:
         *   A) Drop this entire buffer (overflow) and keep writing to same slot
         *   B) Wait (NOT allowed in ISR)
         * We choose A: reset write_pos and overwrite the active buffer from scratch.
         * The current active buffer's content is discarded.
         */
        db->overflow_cnt++;
        db->write_pos = 0u;
        return false;
    }

    /*
     * RELEASE barrier:
     * All writes to buf[active][0..DB_HALF_SIZE-1] above must be
     * globally visible BEFORE ready_buf and ready are updated.
     * Without this barrier: worker could observe ready=1 but see
     * stale data in buf[ready_buf] (still in ISR's store buffer).
     */
    MEM_BARRIER_RELEASE();

    /* Publish: tell worker which buffer is ready */
    db->ready_buf = db->active;
    db->ready     = 1u;            /* "Publication" store */

    /* Swap to the other buffer for continued ISR writes */
    db->active    = db->active ^ 1u;   /* 0→1 or 1→0, single instruction */
    db->write_pos = 0u;

    db->total_handoffs++;
    return true;   /* Caller may use this to signal worker */
}

/* ---- Consumer (Worker) ---------------------------------------------------- */

/*
 * db_get_ready() — get pointer to the full buffer
 *
 * Returns NULL if no buffer ready. Otherwise returns direct pointer into
 * buf[ready_buf] — zero copy. Worker reads data directly from the buffer.
 *
 * ACQUIRE barrier synchronizes with ISR's RELEASE barrier:
 * After acquire-load of ready, all prior ISR writes to buf[] are visible.
 */
const db_data_t *db_get_ready(double_buf_t *db, uint32_t *count)
{
    if (db->ready == 0u) {
        *count = 0u;
        return NULL;
    }

    /*
     * ACQUIRE barrier: synchronize with ISR's RELEASE barrier.
     * Must come AFTER reading ready=1 and BEFORE reading buf[].
     */
    MEM_BARRIER_ACQUIRE();

    *count = DB_HALF_SIZE;
    return db->buf[db->ready_buf];
}

/*
 * db_release() — signal ISR that buffer has been consumed
 *
 * RELEASE barrier BEFORE clearing ready:
 * Ensures all our reads of buf[] are complete before ISR sees ready=0
 * and potentially starts overwriting this buffer.
 */
void db_release(double_buf_t *db)
{
    MEM_BARRIER_RELEASE();
    db->ready = 0u;
}

bool db_is_ready(const double_buf_t *db)
{
    return db->ready != 0u;
}

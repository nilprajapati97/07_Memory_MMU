#ifndef RING_BUFFER_H
#define RING_BUFFER_H

/*
 * ring_buffer.h — Single Producer / Single Consumer (SPSC) Lock-Free Ring Buffer
 *
 * Design Principle:
 *   - head : exclusively written by the ISR (producer)
 *   - tail : exclusively written by the worker task (consumer)
 *   - Two writers never contend on the same variable → no atomic RMW needed
 *
 * Buffer size MUST be a power of 2.
 *   Reason: allows index wrap via bitmask (& MASK) — single AND instruction.
 *   Modulo (%) requires integer division: ~20-40 cycles on ARM Cortex-M.
 *   At 100 kHz ISR rate, every saved cycle matters.
 *
 * Memory Model:
 *   volatile  — prevents compiler from reordering / dead-store eliminating
 *   MEM_BARRIER() — prevents CPU from reordering at hardware level (store buffer)
 *   Both are required: volatile for compiler, barrier for CPU.
 *
 * Cache-line padding:
 *   On multi-core SoCs (Cortex-A, Snapdragon, NVIDIA Tegra), head and tail
 *   on the same 64-byte cache line cause FALSE SHARING: updating either
 *   invalidates the other core's cached copy even though variables are
 *   logically independent. 60-byte padding forces them onto separate lines.
 */

#include <stdint.h>
#include <stdbool.h>

/* ---- Configuration -------------------------------------------------------- */

#define RB_SIZE     4096u               /* entries — MUST be power of 2      */
#define RB_MASK     (RB_SIZE - 1u)      /* bitmask for O(1) wrap              */

/* Compile-time assertion: power-of-2 check */
_Static_assert((RB_SIZE & (RB_SIZE - 1u)) == 0u, "RB_SIZE must be a power of 2");
_Static_assert(RB_SIZE >= 2u,                     "RB_SIZE must be at least 2");

/* ---- Types ---------------------------------------------------------------- */

typedef uint32_t rb_data_t;

/*
 * ring_buf_t — the SPSC ring buffer
 *
 * Memory layout (64-byte cache line boundaries shown):
 *   [0x000] head     (4 bytes) — written by ISR
 *   [0x004..0x03F]   padding   — 60 bytes
 *   [0x040] tail     (4 bytes) — written by worker
 *   [0x044..0x07F]   padding   — 60 bytes
 *   [0x080..] buf[]            — 4096 × 4 = 16 KB data
 */
typedef struct {
    volatile uint32_t head;         /* Cache line 0: ISR writes, worker reads */
    uint8_t           _pad0[60];    /* Force head to its own 64-byte line     */
    volatile uint32_t tail;         /* Cache line 1: worker writes, ISR reads */
    uint8_t           _pad1[60];    /* Force tail to its own 64-byte line     */
    rb_data_t         buf[RB_SIZE]; /* Cache lines 2+: shared data            */
} __attribute__((aligned(64))) ring_buf_t;

/* ISR-safe statistics (ISR only increments, worker only reads snapshot) */
typedef struct {
    volatile uint32_t overflow_count;   /* samples dropped due to full buffer */
    volatile uint32_t total_pushed;     /* total samples pushed by ISR        */
    volatile uint32_t total_popped;     /* total samples popped by worker     */
} rb_stats_t;

/* ---- API ------------------------------------------------------------------ */

void     rb_init(ring_buf_t *rb, rb_stats_t *stats);

/*
 * rb_push_from_isr() — MUST be called ONLY from ISR context (single producer).
 * Returns true on success, false if buffer is full (sample dropped).
 */
bool     rb_push_from_isr(ring_buf_t *rb, rb_stats_t *stats, rb_data_t data);

/*
 * rb_pop() — MUST be called ONLY from worker/task context (single consumer).
 * Returns true and writes to *out on success, false if buffer is empty.
 */
bool     rb_pop(ring_buf_t *rb, rb_stats_t *stats, rb_data_t *out);

/* Query helpers (safe to call from either context — read-only) */
uint32_t rb_used(const ring_buf_t *rb);
bool     rb_is_empty(const ring_buf_t *rb);
bool     rb_is_full(const ring_buf_t *rb);

#endif /* RING_BUFFER_H */

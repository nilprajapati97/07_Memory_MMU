/*
 * ring_buffer.h — Lock-Free SPSC Ring Buffer for High-Frequency ISR
 *
 * Design: Single Producer (ISR) / Single Consumer (Worker Task)
 *         No mutex needed — volatile + memory barrier is sufficient.
 *
 * Target: ARM Cortex-M/A, x86 (simulation)
 * Rate:   100,000 interrupts/sec (10 µs per interrupt budget)
 *
 * Buffer sizing formula:
 *   size = interrupt_rate × worst_case_worker_latency
 *   e.g.  100,000/sec × 1 ms latency = 100 entries minimum
 *         Use 4096 for 40× safety margin.
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

/* ---- Configuration ------------------------------------------------------- */

#define RING_BUF_SIZE   4096u               /* MUST be power-of-2             */
#define RING_BUF_MASK   (RING_BUF_SIZE - 1u)

_Static_assert((RING_BUF_SIZE & (RING_BUF_SIZE - 1u)) == 0u,
               "RING_BUF_SIZE must be a power of 2");

typedef uint32_t rb_data_t;

/* ---- Ring buffer structure ----------------------------------------------- */

typedef struct {
    volatile uint32_t head;             /* written ONLY by ISR (producer)     */
    volatile uint32_t tail;             /* written ONLY by worker (consumer)  */
    rb_data_t         buf[RING_BUF_SIZE];
    volatile uint32_t overflow_count;   /* samples dropped due to full buffer */
    volatile uint32_t total_pushed;     /* lifetime push count                */
    volatile uint32_t total_popped;     /* lifetime pop count                 */
} ring_buf_t;

/* ---- API ----------------------------------------------------------------- */

void rb_init(ring_buf_t *rb);

/**
 * rb_push_from_isr() — Push one item. Call ONLY from ISR context.
 *
 * @param data  The sample/event to store.
 * @return true  item stored successfully.
 *         false buffer full — item dropped, overflow_count incremented.
 */
bool rb_push_from_isr(ring_buf_t *rb, rb_data_t data);

/**
 * rb_pop() — Pop one item. Call from worker task context ONLY.
 *
 * @param out   Pointer to store the retrieved item.
 * @return true  item retrieved, *out is valid.
 *         false buffer empty, *out unchanged.
 */
bool rb_pop(ring_buf_t *rb, rb_data_t *out);

/** rb_used()     — Current number of items waiting in the buffer. */
uint32_t rb_used(const ring_buf_t *rb);

/** rb_is_empty() — True when no items are pending. */
bool rb_is_empty(const ring_buf_t *rb);

#endif /* RING_BUFFER_H */

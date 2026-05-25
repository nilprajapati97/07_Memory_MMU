/*
 * ring_buffer.c — Lock-Free SPSC Ring Buffer Implementation
 *
 * Memory ordering protocol:
 *
 *   ISR (producer):
 *     1. Write data → buf[head]
 *     2. DMB (RELEASE) ← prevents store reordering past head update
 *     3. head = next_head
 *
 *   Worker (consumer):
 *     1. Read head snapshot
 *     2. DMB (ACQUIRE) ← pairs with producer RELEASE; sees buf data
 *     3. data = buf[tail]
 *     4. DMB
 *     5. tail = next_tail
 *
 * Why volatile is not enough alone:
 *   volatile prevents the compiler from caching the variable in a register.
 *   It does NOT prevent the CPU from reordering memory accesses in hardware.
 *   On ARM (weakly ordered), the CPU can write head BEFORE buf[head].
 *   DMB SY orders ALL memory accesses before/after the barrier instruction.
 */

#include "ring_buffer.h"

/* ---- Memory barrier ------------------------------------------------------ */

#if defined(__ARM_ARCH)
    /* ARM: Data Memory Barrier — full system (all observers, all access types) */
    #define MEM_BARRIER() __asm__ volatile ("dmb sy" ::: "memory")
#else
    /* x86 has TSO: all stores are globally visible in program order.
     * Compiler fence prevents compiler reordering — no HW instruction needed. */
    #define MEM_BARRIER() __asm__ volatile ("" ::: "memory")
#endif

/* ---- Implementation ------------------------------------------------------ */

void rb_init(ring_buf_t *rb)
{
    rb->head           = 0u;
    rb->tail           = 0u;
    rb->overflow_count = 0u;
    rb->total_pushed   = 0u;
    rb->total_popped   = 0u;
}

bool rb_push_from_isr(ring_buf_t *rb, rb_data_t data)
{
    uint32_t next_head = (rb->head + 1u) & RING_BUF_MASK;

    /* Check full: buffer is full when next_head would equal tail.
     * We sacrifice one slot to distinguish full from empty without a count. */
    if (next_head == rb->tail) {
        rb->overflow_count++;
        return false;
    }

    /* STEP 1: Write data to the slot FIRST */
    rb->buf[rb->head] = data;

    /* STEP 2: RELEASE barrier — guarantees the CPU has written buf[head]
     * to memory BEFORE it writes the updated head index.
     * Without this, the consumer could read head = next_head but find
     * stale data at buf[old_head] because the write was reordered. */
    MEM_BARRIER();

    /* STEP 3: Publish the new head — consumer can now safely read buf[old_head] */
    rb->head = next_head;

    rb->total_pushed++;
    return true;
}

bool rb_pop(ring_buf_t *rb, rb_data_t *out)
{
    if (rb->tail == rb->head) {
        return false;               /* Buffer empty */
    }

    /* ACQUIRE barrier — pairs with the RELEASE in rb_push_from_isr.
     * Guarantees we see the complete data write BEFORE reading buf[tail]. */
    MEM_BARRIER();

    *out = rb->buf[rb->tail];

    MEM_BARRIER();

    /* Update tail — signals producer that this slot is free */
    rb->tail = (rb->tail + 1u) & RING_BUF_MASK;

    rb->total_popped++;
    return true;
}

uint32_t rb_used(const ring_buf_t *rb)
{
    return (rb->head - rb->tail) & RING_BUF_MASK;
}

bool rb_is_empty(const ring_buf_t *rb)
{
    return rb->head == rb->tail;
}

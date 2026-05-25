/*
 * ring_buffer.c — SPSC Lock-Free Ring Buffer Implementation
 *
 * Architecture: ARM Cortex-M (primary), x86/x86_64 (simulation/testing)
 *
 * Memory barrier strategy:
 *
 *   ARM Cortex-M3/M4 (v7-M):
 *     __DMB() → "dmb sy" — Data Memory Barrier, full system domain.
 *     Required because Cortex-M has a write buffer (store buffer).
 *     Stores to normal memory can be delayed in the store buffer.
 *     DMB forces all preceding stores to complete before the barrier.
 *
 *   ARM Cortex-A (v8-A, multi-core, Snapdragon/NVIDIA):
 *     Same "dmb sy" but inner-shareable domain may suffice: "dmb ish".
 *     Use "dmb sy" for safety until profiling shows it's a bottleneck.
 *
 *   x86/x86_64 (TSO — Total Store Order):
 *     x86 has a strong memory model. Stores are observed in program order
 *     by ALL cores. A compiler fence (__asm__ volatile(""::: "memory"))
 *     is sufficient to prevent compiler reordering. No hardware fence needed
 *     for the SPSC case on x86.
 *
 *   RISC-V:
 *     "fence rw, rw" — full fence (can optimize to "fence w, r" for release)
 */

#include "ring_buffer.h"

/* ---- Platform Memory Barrier ---------------------------------------------- */

#if defined(__ARM_ARCH)
    /* ARM: hardware DMB + compiler fence */
    #define MEM_BARRIER_RELEASE() __asm__ __volatile__("dmb sy" ::: "memory")
    #define MEM_BARRIER_ACQUIRE() __asm__ __volatile__("dmb sy" ::: "memory")
#elif defined(__riscv)
    #define MEM_BARRIER_RELEASE() __asm__ __volatile__("fence w,r"  ::: "memory")
    #define MEM_BARRIER_ACQUIRE() __asm__ __volatile__("fence r,rw" ::: "memory")
#else
    /* x86/x86_64: compiler fence only (TSO guarantees hardware ordering) */
    #define MEM_BARRIER_RELEASE() __asm__ __volatile__("" ::: "memory")
    #define MEM_BARRIER_ACQUIRE() __asm__ __volatile__("" ::: "memory")
#endif

/* ---- Implementation ------------------------------------------------------- */

void rb_init(ring_buf_t *rb, rb_stats_t *stats)
{
    rb->head              = 0u;
    rb->tail              = 0u;
    stats->overflow_count = 0u;
    stats->total_pushed   = 0u;
    stats->total_popped   = 0u;
}

/*
 * rb_push_from_isr() — Producer (ISR) side
 *
 * Correctness proof (why no lock is needed):
 *
 *   Claim: ISR can safely write to rb->buf[head] and update head
 *          without any mutex.
 *
 *   Invariant 1: `head` is written ONLY by this function (ISR).
 *                Worker reads head but never writes it.
 *                → No write-write race on head.
 *
 *   Invariant 2: ISR reads `tail` to check for full condition.
 *                Worst case: tail has advanced (worker consumed more)
 *                since ISR last read it → ISR thinks buffer is fuller
 *                than it actually is → conservative → SAFE.
 *                ISR never reads stale tail and thinks buffer is empty
 *                when it is actually full.
 *
 *   Invariant 3: ISR writes to buf[head] before updating head.
 *                MEM_BARRIER_RELEASE() between the two stores ensures
 *                the worker can never observe the new head without also
 *                observing the corresponding data write.
 *
 * Execution timeline (two threads):
 *
 *   ISR:    buf[head] = data;         ← store A (data write)
 *   ISR:    MEM_BARRIER_RELEASE();    ← barrier (all stores before visible)
 *   ISR:    head = next_head;         ← store B (head update, "publication")
 *                                             ↑
 *                                     happens-before edge
 *                                             ↓
 *   Worker: head_seen = head;         ← load B  (sees new head)
 *   Worker: MEM_BARRIER_ACQUIRE();    ← barrier
 *   Worker: data = buf[tail];         ← load A  (guaranteed to see ISR data)
 *
 * If worker sees store B (new head), it MUST see store A (data). QED.
 */
bool rb_push_from_isr(ring_buf_t *rb, rb_stats_t *stats, rb_data_t data)
{
    uint32_t next_head = (rb->head + 1u) & RB_MASK;

    /*
     * Full check: if advancing head would reach tail, buffer is full.
     * Note: one slot is always "wasted" (full = head+1 == tail).
     * This makes full/empty distinction unambiguous without a separate counter.
     */
    if (next_head == rb->tail) {
        stats->overflow_count++;
        return false;
    }

    /* Write data to current head slot BEFORE updating head */
    rb->buf[rb->head] = data;

    /*
     * RELEASE barrier:
     * Guarantees all writes above (the data store) are globally visible
     * BEFORE the store to head below becomes visible to other cores/observers.
     * Without this, a weakly-ordered CPU could reorder stores, and the worker
     * could observe new head but stale data.
     */
    MEM_BARRIER_RELEASE();

    /* Publish the new head — worker now knows data[old_head] is valid */
    rb->head = next_head;

    stats->total_pushed++;
    return true;
}

/*
 * rb_pop() — Consumer (worker) side
 *
 * Correctness proof:
 *
 *   Invariant 1: `tail` is written ONLY by this function (worker).
 *                ISR reads tail but never writes it.
 *                → No write-write race on tail.
 *
 *   Invariant 2: Worker reads `head` to check for empty condition.
 *                Worst case: head has advanced (ISR pushed more)
 *                since worker last read it → worker thinks buffer is
 *                emptier than it is → conservative → SAFE.
 *
 *   Invariant 3: ACQUIRE barrier after reading head ensures data
 *                written by ISR before its head update is visible to us.
 *
 *   Invariant 4: RELEASE barrier before updating tail ensures our
 *                consumption of data is complete before ISR observes
 *                the freed slot.
 */
bool rb_pop(ring_buf_t *rb, rb_stats_t *stats, rb_data_t *out)
{
    /* Empty check */
    if (rb->tail == rb->head) {
        return false;
    }

    /*
     * ACQUIRE barrier:
     * Synchronizes with ISR's RELEASE barrier.
     * Guarantees: we see the data written by ISR before its head update.
     * Without this, on a weakly-ordered CPU (ARM, POWER), we could read
     * stale data even after observing the new head.
     */
    MEM_BARRIER_ACQUIRE();

    *out = rb->buf[rb->tail];

    /*
     * RELEASE barrier:
     * Ensures data read above is complete before tail is updated.
     * ISR reads tail to check "is there space?". By releasing here,
     * we guarantee the data slot is fully consumed before ISR reuses it.
     */
    MEM_BARRIER_RELEASE();

    rb->tail = (rb->tail + 1u) & RB_MASK;
    stats->total_popped++;
    return true;
}

/* ---- Query helpers -------------------------------------------------------- */

uint32_t rb_used(const ring_buf_t *rb)
{
    /* Snapshot: may be stale by the time caller uses it — use for monitoring */
    return (rb->head - rb->tail) & RB_MASK;
}

bool rb_is_empty(const ring_buf_t *rb)
{
    return rb->head == rb->tail;
}

bool rb_is_full(const ring_buf_t *rb)
{
    return ((rb->head + 1u) & RB_MASK) == rb->tail;
}

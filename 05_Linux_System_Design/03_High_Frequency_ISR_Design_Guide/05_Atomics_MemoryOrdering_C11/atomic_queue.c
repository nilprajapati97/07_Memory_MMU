/*
 * atomic_queue.c — C11 Atomic SPSC Queue Implementation
 *
 * Uses <stdatomic.h> for portable memory ordering.
 * No platform-specific assembly intrinsics.
 * No volatile keyword.
 * ThreadSanitizer-clean (TSan understands C11 atomics).
 *
 * ============================================================
 * WHAT THE COMPILER GENERATES (architecture survey)
 * ============================================================
 *
 * On ARM Cortex-M4 (ARMv7-M):
 *   atomic_store_explicit(head, v, release):
 *     STR r0, [r1]          ; plain store (no DMB for release-only)
 *     DMB ST                 ; store-only barrier (lighter than DMB SY)
 *   Actually GCC may emit: STLB/STLH/STL (release store, ARMv8+)
 *   On ARMv7: STR + DMB ISH
 *
 * On ARM Cortex-A55/A78 (ARMv8-A, AArch64):
 *   atomic_store_explicit(head, v, release):
 *     STLR x0, [x1]         ; Store-Release (single instruction, no DMB)
 *   atomic_load_explicit(head, acquire):
 *     LDAR x0, [x1]         ; Load-Acquire (single instruction, no DMB)
 *   Huge win: no explicit barrier instructions needed on ARMv8+
 *
 * On x86_64:
 *   atomic_store_explicit(head, v, release):
 *     MOV [addr], eax        ; plain store (TSO makes this a release)
 *   atomic_load_explicit(head, acquire):
 *     MOV eax, [addr]        ; plain load (TSO makes this an acquire)
 *   No MFENCE needed for release-acquire on x86.
 *
 * On RISC-V (RV64GC):
 *   atomic_store_explicit(head, v, release):
 *     fence rw, w            ; release fence
 *     sw a0, 0(a1)           ; store
 *   Or: sw.rl a0, 0(a1)      ; release store (A+C extension)
 *   atomic_load_explicit(head, acquire):
 *     lw.aq a0, 0(a1)        ; acquire load (A+C extension)
 *
 * ============================================================
 * WHY C11 ATOMICS ARE BETTER THAN volatile + DMB
 * ============================================================
 *
 * 1. PORTABILITY: volatile + DMB is ARM-specific.
 *    C11 atomics compile correctly on x86, ARM, RISC-V, POWER, MIPS.
 *    The compiler knows the target's memory model and emits optimal code.
 *
 * 2. COMPILER PROOF: ThreadSanitizer (TSan) understands C11 atomics.
 *    It can prove race-free access formally.
 *    TSan does NOT understand volatile + DMB — may report false positives.
 *
 * 3. OPTIMIZATION: On ARMv8: STLR/LDAR are single instructions (no DMB).
 *    On x86: plain load/store (TSO model).
 *    volatile + DMB is conservative: always emits the full barrier even
 *    when the architecture's memory model doesn't require it.
 *
 * 4. CORRECTNESS: volatile can still be reordered by the compiler relative
 *    to non-volatile accesses. C11 atomics with acquire/release prevents
 *    compiler from reordering NON-atomic accesses across the barrier too.
 *    This is the critical guarantee we need for buf[] writes.
 */

#include "atomic_queue.h"

/* ---- Init ----------------------------------------------------------------- */

void aq_init(atomic_queue_t *q, aq_stats_t *s)
{
    atomic_store_explicit(&q->head, 0u, memory_order_relaxed);
    atomic_store_explicit(&q->tail, 0u, memory_order_relaxed);
    atomic_store_explicit(&s->overflow_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&s->total_pushed,   0u, memory_order_relaxed);
    atomic_store_explicit(&s->total_popped,   0u, memory_order_relaxed);
}

/* ---- Producer (ISR) ------------------------------------------------------- */

/*
 * aq_push_from_isr()
 *
 * Memory ordering analysis (step by step):
 *
 * [1] Load tail with memory_order_acquire:
 *     We read tail to check if buffer is full.
 *     Why acquire? We want to see the most recent tail written by consumer.
 *     Technically, for SPSC full-check, memory_order_relaxed is sufficient
 *     (seeing a stale tail only makes us think buffer is fuller = conservative = safe).
 *     But acquire is used here for clarity in the memory model proof.
 *
 *     On ARM: generates LDAR (load-acquire) — waits for all prior loads to drain.
 *     On x86: plain MOV — no extra instruction (TSO provides acquire semantics).
 *
 * [2] Write buf[head] = data (plain, non-atomic store):
 *     This is NOT an atomic operation. It's a plain store to buf[].
 *     SAFE because: head is not yet visible to consumer
 *     (head hasn't been updated yet, so consumer can't access buf[head]).
 *     The release store in [3] creates the visibility boundary.
 *
 * [3] Store head with memory_order_release:
 *     This is the "publication" store — makes our data visible to consumer.
 *
 *     RELEASE SEMANTICS GUARANTEE:
 *     "All memory operations (loads and stores) that appear before this
 *      release store in program order will be visible to any thread that
 *      acquires this variable with memory_order_acquire."
 *
 *     In concrete terms:
 *       - buf[head] = data  (step 2)  IS GUARANTEED visible to consumer
 *         BEFORE consumer observes the new head value.
 *       - Without this: CPU could reorder the buf[] store to happen AFTER
 *         the head update → consumer reads stale buf[] data.
 *
 *     On ARM: generates STLR (store-release, ARMv8) or STR + DMB (ARMv7).
 *     On x86: plain MOV — TSO store order guarantees this already.
 *
 * Formal proof using C11 §5.1.2.4:
 *   - store B (release) synchronizes-with load B (acquire) in consumer
 *   - store A happens-before store B (program order)
 *   - load A happens-after load B (program order, after acquire)
 *   - By transitivity: store A happens-before load A
 *   - Therefore: consumer always reads data written by producer ∎
 */
bool aq_push_from_isr(atomic_queue_t *q, aq_stats_t *s, aq_data_t data)
{
    /* [0] Load head: only we (ISR) write head, so relaxed is fine */
    uint32_t head      = atomic_load_explicit(&q->head, memory_order_relaxed);
    uint32_t next_head = (head + 1u) & AQ_MASK;

    /* [1] Full check: load tail to see if consumer has freed space */
    uint32_t tail = atomic_load_explicit(&q->tail, memory_order_acquire);

    if (next_head == tail) {
        /* Buffer full: increment overflow counter atomically (relaxed OK for stats) */
        atomic_fetch_add_explicit(&s->overflow_count, 1u, memory_order_relaxed);
        return false;
    }

    /* [2] Write data to buf[head] — plain store, safe before head publication */
    q->buf[head] = data;

    /*
     * [3] Publish new head with RELEASE semantics.
     * This is the synchronization point. Consumer's acquire-load of head
     * "sees" all stores that happened before this release store — including
     * our buf[head] = data write above.
     *
     * NOTE: There is NO separate barrier instruction here (unlike approach 01's DMB).
     * The release ordering is baked into the store instruction itself:
     *   ARMv8:  STLR  (Store-Release Register)
     *   x86:    MOV   (TSO provides implicit release on all stores)
     *   RISC-V: SW.RL (Store-Release)
     */
    atomic_store_explicit(&q->head, next_head, memory_order_release);

    atomic_fetch_add_explicit(&s->total_pushed, 1u, memory_order_relaxed);
    return true;
}

/* ---- Consumer (Worker) ---------------------------------------------------- */

/*
 * aq_pop()
 *
 * Memory ordering analysis:
 *
 * [1] Load tail with relaxed (only we write tail):
 *     Same as head in ISR — only consumer writes tail, so relaxed load is fine.
 *
 * [2] Load head with ACQUIRE semantics:
 *     This SYNCHRONIZES WITH the ISR's release store of head.
 *     After this acquire load, all memory operations that happened in ISR
 *     BEFORE its release store are visible to us.
 *     Specifically: buf[old_head] = data is visible.
 *
 * [3] Read buf[tail] — plain load:
 *     Safe because: acquire-load in [2] ensures buf[] data is visible.
 *     Reading AFTER the acquire barrier = reading valid ISR data.
 *
 * [4] Store tail with RELEASE semantics:
 *     Signals to ISR that slot is free (ISR checks tail for full condition).
 *     Release ensures: our read of buf[tail] (step 3) is complete before
 *     ISR observes the new tail and potentially reuses the slot.
 *
 *     Without this: ISR could see new tail (slot freed), write new data to
 *     buf[old_tail] before our read completes → data corruption.
 */
bool aq_pop(atomic_queue_t *q, aq_stats_t *s, aq_data_t *out)
{
    /* [1] Load tail: only we write it, relaxed is fine */
    uint32_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);

    /* [2] Load head with ACQUIRE: synchronize with ISR's release store */
    uint32_t head = atomic_load_explicit(&q->head, memory_order_acquire);

    if (tail == head) {
        return false;   /* empty */
    }

    /* [3] Read data: safe after acquire barrier */
    *out = q->buf[tail];

    /* [4] Update tail with RELEASE: free the slot for ISR */
    atomic_store_explicit(&q->tail, (tail + 1u) & AQ_MASK, memory_order_release);

    atomic_fetch_add_explicit(&s->total_popped, 1u, memory_order_relaxed);
    return true;
}

/* ---- Query ---------------------------------------------------------------- */

uint32_t aq_size(const atomic_queue_t *q)
{
    /* Consistent snapshot (acquire both): may still be stale by call return */
    uint32_t h = atomic_load_explicit(&q->head, memory_order_acquire);
    uint32_t t = atomic_load_explicit(&q->tail, memory_order_acquire);
    return (h - t) & AQ_MASK;
}

bool aq_is_empty(const atomic_queue_t *q)
{
    uint32_t h = atomic_load_explicit(&q->head, memory_order_acquire);
    uint32_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);
    return h == t;
}

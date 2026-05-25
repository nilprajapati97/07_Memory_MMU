#ifndef ATOMIC_QUEUE_H
#define ATOMIC_QUEUE_H

/*
 * atomic_queue.h — SPSC Lock-Free Queue Using C11 stdatomic
 *
 * This is the PORTABLE, standards-compliant version of Approach 01.
 *
 * Approach 01 uses:
 *   volatile + __DMB() (ARM-specific assembly barrier)
 *
 * This approach uses:
 *   _Atomic + memory_order_acquire/release (ISO C11, any architecture)
 *
 * ============================================================
 * C11 MEMORY MODEL PRIMER
 * ============================================================
 *
 * The C11 memory model defines a partial order ("happens-before") on
 * memory operations across threads. Without explicit ordering, the CPU
 * and compiler are free to reorder operations for performance.
 *
 * Memory orders for atomic operations:
 *
 *   memory_order_relaxed:
 *     No ordering constraint. Only atomicity guaranteed.
 *     Use for: statistics counters, sequence numbers where order doesn't matter.
 *     Fastest: no barrier instruction emitted on most architectures.
 *
 *   memory_order_acquire  (for loads):
 *     No SUBSEQUENT memory operation (load or store) in this thread can be
 *     reordered to appear BEFORE this load.
 *     Synchronizes with: a release store to the same atomic variable.
 *     Think: "acquire the lock, now I can read protected data safely"
 *
 *   memory_order_release  (for stores):
 *     No PRECEDING memory operation (load or store) in this thread can be
 *     reordered to appear AFTER this store.
 *     Synchronizes with: an acquire load of the same atomic variable.
 *     Think: "all my writes are done, releasing the lock"
 *
 *   memory_order_acq_rel  (for read-modify-write, e.g., fetch_add):
 *     Both acquire AND release semantics. Used for CAS operations.
 *
 *   memory_order_seq_cst:
 *     Strongest. Full sequential consistency. All seq_cst operations appear
 *     to all threads in the same global order.
 *     Most expensive: generates MFENCE on x86, DMB sy on ARM.
 *     Use sparingly; acquire/release is usually sufficient.
 *
 * ============================================================
 * RELEASE-ACQUIRE SYNCHRONIZATION (the SPSC protocol)
 * ============================================================
 *
 * ISR (producer):
 *   buf[head] = data;                              // store A (plain)
 *   atomic_store_explicit(head, next, release);   // store B (release)
 *
 * Worker (consumer):
 *   h = atomic_load_explicit(head, acquire);      // load B (acquire)
 *   data = buf[tail];                             // load A (plain, after barrier)
 *
 * The release store of B "synchronizes with" the acquire load of B.
 * This creates a happens-before edge:
 *   "everything before store B" happens-before "everything after load B"
 *
 * Therefore: worker's load A (reading buf[tail]) is guaranteed to see
 * ISR's store A (writing buf[head]) because store A precedes store B
 * and load A follows load B.
 *
 * This is formally defined in C11 §5.1.2.4 and C++11 §1.10.
 * No undefined behavior. Provably correct. ThreadSanitizer-clean.
 *
 * ============================================================
 * COMPILER SUPPORT
 * ============================================================
 *
 *   GCC:    4.9+  (stdatomic.h)
 *   Clang:  3.1+  (stdatomic.h)
 *   MSVC:   2015+ (<stdatomic.h> from C11 update, or use C++ <atomic>)
 *   ARM GCC: Full support including Cortex-M and Cortex-A targets
 *
 * C++ note: In C++11, use std::atomic<T> from <atomic>.
 *   Semantics are identical; only syntax differs.
 *   _Atomic uint32_t head;  ←→  std::atomic<uint32_t> head;
 *   atomic_store_explicit(&head, v, order)  ←→  head.store(v, order)
 *   atomic_load_explicit(&head, order)      ←→  head.load(order)
 */

#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

/* ---- Configuration -------------------------------------------------------- */

#define AQ_SIZE     4096u
#define AQ_MASK     (AQ_SIZE - 1u)

_Static_assert((AQ_SIZE & (AQ_SIZE - 1u)) == 0u, "AQ_SIZE must be power of 2");

/* ---- Types ---------------------------------------------------------------- */

typedef uint32_t aq_data_t;

/*
 * atomic_queue_t — C11 atomic SPSC ring buffer
 *
 * Key difference from ring_buffer_t (approach 01):
 *   head and tail are _Atomic — generates proper acquire/release
 *   instructions for each access.
 *
 * buf[] is NOT atomic: it's protected by the release-acquire ordering
 * on head and tail. Making buf[] atomic would be overly conservative
 * and much slower (atomic read per element).
 *
 * Cache-line padding: same rationale as approach 01.
 */
typedef struct {
    _Atomic uint32_t head;          /* [cache line 0] ISR writes              */
    uint8_t          _pad0[60];     /* prevent false sharing (64B cache line) */
    _Atomic uint32_t tail;          /* [cache line 1] worker writes           */
    uint8_t          _pad1[60];     /* prevent false sharing                  */
    aq_data_t        buf[AQ_SIZE];  /* [cache line 2+] shared data            */
} __attribute__((aligned(64))) atomic_queue_t;

typedef struct {
    _Atomic uint32_t overflow_count;
    _Atomic uint32_t total_pushed;
    _Atomic uint32_t total_popped;
} aq_stats_t;

/* ---- API ------------------------------------------------------------------ */

void     aq_init(atomic_queue_t *q, aq_stats_t *s);

/*
 * aq_push_from_isr() — ISR (producer) side
 * Uses memory_order_release on head store.
 * Returns false if full.
 */
bool     aq_push_from_isr(atomic_queue_t *q, aq_stats_t *s, aq_data_t data);

/*
 * aq_pop() — worker (consumer) side
 * Uses memory_order_acquire on head load.
 * Returns false if empty.
 */
bool     aq_pop(atomic_queue_t *q, aq_stats_t *s, aq_data_t *out);

/* Query */
uint32_t aq_size(const atomic_queue_t *q);
bool     aq_is_empty(const atomic_queue_t *q);

#endif /* ATOMIC_QUEUE_H */

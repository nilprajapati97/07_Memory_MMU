# Q40: Lock-Free Stack with CAS

**Section:** Performance & Algorithms | **Difficulty:** Hard | **Topics:** `cmpxchg`, CAS, ABA problem, lock-free stack, `cmpxchg128`, pointer tagging, memory ordering

---

## Question

Implement a lock-free stack using `cmpxchg` (compare-and-swap) and discuss the ABA problem.

---

## Answer

```c
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/cpu.h>

/* ─── Lock-Free Stack Node ────────────────────────────────────────────────*/
struct lf_node {
    void           *data;
    struct lf_node *next;
};

/* ─── Lock-Free Stack (Version 1: Simple CAS — vulnerable to ABA) ────────*/
struct lf_stack {
    struct lf_node * volatile top;  /* atomic head pointer */
};

/*
 * Simple CAS push: NOT ABA-safe.
 * Demonstrates the basic CAS loop pattern.
 */
void lf_push_simple(struct lf_stack *stack, struct lf_node *node)
{
    struct lf_node *old_top;

    do {
        old_top    = stack->top;       /* read current top           */
        node->next = old_top;          /* link new node to old top   */
        /*
         * cmpxchg(ptr, expected, new): atomically:
         *   if (*ptr == expected) { *ptr = new; return expected; }
         *   else { return *ptr; }
         * Retry if another thread changed top between our read and CAS.
         */
    } while (cmpxchg(&stack->top, old_top, node) != old_top);
}

/*
 * Simple CAS pop: NOT ABA-safe.
 * ABA problem: top was A, popped to B (A freed), new A allocated at same
 * address. CAS succeeds even though A was freed and reallocated.
 */
void *lf_pop_simple(struct lf_stack *stack)
{
    struct lf_node *old_top, *new_top;

    do {
        old_top = stack->top;
        if (!old_top)
            return NULL; /* stack empty */
        new_top = old_top->next;
    } while (cmpxchg(&stack->top, old_top, new_top) != old_top);

    return old_top->data;
}


/* ─── Version 2: ABA-Safe Stamped Pointer ────────────────────────────────
 * Solution: pack a version counter with the pointer.
 * Push increments the version. Pop only succeeds if pointer+version match.
 * ABA scenario: even if address is reused, version won't match.
 *
 * On x86-64: use __int128 CAS (CMPXCHG16B) for 8-byte ptr + 8-byte version.
 * ARM64: use CASP (compare-and-swap pair) instruction.
 */

struct stamped_ptr {
    struct lf_node *ptr;      /* 8 bytes: pointer to top node */
    u64             stamp;    /* 8 bytes: monotonic version   */
};

struct lf_stack_aba {
    /*
     * struct stamped_ptr must be 16-byte aligned for CMPXCHG16B.
     * The struct is stored as a __int128 in hardware.
     */
    struct stamped_ptr head __attribute__((aligned(16)));
};

static inline bool stamped_cmpxchg(struct stamped_ptr *addr,
                                     struct stamped_ptr expected,
                                     struct stamped_ptr new_val)
{
    /*
     * cmpxchg_double: Linux kernel API for 128-bit CAS on supported arches.
     * On x86: generates CMPXCHG16B instruction.
     * Returns true if the exchange succeeded.
     */
    return cmpxchg_double(&addr->ptr, &addr->stamp,
                           expected.ptr, expected.stamp,
                           new_val.ptr, new_val.stamp);
}

void lf_aba_push(struct lf_stack_aba *stack, struct lf_node *node)
{
    struct stamped_ptr old_head, new_head;

    do {
        old_head       = READ_ONCE(stack->head);  /* atomic 128-bit read  */
        node->next     = old_head.ptr;
        new_head.ptr   = node;
        new_head.stamp = old_head.stamp + 1;      /* increment version    */
    } while (!stamped_cmpxchg(&stack->head, old_head, new_head));
}

void *lf_aba_pop(struct lf_stack_aba *stack)
{
    struct stamped_ptr old_head, new_head;

    do {
        old_head = READ_ONCE(stack->head);
        if (!old_head.ptr)
            return NULL; /* empty */
        new_head.ptr   = old_head.ptr->next;
        new_head.stamp = old_head.stamp + 1;      /* new version on pop   */
    } while (!stamped_cmpxchg(&stack->head, old_head, new_head));

    return old_head.ptr->data;
}


/* ─── Version 3: RCU-based lock-free stack (kernel-idiomatic) ───────────
 * For production use: use RCU for reads and spinlock for writes.
 * Avoids CMPXCHG16B (not available on all architectures).
 * RCU readers get consistent snapshots without any locks.
 */
struct rcu_lf_stack {
    struct lf_node __rcu *top; /* RCU-protected pointer */
    spinlock_t            lock; /* protects push/pop writes */
};

void rcu_lf_push(struct rcu_lf_stack *stack, struct lf_node *node)
{
    spin_lock(&stack->lock);
    node->next = rcu_dereference_protected(stack->top,
                     lockdep_is_held(&stack->lock));
    rcu_assign_pointer(stack->top, node); /* publishes via memory barrier */
    spin_unlock(&stack->lock);
}

void *rcu_lf_pop(struct rcu_lf_stack *stack)
{
    struct lf_node *node;
    void *data = NULL;

    spin_lock(&stack->lock);
    node = rcu_dereference_protected(stack->top,
               lockdep_is_held(&stack->lock));
    if (node) {
        rcu_assign_pointer(stack->top, node->next);
        data = node->data;
    }
    spin_unlock(&stack->lock);

    if (node)
        kfree_rcu(node, rcu_head); /* free node after RCU grace period */
    return data;
}
```

---

## Explanation

### Core Concept

**The ABA Problem:**

```
Thread 1: reads top = A
Thread 2: pops A (frees it), pops B, pushes new A (same address, reused)
Thread 1: CAS(top, A, A.next) — succeeds! But A was freed/reallocated.
          A.next is now stale — stack is corrupt.

Fix: stamp the pointer with a version.
  top = (A, v=1)
  Thread 2: pops to (B, v=2), pushes (A, v=3)
  Thread 1: CAS((A,v=1), (A,v=3)→expected mismatch) — FAILS. Retries.
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `cmpxchg(ptr, old, new)` | Atomic CAS on pointer-sized value |
| `cmpxchg_double(ptr1, ptr2, o1, o2, n1, n2)` | 128-bit CAS (CMPXCHG16B) |
| `READ_ONCE(x)` | Atomic read without tear (prevents compiler optimization) |
| `rcu_assign_pointer(ptr, val)` | Publish new pointer with `smp_store_release` |
| `rcu_dereference_protected(ptr, cond)` | Read RCU pointer under lock |
| `kfree_rcu(ptr, head)` | Free struct after current RCU grace period |

### Trade-offs & Pitfalls

- **`cmpxchg_double` (CMPXCHG16B) is not universally available.** x86-64 has it since Core 2. ARM64 has CASP. RISC-V: LR/SC sequences for 128-bit. Check `CONFIG_CMPXCHG_DOUBLE`. The RCU version (Version 3) is portable and preferred in Linux kernel code.
- **Spurious CAS retry.** Under high contention, the CAS loop may spin many times. For a GPU command submission buffer: a spinlock may be faster than CAS under high contention (spinlock: at most one retry; CAS: O(n) retries with n threads).

### NVIDIA / GPU Context

NVIDIA uses lock-free structures for GPU command submission hot paths where spinlocks would cause priority inversion. The UVM driver uses RCU-based lists (similar to Version 3) for tracking GPU VA blocks — readers (GPU fault handlers) are lock-free, writers (allocators) hold a spinlock.

---

## Cross Questions & Answers

**CQ1: Why is a simple CAS loop not sufficient to fix the ABA problem?**
> CAS checks pointer equality only — `cmpxchg(&top, A, A.next)` succeeds if `top == A`. If A was freed and a new object was allocated at the exact same address with the same `next` pointer value, CAS cannot distinguish the original A from the new A. The version/stamp counter ensures even if the pointer value is reused, `(A, stamp=1) ≠ (A, stamp=3)` — CAS fails correctly. Without the stamp, CAS has no way to detect that the pointer was reused between the read and the CAS attempt.

**CQ2: What is `READ_ONCE` and why is it critical for lock-free code?**
> `READ_ONCE(x)` prevents the compiler from: (1) caching the value in a register and reading it multiple times from the register (instead of memory), (2) splitting a single memory read into multiple narrow reads (tearing). Without `READ_ONCE`, the compiler may read `stack->top` twice: once to compare and once to dereference — another thread may change it between the two reads, causing a torn read. `READ_ONCE` compiles to a single atomic-width load instruction.

**CQ3: What memory ordering does `cmpxchg` provide on x86 vs ARM?**
> x86: `CMPXCHG` has full sequentially-consistent (SC) ordering — all prior stores are visible before the CAS, and the CAS result is visible before subsequent loads. ARM64: `CAS` instruction has acquire/release semantics by default (`CASA` = CAS + acquire, `CASL` = CAS + release, `CASAL` = SC). For lock-free stack: need at least acquire on CAS in pop (to see the node's data) and release on CAS in push (to publish the node). `cmpxchg()` in Linux kernel uses the appropriate instruction for the architecture.

**CQ4: When should you use a lock-free stack vs a mutex-protected stack?**
> Lock-free is better when: (1) lock holder may be preempted by a higher-priority task needing the stack (priority inversion), (2) lock contention is extremely high with many CPUs — CAS retry cost < mutex wait + context switch cost, (3) the stack is accessed from IRQ/NMI context (no sleeping allowed). Mutex is better when: (1) low contention (mutex is faster than CAS when no contention), (2) complex multi-step operations that need atomicity beyond a single CAS, (3) portability (no CMPXCHG16B requirement). In practice: NVIDIA uses mutex-protected pools for GPU memory allocation (complex operations) and lock-free for fast command ring submission (single pointer update).

**CQ5: What is the "hazard pointer" technique as an alternative to stamped pointers for ABA prevention?**
> Hazard pointers: before accessing a node, a thread publishes the node's address to a thread-local hazard pointer slot. Before freeing a node, the freeing thread checks all hazard pointer slots — if any thread has the address, defer the free. No CMPXCHG16B needed — works with 64-bit CAS. Cost: O(n) scan of hazard pointer table on free. Used in Java's `ConcurrentLinkedDeque`. In Linux kernel: similar concept via SRCU (Sleepable RCU) or QRCU for structures that need deferred freeing without 128-bit CAS.

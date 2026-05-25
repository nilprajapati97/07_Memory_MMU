# Q02: Lock-Free SPSC Ring Buffer (Single Producer Single Consumer)

**Section:** Linux Kernel Internals | **Difficulty:** Hard | **Topics:** lock-free, ring buffer, memory barriers, `smp_store_release`, `smp_load_acquire`

---

## Question

Design and implement a lock-free ring buffer (SPSC) for kernel use.

---

## Answer

```c
#include <linux/slab.h>
#include <linux/atomic.h>

#define RING_SIZE 1024  /* Must be power of 2 */
#define RING_MASK (RING_SIZE - 1)

struct spsc_ring {
    void        *buf[RING_SIZE];
    atomic_t     head;  /* producer writes here */
    atomic_t     tail;  /* consumer reads from here */
};

struct spsc_ring *ring_alloc(void)
{
    struct spsc_ring *r = kzalloc(sizeof(*r), GFP_KERNEL);
    if (!r)
        return NULL;
    atomic_set(&r->head, 0);
    atomic_set(&r->tail, 0);
    return r;
}

/* Producer: returns 0 on success, -ENOSPC if full */
int ring_push(struct spsc_ring *r, void *item)
{
    int head = atomic_read(&r->head);
    int next = (head + 1) & RING_MASK;

    /* Full: next write position equals consumer's current position */
    if (next == atomic_read(&r->tail))
        return -ENOSPC;

    r->buf[head] = item;
    /* Release barrier: ensures buf[head] write is visible before head update */
    smp_store_release(&r->head.counter, next);
    return 0;
}

/* Consumer: returns NULL if empty */
void *ring_pop(struct spsc_ring *r)
{
    int tail = atomic_read(&r->tail);
    void *item;

    /* Acquire barrier: ensures we read buf[] after observing updated head */
    if (tail == smp_load_acquire(&r->head.counter))
        return NULL;

    item = r->buf[tail];
    atomic_set(&r->tail, (tail + 1) & RING_MASK);
    return item;
}

void ring_free(struct spsc_ring *r)
{
    kfree(r);
}
```

---

## Explanation

### Core Concept

An **SPSC (Single Producer Single Consumer) ring buffer** is a circular queue where exactly one thread produces items and one thread consumes them — with no locks. This is possible because:

1. The **producer** only writes `head` and reads `tail` (to check full)
2. The **consumer** only writes `tail` and reads `head` (to check empty)

There is no concurrent write to the same variable, so atomicity of individual reads/writes is sufficient. The challenge is **memory ordering** — the CPU and compiler may reorder operations. `smp_store_release` and `smp_load_acquire` create a happens-before relationship:
- Producer: write `buf[head]` **happens-before** update of `head` (release)
- Consumer: read of `head` **happens-before** read of `buf[tail]` (acquire)

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `smp_store_release(&var, val)` | Store with release semantics — prior writes are visible before this store |
| `smp_load_acquire(&var)` | Load with acquire semantics — subsequent reads see all prior release stores |
| `atomic_read(&v)` | Plain relaxed atomic read |
| `atomic_set(&v, val)` | Plain atomic set |
| `kzalloc(size, GFP_KERNEL)` | Zero-initialized kernel allocation |

### Trade-offs & Pitfalls

- **SPSC only.** This design is NOT safe for multiple producers or multiple consumers. For MPSC/MPMC, use a more complex structure (e.g., `kfifo` in kernel, or a CAS-based MPMC queue).
- **Power-of-2 ring size** is required for the bitmask trick `(head + 1) & RING_MASK`. A non-power-of-2 requires modulo division, which is slower.
- **No ABA problem** here because head/tail are monotonically increasing indices — they never "wrap around" to cause confusion.
- **Cache alignment:** In a real driver, `head` and `tail` should be in separate cachelines (`____cacheline_aligned`) to avoid false sharing between producer and consumer CPUs.

### NVIDIA / GPU Context

NVIDIA GPU drivers use SPSC ring buffers between:
- The **submit thread** (producer: pushes GPU commands) and the **interrupt handler** (consumer: processes completions)
- The **fault handler work queue** (producer) and the **fault replay thread** (consumer)

The lock-free nature is critical because the interrupt handler cannot block — it must process completions in microseconds, and acquiring a mutex could take milliseconds if another thread holds it.

---

## Cross Questions & Answers

**CQ1: Why can't you use a plain `volatile` variable instead of `smp_store_release`/`smp_load_acquire`?**
> `volatile` in C only prevents the compiler from caching a variable in a register — it does NOT prevent CPU reordering of memory accesses. On a modern out-of-order CPU, a store to `buf[head]` could become visible to other CPUs *after* the store to `head` even if the C code has them in the right order. `smp_store_release` inserts the necessary hardware memory barrier (`SFENCE`/`DMB`) to enforce ordering.

**CQ2: What is the `kfifo` API in the Linux kernel and when would you use it over a custom SPSC ring?**
> `kfifo` (`include/linux/kfifo.h`) is the kernel's built-in lock-free FIFO, optimized for SPSC use cases. It uses power-of-2 sizing, proper memory barriers, and is production-tested. Use `kfifo` unless you need to store arbitrary pointer types or have very specific alignment requirements. For NVIDIA GPU drivers, custom rings are sometimes preferred for tighter control over cache layout and batching semantics.

**CQ3: How would you extend this SPSC ring buffer to support batched pushes (pushing N items atomically)?**
> Reserve N slots by computing `new_head = (head + N) & RING_MASK` and checking that `new_head` doesn't overlap `tail`. Write all N items, then do a single `smp_store_release` of the new head. This is how GPU command rings work — the driver writes a batch of commands, then writes the doorbell register once to notify the GPU, amortizing the costly hardware notification.

**CQ4: What happens if RING_SIZE is not a power of 2?**
> The bitmask `& RING_MASK` optimization breaks — you cannot use a bitmask to wrap around a non-power-of-2 size. You must use modulo: `(head + 1) % RING_SIZE`, which is slower (division instruction). Additionally, checking fullness/emptiness using the one-slot-gap strategy (full when `next == tail`) becomes trickier and must be verified carefully.

**CQ5: How does the NVIDIA GPU's hardware command ring buffer differ from this software ring?**
> The GPU's hardware ring buffer has a **write pointer** (CPU-writable register, called the "put pointer" or doorbell) and a **read pointer** (GPU-internal, readable by CPU via MMIO). The CPU writes commands into GPU-accessible memory, advances the put pointer with a `writel()` to a doorbell register, and the GPU's command processor fetches and executes commands. The CPU must use `wmb()` before writing the doorbell to guarantee command writes are flushed to PCIe before the GPU sees the updated pointer.

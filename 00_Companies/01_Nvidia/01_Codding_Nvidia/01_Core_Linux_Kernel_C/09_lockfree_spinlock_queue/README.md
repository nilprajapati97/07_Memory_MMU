# Q9: Write a lock-free or spinlock-protected queue

## In-depth Explanation (Nvidia Interview Style)

Queues are fundamental data structures in kernel and driver code for buffering data between producers and consumers. In the Linux kernel, queues are often protected by spinlocks for concurrency, or implemented lock-free for performance.

### Spinlock-Protected Queue
- Use a spinlock to protect enqueue and dequeue operations.
- Suitable for short critical sections and when running in atomic context.

### Lock-Free Queue
- Uses atomic operations (e.g., compare-and-swap) to avoid locks.
- More complex, but can improve performance on multi-core systems.

### Example (Spinlock-Protected)
```c
#include <linux/spinlock.h>
struct queue {
    int data[SIZE];
    int head, tail;
    spinlock_t lock;
};

void enqueue(struct queue *q, int val) {
    spin_lock(&q->lock);
    // ... add to queue ...
    spin_unlock(&q->lock);
}
```

### Why is this important for Nvidia/Linux Kernel?
- Used in interrupt handlers, work queues, and device drivers.
- Correct synchronization is critical for data integrity.

### Interview Tips
- Be ready to discuss when to use spinlocks vs. lock-free.
- Know the trade-offs: simplicity vs. performance.

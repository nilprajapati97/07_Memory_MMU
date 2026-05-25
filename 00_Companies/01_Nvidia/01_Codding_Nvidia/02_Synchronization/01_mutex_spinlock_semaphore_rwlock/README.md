# Q11: Difference between mutex, spinlock, semaphore, rwlock

## In-depth Explanation (Nvidia Interview Style)

### Mutex
- Sleeping lock; only one thread can hold it at a time.
- Used when the critical section may sleep.
- Not usable in interrupt context.

### Spinlock
- Busy-wait lock; spins until lock is available.
- Used in atomic/interrupt context.
- No sleeping allowed while holding a spinlock.

### Semaphore
- Counting lock; allows multiple holders up to a limit.
- Used for resource pools or producer-consumer problems.

### Read-Write Lock (rwlock)
- Allows multiple readers or one writer.
- Improves concurrency for read-heavy workloads.

### Interview Tips
- Know when to use each type.
- Be ready to discuss deadlocks, priority inversion, and kernel context restrictions.

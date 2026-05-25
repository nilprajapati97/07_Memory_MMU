# Q15: Explain deadlock from nested locks

## In-depth Explanation (Nvidia Interview Style)

Deadlocks can occur when two or more threads acquire locks in different orders, causing a circular wait. This is common with nested locks.

### Example
- Thread 1: lock A, then lock B
- Thread 2: lock B, then lock A

### How to avoid
- Always acquire locks in a consistent global order.
- Use lockdep (kernel lock dependency checker) to detect potential deadlocks.

### Interview Tips
- Be ready to discuss lock ordering and lockdep.
- Know how to debug deadlocks in kernel code.

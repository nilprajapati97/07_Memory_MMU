# Q16: Explain and implement RCU (Read-Copy-Update) for a read-heavy data structure

## Code
See solution.c for a simple RCU-protected linked list implementation.

## In-depth Explanation (Nvidia Interview Style)

### What is RCU?
- RCU (Read-Copy-Update) is a synchronization mechanism optimized for read-heavy workloads.
- Allows multiple readers to access data concurrently without locks, while writers update data by creating a new copy and updating pointers.

### How does it work?
- Readers use `rcu_read_lock()`/`rcu_read_unlock()` to mark critical sections.
- Writers update data by copying and then use `synchronize_rcu()` to wait for readers to finish before freeing old data.
- Used in the Linux kernel for lists, routing tables, and other read-mostly structures.

### Why is this important for Nvidia/Linux Kernel?
- Enables high concurrency and scalability for read-heavy data structures.
- Reduces contention and improves performance on multi-core systems.

### Interview Tips
- Be ready to explain grace periods, pointer updates, and memory reclamation.
- Know the difference between RCU and traditional locking.
- Understand where RCU is used in the kernel (e.g., networking, process lists).

Here are **Linux Kernel coding questions** commonly useful for an **NVIDIA interview**:

## Core Linux Kernel C

1. Implement `container_of()` macro.
2. Implement a doubly linked list like Linux `list_head`.
3. Write a kernel-style circular buffer.
4. Implement reference counting with safe increment/decrement.
5. Write code to align an address to page boundary.
6. Implement bit operations: set, clear, test bit.
7. Detect integer overflow in kernel C.
8. Implement a bitmap allocator.
9. Write a lock-free or spinlock-protected queue.
10. Explain `volatile` vs memory barriers.

## Synchronization

11. Difference between mutex, spinlock, semaphore, rwlock.
12. When can you not sleep in kernel code?
13. Implement producer-consumer using wait queues.
14. Fix a race condition in shared counter code.
15. Explain deadlock from nested locks.
16. What is atomic context?
17. Use `atomic_t` correctly.
18. Explain `spin_lock_irqsave()` vs `spin_lock()`.

## Memory Management

19. Difference between `kmalloc`, `vmalloc`, `alloc_pages`.
20. Implement page-aligned allocation logic.
21. Explain GFP flags: `GFP_KERNEL`, `GFP_ATOMIC`.
22. What happens if `copy_from_user()` fails?
23. Write safe user-kernel buffer copy code.
24. Explain slab allocator.
25. Debug a memory leak in kernel module.

## Device Drivers

26. Write a simple character device driver.
27. Implement `open`, `read`, `write`, `release`.
28. Explain `ioctl`.
29. Implement blocking read using wait queue.
30. Explain `poll/select/epoll` support in driver.
31. Implement interrupt handler + bottom half.
32. Difference between tasklet, workqueue, threaded IRQ.
33. Explain DMA mapping APIs.
34. Difference between coherent and streaming DMA.
35. Explain PCIe device probe/remove flow.

## NVIDIA/GPU-Relevant Topics

36. Explain IOMMU and DMA address translation.
37. How does PCI BAR mapping work?
38. Explain `ioremap()` and MMIO access.
39. Why use `readl()`/`writel()` instead of raw pointer access?
40. Explain cache coherency issues with DMA.
41. What is pinned memory?
42. Explain zero-copy memory.
43. How would a GPU driver submit work to hardware?
44. Explain command buffers and doorbells.
45. How would you handle GPU interrupt completion?

## Debugging Questions

46. Kernel panic: how do you debug?
47. Use `dmesg`, `ftrace`, `perf`, `kgdb`.
48. Explain `BUG_ON`, `WARN_ON`.
49. How to debug use-after-free?
50. How to debug race conditions?

## Example Coding Question

**Implement `container_of`:**

```c
#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);           \
    (type *)((char *)__mptr - offsetof(type, member));           \
})
```

## High-frequency interview areas

Focus most on:

**C pointers, memory alignment, linked lists, synchronization, interrupt handling, DMA, PCIe, MMIO, and race-condition debugging.**

# Q10: Explain `volatile` vs memory barriers

## In-depth Explanation (Nvidia Interview Style)

### `volatile`
- Tells the compiler not to optimize accesses to a variable.
- Does **not** provide any CPU or compiler ordering guarantees.
- Not sufficient for synchronization in kernel code.

### Memory Barriers
- Ensure ordering of memory operations across CPUs and compiler.
- Types: `mb()` (full barrier), `rmb()` (read barrier), `wmb()` (write barrier).
- Used to prevent reordering of reads/writes in concurrent code.

### Why is this important for Nvidia/Linux Kernel?
- Correct use of barriers is critical for SMP (multi-core) systems.
- Prevents subtle bugs in device drivers and low-level code.

### Interview Tips
- Be ready to explain why `volatile` is not enough for concurrency.
- Know Linux barrier APIs and when to use them.

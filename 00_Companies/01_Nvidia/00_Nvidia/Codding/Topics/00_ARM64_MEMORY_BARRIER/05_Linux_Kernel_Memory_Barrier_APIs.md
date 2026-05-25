
# Linux Kernel Memory Barrier APIs

## Overview
The Linux kernel provides architecture-independent APIs for memory barriers, abstracting away the hardware details and ensuring correct ordering on all supported platforms. Using these APIs is critical for writing portable, correct, and performant kernel code.

## Common APIs and Their Semantics

- `smp_mb()`   — Full memory barrier (orders all loads and stores)
- `smp_rmb()`  — Read (load) barrier (orders loads only)
- `smp_wmb()`  — Write (store) barrier (orders stores only)
- `barrier()`  — Compiler barrier only (prevents compiler reordering, not hardware)

### Additional APIs
- `READ_ONCE(x)` / `WRITE_ONCE(x, v)` — Prevents compiler and CPU from optimizing away or reordering accesses to `x`.
- `smp_store_release()` / `smp_load_acquire()` — Release/acquire semantics for lock-free data structures.

## Usage Patterns

### Example: Producer-Consumer
```c
// Producer
data = value;
smp_wmb();
flag = 1;

// Consumer
if (flag) {
	smp_rmb();
	assert(data == value);
}
```

### Example: Lock-Free Queue
```c
smp_store_release(&q->tail, new_tail);
// ...
val = smp_load_acquire(&q->head);
```

## Why Use These APIs?
- They abstract away architecture-specific details (e.g., DMB/DSB/ISB on ARM64, MFENCE/LFENCE/SFENCE on x86).
- They ensure correct ordering on all supported platforms, preventing subtle, platform-specific bugs.
- They are required for correct implementation of spinlocks, RCU, lock-free algorithms, and device drivers.

## Interview-Ready Insights
- Be able to explain the difference between hardware and compiler barriers.
- Know when to use each API, and why using the wrong one can break code on ARM64.
- Be ready to walk through code and identify where barriers are needed.

---

**Interview Tip:**
Always use the kernel APIs, not raw assembly, unless you have a very good reason. Be able to justify your barrier placement in code reviews and interviews.

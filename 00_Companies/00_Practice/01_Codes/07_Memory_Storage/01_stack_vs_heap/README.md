# 01 — Stack vs Heap

## Problem
Explain the differences between **stack** and **heap** memory in a C program, when each is used, and the failure modes of each.

## Why It Matters
The single most common systems interview question. Wrong intuition about lifetime and ownership leads to use-after-free, stack overflow, fragmentation, and security holes (stack smashing, heap-spray exploits).

## The Four Angles (treat each as an "approach")

### Angle 1 — Allocation Mechanism
| | Stack | Heap |
|---|---|---|
| Allocated by | function prologue (`sub rsp, N`) | `malloc / sbrk / mmap` |
| Freed by | function epilogue (`add rsp, N`) | `free / munmap` |
| Manager | compiler | runtime allocator |
| Cost per alloc | ~1 instruction | dozens–hundreds (free-list walk, syscalls) |

### Angle 2 — Lifetime & Scope
- **Stack**: lifetime tied to the enclosing function. Object dies when the function returns.
- **Heap**: lifetime explicit (`malloc` → `free`). Outlives the calling function.
- Returning a pointer to a stack local = **dangling pointer** (undefined behaviour).
```c
int *bad() { int x = 5; return &x; }  // x dies here — UB to read *p later
int *ok () { int *p = malloc(sizeof *p); *p = 5; return p; }  // caller must free
```

### Angle 3 — Layout & Growth Direction
```
0xFFFF... +-----------------+   high addr
          |     stack       |   grows DOWN
          |       v         |
          |       .         |
          |       ^         |
          |     heap        |   grows UP
          +-----------------+
          |   bss / data    |
          |   text          |   low addr
0x0000... +-----------------+
```
- Stack and heap grow toward each other; collision = OOM or stack overflow.
- Heap is contiguous virtual but physically scattered (paged on demand).

### Angle 4 — Failure Modes & Diagnostics
| Failure | Where | Symptom | Tool |
|---|---|---|---|
| Stack overflow | stack | SIGSEGV at top of stack | `ulimit -s`, gdb backtrace |
| Use-after-free | heap | random corruption, sometimes works | AddressSanitizer |
| Double free | heap | abort / heap corruption | ASan, glibc detection |
| Memory leak | heap | RSS grows | Valgrind, LeakSanitizer |
| Fragmentation | heap | malloc fails despite free RAM | jemalloc stats, custom pool |
| Stack smashing | stack | RIP overwritten | `-fstack-protector`, ASLR |

## Comparison Table
| Property | Stack | Heap |
|---|---|---|
| Speed | nanoseconds | tens–hundreds of ns |
| Size | small (KB–MB) | large (GB) |
| Fragmentation | none (LIFO) | possible (free-list) |
| Thread-safety of alloc | inherently per-thread | needs locks / per-thread arenas |
| Predictability | deterministic | depends on allocator state |
| Suits | short-lived, fixed size | dynamic size, long-lived |

## Key Insight
**The difference is lifetime, not location.** Stack memory is automatically reclaimed at scope exit; heap memory is reclaimed only when *you* `free` it. Speed and contiguity follow from that constraint.

## Pitfalls
- Returning address of a local variable
- Huge VLAs (`int buf[1<<20]`) on stack → overflow on embedded targets
- `alloca()` looks like stack alloc but still respects scope; danger in loops
- Not checking `malloc` return for `NULL`
- Mixing allocators (`malloc`'d, `delete`d in C++) → corruption
- Heap fragmentation in long-running embedded systems → use a fixed-size pool

## Interview Tips
1. Lead with "lifetime is the difference"; speed is a consequence.
2. Always mention the failure mode pair (overflow / use-after-free / fragmentation).
3. Cite the diagnostic tool — `ulimit -s`, Valgrind, ASan — shows real experience.
4. Embedded twist: "we ban heap entirely in safety-critical firmware; static pools only".

## Related / Follow-ups
- [04_memory_layout](../04_memory_layout/) — full memory map
- [11_malloc_free_impl (OS section)](../../08_OS_Kernel_Concurrency/11_malloc_free_impl/)
- `alloca()` vs `malloc()` vs VLAs
- Memory pools / slab allocators for embedded

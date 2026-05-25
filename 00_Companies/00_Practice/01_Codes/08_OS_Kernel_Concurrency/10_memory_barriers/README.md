# 10 — Memory Barriers (Fences)

## Problem
On a multicore CPU, both the **compiler** and the **hardware** may reorder loads and stores. To make one thread's writes visible to another in the intended order, you need barriers.

## Why It Matters
Every lock-free primitive (ring buffer, RCU, double-checked locking) is built on barriers. Get them wrong and your code "works" on x86 but corrupts data on ARM.

## Two Kinds of Reordering

### 1. Compiler Reordering
The compiler may move loads/stores around for optimisation, as long as single-threaded behaviour is preserved.
Fix: **compiler barrier** — `asm volatile("" ::: "memory")`, `atomic_signal_fence(memory_order_seq_cst)`, or any C11 atomic op (acts as compiler barrier at minimum).

### 2. CPU Reordering
Even after compilation, the CPU may execute instructions out of order, store-buffer writes, and let other cores observe them out of program order.
Fix: **hardware barrier** — `mfence`/`lfence`/`sfence` (x86), `dmb`/`dsb`/`isb` (ARM), `lwsync`/`sync` (POWER).

## Barrier Types

| Barrier | Prevents reordering | Notes |
|---|---|---|
| **Acquire** | later loads/stores moved **before** | pairs with release; cheap on x86 |
| **Release** | earlier loads/stores moved **after** | pairs with acquire |
| **Acq-Rel** | both | typical RMW |
| **Full / seq_cst** | all reordering, global total order | most expensive |
| **Compiler-only** | compiler reorderings only | for ISR-vs-main on a single core |

## Approaches

### Approach 1 — Use Atomics with Explicit Order (Portable, Recommended)
```text
// producer
data = 42;
atomic_store_explicit(&ready, 1, memory_order_release);

// consumer
if (atomic_load_explicit(&ready, memory_order_acquire) == 1)
    use(data);                          // guaranteed to see data == 42
```
- The release-acquire pair propagates **all** prior writes.

### Approach 2 — Standalone Fences
```text
atomic_thread_fence(memory_order_release);
atomic_thread_fence(memory_order_acquire);
```
Used when the flag isn't itself atomic, or for inter-thread visibility around plain loads/stores. Rarely needed if you use atomic ops on the flag.

### Approach 3 — Linux Kernel-Style Barriers
- `barrier()` — compiler only (`asm volatile("" ::: "memory")`)
- `smp_mb()` — full memory barrier (on UP: just compiler)
- `smp_rmb()` / `smp_wmb()` — read- / write-only
- `READ_ONCE(x)` / `WRITE_ONCE(x)` — prevent torn / coalesced accesses
- `smp_load_acquire` / `smp_store_release` — typed wrappers

### Approach 4 — Compiler Barrier vs Hardware Barrier
- ISR on the **same** core sharing data with main loop → only need a **compiler** barrier (the core's pipeline preserves program order locally).
- Cross-core sharing → need **hardware** barrier.
- Don't confuse the two: `volatile` ≈ compiler barrier for that single access; useless across cores.

### Approach 5 — Use Lock-Inclusive Primitives Where Possible
A mutex unlock implies release; lock implies acquire. As long as you use these primitives, you almost never write explicit fences in user-space app code — fences are a **lock-free coding** concern.

## ASCII — Why Acquire/Release Works
```
T1 (producer):       T2 (consumer):
  data = 42;            r1 = load_acquire(ready);
  store_release(ready,1);   if (r1==1)
                            r2 = data;   // sees 42
                            
Release: all earlier writes (data=42) happen-before the store to ready.
Acquire: the load of ready=1 happens-before all later reads (read of data).
Composition: data=42 happens-before read-of-data.
```

## Comparison: Architectures
| Arch | x86/x64 | ARMv8 | POWER | RISC-V |
|---|---|---|---|---|
| Default ordering | TSO (strong) | weak | weak | weak (default; Ztso opt-in) |
| Load-load reorder | no | yes | yes | yes |
| Store-store reorder | no | yes | yes | yes |
| Load-store reorder | no | yes | yes | yes |
| Store-load reorder | **yes** | yes | yes | yes |
| Need explicit fences | rarely | often | often | often |

On x86 you can "get lucky" omitting acquire/release; the same code crashes on ARM.

## Pitfalls
- `volatile` cross-thread visibility — **wrong**. It's for compiler-reorder-prevention on a single access, not cross-CPU.
- Double-checked locking without acquire/release on the flag — classic broken pattern; works only on x86, breaks on ARM.
- Fences on the wrong variable: release-acquire must pair on the **same** atomic variable.
- Mixing relaxed and seq_cst across cooperating threads → may violate transitivity.
- Cost: `mfence` on x86 ≈ 30+ cycles; `dmb ish` on ARM is cheaper but still tens of cycles. Don't sprinkle.
- `atomic_signal_fence` only orders against signal handlers on the **same** thread; not for cross-thread.
- Cache coherency ≠ memory ordering. Coherency guarantees eventual single-value-per-line; ordering controls when other accesses become visible.

## Interview Tips
1. State the two reorderings (compiler + CPU) and the two barriers (compiler / hardware).
2. Show the release/acquire example as the canonical "flag publishes data" pattern.
3. Cite that x86 is TSO (mostly), ARM is weak — code must be written for the weakest model you target.
4. `volatile` is not a memory barrier; atomics are. Mention this preemptively.

## Related / Follow-ups
- [09_atomic_counter](../09_atomic_counter/)
- [01_ring_buffer](../01_ring_buffer/) — release on producer tail, acquire on consumer
- Hans Boehm, "Threads cannot be implemented as a library"
- Linux `Documentation/memory-barriers.txt`
- C++ memory model (same as C11)

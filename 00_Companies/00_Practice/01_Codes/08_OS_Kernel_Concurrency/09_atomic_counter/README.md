# 09 — Atomic Counter

## Problem
A counter incremented and read by multiple threads without locks, with well-defined memory ordering.

## Why It Matters
The first stepping stone into lock-free code. Misuse of `volatile` for atomicity is one of the most common embedded/firmware bugs — atomics are the standardised, portable, ordered fix.

## Approaches

### Approach 1 — Mutex Around an `int` (Baseline)
```text
lock(m); count++; unlock(m);
```
- Trivially correct, portable.
- Mutex overhead per op (~25 ns uncontended).

### Approach 2 — GCC `__sync_*` Builtins (Legacy)
```text
__sync_fetch_and_add(&count, 1);
__sync_val_compare_and_swap(&count, old, new);
```
- Full memory barriers around every op.
- Works on GCC/Clang/ICC; deprecated in favour of `__atomic_*`.

### Approach 3 — GCC `__atomic_*` Builtins (Recommended for C ≤99)
```text
__atomic_fetch_add(&count, 1, __ATOMIC_RELAXED);
__atomic_load_n     (&count,    __ATOMIC_ACQUIRE);
__atomic_store_n    (&count, v, __ATOMIC_RELEASE);
```
- Per-op memory order: `relaxed`, `consume`, `acquire`, `release`, `acq_rel`, `seq_cst`.
- Available even in C89/C99 code that needs portability across compilers (with builtins).

### Approach 4 — C11 `<stdatomic.h>` (Standard)
```c
#include <stdatomic.h>
atomic_int count = 0;
atomic_fetch_add_explicit(&count, 1, memory_order_relaxed);
int v = atomic_load_explicit(&count, memory_order_acquire);
```
- Standard since C11. Equivalent semantics to `__atomic_*`.
- `atomic_fetch_add` (no `_explicit`) → defaults to `seq_cst` — easy but slow.

### Approach 5 — Per-CPU Counters + Aggregate on Read (Scalable)
For high-contention counters (network packets, stats), give each CPU its own counter; increment locally (no contention), sum on read.
- Linux: `percpu_counter`, `local_t`.
- O(1) increment, O(NCPU) read.

### Approach 6 — RCU-Protected Counter
For very-read-heavy stats: increment under a brief lock or per-CPU, read locklessly with seqlock.

## Memory Order Choices for a Counter
- **`relaxed`** — only atomicity, no ordering. Correct for **pure counters** where the value isn't used to gate other memory accesses.
- **`acquire`/`release`** — pair when the counter publishes data ("ready" flag).
- **`seq_cst`** — global total order; default; slowest. Use when in doubt.

```text
// Producer publishes data with release; consumer reads with acquire
store_release(&flag, 1);    // any prior writes visible after acquire
if load_acquire(&flag) == 1: read data
```

## Comparison
| Approach | Atomicity | Ordering control | Speed | Portability |
|---|---|---|---|---|
| Mutex + int | yes | sequential (lock) | low | universal |
| `__sync_*` | yes | full barrier each op | medium | GCC family |
| `__atomic_*` | yes | per-op | high | GCC/Clang |
| C11 atomics | yes | per-op | high | C11+ |
| Per-CPU + agg | yes | local | very high | Linux idiom |

## Key Insight
- `volatile` ≠ atomic. `volatile` prevents the compiler from optimising loads/stores away; it does **not** prevent torn reads, CPU reordering, or guarantee single-instruction RMW. **Use atomics for cross-thread; `volatile` only for MMIO / setjmp / signal.**
- Pick the **weakest** memory order that's correct; `seq_cst` is the safe-but-slow default.
- A counter that doesn't gate other memory can almost always be `relaxed`.

## Pitfalls
- `count++` on a `volatile int` shared between threads — not atomic (load, add, store; lost update under contention)
- Defaulting all atomics to `seq_cst` "to be safe" → 2–10× slower on weakly-ordered CPUs (ARM, POWER)
- Storing a pointer with `relaxed` and reading it with `acquire` → consumer sees pointer but stale data (release-acquire pair must be on the **same** variable)
- Mixing atomic and non-atomic accesses to the same location → UB
- 64-bit counter on a 32-bit MCU: not naturally atomic; use `atomic_uint_fast64_t` (still may use a lock under the hood) or two-32-bit-words tricks
- ABA on a counter used as a generation tag — use 128-bit or wide tag

## Interview Tips
1. Open by stating "`volatile` is not atomic" — directly addresses the most-common interview trap.
2. Show the four memory orders (`relaxed`, `acquire`, `release`, `seq_cst`) and when each fits.
3. Mention `<stdatomic.h>` (C11) as the portable answer; mention `__atomic_*` as the GCC fallback.
4. For high-throughput stats, propose per-CPU counters — shows scaling awareness.

## Related / Follow-ups
- [10_memory_barriers](../10_memory_barriers/)
- [03_volatile_keyword](../../07_Memory_Storage/03_volatile_keyword/)
- Linux `atomic_t`, `percpu_counter`, `READ_ONCE`/`WRITE_ONCE`
- C++11 `std::atomic`
- Maurice Herlihy, *The Art of Multiprocessor Programming*

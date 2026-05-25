# Reverse Bits of a 32-bit Unsigned Integer

## Problem
Given a 32-bit unsigned integer `n`, return the integer obtained by
reversing the order of its bits.

**Example**
```
Input  : n = 43261596            (00000010100101000001111010011100)
Output : 964176192               (00111001011110000010100101000000)
```

---

## Approaches (Ranked: Most Optimized → Least)

| Rank | Approach              | File                                           | Time      | Space  | Notes                                       |
|------|-----------------------|------------------------------------------------|-----------|--------|---------------------------------------------|
| 1    | Lookup Table          | [rb_lookup_table.c](rb_lookup_table.c)         | O(1)      | O(256) | Best for repeated calls (precomputed bytes) |
| 2    | Byte Swap + Mask      | [rb_byte_swap.c](rb_byte_swap.c)               | O(1)      | O(1)   | `__builtin_bswap32` + 3 mask swaps          |
| 3    | Divide & Conquer      | [rb_parallel_swap.c](rb_parallel_swap.c)       | O(1)      | O(1)   | 5 parallel mask swaps, no builtins          |
| 4    | Recursive D&C         | [rb_recursive_divide.c](rb_recursive_divide.c) | O(log 32) | O(log) | Same idea, recursion stack overhead         |
| 5    | Two Pointer XOR Swap  | [rb_xor_mirror.c](rb_xor_mirror.c)             | O(32/2)   | O(1)   | Swap bit `i` with bit `31-i` via XOR        |
| 6    | Conditional XOR       | [rb_conditional_xor.c](rb_conditional_xor.c)   | O(16)     | O(1)   | Skip swap if bits already match             |
| 7    | Bit by Bit (Simple)   | [rb_bitwise_shift.c](rb_bitwise_shift.c)       | O(32)     | O(1)   | Easiest to understand, baseline             |

> **Interview pick:** Parallel Swap (rb_parallel_swap.c) — fastest portable
> solution without lookup memory or compiler builtins.
>
> **Production pick (many queries):** Lookup Table (rb_lookup_table.c).
>
> **Embedded/Resource-constrained pick:** Parallel Swap (rb_parallel_swap.c) —
> zero loops, zero branches, zero extra memory, deterministic 5 operations.

---

## Why the Top Approaches Are Optimal

### 1. Lookup Table — fastest amortized
- Precompute reverse of every byte (0..255) once → 256-entry table.
- Split 32-bit value into 4 bytes, look each up, place in mirrored slot.
- Only 4 table reads + shifts per call → near memory-bound speed.

### 2. Byte Swap + Mask
- `__builtin_bswap32` reverses byte order in a single CPU instruction.
- Then reverse bits *inside* each byte with 3 parallel mask swaps
  (nibble → 2-bit → 1-bit).
- ~4 fast ops, no branches, no memory loads.

### 3. Divide & Conquer (Pure Bitmask)
Swap progressively smaller chunks in parallel:
1. Swap 16-bit halves
2. Swap 8-bit chunks within each 16-bit half
3. Swap 4-bit nibbles
4. Swap 2-bit pairs
5. Swap adjacent bits

Five operations, fully branchless — ideal when builtins aren't allowed.

---

## Complexity Summary

All approaches are O(1) for a fixed 32-bit width; what really matters is
the *operation count* per call.

| Metric              | Best                     | Worst                |
|---------------------|--------------------------|----------------------|
| Operations per call | ~4 (lookup/byte swap)    | ~64 (simple loop)    |
| Extra memory        | 0 (parallel swap)        | 256 bytes (lookup)   |
| Branches            | 0 (parallel swap)        | 32 (simple loop)     |
| Loop iterations     | 0 (parallel swap)        | 32 (simple loop)     |

---

## Best Approach by Use Case

| Use Case                          | Recommended Approach       | Reason                                           |
|-----------------------------------|----------------------------|--------------------------------------------------|
| **Embedded systems**              | rb_parallel_swap.c         | Zero loops, zero branches, O(1) stack, 5 ops    |
| **Real-time / deterministic**     | rb_parallel_swap.c         | No loops = predictable execution time            |
| **High throughput (many calls)**  | rb_lookup_table.c          | Amortized fastest with precomputed table         |
| **Code readability / learning**   | rb_bitwise_shift.c         | Simplest logic, easy to understand               |
| **Compiler optimization allowed** | rb_byte_swap.c             | Uses `__builtin_bswap32` for single instruction  |
| **Interview / whiteboard**        | rb_parallel_swap.c         | Shows bit manipulation mastery, no dependencies  |

---

## Build & Run
```bash
gcc rb_parallel_swap.c -O2 -o reverse && ./reverse
```

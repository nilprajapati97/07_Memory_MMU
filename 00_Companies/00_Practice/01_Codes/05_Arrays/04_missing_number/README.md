# 04 — Missing Number from 1..N

## Problem
An array contains `n-1` distinct integers from `1..n`. Find the one that's missing.

```
Input : [3, 7, 1, 2, 8, 4, 5], n=8
Output: 6
```

## Why It Matters
Tests command of arithmetic formulas, bitwise identities, and overflow awareness — all daily concerns in embedded code. Variants underlie sequence-number checks, packet-loss detection, dropped-frame counters.

## Approaches

### Approach 1 — Sort and Scan
```text
sort(a)
for i in 0..n-2:
    if a[i] != i+1: return i+1
return n
```
- Time: **O(n log n)**, Space: **O(1)** — destroys order

### Approach 2 — Hash Set / Bitmap
Mark each seen value; scan 1..n for the unmarked one.
- Time: **O(n)**, Space: **O(n)** bits

### Approach 3 — Sum Formula (Gauss)
```text
expected = n*(n+1)/2
actual   = sum(a)
missing  = expected - actual
```
- Time: **O(n)**, Space: **O(1)**
- **Overflow risk:** `n*(n+1)` overflows 32-bit at `n ≈ 92681`. Use 64-bit accumulator or pairwise diff (Approach 3b).

### Approach 3b — Pairwise Subtraction (Overflow-Safe)
```text
missing = n
for i in 0..n-2:
    missing += (i+1) - a[i]
```
Each partial value stays bounded by `n`.

### Approach 4 — XOR (Best, Overflow-Free)
```text
x = 0
for i in 1..n:    x ^= i
for v in a:       x ^= v
return x          // duplicates cancel; the missing one survives
```
ASCII on `[3,1,2]`, n=4:
```
1^2^3^4 = 4
4 ^ 3 ^ 1 ^ 2 = 4  ✓
```
- Time: **O(n)**, Space: **O(1)** — no overflow, single accumulator
- Works for any width / signed-ness

### Approach 5 — Cyclic Placement (In-Place)
For each index, swap `a[i]` to position `a[i]-1` until correct, then scan for `a[i] != i+1`.
- Time: **O(n)** amortised, Space: **O(1)**
- Modifies input but doesn't need extra accumulator. Useful when arrays are mutable and you want to detect *both* missing and duplicate (see 05_duplicate_number).

## Comparison
| Approach | Time | Space | Overflow-safe | When to use |
|---|---|---|---|---|
| Sort | O(n log n) | O(1) | yes | Already sorting |
| Hash | O(n) | O(n) | yes | When range ≠ 1..n |
| Sum | O(n) | O(1) | **no** | Quick whiteboard |
| Pairwise sum | O(n) | O(1) | yes | Embedded, 32-bit limit |
| **XOR** | O(n) | O(1) | **yes** | **Default answer** |
| Cyclic | O(n) | O(1) | yes | Also finds duplicate |

## Key Insight
XOR is its own inverse: `x ^ x = 0`. XOR-ing `1..n` with all array elements cancels every value that appears in both — only the missing one stays.

## Pitfalls
- `n*(n+1)/2` overflowing — use `1ULL * n * (n+1) / 2`
- Off-by-one when array uses 0-based values 0..n
- Two missing → sum/XOR alone is insufficient (need to also use sum-of-squares or split-by-bit, see 05_two_non_repeating in Bit-Manipulation)
- Cyclic placement on duplicates → infinite swap loop unless guarded

## Interview Tips
1. Ask: are values exactly `1..n` or `0..n`? Exactly one missing?
2. Lead with XOR — shows you know overflow matters.
3. If they say "one missing AND one duplicate" → cyclic placement (Approach 5) or `XOR + sum` combo.

## Related / Follow-ups
- Two missing numbers — XOR + bit split trick
- Missing in `1..n` with duplicates allowed
- [05_duplicate_number](../05_duplicate_number/) — symmetric problem
- [Bit Manipulation 05_two_non_repeating](../../01_Bit_Manipulation/05_two_non_repeating/)

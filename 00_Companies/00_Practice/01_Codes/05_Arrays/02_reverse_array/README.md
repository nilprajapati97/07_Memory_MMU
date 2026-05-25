# 02 — Reverse an Array In Place

## Problem
Reverse the elements of an array of length `n` using **O(1) extra space**.

```
Input : [1, 2, 3, 4, 5]
Output: [5, 4, 3, 2, 1]
```

## Why It Matters
Building block for: array rotation (reversal trick), palindrome checks, stack simulation, DMA descriptor ring re-ordering, endian flip on a buffer.

## Approaches

### Approach 1 — Two-Pointer Swap (Best)
```text
l = 0; r = n-1
while l < r:
    swap(a[l], a[r])
    l++; r--
```
ASCII:
```
[1 2 3 4 5]   l=0 r=4 → swap
[5 2 3 4 1]   l=1 r=3 → swap
[5 4 3 2 1]   l=2 r=2 → stop
```
- Time: **O(n/2)**, Space: **O(1)** — branch-predictable, cache-friendly

### Approach 2 — Recursion
```text
reverse(a, l, r):
    if l >= r: return
    swap(a[l], a[r])
    reverse(a, l+1, r-1)
```
- Time: **O(n)**, Space: **O(n)** stack — risks overflow on big `n`

### Approach 3 — XOR Swap (Trick)
Same as Approach 1 but `swap` via XOR:
```text
a[l] ^= a[r]; a[r] ^= a[l]; a[l] ^= a[r]
```
- No temp variable. **Pitfall:** undefined if `l == r` (XOR with self → 0). Guard with `if l != r`.
- Time/Space same as Approach 1. Generally **slower** than a temp swap on modern CPUs.

### Approach 4 — Auxiliary Array
```text
for i in 0..n-1: b[i] = a[n-1-i]
copy b -> a
```
- Time: **O(n)**, Space: **O(n)** — disqualifies "in place"; mention only to contrast.

### Approach 5 — Library
`std::reverse` (C++) / `memrev`-style helpers. In C use Approach 1 — no standard library reverse for arrays.

## Comparison
| Approach | Time | Space | When to use |
|---|---|---|---|
| Two-pointer | O(n) | O(1) | **Default** |
| Recursion | O(n) | O(n) stack | Teaching recursion only |
| XOR swap | O(n) | O(1) | Embedded "no temp" trivia |
| Aux array | O(n) | O(n) | Never (fails in-place) |

## Key Insight
Walk two indices toward the middle. Loop terminates at `l >= r` — natural stop for both even (`l > r`) and odd (`l == r`) lengths.

## Pitfalls
- XOR self-aliasing → zero the value (use `if l != r`)
- Off-by-one: `r = n` instead of `n-1`
- Reverse on a `char*` C-string: stop at `n-1` (don't swap the terminating `\0`)
- Multi-byte / wide chars: reversing bytes corrupts UTF-8 — reverse code-points instead

## Interview Tips
1. State complexity first: O(n) time, O(1) space.
2. Write the two-pointer loop; mention recursion only as contrast.
3. Common follow-up: **reverse a range `[l, r]`** — same routine, parameterised → enables the array-rotation reversal trick (see 03_rotate_array).
4. Follow-up: reverse a linked list — different pattern (pointer-rewire, not swap).

## Related / Follow-ups
- [03_rotate_array](../03_rotate_array/) — uses reverse-3-times trick
- Reverse string in C with `\0` (Strings section)
- Reverse linked list (Linked Lists section)
- Reverse bits of an integer (Bit Manipulation 07)

# 03 — Rotate Array by K Positions

## Problem
Rotate an array of size `n` to the right by `k` positions (left-rotation is symmetric).

```
Input : a=[1,2,3,4,5,6,7], k=3
Output: [5,6,7,1,2,3,4]
```

## Why It Matters
Ring-buffer index math, circular shift in DSP, cache line rotation, register file renaming. The "reversal trick" is a classic minimum-space algorithm interviewers love.

## Approaches

### Approach 1 — Extra Array
```text
k = k % n
for i in 0..n-1:
    b[(i+k) % n] = a[i]
copy b -> a
```
- Time: **O(n)**, Space: **O(n)** — clearest but uses aux memory

### Approach 2 — Shift-by-One, K Times
Rotate by 1 (save last, shift all right, restore at index 0). Repeat `k` times.
- Time: **O(n·k)**, Space: **O(1)** — terrible for large `k`

### Approach 3 — Juggling Algorithm (GCD Cycles)
```text
k = k % n
for start in 0..gcd(n,k)-1:
    temp = a[start]; i = start
    loop:
        j = (i + k) % n
        if j == start: break
        a[i] = a[j]; i = j
    a[i] = temp
```
- Time: **O(n)**, Space: **O(1)** — moves each element exactly once
- Conceptually trickiest; need to grasp that orbits = `gcd(n,k)` disjoint cycles

### Approach 4 — Reversal Trick (Best Interview Answer)
```text
k = k % n
reverse(a, 0,   n-1)   // entire array
reverse(a, 0,   k-1)   // first k
reverse(a, k,   n-1)   // rest
```
ASCII on `[1,2,3,4,5,6,7]`, k=3:
```
reverse all   → [7,6,5,4,3,2,1]
reverse 0..2  → [5,6,7,4,3,2,1]
reverse 3..6  → [5,6,7,1,2,3,4]   ✓
```
- Time: **O(n)** (3 passes ≈ 2n swaps), Space: **O(1)**
- Easiest to remember & explain

### Approach 5 — Block Swap (Recursive)
Treat the array as `A|B` with `|A|=n-k, |B|=k`; recursively swap blocks. Time O(n), Space O(log n) stack. Rarely asked.

## Comparison
| Approach | Time | Space | When to use |
|---|---|---|---|
| Extra array | O(n) | O(n) | Memory cheap, clarity wins |
| Shift × k | O(n·k) | O(1) | Never beyond tiny `k` |
| Juggling | O(n) | O(1) | Show off cycle math |
| **Reversal** | O(n) | O(1) | **Default answer** |
| Block swap | O(n) | O(log n) | Recursion practice |

## Key Insight
Right-rotate by `k` ≡ split at `n-k` and swap the two halves. Reversal trick implements this swap without extra memory: reverse whole, then each half independently.

## Pitfalls
- Forgetting `k %= n` → out-of-bounds or wasted work
- Negative `k` (left rotation) → use `k = ((k % n) + n) % n`
- `n == 0` → divide by zero on `k % n`
- Off-by-one in `reverse(a, k, n-1)` — endpoints inclusive
- In Approach 3, looping using `count` instead of GCD cycles → revisits or misses

## Interview Tips
1. Clarify: in-place required? Left or right? `k > n` allowed? Negative `k`?
2. Lead with **reversal trick** — clean, O(1) space, easy to whiteboard.
3. Mention juggling when asked "any way without 3 passes?" — single-pass O(n).
4. For huge arrays held in mmap'd files, prefer block-swap to limit page touches.

## Related / Follow-ups
- [02_reverse_array](../02_reverse_array/) — building block
- Rotate by 1 → ring buffer next-pointer move
- Search in rotated sorted array → [10_binary_search](../10_binary_search/)
- String rotation → check if `s2` is a rotation of `s1` via `(s1+s1).contains(s2)`

# 05 — Duplicate Number in O(1) Space

## Problem
Given an array of `n+1` integers where each value is in `1..n`, exactly one number is duplicated (it may appear more than twice). Find it **without modifying the array** and in **O(1) extra space** if possible.

```
Input : [1, 3, 4, 2, 2]
Output: 2
```

## Why It Matters
Classic "find the loop entry" trick (Floyd's algorithm) reused in linked-list cycle detection, hash collisions, and detecting repeated sequence numbers in protocol streams.

## Approaches

### Approach 1 — Sort
Sort, then find adjacent equal pair.
- Time: **O(n log n)**, Space: **O(1)**, **mutates array**

### Approach 2 — Hash Set
First repeated insert wins.
- Time: **O(n)**, Space: **O(n)**

### Approach 3 — Sum / XOR (Single Duplicate, Single Range)
If exactly one number repeats *exactly twice*: `dup = sum(a) - n*(n+1)/2`.
- Overflow risk; doesn't work if duplicate appears 3+ times

### Approach 4 — Negative Marking (Mutates)
```text
for x in a:
    idx = abs(x) - 1
    if a[idx] < 0: return abs(x)
    a[idx] = -a[idx]
```
- Time: **O(n)**, Space: **O(1)**; **mutates input** — must restore after

### Approach 5 — Cyclic Swap In-Place
Place each value `v` at index `v-1`. If destination already holds `v`, it's the duplicate.
- Time: **O(n)** amortised, Space: **O(1)**, mutates

### Approach 6 — Floyd's Tortoise & Hare (Best when read-only)
Treat `a[i]` as a "next pointer": `i → a[i]`. With `n+1` values in `1..n`, the function `f(i)=a[i]` must form a cycle, and its entry point is the duplicate.
```text
// Phase 1: find meeting point
slow = a[0]; fast = a[0]
do:
    slow = a[slow]
    fast = a[a[fast]]
while slow != fast

// Phase 2: find cycle entry = duplicate
slow = a[0]
while slow != fast:
    slow = a[slow]
    fast = a[fast]
return slow
```
ASCII on `[1,3,4,2,2]` (indices 0..4, values are pointers):
```
0 -> 1 -> 3 -> 2 -> 4 -> 2 -> 4 ...
                    ^----cycle----^
                    entry = 2 = duplicate
```
- Time: **O(n)**, Space: **O(1)**, **read-only** — the textbook answer

## Comparison
| Approach | Time | Space | Read-only | When to use |
|---|---|---|---|---|
| Sort | O(n log n) | O(1) | no | Trivial code |
| Hash | O(n) | O(n) | yes | Memory cheap |
| Sum/XOR | O(n) | O(1) | yes | Exactly twice, no overflow |
| Negative mark | O(n) | O(1) | no | Allowed to mutate |
| Cyclic swap | O(n) | O(1) | no | Also detect missing |
| **Floyd cycle** | O(n) | O(1) | **yes** | **Default answer** |

## Key Insight
With `n+1` values drawn from `1..n`, the array defines a function `f: [0..n] → [1..n]` whose functional graph **must** contain a cycle (pigeonhole). The cycle's entry point is reached by exactly two predecessors — those are the two occurrences of the duplicate.

## Pitfalls
- Index 0 still valid as a starting point; values are 1..n so `a[0]` never points to 0 → never returns to start except via the cycle.
- If values were `0..n-1`, Floyd needs adjustment (shift or different start).
- Negative-marking on already-negative input → must use `abs()` and document mutation.
- Multiple duplicates → Floyd finds one; problem statement usually guarantees uniqueness.

## Interview Tips
1. Ask: can I modify the array? Memory budget? More than one duplicate possible?
2. If read-only + O(1) space → Floyd. Walk them through the pointer-graph analogy.
3. If mutation allowed → negative marking is shorter to code.

## Related / Follow-ups
- Linked-list cycle detection (same Floyd algorithm)
- [04_missing_number](../04_missing_number/) — symmetric
- Find both missing & duplicate → cyclic placement
- "Happy number" problem — Floyd on a number sequence

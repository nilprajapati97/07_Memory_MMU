# 11 — Pair with Given Sum

## Problem
Given an array and a target sum `S`, find any pair (or all pairs / count) whose sum equals `S`.

```
Input : a=[2, 7, 11, 15], S=9
Output: (2, 7)  or indices (0, 1)
```

## Why It Matters
Two-sum is the gateway to many "sum problems" (3-sum, 4-sum, closest sum, subarray sum equals K). Drives core patterns: hashing and two-pointer-on-sorted.

## Approaches

### Approach 1 — Brute Force
Try every pair.
- Time: **O(n²)**, Space: **O(1)**

### Approach 2 — Sort + Two Pointer (Best for sorted / when O(1) memory)
```text
sort(a)
l = 0; r = n-1
while l < r:
    s = a[l] + a[r]
    if s == S: return (a[l], a[r])
    if s <  S: l++
    else:      r--
return NONE
```
ASCII on sorted `[1,2,4,7,11]`, S=9:
```
l=0 r=4  1+11=12 >9 → r=3
l=0 r=3  1+7 =8  <9 → l=1
l=1 r=3  2+7 =9  ✓
```
- Time: **O(n log n)** (sort) + **O(n)** (scan), Space: **O(1)** if sort is in-place
- Drops to **O(n)** total if already sorted

### Approach 3 — Hash Set (Best for unsorted, original indices)
```text
seen = {}
for i, x in a:
    if (S - x) in seen: return (seen[S-x], i)
    seen[x] = i
return NONE
```
- Time: **O(n)**, Space: **O(n)**
- Single pass; preserves original indices

### Approach 4 — BST / Balanced Tree
Insert each element; for next element check if `S - x` exists.
- Time: **O(n log n)**, Space: **O(n)** — rarely justified vs hash

### Approach 5 — Bitmap (Small Value Range)
If values fit a small range (e.g. uint16), a bitset replaces the hash set.
- Time: **O(n)**, Space: **O(range/8)** bytes
- Great for embedded with constrained types

## Comparison
| Approach | Time | Space | Needs sorted | Keeps indices |
|---|---|---|---|---|
| Brute | n² | 1 | no | yes |
| **Two-pointer** | n log n | 1 | yes | loses original idx |
| **Hash** | n | n | no | yes |
| BST | n log n | n | no | yes |
| Bitmap | n | range/8 | no | one occurrence only |

## Key Insight
- On sorted data, monotonicity lets the two-pointer prune half the search space at each step.
- On unsorted data, hashing turns "find complement" into O(1) lookup.

## Pitfalls
- Returning the same index twice when `x == S/2` and the value appears only once — check `seen[S-x] != i`
- Duplicates: if asked for **all** pairs, two-pointer needs skip-over loops to avoid repeats
- Overflow on `a[l] + a[r]` — use wider type if values near INT_MAX
- Floating-point sums — exact equality is fragile; use tolerance
- Asked for indices on already-sorted input but with positions of the *original* array → must hash

## Interview Tips
1. Ask: sorted? need indices? all pairs or just one? duplicates allowed?
2. Hash is the universal answer; two-pointer wins only when sort is free or memory is tight.
3. Natural escalation: 3-sum → fix one element, two-pointer the rest → O(n²).

## Related / Follow-ups
- 3-sum, 4-sum, closest sum
- Subarray sum equals K → prefix sum + hashmap
- Pair with given difference (sort + two-pointer same-direction)
- Count pairs with sum < S (two-pointer counting)

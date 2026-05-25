# 07 — Kadane's Algorithm — Max Subarray Sum

## Problem
Given an integer array (positive, negative, zero), find the contiguous subarray with the largest sum.

```
Input : [-2, 1, -3, 4, -1, 2, 1, -5, 4]
Output: 6   (subarray [4, -1, 2, 1])
```

## Why It Matters
Foundation for many DP problems; appears as a building block in signal-processing peak detection, profit-from-stock variants, and longest run of "good" status codes in monitoring.

## Approaches

### Approach 1 — Brute Force (Triple Loop)
Try every (i,j) and sum.
- Time: **O(n³)**, Space: **O(1)**

### Approach 2 — Brute Force with Running Sum
Fix `i`, sweep `j`, accumulate.
- Time: **O(n²)**, Space: **O(1)**

### Approach 3 — Prefix Sum + Min Tracking
```text
prefix = 0; min_prefix = 0; best = -INF
for x in a:
    prefix += x
    best = max(best, prefix - min_prefix)
    min_prefix = min(min_prefix, prefix)
```
- Time: **O(n)**, Space: **O(1)** — equivalent to Kadane

### Approach 4 — Kadane (Best)
```text
best = -INF
cur  = 0
for x in a:
    cur  = max(x, cur + x)    // start fresh or extend
    best = max(best, cur)
```
ASCII trace on `[-2,1,-3,4,-1,2,1,-5,4]`:
```
x=-2  cur=-2  best=-2
x= 1  cur= 1  best= 1
x=-3  cur=-2  best= 1
x= 4  cur= 4  best= 4
x=-1  cur= 3  best= 4
x= 2  cur= 5  best= 5
x= 1  cur= 6  best= 6   ← answer
x=-5  cur= 1  best= 6
x= 4  cur= 5  best= 6
```
- Time: **O(n)**, Space: **O(1)**

### Approach 5 — Divide & Conquer
Best subarray is in left half, right half, or crosses the mid (compute via two linear sweeps).
- Time: **O(n log n)**, Space: **O(log n)** — useful as a teaching segue to segment trees

### Approach 6 — Circular Variant
Max circular subarray = max( Kadane(a), total - Kadane_min(a) ). Edge case: all negative → return Kadane(a).
- Time: **O(n)**, Space: **O(1)**

## Comparison
| Approach | Time | Space | When to use |
|---|---|---|---|
| Brute O(n³) | n³ | 1 | Never |
| Brute O(n²) | n² | 1 | Tiny `n`, clarity |
| Prefix-min | n | 1 | Generalises to range queries |
| **Kadane** | n | 1 | **Default answer** |
| D&C | n log n | log n | Lead-in to segment trees |
| Circular | n | 1 | Wrap-around variant |

## Key Insight
At each index, the answer either **starts fresh at x** or **extends the previous best ending here**. Choosing the maximum of the two is greedy-optimal because any larger sum ending later must include this choice or restart.

## Pitfalls
- All-negative input — initializing `best = 0` returns wrong answer; use `-INF` or `a[0]`
- Empty array — return error / 0 by convention (clarify with interviewer)
- Overflow on large positive sums — use 64-bit accumulator if values are 32-bit max
- Asked for indices: also track `start` (reset when `cur` resets) and `end`
- Circular variant: handle "all negative" specially (don't subtract entire array)

## Interview Tips
1. Clarify: empty? all-negative? need indices? circular?
2. Lead with Kadane. State invariant: "`cur` is the best sum ending at this index."
3. Often paired follow-up: max product subarray — track both max **and min** (negatives flip signs).

## Related / Follow-ups
- Max product subarray (track min too)
- Max sum rectangle in 2D (Kadane over compressed rows)
- Longest subarray with sum ≤ K
- Stock buy-sell with one transaction = Kadane on differences

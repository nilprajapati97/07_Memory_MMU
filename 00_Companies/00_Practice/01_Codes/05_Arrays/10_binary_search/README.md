# 10 — Binary Search (Iterative, Recursive, Variants)

## Problem
Find target `t` in a sorted array; return its index or −1. Variants: first/last occurrence, lower/upper bound, search in rotated array.

```
Input : a=[1,3,5,7,9,11], t=7
Output: 3
```

## Why It Matters
Universal logarithmic-time lookup. Generalises beyond arrays: searching for a "smallest valid answer" in a monotonic predicate (binary search on the answer) is a top algorithmic pattern.

## Approaches

### Approach 1 — Iterative (Best Default)
```text
lo = 0; hi = n - 1
while lo <= hi:
    mid = lo + (hi - lo) / 2         // overflow-safe
    if a[mid] == t: return mid
    if a[mid] <  t: lo = mid + 1
    else:           hi = mid - 1
return -1
```
- Time: **O(log n)**, Space: **O(1)**

### Approach 2 — Recursive
Same logic; recurse on the appropriate half.
- Time: **O(log n)**, Space: **O(log n)** stack — risk on deeply embedded stacks

### Approach 3 — Lower Bound (First `≥ t`)
```text
lo = 0; hi = n
while lo < hi:
    mid = lo + (hi - lo) / 2
    if a[mid] < t: lo = mid + 1
    else:          hi = mid
return lo    // may equal n if none
```
- Half-open `[lo, hi)` — easier off-by-one reasoning

### Approach 4 — Upper Bound (First `> t`)
Same as lower bound but `a[mid] <= t → lo = mid + 1`. Count of `t` = `upper(t) - lower(t)`.

### Approach 5 — First / Last Occurrence
Binary search but never stop on match — keep narrowing toward the desired side.
```text
// first occurrence
res = -1
while lo <= hi:
    mid = lo + (hi-lo)/2
    if a[mid] == t: res = mid; hi = mid - 1
    else if a[mid] < t: lo = mid + 1
    else: hi = mid - 1
return res
```

### Approach 6 — Search in Rotated Sorted Array
```text
while lo <= hi:
    mid = lo + (hi-lo)/2
    if a[mid] == t: return mid
    if a[lo] <= a[mid]:                   // left half sorted
        if a[lo] <= t < a[mid]: hi = mid - 1
        else:                   lo = mid + 1
    else:                                  // right half sorted
        if a[mid] < t <= a[hi]: lo = mid + 1
        else:                   hi = mid - 1
return -1
```
- One half is always sorted; decide which half could contain `t`.

### Approach 7 — Binary Search on the Answer
When you have a monotonic predicate `feasible(k)`, binary-search the smallest `k` such that `feasible(k)` is true. Used for "minimum capacity to ship", "min eating speed", etc.

## Comparison
| Variant | Time | Space | Notes |
|---|---|---|---|
| Iterative | log n | 1 | Default |
| Recursive | log n | log n | Education only |
| Lower bound | log n | 1 | Insertion position |
| Upper bound | log n | 1 | Count via diff |
| First/Last | log n | 1 | Track best so far |
| Rotated | log n | 1 | Two-case branch |
| On answer | log(range)·f | 1 | Predicate-driven |

## Key Insight
- Use `mid = lo + (hi - lo)/2` to avoid `lo + hi` overflow.
- Choose your interval convention (`[lo, hi]` closed vs `[lo, hi)` half-open) **once** and stick with it; off-by-one bugs trace to mixing them.

## Pitfalls
- `(lo + hi) / 2` overflowing on large arrays (mainly 32-bit indices)
- Infinite loop when `lo = mid` instead of `mid + 1` in closed-interval form
- Searching unsorted input → undefined results
- Floating-point binary search: stop when `hi - lo < eps`, never on equality
- Rotated array with duplicates → worst case O(n) (must linearly skip equal endpoints)

## Interview Tips
1. State your interval convention up front; it removes 90% of bugs.
2. Always show the **overflow-safe mid**; interviewers grade on it.
3. If asked count of `t` → "upper - lower" in two log-n searches.
4. Generalise: "anywhere I have a monotonic property, binary search applies."

## Related / Follow-ups
- Find peak element
- Median of two sorted arrays (binary search on partition)
- Search in 2D matrix (treat as 1D, or staircase)
- [03_rotate_array](../03_rotate_array/) — provides the rotated-search setup

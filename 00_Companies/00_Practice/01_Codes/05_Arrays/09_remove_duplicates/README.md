# 09 — Remove Duplicates from Sorted Array

## Problem
Given a **sorted** array, remove duplicates **in place** so each element appears once. Return the new length; elements beyond the returned length are don't-care.

```
Input : [1,1,2,2,3,4,4,5]
Output: len=5, A[:5]=[1,2,3,4,5]
```

## Why It Matters
In-place compaction is the basis of stream deduplication, sensor reading consolidation, and shrinking ring buffers without realloc. The two-pointer pattern shows up in dozens of array problems.

## Approaches

### Approach 1 — Auxiliary Array
Copy unique values to `B`, copy back.
- Time: **O(n)**, Space: **O(n)** — fails the in-place rule

### Approach 2 — Hash Set
Generic dedupe (also for unsorted arrays).
- Time: **O(n)**, Space: **O(n)** — overkill when input is sorted

### Approach 3 — Two-Pointer (Best)
```text
if n == 0: return 0
w = 0                          // write index of last unique
for r in 1..n-1:
    if a[r] != a[w]:
        w++
        a[w] = a[r]
return w + 1
```
ASCII on `[1,1,2,2,3,4,4,5]`:
```
r=1 a[1]=1 == a[0]=1 → skip
r=2 a[2]=2 != 1 → w=1 a=[1,2,2,2,3,4,4,5]
r=3 a[3]=2 == 2 → skip
r=4 a[4]=3 != 2 → w=2 a=[1,2,3,2,3,4,4,5]
r=5 a[5]=4 != 3 → w=3 a=[1,2,3,4,3,4,4,5]
r=6 a[6]=4 == 4 → skip
r=7 a[7]=5 != 4 → w=4 a=[1,2,3,4,5,4,4,5]
return 5  ✓
```
- Time: **O(n)**, Space: **O(1)**, one pass

### Approach 4 — Generalised "At Most K Duplicates"
Keep at most `k` copies of each value. Compare `a[r]` against `a[w-k]`.
```text
w = 0
for r in 0..n-1:
    if w < k or a[r] != a[w-k]:
        a[w++] = a[r]
return w
```
- Time: **O(n)**, Space: **O(1)** — handy follow-up

### Approach 5 — `std::unique` (C++) / Library
Same algorithm wrapped; in C you write it manually.

## Comparison
| Approach | Time | Space | In place | When to use |
|---|---|---|---|---|
| Aux array | O(n) | O(n) | no | Memory abundant |
| Hash | O(n) | O(n) | no | Unsorted input |
| **Two pointer** | O(n) | O(1) | yes | **Default** |
| At-most-k generalised | O(n) | O(1) | yes | "Keep ≤k copies" variant |

## Key Insight
Because the array is sorted, equal values are contiguous. A single write index `w` always points to the last accepted unique value; the read index `r` scouts ahead and only advances `w` on a change.

## Pitfalls
- Empty array — handle `n == 0` early
- Returning `w` instead of `w + 1` — off-by-one (`w` is an index, length is `+1`)
- Trying it on an **unsorted** array → falls back to hashing
- Using `memmove` for each duplicate → O(n²)
- For records (struct with key), compare keys, copy the whole struct

## Interview Tips
1. Confirm: sorted? in-place? what about contents beyond the new length?
2. Lead with two-pointer. State invariants: `a[0..w]` is the deduped prefix; `r > w` always.
3. Follow-up: "keep at most 2 of each" → Approach 4.

## Related / Follow-ups
- Move zeros to end (same two-pointer pattern)
- Partition array around a pivot (Dutch national flag, 3-way)
- Remove element equal to a target (LeetCode 27)
- Dedupe a sorted linked list (pointer rewire, no shift)

# 08 — Merge Two Sorted Arrays In-Place

## Problem
`A` has size `m+n` with the first `m` slots filled (sorted) and `n` empty slots at the end. `B` has `n` sorted elements. Merge `B` into `A` so `A` is fully sorted.

```
A = [1,3,5,_,_,_]   m=3
B = [2,4,6]         n=3
→ A = [1,2,3,4,5,6]
```

## Why It Matters
External merge sort, log file consolidation, joining sorted index segments — anywhere a stream must merge into a pre-allocated buffer without extra heap allocations.

## Approaches

### Approach 1 — Extra Buffer
Standard merge into a third array, copy back.
- Time: **O(m+n)**, Space: **O(m+n)** — fails the in-place requirement

### Approach 2 — Insertion (Per Element of B)
For each `b` in B, find its slot in A, shift right, insert.
- Time: **O(m·n)** worst case, Space: **O(1)**
- Acceptable only for tiny `n`

### Approach 3 — Fill From the End (Best)
Three pointers — write at the back, read from the tails of A and B.
```text
i = m-1; j = n-1; k = m+n-1
while j >= 0:
    if i >= 0 and A[i] > B[j]:
        A[k--] = A[i--]
    else:
        A[k--] = B[j--]
```
- Time: **O(m+n)**, Space: **O(1)**
- Writing back-to-front means we never overwrite an unread element of A.

ASCII:
```
A=[1,3,5,_,_,_]  i=2  k=5  B=[2,4,6]  j=2
A[5]=6  (B[j]>A[i])     → [1,3,5,_,_,6] j=1 k=4
A[4]=5  (A[i]>B[j])     → [1,3,5,_,5,6] i=1 k=3
A[3]=4                   → [1,3,5,4,5,6] j=0 k=2
A[2]=3                   → [1,3,3,4,5,6] i=0 k=1
A[1]=2                   → [1,2,3,4,5,6] j=-1 done
```

### Approach 4 — Gap Method (Shellsort-Style)
No spare space at all — both arrays are dense. Treat them as one logical array; with gap = ⌈(m+n)/2⌉, halve each round, swap out-of-order pairs.
- Time: **O((m+n) log(m+n))**, Space: **O(1)**
- Use when no trailing space exists

## Comparison
| Approach | Time | Space | Needs trailing space | When to use |
|---|---|---|---|---|
| Extra buffer | O(m+n) | O(m+n) | no | Memory abundant |
| Insertion | O(m·n) | O(1) | yes | Very small `n` |
| **Fill-from-end** | O(m+n) | O(1) | yes | **Default** |
| Gap method | O((m+n) log(m+n)) | O(1) | no | True in-place |

## Key Insight
Writing the **largest** value into the **last** slot leaves the front of A untouched — and that front still has all the read positions you need. Iterating tails backward eliminates shifts entirely.

## Pitfalls
- Forgetting `i >= 0` guard → reads `A[-1]`
- Loop on `i >= 0 && j >= 0` then forgetting that any remaining `B[j]`s must still be copied. Loop on `j >= 0` instead.
- Equal elements — use `>` not `>=` to keep stability (A first)
- Off-by-one in `k = m+n-1`

## Interview Tips
1. Confirm: trailing space available? If yes → fill-from-end. If no → gap method.
2. Whiteboard the three-pointer state explicitly. Show why iterating forward fails (overwrite hazard).
3. Mention stability if elements are records with keys.

## Related / Follow-ups
- Merge K sorted arrays → min-heap of K iterators (O(N log K))
- Merge step of merge sort
- Merge two sorted linked lists (no shifting, pointer rewire)
- External merge sort for files larger than RAM

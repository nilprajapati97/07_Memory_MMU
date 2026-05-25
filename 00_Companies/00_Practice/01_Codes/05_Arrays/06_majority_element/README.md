# 06 — Majority Element (Boyer–Moore)

## Problem
Find the element appearing **more than ⌊n/2⌋ times** in an array (assume it exists).

```
Input : [2, 2, 1, 1, 1, 2, 2]
Output: 2  (appears 4 times, n=7, threshold=3)
```

## Why It Matters
Voting-based leader election (Paxos-flavour primitives), quorum decisions, "most-common" stream analytics on streaming devices with constant memory.

## Approaches

### Approach 1 — Hash Count
Count frequencies; return any > n/2.
- Time: **O(n)**, Space: **O(n)**

### Approach 2 — Sort and Pick Middle
After sort, `a[n/2]` is always the majority (if one exists).
- Time: **O(n log n)**, Space: **O(1)**
- Nice one-liner; relies on the pigeon-hole property

### Approach 3 — Bit-Counting (32 passes)
For each bit position 0..31, count how many array values have it set. If count > n/2, set that bit in the answer.
- Time: **O(32 n)**, Space: **O(1)**
- Useful when elements are wide and equality compare is expensive

### Approach 4 — Boyer–Moore Vote (Best)
```text
// Phase 1: candidate
cand = 0; count = 0
for x in a:
    if count == 0: cand = x
    count += (x == cand) ? +1 : -1

// Phase 2 (optional): verify cand appears > n/2 times
```
ASCII on `[2,2,1,1,1,2,2]`:
```
x=2 cnt=0 → cand=2, cnt=1
x=2          cnt=2
x=1          cnt=1
x=1          cnt=0
x=1 cnt=0 → cand=1, cnt=1
x=2          cnt=0
x=2 cnt=0 → cand=2, cnt=1   → answer 2 ✓
```
- Time: **O(n)**, Space: **O(1)** — single pass, ideal

### Approach 5 — Divide & Conquer
Find majority of left half and right half; if equal return it, else count occurrences of each in full range.
- Time: **O(n log n)**, Space: **O(log n)** recursion — academic interest

## Comparison
| Approach | Time | Space | When to use |
|---|---|---|---|
| Hash | O(n) | O(n) | Need full frequency map |
| Sort + middle | O(n log n) | O(1) | Already sorted |
| Bit count | O(32n) | O(1) | Wide types, no equality op |
| **Boyer–Moore** | O(n) | O(1) | **Default answer** |
| D&C | O(n log n) | O(log n) | Theory illustration |

## Key Insight
Pair each majority element with one non-majority element — they cancel. Because the majority appears > n/2 times, at least one copy survives all cancellations and ends up as the final candidate.

## Pitfalls
- Skipping Phase 2 verification when existence is **not** guaranteed → returns garbage candidate
- Generalising to "appears > n/3 times" → need **two** candidates and counts (Boyer–Moore extended)
- Streaming variant: same algorithm works for one pass over an infinite stream with O(1) memory

## Interview Tips
1. Confirm: is existence guaranteed? If not, always run Phase 2.
2. Lead with Boyer–Moore. If they look puzzled, give the pairing intuition.
3. Common follow-up: "> n/3 times" → two candidates, two counters. Same idea.

## Related / Follow-ups
- Misra–Gries heavy hitters (k candidates)
- Count-Min Sketch (probabilistic, streaming)
- Plurality vs majority — distinction matters
- [05_duplicate_number](../05_duplicate_number/) — also uses cancellation intuition

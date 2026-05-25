# 01 — Find Largest / Smallest / 2nd Largest

## Problem
Given an array of `n` integers, find the largest, smallest, and the second-largest distinct element.

```
Input : [12, 35, 1, 10, 34, 1]
Output: max=35, min=1, second_max=34
```

## Why It Matters
Foundation for selection problems (top-K, median-of-medians, partial sorts in schedulers, leaderboards). Tests whether you handle duplicates, ties, and single-element edge cases.

## Approaches

### Approach 1 — Sort then Pick
```text
sort(a)
min = a[0]; max = a[n-1]
i = n-2
while i >= 0 and a[i] == max: i--
second_max = a[i] if i >= 0 else NONE
```
- Time: **O(n log n)**, Space: **O(1)** in-place sort
- Pros: trivial; Cons: destroys order, overkill

### Approach 2 — Two Linear Passes
Pass 1 finds `max`. Pass 2 finds the largest value `< max`.
- Time: **O(2n)**, Space: **O(1)**
- Pros: simple; Cons: 2 passes, bad cache for huge `n`

### Approach 3 — Single Pass, Track Two Values (Best)
```text
max = second = -INF
for x in a:
    if x > max:
        second = max
        max = x
    else if x > second and x != max:
        second = x
```
ASCII trace on `[12, 35, 1, 10, 34, 1]`:
```
x=12  max=12  sec=-INF
x=35  max=35  sec=12
x=1   max=35  sec=12
x=10  max=35  sec=12
x=34  max=35  sec=34
x=1   max=35  sec=34
```
- Time: **O(n)**, Space: **O(1)** — single pass, branch-predictable

### Approach 4 — Tournament / Divide & Conquer
Split array, recursively find {max, second} of each half, combine in O(1).
- Time: **O(n)** with **n + log n − 2** comparisons (optimal for 2nd-max)
- Space: **O(log n)** recursion
- Pros: provably optimal comparisons; Cons: more code, rarely needed

## Comparison
| Approach | Time | Space | When to use |
|---|---|---|---|
| Sort | O(n log n) | O(1) | Already need sorted output |
| Two passes | O(n) | O(1) | Tiny arrays, readability |
| Single pass | O(n) | O(1) | **Default interview answer** |
| Tournament | O(n) | O(log n) | Minimum-comparison guarantee |

## Key Insight
Update `second` **before** overwriting `max` — the old max becomes the new second. Skip values equal to `max` to handle duplicates correctly.

## Pitfalls
- Initializing with `a[0]` and `a[1]` instead of `INT_MIN` — breaks on `n<2`
- Using `INT_MIN` with unsigned arrays — use type-appropriate sentinel
- Forgetting duplicate handling → `second_max == max`
- Empty array — return error / sentinel, do not deref `a[0]`

## Interview Tips
1. Confirm: distinct second-max or just second-largest by index? Empty / single-element behavior?
2. Lead with single-pass O(n) — interviewer is checking the duplicate edge case.
3. Mention tournament when asked "can you do better in *comparisons*?"

## Related / Follow-ups
- Top-K elements → heap (size K)
- k-th largest → Quickselect, O(n) avg
- Median → median-of-medians
- See [07_kadane_algorithm](../07_kadane_algorithm/) (also a single-pass O(n) scan)

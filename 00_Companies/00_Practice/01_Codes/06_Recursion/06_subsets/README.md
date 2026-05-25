# 06 — Print All Subsets of a Set (Power Set)

## Problem
Given a set / array of `n` distinct elements, list all `2ⁿ` subsets.

```
Input : [1, 2, 3]
Output: [], [1], [2], [3], [1,2], [1,3], [2,3], [1,2,3]
```

## Why It Matters
Backbone of combinatorial search: subset-sum, masks for DP on subsets (TSP, bitmask DP), feature-flag exploration, enumeration over hardware configuration bits.

## Approaches

### Approach 1 — Recursion: Include / Exclude (Best Teaching)
```text
subsets(idx, current):
    if idx == n:
        emit current
        return
    subsets(idx+1, current)              // exclude a[idx]
    current.push(a[idx])
    subsets(idx+1, current)              // include a[idx]
    current.pop()                        // backtrack
```
Recursion tree for `[1,2,3]`:
```
                       []
                /             \
             []                [1]
           /    \            /     \
         []    [2]         [1]    [1,2]
        /  \  /   \       /  \    /   \
      [] [3] [2] [2,3] [1] [1,3] [1,2] [1,2,3]
```
- Time: **O(n · 2ⁿ)** (n work to emit each subset), Space: **O(n)** stack + output

### Approach 2 — Iterative Bitmask (Best Default)
Each integer mask in `0..2ⁿ-1` represents a subset; bit `i` set ⇒ include `a[i]`.
```text
for mask in 0..(1<<n) - 1:
    subset = []
    for i in 0..n-1:
        if mask & (1 << i): subset.push(a[i])
    emit subset
```
- Time: **O(n · 2ⁿ)**, Space: **O(1)** beyond output
- Concise, deterministic order; trivially parallelisable

### Approach 3 — Backtracking with Start Index (Combinations Order)
```text
backtrack(start, current):
    emit current
    for i in start..n-1:
        current.push(a[i])
        backtrack(i+1, current)
        current.pop()
```
- Emits in lexicographic combination order
- Useful base for "all subsets of size k", "subset-sum"

### Approach 4 — Cascading (Iterative, No Bits)
```text
result = [[]]
for x in a:
    for s in copy(result):
        result.push(s + [x])
```
- Time: **O(n · 2ⁿ)**, Space: **O(n · 2ⁿ)** for result
- Easiest to explain without mentioning bits

### Approach 5 — Gray-Code Order
Visit subsets so each differs from the previous by exactly one element flip — useful when "modify last decision" is cheap (state machines, subset sum updates).

## Comparison
| Approach | Time | Space | Order | When to use |
|---|---|---|---|---|
| Include/exclude recursion | n·2ⁿ | n stack | DFS | Teaching, backtracking variants |
| **Iterative bitmask** | n·2ⁿ | 1 | numeric | **Default**, parallelisable |
| Backtracking start-idx | n·2ⁿ | n | combo lex | Combinations / subset-sum |
| Cascading | n·2ⁿ | 2ⁿ | level-by-level | No-bits explanation |
| Gray-code | n·2ⁿ | 1 | one-flip | Incremental computation |

## Key Insight
Every subset corresponds to a binary string of length `n`: bit `i` = "is `a[i]` in the subset?". This bijection makes the count `2ⁿ` obvious and the bitmask iteration trivial.

## Pitfalls
- `n > 30` with 32-bit mask → overflow; use `unsigned long long` (`n ≤ 63`); beyond that, no enumeration approach is viable anyway (`2⁶⁴` subsets)
- Duplicate elements in input → use sort + skip-equal (the "II" variant)
- Mutating `current` without `pop()` in backtracking → contaminated output
- Emitting a reference vs a copy — output should be a copy (or stream immediately)
- Order matters in some judges (lexicographic vs bitmask order)

## Interview Tips
1. Ask: distinct elements? Order of output? Limit on `n`?
2. Lead with **bitmask iterative** — shortest, fastest to whiteboard.
3. Then "recursion form for backtracking variants" — segues into subset-sum, N-queens, combinations.
4. Always state size: `2ⁿ` subsets; problem only tractable for small `n` (~25).

## Related / Follow-ups
- Subsets II (with duplicates)
- Combinations (subsets of size k)
- Permutations (different structure — swap-based backtracking)
- Subset-sum, partition equal subset sum (DP)
- [07_n_queens](../07_n_queens/) — backtracking sibling

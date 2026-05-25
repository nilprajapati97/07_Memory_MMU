# 13 — Spiral Traversal of a Matrix

## Problem
Print the elements of an `m × n` matrix in spiral order (clockwise from top-left).

```
[[ 1, 2, 3, 4],
 [ 5, 6, 7, 8],     → 1 2 3 4 8 12 11 10 9 5 6 7
 [ 9,10,11,12]]
```

## Why It Matters
Scan patterns for image kernels, antenna beam sweeps, snake-style display refreshes. Tests boundary management — a perennial source of off-by-one bugs.

## Approaches

### Approach 1 — Boundary Shrinking (Best)
Maintain four pointers: `top, bottom, left, right`. Walk each edge, then shrink.
```text
top=0; bottom=m-1; left=0; right=n-1
while top <= bottom and left <= right:
    for j in left..right:   print A[top][j]
    top++
    for i in top..bottom:   print A[i][right]
    right--
    if top <= bottom:
        for j in right..left step -1: print A[bottom][j]
        bottom--
    if left <= right:
        for i in bottom..top step -1: print A[i][left]
        left++
```
ASCII walk:
```
+---------> right edge
|           |
^           v
left edge <--+ bottom edge
```
- Time: **O(m·n)**, Space: **O(1)** beyond output

### Approach 2 — Direction Vector + Visited Matrix
Track `(dr, dc)` cycling through `(0,1),(1,0),(0,-1),(-1,0)`. Mark visited cells; rotate when next cell is out of bounds or visited.
- Time: **O(m·n)**, Space: **O(m·n)** for `visited` flag
- Clean to code; uses extra memory

### Approach 3 — Direction Vector, No Visited (Computed Bounds)
Same as 2 but instead of `visited`, shrink the active rectangle each time the direction changes — basically a re-derivation of Approach 1.
- Time: **O(m·n)**, Space: **O(1)**

### Approach 4 — Recursive Layers
Recurse on the outer ring, then on the submatrix `[top+1..bottom-1, left+1..right-1]`.
```text
spiral(top, bottom, left, right):
    if top > bottom or left > right: return
    print top row, right col, bottom row (if dist), left col (if dist)
    spiral(top+1, bottom-1, left+1, right-1)
```
- Time: **O(m·n)**, Space: **O(min(m,n))** recursion

## Comparison
| Approach | Time | Space | Notes |
|---|---|---|---|
| **Boundary shrink** | m·n | 1 | **Default** |
| Direction + visited | m·n | m·n | Easiest to code |
| Direction + bounds | m·n | 1 | Same as #1 in disguise |
| Recursive layers | m·n | min(m,n) stack | Elegant; stack risk |

## Key Insight
After walking the **top** row, increment `top`. After the **right** column, decrement `right`. Crucially, guard the **bottom** and **left** walks with the post-shrink check `top <= bottom` / `left <= right` to avoid duplicate prints when only a single row or column remains.

## Pitfalls
- **Rectangular** matrices (`m ≠ n`): without the guard, single-row residue gets printed twice (forward then backward)
- Off-by-one: `for j in left..right` vs `left..right+1` depending on inclusive convention
- Recursion stack overflow on huge matrices
- Counter-clockwise variant: reverse the four directions and start order
- 1×n or n×1 matrix → must return early correctly

## Interview Tips
1. Always handle the 1-row / 1-column degenerate case explicitly — interviewers test it.
2. Boundary-shrink with the two extra guards is the standard answer; whiteboard it slowly.
3. Mention direction-vector approach if asked about generalising to arbitrary scan patterns (zig-zag, diagonal).

## Related / Follow-ups
- Generate spiral matrix (fill an n×n with 1..n²)
- Diagonal traversal of a matrix
- [12_matrix_rotation](../12_matrix_rotation/) — same layered thinking
- Zig-zag traversal for JPEG DCT order

# 12 — Matrix Rotation by 90°

## Problem
Rotate an `n×n` matrix by 90° clockwise (or counter-clockwise) **in place**.

```
[[1,2,3],          [[7,4,1],
 [4,5,6],   --->    [8,5,2],
 [7,8,9]]           [9,6,3]]
```

## Why It Matters
Image processing, display orientation, framebuffer manipulation on embedded GPUs. The "transpose + reverse" insight is famous; the layer-by-layer cycle is what shows real understanding.

## Approaches

### Approach 1 — Auxiliary Matrix
```text
for i in 0..n-1:
    for j in 0..n-1:
        B[j][n-1-i] = A[i][j]
copy B -> A
```
- Time: **O(n²)**, Space: **O(n²)**

### Approach 2 — Transpose then Reverse Rows (Best Default)
```text
// Transpose (swap A[i][j] with A[j][i] for i<j)
for i in 0..n-1:
    for j in i+1..n-1:
        swap(A[i][j], A[j][i])
// Reverse each row
for i in 0..n-1:
    reverse(A[i])
```
For counter-clockwise: transpose then **reverse columns** (or reverse rows first).
- Time: **O(n²)**, Space: **O(1)**
- Two passes but mechanically simple

### Approach 3 — Layer-by-Layer 4-Cycle (Single Pass)
Process concentric rings; each cell moves through a 4-cycle.
```text
for layer in 0..n/2 - 1:
    first = layer
    last  = n - 1 - layer
    for i in first..last-1:
        offset = i - first
        top                    = A[first][i]
        A[first][i]            = A[last - offset][first]   // left  -> top
        A[last - offset][first]= A[last][last - offset]    // bottom-> left
        A[last][last - offset] = A[i][last]                // right -> bottom
        A[i][last]             = top                       // top   -> right
```
ASCII (single 4-cycle in a 3×3 corner):
```
top    = (0, i)         → goes to (i, n-1)
right  = (i, n-1)       → goes to (n-1, n-1-i)
bottom = (n-1, n-1-i)   → goes to (n-1-i, 0)
left   = (n-1-i, 0)     → goes to (0, i)
```
- Time: **O(n²)**, Space: **O(1)** — single pass, optimal moves

### Approach 4 — Non-Square (m×n)
Cannot rotate in-place into the same buffer — output dimensions differ. Allocate `n×m` and assign `B[j][m-1-i] = A[i][j]`. Time **O(m·n)**, Space **O(m·n)**.

## Comparison
| Approach | Time | Space | Square only | Passes |
|---|---|---|---|---|
| Aux matrix | n² | n² | no | 1 |
| **Transpose+Reverse** | n² | 1 | yes | 2 |
| 4-cycle layers | n² | 1 | yes | 1 |
| Non-square | m·n | m·n | no | 1 |

## Key Insight
A 90° clockwise rotation is the composition **transpose → reverse each row**.
- Transpose flips across main diagonal: `(i,j) → (j,i)`.
- Reversing each row sends `(j,i) → (j, n-1-i)`.
- Combined: `(i,j) → (j, n-1-i)` — the rotation formula.

## Pitfalls
- Transpose loop with `j` starting at `0` instead of `i+1` → undoes itself
- Layer rotation off-by-one: inner loop is `[first, last)`, not `[first, last]`
- Counter-clockwise: easy bug — wrong direction. Use transpose + reverse **columns** (or reverse rows then transpose).
- Non-square matrices can't rotate in the same buffer
- Row-major vs column-major storage affects cache behaviour; tile (`32×32`) for large matrices

## Interview Tips
1. Ask: clockwise or CCW? square? in-place?
2. Lead with transpose+reverse — fastest to write and explain.
3. Mention the 4-cycle layered approach for "single-pass" or "minimum writes".
4. Mention cache-blocking when matrices exceed L1/L2 (real-world).

## Related / Follow-ups
- [14_transpose_matrix](../14_transpose_matrix/) — half of this problem
- [13_spiral_traversal](../13_spiral_traversal/) — layer-walk cousin
- Rotate 180° = reverse each row + reverse rows order (two reversals)
- Rotate an image stored as 1D buffer with stride

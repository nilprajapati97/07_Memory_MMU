# 14 — Transpose of a Matrix

## Problem
Transpose an `m × n` matrix: `B[j][i] = A[i][j]`. For square matrices, do it in place.

```
[[1,2,3],          [[1,4],
 [4,5,6]]   --->    [2,5],
                    [3,6]]
```

## Why It Matters
Pre-step for rotations, matrix multiplication ordering, SIMD layout swaps (AoS↔SoA), strided memory access optimisation. In-place square transpose is the textbook "swap upper-triangular with lower-triangular" pattern.

## Approaches

### Approach 1 — Out-of-Place (Works for Any Shape)
```text
allocate B[n][m]
for i in 0..m-1:
    for j in 0..n-1:
        B[j][i] = A[i][j]
```
- Time: **O(m·n)**, Space: **O(m·n)**
- The only choice for rectangular matrices

### Approach 2 — In-Place, Square Only (Best)
```text
for i in 0..n-1:
    for j in i+1..n-1:
        swap(A[i][j], A[j][i])
```
ASCII (3×3):
```
       j=0 j=1 j=2
i=0 [   . swap swap ]
i=1 [   .   . swap  ]
i=2 [   .   .   .   ]
```
- Time: **O(n²/2)**, Space: **O(1)**
- Iterate strict upper triangle only (`j > i`) — swapping the full matrix undoes itself.

### Approach 3 — Cache-Blocked (Tiled) Transpose
For large matrices, swap in `B × B` tiles to keep both source and destination in L1.
```text
for ii in 0..n step B:
    for jj in 0..n step B:
        for i in ii..min(ii+B,n):
            for j in max(jj, i+1)..min(jj+B,n):
                swap(A[i][j], A[j][i])
```
- Time: **O(n²)**, Space: **O(1)** — same asymptotic, vastly better cache
- Typical `B = 16` or `32` (one cache line of doubles)

### Approach 4 — Bit-Matrix Transpose (8×8 in 32 ops)
For tightly packed 8×8 bit matrices, transpose with a sequence of mask-and-shift XOR swaps (Hacker's Delight). Time **O(1)** for fixed 8×8.
- Embedded image kernels, font rotation

### Approach 5 — Library / BLAS
`cblas_omatcopy` / `mkl_somatcopy`. Mention only in production context.

## Comparison
| Approach | Time | Space | Shape | Use case |
|---|---|---|---|---|
| Out-of-place | m·n | m·n | any | Rectangular |
| **In-place square** | n²/2 | 1 | square | **Default for n×n** |
| Tiled | n² | 1 | square | Large, cache-bound |
| 8×8 bit trick | const | 1 | 8×8 bits | Embedded graphics |
| BLAS | n² | n² (or 1) | any | Production |

## Key Insight
Square transpose swaps each `(i,j)` with `(j,i)` exactly once — restrict the inner loop to `j > i` to avoid double-swapping back to the original.

## Pitfalls
- Looping `j` from `0` instead of `i+1` → matrix unchanged (every swap reverts itself)
- Trying in-place on non-square — impossible without a permutation algorithm (cycle-following) which is much harder
- Cache thrashing on large `n`: naive transpose touches non-contiguous memory; switch to tiled at `n ≳ 1024`
- Row-major vs column-major confusion when interfacing with FORTRAN/BLAS
- Stride/pitch on framebuffers — rows may not be tightly packed

## Interview Tips
1. Ask: square or rectangular? In-place required? Element size (cache budget)?
2. Lead with strict upper-triangle in-place loop for square.
3. Mention tiling immediately when "n is large" or "you have a cache" comes up.
4. For rectangular in-place — say "true in-place needs a cycle-following permutation; usually not worth it; allocate".

## Related / Follow-ups
- [12_matrix_rotation](../12_matrix_rotation/) — transpose is half the job
- Matrix multiplication with transposed B for cache locality
- AoS↔SoA conversion in SIMD pipelines
- In-place rectangular transpose (Catanzaro/Gustavson cycle algorithm) — graduate-level

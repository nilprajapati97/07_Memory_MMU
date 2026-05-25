# 07 — N-Queens

## Problem
Place `N` queens on an `N × N` chessboard so no two attack each other (no shared row, column, or diagonal). Count solutions or list all configurations.

```
N=4 → 2 solutions:
. Q . .       . . Q .
. . . Q       Q . . .
Q . . .       . . . Q
. . Q .       . Q . .
```

## Why It Matters
The textbook **backtracking** problem. Sharpens pruning intuition, bitmask state representation, and recursion-vs-iteration trade-offs. Same skeleton solves Sudoku, graph colouring, constraint satisfaction.

## Approaches

### Approach 1 — Naive Generate-and-Test
Enumerate every placement of N queens on N² squares, check validity.
- Time: **O(C(N², N) · N²)** — astronomically slow; mention only as anti-pattern

### Approach 2 — Backtracking Row-by-Row (Best Teaching)
Place one queen per row; for each row try every column; recurse if safe.
```text
solve(row, cols, diag1, diag2):
    if row == N: count++; emit board; return
    for col in 0..N-1:
        if col in cols or (row-col) in diag1 or (row+col) in diag2:
            continue
        cols.add(col); diag1.add(row-col); diag2.add(row+col)
        board[row] = col
        solve(row+1, ...)
        cols.remove(col); diag1.remove(...); diag2.remove(...)
```
- Time: **O(N!)** worst case (much less in practice due to pruning)
- Space: **O(N)** recursion + **O(N)** state sets

### Approach 3 — Bitmask Backtracking (Best Performance)
Represent `cols`, `diag1`, `diag2` as three `int`s; "free" positions in this row = `~(cols | diag1 | diag2) & ((1<<N)-1)`.
```text
solve(cols, d1, d2):
    if cols == FULL: count++; return
    free = ~(cols | d1 | d2) & FULL
    while free:
        bit  = free & -free        // lowest free column
        free ^= bit
        solve(cols | bit,
              (d1 | bit) << 1,
              (d2 | bit) >> 1)
```
- Time: still **O(N!)** worst, but with tiny constant — ~30× faster than set-based
- Space: **O(N)** recursion only
- The standard high-performance solution; N=15 in milliseconds

### Approach 4 — Iterative Stack Backtracking
Replace recursion with an explicit stack of `(row, col)`. Same complexity; useful when stack is constrained.

### Approach 5 — Symmetry Pruning
Solutions come in mirror/rotational families of 8 (or 4 if symmetric). Enumerate canonical only, multiply count by 8 (adjusting for the rare symmetric case at N=1).
- Cuts work by ~8×; common in counting-only variants

### Approach 6 — Local Search / Min-Conflicts
For very large N (≥100), exact backtracking is fine but heuristic min-conflicts solves random instances of N=10⁶ in ~50 swaps. Used for counting-only / single-solution problems.

## Comparison
| Approach | Time | Space | Output | When to use |
|---|---|---|---|---|
| Generate-and-test | huge | 1 | all | Never |
| Backtracking sets | N! / pruned | N | all | Teaching |
| **Bitmask backtracking** | N! / fast | N | all | **Default high-perf** |
| Iterative stack | same | N heap | all | Stack-constrained env |
| + Symmetry | N!/8 | N | canonical | Counting only |
| Min-conflicts | heuristic | N | one | N ≫ 30 |

## Key Insight
- Columns: one bit each — `cols & (1<<c)` checks attack.
- Diagonals: row + col is constant along `/` diagonals (one bit each across 2N-1 diagonals). As we descend one row, the "occupied `/`" mask **shifts right**.
- Anti-diagonals: row − col is constant along `\` diagonals; mask **shifts left**.
- A single bitwise OR captures all attacks; `~mask & FULL` exposes all candidate columns; `free & -free` extracts the lowest one — three bit tricks combined.

## Pitfalls
- `1 << N` overflows when `N ≥ 32` (use `unsigned long long`, or skip — problem is intractable beyond ~32 anyway)
- Diagonal indexing: `(row + col)` ranges `0..2N-2`; with set/array, allocate `2N-1`
- Forgetting to **undo** state when backtracking → contaminated state
- Counting vs listing: listing is bounded by output size, can dominate runtime
- Heuristic methods solve "find one" but not "find all"

## Interview Tips
1. Whiteboard the row-by-row backtracking with three sets. Make the diagonal indices explicit.
2. If they ask for "fastest possible" → bitmask version; explain the three shifting masks.
3. Mention solution counts as sanity check: N=4→2, N=8→92, N=12→14200. Famous numbers.
4. Generalisation: same skeleton → Sudoku, Latin squares, map colouring.

## Related / Follow-ups
- Sudoku solver (backtracking with constraint sets)
- Knight's tour (backtracking with Warnsdorff heuristic)
- Graph colouring
- [06_subsets](../06_subsets/) — simpler backtracking warm-up
- Symmetry breaking in constraint satisfaction

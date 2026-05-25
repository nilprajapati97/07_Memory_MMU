# 03 — Tower of Hanoi

## Problem
Move `n` disks from peg `A` to peg `C` using peg `B` as auxiliary, with rules:
1. Move one disk at a time.
2. Only the top disk of a peg may move.
3. A larger disk may never sit on a smaller disk.

```
n=3:  A→C, A→B, C→B, A→C, B→A, B→C, A→C    (7 moves)
```

## Why It Matters
Canonical demo of recursion and exponential complexity. Real uses: backup tape rotation schedule, multi-bit Gray-code state transitions.

## Approaches

### Approach 1 — Recursive (Best Default)
```text
hanoi(n, src, aux, dst):
    if n == 0: return
    hanoi(n-1, src, dst, aux)         // 1. move top n-1 to aux
    print "move disk n from src to dst" // 2. move biggest
    hanoi(n-1, aux, src, dst)         // 3. move n-1 from aux to dst
```
Call tree for `n=3`:
```
hanoi(3,A,B,C)
├─ hanoi(2,A,C,B)
│  ├─ hanoi(1,A,B,C) → A→C
│  ├─ A→B
│  └─ hanoi(1,C,A,B) → C→B
├─ A→C
└─ hanoi(2,B,A,C)
   ├─ hanoi(1,B,C,A) → B→A
   ├─ B→C
   └─ hanoi(1,A,B,C) → A→C
```
- Time: **O(2ⁿ − 1)** moves (proven optimal), Space: **O(n)** stack

### Approach 2 — Iterative with Explicit Stack
Replace recursion with a `stack<(n, src, aux, dst, phase)>` and simulate.
- Same time; Space: **O(n)** on heap instead of call stack
- Useful when call stack is tiny (embedded firmware)

### Approach 3 — Iterative Bit Trick (Frame-Free)
On move number `m` (1-indexed), the disk to move is `ctz(m) + 1` (count-trailing-zeros). Direction alternates based on parity of `n` and disk number.
```text
for m in 1..(2^n - 1):
    disk = ctz(m) + 1
    if n is even:
        cycle order A→B→C→A      (smallest disk rotates this way)
    else:
        cycle order A→C→B→A
```
- Time: **O(2ⁿ)**, Space: **O(1)**
- Elegant; rarely required but shows deep understanding

### Approach 4 — Generalised: 4-Peg (Frame-Stewart)
With 4 pegs, optimal sequence drops from `2ⁿ−1` to ≈ `2·√(2n)` — open conjecture, proven for small n. Skip unless asked.

## Comparison
| Approach | Moves | Space | When to use |
|---|---|---|---|
| **Recursive** | 2ⁿ−1 | n stack | **Default** |
| Iterative stack | 2ⁿ−1 | n heap | Embedded, small call stack |
| Bit trick | 2ⁿ−1 | 1 | Show-off / firmware |
| 4-peg Frame-Stewart | ≈ 2·√(2n) | n | Variant; rare |

## Key Insight
Solving for `n` disks reduces to solving for `n−1` disks twice plus one move of the largest — a classic "divide and reuse aux peg" pattern. The recurrence `T(n) = 2·T(n−1) + 1` solves to `T(n) = 2ⁿ − 1`.

## Pitfalls
- Forgetting the base case → stack overflow
- Mixing up `aux` and `dst` in the recursive calls — write the role each peg plays
- Asking "is `2ⁿ−1` always optimal?" Yes, provable by induction (the biggest disk must move at least once; before it moves, the other `n−1` must clear the path).
- Bit-trick variant: direction depends on parity of `n`; off-by-one easy

## Interview Tips
1. State the recurrence aloud: "move n−1 off, move biggest, move n−1 back."
2. Whiteboard the 3-line recursive version; trace n=3.
3. Compute moves: `2ⁿ − 1`. Stress this is exponential — moving 64 disks at 1 move/sec ≈ 585 billion years (the "end of the world" legend).
4. If asked for "without recursion" → iterative stack first, mention bit-trick.

## Related / Follow-ups
- Gray codes (same disk-flip pattern, single bit changes)
- Generalised TOH with `k` pegs (Frame-Stewart)
- Recurrence-relation analysis (`T(n) = 2T(n-1) + 1`)
- Counting moves of each disk (disk `k` moves `2^(n-k)` times)

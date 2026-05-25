# 06 — Recursion

Recursion is two things in interviews: a **problem-solving lens** (reduce a problem to smaller versions of itself) and a **code mechanism** (function calls + stack). This section trains both — from elementary linear recursion (factorial) through exponential decomposition (Fibonacci, Hanoi), divide-and-conquer (fast power), to combinatorial backtracking (subsets, N-queens).

## Topics
| # | Topic | Pattern | Difficulty |
|---|---|---|---|
| 01 | [Factorial & Fibonacci](01_factorial_fibonacci/) | Linear / overlapping subproblems → memoization | Easy–Med |
| 02 | [GCD / LCM](02_gcd_lcm/) | Tail recursion, modular reduction | Easy |
| 03 | [Tower of Hanoi](03_tower_of_hanoi/) | Double-recursion `2T(n-1)+1` | Med |
| 04 | [Power Function](04_power_function/) | Divide & conquer (halve exponent) | Med |
| 05 | [Reverse String / Number](05_reverse_recursion/) | Head–tail / accumulator | Easy |
| 06 | [Subsets (Power Set)](06_subsets/) | Include–exclude backtracking | Med |
| 07 | [N-Queens](07_n_queens/) | Backtracking with constraint pruning | Hard |

## Suggested Learning Order
1. **01 → 02 → 04** — linear, tail, divide-and-conquer (build mental model of recurrences)
2. **05** — apply to mutation (in-place reverse)
3. **03** — exponential `2T(n-1)+1` recurrence, recursion tree drawing
4. **06 → 07** — backtracking; learn pruning, state restoration, bitmask state

## Cross-Section Prerequisites
- **Pointers** (02_Pointers) before in-place string recursion
- **01_Bit_Manipulation** before the bitmask versions of subsets and N-queens
- **Memory_Storage** (07_Memory_Storage/12_static_keyword) — relevant when memoization tables live across calls

## Mental Model Cheat-Sheet
| Recurrence | Closed Form | Example |
|---|---|---|
| `T(n) = T(n-1) + 1` | O(n) | factorial, linear scan |
| `T(n) = 2T(n-1) + 1` | O(2ⁿ) | Tower of Hanoi |
| `T(n) = T(n-1) + T(n-2)` | O(φⁿ) | naive Fibonacci |
| `T(n) = T(n/2) + 1` | O(log n) | binary search |
| `T(n) = 2T(n/2) + n` | O(n log n) | merge sort |
| `T(n) = T(n/2) + O(1)` (squaring) | O(log n) | fast power |
| `T(n) = n · T(n-1)` | O(n!) | permutations, N-queens upper bound |

## Stack-Safety Rules of Thumb
- Default stack on Linux: 8 MB. Embedded MCUs: 1–8 KB. Linux kernel: 8–16 KB.
- A recursive call ≈ 32–64 bytes (return address + saved regs + locals).
- Convert deep recursion (>1000 frames) to iteration **or** ensure tail recursion + `-O2`.
- Memoization tables grow with input — choose between heap (large) vs static (reentrancy risk).

# 05 — Arrays

Array problems are the bread and butter of interview screens. They test pointer math, loop invariants, in-place transformations, and the ability to spot a hidden monotonic / cancellation structure. Master the two-pointer pattern, prefix-sum trick, cycle/orbit ideas, and binary-search-on-answer — these recur across every later section.

## Topics
| # | Topic | Key Technique | Difficulty |
|---|---|---|---|
| 01 | [Find Largest / 2nd Largest](01_find_largest/) | Single-pass, two trackers | Easy |
| 02 | [Reverse Array](02_reverse_array/) | Two-pointer swap | Easy |
| 03 | [Rotate by K](03_rotate_array/) | Reversal trick, juggling | Easy–Med |
| 04 | [Missing Number 1..N](04_missing_number/) | XOR, Gauss sum | Easy |
| 05 | [Duplicate in O(1) Space](05_duplicate_number/) | Floyd cycle, neg-marking | Medium |
| 06 | [Majority Element](06_majority_element/) | Boyer–Moore vote | Medium |
| 07 | [Kadane Max Subarray](07_kadane_algorithm/) | DP / single-pass | Medium |
| 08 | [Merge Sorted In-Place](08_merge_sorted_arrays/) | Fill-from-end, gap | Medium |
| 09 | [Remove Duplicates Sorted](09_remove_duplicates/) | Two-pointer write idx | Easy |
| 10 | [Binary Search Variants](10_binary_search/) | log n; lower/upper bound | Med |
| 11 | [Pair with Sum](11_pair_sum/) | Hash, two-pointer | Easy |
| 12 | [Matrix Rotate 90°](12_matrix_rotation/) | Transpose+reverse, 4-cycle | Medium |
| 13 | [Spiral Traversal](13_spiral_traversal/) | Boundary shrink | Medium |
| 14 | [Transpose Matrix](14_transpose_matrix/) | Strict upper triangle | Easy–Med |

## Suggested Learning Order
1. Foundations: **02 → 01 → 09** (two-pointer, write index)
2. Cancellation tricks: **04 → 05 → 06** (XOR, Floyd, Boyer–Moore)
3. DP & sums: **07 → 11** (Kadane, hash sums)
4. Sorted-array methods: **08 → 10 → 03** (merge, binary search, reversal trick)
5. 2D: **14 → 12 → 13** (transpose builds rotation; spiral builds boundary walking)

## Cross-Section Prerequisites
- **02_Pointers** before any in-place work (pointer arithmetic, aliasing)
- **01_Bit_Manipulation** (XOR identities) before 04 and 05
- **06_Recursion** before reading the divide-and-conquer variants in 07, 10

## Recurring Patterns Cheat Sheet
- **Two-pointer (opposite ends)** — reverse, pair-sum on sorted, palindrome
- **Two-pointer (same direction)** — dedupe, partitioning, window
- **Write-index** — in-place compaction (09, "remove element")
- **Prefix sum** — subarray sum, Kadane's relative
- **Cycle / Floyd** — duplicate detection (05), linked-list cycles
- **Reversal trick** — rotate (03), in-place permutation
- **Boundary shrink** — spiral (13), layer rotation
- **Binary search on answer** — generalised log-search (10)

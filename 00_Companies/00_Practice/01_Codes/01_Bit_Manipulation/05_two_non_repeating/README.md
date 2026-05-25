# Find Two Non-Repeating Numbers

## Problem
All numbers repeat twice except two. Find them.

## Approaches

### Approach 1: XOR Partitioning (Best)
- Time: O(n), Space: O(1)
- XOR all → get x^y
- Find rightmost set bit
- Partition array by that bit
- Interview gold standard

### Approach 2: Hash Map
- Time: O(n), Space: O(n)
- Count occurrences
- Simple but uses space

## Algorithm Steps
1. XOR all elements → x^y
2. Find rightmost set bit in x^y
3. Partition array into two groups
4. XOR each group separately

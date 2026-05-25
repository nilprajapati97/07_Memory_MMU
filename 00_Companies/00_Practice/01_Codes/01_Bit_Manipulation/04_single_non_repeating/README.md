# Find Single Non-Repeating Number

## Problem
All numbers repeat twice except one. Find it.

## Approaches

### Approach 1: XOR (Best)
- Time: O(n), Space: O(1)
- Property: a ^ a = 0, a ^ 0 = a
- Interview favorite

### Approach 2: Hash Map
- Time: O(n), Space: O(n)
- Count occurrences
- Straightforward

### Approach 3: Sorting
- Time: O(n log n), Space: O(1)
- Compare adjacent pairs
- No extra space

## XOR Properties
- Commutative: a ^ b = b ^ a
- Associative: (a ^ b) ^ c = a ^ (b ^ c)
- Self-inverse: a ^ a = 0

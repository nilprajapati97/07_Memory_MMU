# Count set bits (optimized)

## Code
See solution.c for Brian Kernighan's algorithm.

## In-depth Explanation (Nvidia Interview Style)

- Brian Kernighan's algorithm clears the lowest set bit in each iteration, making it efficient (O(number of set bits)).
- Useful for bitmask operations in kernel and driver code.

### Interview Tips
- Be ready to discuss time complexity and alternative methods (lookup table, __builtin_popcount).

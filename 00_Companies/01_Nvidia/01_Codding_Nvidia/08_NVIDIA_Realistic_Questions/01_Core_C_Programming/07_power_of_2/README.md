# Check if a number is power of 2

## Code
See solution.c for the implementation.

## In-depth Explanation (Nvidia Interview Style)

- A number is a power of 2 if it has exactly one set bit.
- The expression `n && !(n & (n - 1))` checks this efficiently.

### Interview Tips
- Be ready to discuss edge cases (zero, negatives) and bitwise logic.

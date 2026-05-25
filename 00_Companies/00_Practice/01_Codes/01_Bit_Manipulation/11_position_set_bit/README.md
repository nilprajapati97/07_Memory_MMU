# Find Position of Only Set Bit

## Problem
Given number with exactly one bit set, find its position.

## Approaches

### Approach 1: Check and Count (Simple)
- Verify single bit: `n & (n-1) == 0`
- Count shifts
- Interview friendly

### Approach 2: Using Log2
- Mathematical approach
- `log2(n)` gives position
- Requires math.h

### Approach 3: GCC Builtin (Best)
- `__builtin_ctz()` - count trailing zeros
- Single instruction
- Fastest

## Key Check
`n && !(n & (n-1))` verifies exactly one bit set

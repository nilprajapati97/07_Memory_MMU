# Multiply/Divide by Powers of 2

## Approaches

### Approach 1: Left/Right Shift (Best)
- Time: O(1), Space: O(1)
- `n << k` = n * 2^k
- `n >> k` = n / 2^k
- Interview answer

### Approach 2: Loop-based
- Time: O(k), Space: O(1)
- Repeated doubling/halving
- Educational only

### Approach 3: Bit Manipulation Tricks
- Fast multiply by constants
- Combine shifts and adds
- Compiler optimization

## Key Points
- Left shift = multiply by 2
- Right shift = divide by 2 (floor)
- Arithmetic right shift preserves sign
- Logical right shift fills with 0

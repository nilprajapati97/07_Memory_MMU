# Log Base 2 Using Bit Operations

## Approaches

### Approach 1: Right Shift Count
- Time: O(log n), Space: O(1)
- Count shifts until 0
- Simple and clear

### Approach 2: MSB Position (Binary Search)
- Time: O(log 32) = O(1)
- Divide and conquer
- Faster for large numbers

### Approach 3: GCC Builtin (Best)
- Time: O(1) - single instruction
- Uses CLZ (count leading zeros)
- Production choice

## Formula
log2(n) = 31 - CLZ(n)
where CLZ = count leading zeros

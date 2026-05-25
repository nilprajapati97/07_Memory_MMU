# Find Highest Set Bit (MSB Position)

## Approaches

### Approach 1: Right Shift Count
- Time: O(log n), Space: O(1)
- Count shifts until 0
- Simple and clear

### Approach 2: Binary Search (Lookup)
- Time: O(log 32) = O(1)
- Divide and conquer
- Faster for large numbers

### Approach 3: GCC Builtin (Best)
- Time: O(1) - single instruction
- Uses CLZ instruction
- Production choice

## Formula
MSB position = 31 - CLZ(n)
where CLZ = count leading zeros

## Applications
- Log base 2
- Power of 2 check
- Bit width calculation

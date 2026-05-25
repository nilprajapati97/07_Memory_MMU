# Check if Power of 2

## Approaches

### Approach 1: Bit Trick (Best)
- `n && !(n & (n-1))`
- Time: O(1), Space: O(1)
- One-liner solution

### Approach 2: Count Set Bits
- Check if exactly 1 bit set
- Time: O(k), Space: O(1)
- More intuitive

### Approach 3: Division
- Keep dividing by 2
- Time: O(log n), Space: O(1)
- Slowest but simple

## Why It Works
Power of 2 has exactly one bit set
- 2 = 0010
- 4 = 0100
- 8 = 1000

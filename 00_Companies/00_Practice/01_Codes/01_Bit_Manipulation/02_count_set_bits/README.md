# Count Set Bits (Hamming Weight)

Count the number of 1's in the binary representation of an integer.

## Files Overview

1. **01_brian_kernighan.c** - Best interview answer ⭐
2. **02_naive.c** - Simple loop approach
3. **03_lookup_table.c** - Fastest for repeated calls
4. **04_builtin.c** - Production code choice
5. **05_Recursive.c** - Recursive implementation
6. **06_Compare_All.c** - Side-by-side comparison
7. **CountOnes.c** - Comprehensive reference (7 approaches)

## Approach Comparison

| Approach | Time | Space | Best For |
|----------|------|-------|----------|
| Brian Kernighan | O(k) k=set bits | O(1) | Interviews, sparse bits |
| Naive Loop | O(log n) | O(1) | Learning, simple code |
| Lookup Table | O(1) | O(256) | Repeated calls, speed critical |
| Builtin | O(1) | O(1) | Production code |
| Recursive | O(log n) | O(log n) | Academic interest |

## Key Insight

**Brian Kernighan's trick:** `n & (n-1)` clears the rightmost set bit

Example:
```
n = 12 (1100)
n-1 = 11 (1011)
n & (n-1) = 8 (1000)  // Rightmost 1 cleared
```

## When to Use What

- **Interview:** Brian Kernighan (elegant, efficient)
- **Production:** `__builtin_popcount()` (hardware optimized)
- **Embedded/Speed:** Lookup table (constant time)
- **Learning:** Naive approach (easiest to understand)

## Compile & Run

```bash
gcc 01_brian_kernighan.c -o brian && ./brian
gcc 06_Compare_All.c -o compare && ./compare
```

# Bit Manipulation - Complete Index

## Quick Navigation

| # | Question | Best Approach | Complexity | File |
|---|----------|---------------|------------|------|
| 1 | Set/Clear/Toggle/Check Nth bit | Macros | O(1) | [01_nth_bit_operations](01_nth_bit_operations/) |
| 2 | Count set bits | Brian Kernighan's | O(k) | [02_count_set_bits](02_count_set_bits/) |
| 3 | Check power of 2 | Bit trick | O(1) | [03_power_of_2](03_power_of_2/) |
| 4 | Single non-repeating | XOR | O(n) | [04_single_non_repeating](04_single_non_repeating/) |
| 5 | Two non-repeating | XOR partition | O(n) | [05_two_non_repeating](05_two_non_repeating/) |
| 6 | Swap without temp | XOR swap | O(1) | [06_swap_without_temp](06_swap_without_temp/) |
| 7 | Reverse bits | Divide & conquer | O(1) | [07_reverse_bits](07_reverse_bits/) |
| 8 | Check endianness | Pointer cast | O(1) | [08_endianness_check](08_endianness_check/) |
| 9 | Swap endianness | Builtin bswap | O(1) | [09_swap_endianness](09_swap_endianness/) |
| 10 | Rotate bits | Shift & OR | O(1) | [10_rotate_bits](10_rotate_bits/) |
| 11 | Position of set bit | Builtin ctz | O(1) | [11_position_set_bit](11_position_set_bit/) |
| 12 | Log base 2 | Builtin clz | O(1) | [12_log_base_2](12_log_base_2/) |
| 13 | Multiply/divide by 2^k | Shift | O(1) | [13_multiply_divide_shift](13_multiply_divide_shift/) |
| 14 | Bit macros | Advanced macros | O(1) | [14_bit_macros](14_bit_macros/) |
| 15 | Highest set bit (MSB) | Builtin clz | O(1) | [15_highest_set_bit](15_highest_set_bit/) |

## Cheat Sheet

### Essential Patterns

```c
// 1. Set bit n
num |= (1 << n)

// 2. Clear bit n
num &= ~(1 << n)

// 3. Toggle bit n
num ^= (1 << n)

// 4. Check bit n
(num >> n) & 1

// 5. Clear rightmost set bit
n &= (n - 1)

// 6. Isolate rightmost set bit
n & -n

// 7. Check power of 2
n && !(n & (n - 1))

// 8. XOR swap
a ^= b; b ^= a; a ^= b;

// 9. Count set bits (Kernighan)
while (n) { n &= (n - 1); count++; }

// 10. MSB position
31 - __builtin_clz(n)

// 11. LSB position
__builtin_ctz(n)

// 12. Rotate left
(n << d) | (n >> (32 - d))

// 13. Rotate right
(n >> d) | (n << (32 - d))

// 14. Swap endianness
__builtin_bswap32(n)

// 15. Reverse bits (divide & conquer)
n = ((n & 0xFFFF0000) >> 16) | ((n & 0x0000FFFF) << 16);
n = ((n & 0xFF00FF00) >> 8)  | ((n & 0x00FF00FF) << 8);
n = ((n & 0xF0F0F0F0) >> 4)  | ((n & 0x0F0F0F0F) << 4);
n = ((n & 0xCCCCCCCC) >> 2)  | ((n & 0x33333333) << 2);
n = ((n & 0xAAAAAAAA) >> 1)  | ((n & 0x55555555) << 1);
```

### GCC Builtins

```c
__builtin_popcount(n)     // Count set bits
__builtin_clz(n)          // Count leading zeros
__builtin_ctz(n)          // Count trailing zeros
__builtin_parity(n)       // Parity (XOR of all bits)
__builtin_bswap16(n)      // Byte swap 16-bit
__builtin_bswap32(n)      // Byte swap 32-bit
__builtin_bswap64(n)      // Byte swap 64-bit
__builtin_ffs(n)          // Find first set (1-indexed)
```

### Hardware Register Operations

```c
// Define register address
#define REG_ADDR 0x40021000
volatile uint32_t *reg = (volatile uint32_t *)REG_ADDR;

// Read
uint32_t val = *reg;

// Write
*reg = 0x1234;

// Set bits 5-7
*reg |= (0x7 << 5);

// Clear bits 5-7
*reg &= ~(0x7 << 5);

// Toggle bit 3
*reg ^= (1 << 3);

// Check bit 2
if (*reg & (1 << 2)) { /* bit is set */ }

// Extract field (bits 11-8)
uint32_t field = (*reg >> 8) & 0xF;

// Set field (bits 11-8 to value)
*reg = (*reg & ~(0xF << 8)) | ((value & 0xF) << 8);
```

## Interview Strategy

### 1. Start Simple
- Explain brute force first
- Then optimize with bit manipulation
- Mention time/space complexity

### 2. Common Questions
- "Why use unsigned?" → Avoid sign extension
- "What if n >= 32?" → Undefined behavior
- "Why volatile?" → Prevent compiler optimization

### 3. Edge Cases
- n = 0
- n = all 1s (0xFFFFFFFF)
- Single bit set
- No bits set

### 4. Follow-up Questions
- "Can you do it in O(1)?" → Use builtin
- "Without builtin?" → Show manual approach
- "For 64-bit?" → Adjust constants

## Complexity Reference

| Operation | Time | Space | Notes |
|-----------|------|-------|-------|
| Set/Clear/Toggle bit | O(1) | O(1) | Single operation |
| Check bit | O(1) | O(1) | Single operation |
| Count set bits (Kernighan) | O(k) | O(1) | k = set bits |
| Count set bits (builtin) | O(1) | O(1) | Hardware instruction |
| Power of 2 check | O(1) | O(1) | Single comparison |
| XOR single number | O(n) | O(1) | One pass |
| XOR two numbers | O(n) | O(1) | Two passes |
| Swap (XOR) | O(1) | O(1) | Three operations |
| Reverse bits (simple) | O(32) | O(1) | Fixed iterations |
| Reverse bits (D&C) | O(log 32) | O(1) | Parallel swaps |
| Endianness check | O(1) | O(1) | Memory access |
| Byte swap | O(1) | O(1) | Shift operations |
| Rotate | O(1) | O(1) | Two shifts + OR |
| MSB/LSB position | O(1) | O(1) | With builtin |

## Common Mistakes to Avoid

1. ❌ Forgetting parentheses in macros
2. ❌ Using signed int for bit operations
3. ❌ Shifting by >= width (undefined behavior)
4. ❌ Not checking for n = 0
5. ❌ Forgetting volatile for hardware registers
6. ❌ XOR swap without checking a != b
7. ❌ Sign extension with right shift on signed
8. ❌ Overflow in arithmetic swap

## Testing Commands

```bash
# Compile and test all
for dir in */; do
    echo "=== Testing $dir ==="
    cd "$dir"
    for f in approach*.c; do
        gcc -Wall -Wextra "$f" -o test 2>/dev/null && ./test
    done
    cd ..
done

# Test specific approach
cd 02_count_set_bits
gcc approach1_kernighan.c -o test && ./test

# Compile with optimizations
gcc -O3 -march=native approach1_kernighan.c -o test

# Check assembly
gcc -S -O2 approach1_kernighan.c
```

## Related Topics

- Bitwise operators: &, |, ^, ~, <<, >>
- Two's complement representation
- Bit fields in structures
- Packed structures
- Memory alignment
- Endianness in networking
- Hardware register programming
- Atomic operations
- Memory barriers

## Resources

- [README.md](README.md) - Complete guide
- [SUMMARY.md](SUMMARY.md) - Implementation summary
- Each subdirectory has detailed README

## Interview Companies

These questions are frequently asked at:
- Qualcomm, Intel, NVIDIA, AMD
- Samsung, Broadcom, Texas Instruments
- Apple, Google, Microsoft (systems roles)
- Embedded/Firmware positions
- Linux Kernel development roles

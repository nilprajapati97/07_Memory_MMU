# Swap Without Temp Variable

## Approaches

### Approach 1: XOR Swap (Best for Bit Manipulation)
- Time: O(1), Space: O(1)
- No overflow issues
- Works with same address check

### Approach 2: Arithmetic Swap
- Time: O(1), Space: O(1)
- Can overflow
- Addition/subtraction based

### Approach 3: Multiplication/Division
- Time: O(1), Space: O(1)
- Fails with zero
- Not recommended

## XOR Swap Steps
```
a = a ^ b
b = a ^ b  // b = (a^b) ^ b = a
a = a ^ b  // a = (a^b) ^ a = b
```

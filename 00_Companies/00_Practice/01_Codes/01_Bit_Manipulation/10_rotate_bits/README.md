# Rotate Bits Left/Right

## Approaches

### Approach 1: Standard (Best)
- Time: O(1), Space: O(1)
- Clean and portable
- Interview answer

### Approach 2: With Modulo (Safe)
- Handles d >= BITS
- Prevents undefined behavior
- Production ready

### Approach 3: Assembly (x86)
- Uses ROL/ROR instructions
- Fastest on x86
- Platform specific

## Rotate vs Shift
**Shift**: Bits fall off, zeros fill
**Rotate**: Bits wrap around

Example: 10110 rotate left by 2
- Result: 11010
- Lost bits come back on right

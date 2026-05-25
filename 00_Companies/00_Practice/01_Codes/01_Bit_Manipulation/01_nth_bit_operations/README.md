# Set/Clear/Toggle/Check Nth Bit

## Approaches

### Approach 1: Functions
- Returns new value
- Clean, reusable
- Time: O(1), Space: O(1)

### Approach 2: Macros (Preferred)
- No function call overhead
- Inline expansion
- Best for embedded/kernel

### Approach 3: In-place
- Modifies original
- Pointer-based
- Memory efficient

## Bit Operations
- **Set**: `num | (1 << n)`
- **Clear**: `num & ~(1 << n)`
- **Toggle**: `num ^ (1 << n)`
- **Check**: `(num >> n) & 1`

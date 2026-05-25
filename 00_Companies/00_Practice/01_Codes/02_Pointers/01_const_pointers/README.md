# Const Pointer Variations

## Three Types

### 1. `const int *p` - Pointer to Constant Int
```c
const int *p = &x;
*p = 10;    // ERROR: Cannot modify value
p = &y;     // OK: Can change pointer
```
- **Read as**: Pointer to a constant integer
- **Memory**: `[p] -> [const int]`
- **Use case**: Function parameters to prevent modification

### 2. `int * const p` - Constant Pointer to Int
```c
int * const p = &x;
*p = 10;    // OK: Can modify value
p = &y;     // ERROR: Cannot change pointer
```
- **Read as**: Constant pointer to an integer
- **Memory**: `[const p] -> [int]`
- **Use case**: Fixed pointer that won't be reassigned

### 3. `const int * const p` - Constant Pointer to Constant Int
```c
const int * const p = &x;
*p = 10;    // ERROR: Cannot modify value
p = &y;     // ERROR: Cannot change pointer
```
- **Read as**: Constant pointer to a constant integer
- **Memory**: `[const p] -> [const int]`
- **Use case**: Completely read-only access

## Reading Trick

**Read from right to left:**
- `const int *p` → p is a pointer to int that is const
- `int * const p` → p is a const pointer to int
- `const int * const p` → p is a const pointer to int that is const

## Interview Tips

1. **Clockwise/Spiral Rule**: Start from identifier, go clockwise
2. **const before `*`**: Data is constant
3. **const after `*`**: Pointer is constant
4. **Common use**: Function parameters for safety

## Compilation Errors

```c
const int *p = &x;
*p = 10;  // error: assignment of read-only location '*p'

int * const p = &x;
p = &y;   // error: assignment of read-only variable 'p'
```

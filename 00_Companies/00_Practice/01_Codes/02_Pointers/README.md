# Pointers - Complete Guide

## Topics Covered

### 1. Const Pointers (3 variations)
- `const int *p` - Pointer to constant
- `int * const p` - Constant pointer
- `const int * const p` - Both constant

### 2. Function Pointers
- Basic function pointers
- Array of function pointers
- Callbacks and dispatch tables

### 3. Function Returning Function Pointer
- Complex declarations
- State machines
- Strategy pattern

### 4. Pointer vs Array Declarations
- Pointer to array: `int (*p)[10]`
- Array of pointers: `int *p[10]`

### 5. Pointer Types
- **Dangling**: Points to freed memory
- **Wild**: Uninitialized pointer
- **Void**: Generic pointer
- **NULL**: Explicitly null pointer

### 6. Memory Functions
- `memcpy` - Copy memory (no overlap)
- `memmove` - Copy with overlap handling
- `memset` - Fill memory
- `memcmp` - Compare memory

### 7. String Functions
- `strcpy`, `strncpy` - Copy strings
- `strlen` - String length
- `strcmp` - Compare strings
- `strcat` - Concatenate strings
- `strstr` - Find substring
- `strtok` - Tokenize string

### 8. String Conversion
- `atoi` - ASCII to integer
- `itoa` - Integer to ASCII
- `atof` - ASCII to float

### 9. 2D Array Passing
- Fixed size: `void func(int arr[3][4])`
- Pointer to array: `void func(int (*arr)[4], int rows)`
- Pointer to pointer: `void func(int **arr, int rows, int cols)`
- Single pointer: `void func(int *arr, int rows, int cols)`

### 10. Dynamic 2D/3D Arrays
- Array of pointers method
- Contiguous memory method
- 3D array allocation

### 11. malloc/calloc/realloc
- **malloc**: Allocates, no initialization
- **calloc**: Allocates, zero initialization
- **realloc**: Resizes, preserves data
- **free**: Deallocates

### 12. Custom Allocator
- Simple allocator with free list
- Buddy allocator
- Memory pool

## Quick Reference

### Const Pointers
```c
const int *p;        // Can't modify *p
int * const p;       // Can't modify p
const int * const p; // Can't modify either
```

### Function Pointers
```c
int (*func)(int, int);           // Function pointer
int (*arr[10])(int, int);        // Array of function pointers
int (*get_func())(int, int);     // Returns function pointer
```

### Memory Allocation
```c
int *p = malloc(n * sizeof(int));        // Uninitialized
int *p = calloc(n, sizeof(int));         // Zero-initialized
p = realloc(p, new_size * sizeof(int));  // Resize
free(p);                                  // Deallocate
```

### 2D Array
```c
// Method 1: Array of pointers
int **arr = malloc(rows * sizeof(int *));
for (int i = 0; i < rows; i++)
    arr[i] = malloc(cols * sizeof(int));

// Method 2: Contiguous
int *arr = malloc(rows * cols * sizeof(int));
// Access: arr[i * cols + j]
```

## Interview Tips

1. **Always check malloc return**: Can return NULL
2. **Free in reverse order**: Of allocation
3. **Set to NULL after free**: Prevent dangling pointers
4. **Use const for safety**: Function parameters
5. **Understand pointer arithmetic**: Based on type size

## Common Mistakes

```c
// Mistake 1: Memory leak
int *p = malloc(100);
p = malloc(200);  // Lost first allocation

// Mistake 2: Use after free
free(p);
*p = 10;  // Undefined behavior

// Mistake 3: Double free
free(p);
free(p);  // Undefined behavior

// Mistake 4: Not checking NULL
int *p = malloc(100);
*p = 10;  // p might be NULL
```

## Best Practices

```c
// Always check allocation
int *p = malloc(size);
if (!p) {
    // Handle error
    return;
}

// Use and free
// ... use p ...
free(p);
p = NULL;  // Prevent dangling pointer

// Safe realloc
int *new_p = realloc(p, new_size);
if (!new_p) {
    free(p);  // Original still valid
    return;
}
p = new_p;
```

## Complexity

| Operation | Time | Space |
|-----------|------|-------|
| malloc | O(1) amortized | O(n) |
| calloc | O(n) | O(n) |
| realloc | O(n) worst case | O(n) |
| free | O(1) | - |
| memcpy | O(n) | O(1) |
| memmove | O(n) | O(1) |
| strcpy | O(n) | O(1) |
| strlen | O(n) | O(1) |

## Files in This Section

- 01_const_pointers/ - Const variations
- 02_function_pointers/ - Function pointer patterns
- 03_function_returning_pointer/ - Complex declarations
- 04_pointer_array_declarations/ - Pointer vs array
- 05_pointer_types/ - Dangling, wild, void, NULL
- 06_memory_functions/ - memcpy, memmove, etc.
- 07_string_functions/ - strcpy, strlen, etc.
- 08_string_conversion/ - atoi, itoa, atof
- 09_2d_array_passing/ - Multiple methods
- 10_dynamic_2d_3d_arrays/ - Dynamic allocation
- 11_malloc_calloc_realloc/ - Comparison
- 12_custom_allocator/ - Custom implementations

## Status

✅ All 12 topics implemented
✅ Multiple approaches per topic
✅ Comprehensive documentation
✅ Interview-ready examples

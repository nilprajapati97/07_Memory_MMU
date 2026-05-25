# Pointers - Quick Index

## 📑 Quick Navigation

| Topic | Files | Status |
|-------|-------|--------|
| [Const Pointers](#1-const-pointers) | 2 + README | ✅ |
| [Function Pointers](#2-function-pointers) | 3 + README | ✅ |
| [Function Returning Pointer](#3-function-returning-pointer) | 2 | ✅ |
| [Pointer/Array Declarations](#4-pointerarray-declarations) | 2 | ✅ |
| [Pointer Types](#5-pointer-types) | 1 + README | ✅ |
| [Memory Functions](#6-memory-functions) | 4 + README | ✅ |
| [String Functions](#7-string-functions) | 4 | ✅ |
| [String Conversion](#8-string-conversion) | 3 | ✅ |
| [2D Array Passing](#9-2d-array-passing) | 2 | ✅ |
| [Dynamic 2D/3D Arrays](#10-dynamic-2d3d-arrays) | 3 | ✅ |
| [malloc/calloc/realloc](#11-malloccallocrealloc) | 1 | ✅ |
| [Custom Allocator](#12-custom-allocator) | 3 + README | ✅ |

## 1. Const Pointers

### Files
- `approach1_demo.c` - Basic demonstrations
- `approach2_functions.c` - Function parameters
- `README.md` - Complete guide

### Key Concepts
```c
const int *p;        // Pointer to const int
int * const p;       // Const pointer to int
const int * const p; // Both const
```

## 2. Function Pointers

### Files
- `approach1_basic.c` - Basic function pointers
- `approach2_array.c` - Array of function pointers
- `approach3_callback.c` - Callback functions
- `README.md` - Patterns and use cases

### Key Concepts
```c
int (*func)(int, int);           // Function pointer
int (*arr[10])(int, int);        // Array of function pointers
void process(int, void (*cb)(int)); // Callback
```

## 3. Function Returning Pointer

### Files
- `approach1_basic.c` - Basic example
- `approach2_state_machine.c` - State machine pattern

### Key Concepts
```c
int (*get_func(char))(int, int);  // Returns function pointer
typedef int (*op_t)(int, int);    // Using typedef
```

## 4. Pointer/Array Declarations

### Files
- `approach1_demo.c` - Basic examples
- `approach2_complex.c` - Complex declarations

### Key Concepts
```c
int (*p)[10];  // Pointer to array of 10 ints
int *p[10];    // Array of 10 int pointers
```

## 5. Pointer Types

### Files
- `approach1_demo.c` - All types demonstrated
- `README.md` - Comprehensive guide

### Types
- **Dangling**: Points to freed memory
- **Wild**: Uninitialized
- **Void**: Generic pointer
- **NULL**: Explicitly null

## 6. Memory Functions

### Files
- `approach1_memcpy_byte.c` - Byte-wise copy
- `approach2_memcpy_word.c` - Word-wise (optimized)
- `approach1_memmove.c` - Overlap handling
- `approach1_memset_memcmp.c` - Set and compare
- `README.md` - Optimization guide

### Functions
```c
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
```

## 7. String Functions

### Files
- `approach1_strcpy_strlen.c` - Copy and length
- `approach2_strcmp_strcat.c` - Compare and concatenate
- `approach3_strstr_strtok.c` - Search and tokenize
- `approach4_strncpy.c` - Safe copy

### Functions
```c
char *strcpy(char *dest, const char *src);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
char *strcat(char *dest, const char *src);
char *strstr(const char *haystack, const char *needle);
char *strtok(char *str, const char *delim);
```

## 8. String Conversion

### Files
- `approach1_atoi.c` - ASCII to integer
- `approach2_itoa.c` - Integer to ASCII
- `approach3_atof.c` - ASCII to float

### Functions
```c
int atoi(const char *str);
char *itoa(int num, char *str, int base);
double atof(const char *str);
```

## 9. 2D Array Passing

### Files
- `approach1_fixed_size.c` - Fixed size methods
- `approach2_dynamic.c` - Dynamic methods

### Methods
```c
void func(int arr[3][4]);              // Fixed size
void func(int (*arr)[4], int rows);    // Pointer to array
void func(int **arr, int r, int c);    // Pointer to pointer
void func(int *arr, int r, int c);     // Single pointer
```

## 10. Dynamic 2D/3D Arrays

### Files
- `approach1_array_of_pointers.c` - Array of pointers
- `approach2_contiguous.c` - Contiguous memory
- `approach3_3d_array.c` - 3D array

### Allocation
```c
// Array of pointers
int **arr = malloc(rows * sizeof(int *));
for (int i = 0; i < rows; i++)
    arr[i] = malloc(cols * sizeof(int));

// Contiguous
int **arr = malloc(rows * sizeof(int *) + rows * cols * sizeof(int));
```

## 11. malloc/calloc/realloc

### Files
- `approach1_comparison.c` - Complete comparison

### Differences
```c
malloc(size);           // Allocates, NO init
calloc(n, size);        // Allocates, ZERO init
realloc(ptr, new_size); // Resizes, preserves data
free(ptr);              // Deallocates
```

## 12. Custom Allocator

### Files
- `approach1_simple.c` - Simple free list
- `approach2_buddy.c` - Buddy allocator
- `approach3_pool.c` - Pool allocator
- `README.md` - Comprehensive guide

### Types
1. **Simple**: Free list, variable size
2. **Buddy**: Power-of-2 sizes, fast coalescing
3. **Pool**: Fixed size, O(1) operations

## 🎯 Interview Cheat Sheet

### Most Asked
1. **memcpy vs memmove** - Overlap handling
2. **malloc vs calloc** - Initialization
3. **Dangling pointers** - Prevention
4. **Function pointers** - Callbacks
5. **2D array allocation** - Methods

### Common Mistakes
```c
// 1. Not checking malloc
int *p = malloc(size);
*p = 10;  // WRONG: p might be NULL

// 2. Memory leak
int *p = malloc(100);
p = malloc(200);  // Lost first allocation

// 3. Use after free
free(p);
*p = 10;  // WRONG: Undefined behavior

// 4. Dangling pointer
free(p);
// p still points to freed memory
p = NULL;  // CORRECT: Set to NULL
```

### Best Practices
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
p = NULL;  // Prevent dangling

// Safe realloc
int *new_p = realloc(p, new_size);
if (!new_p) {
    free(p);  // Original still valid
    return;
}
p = new_p;
```

## 📊 Complexity Reference

| Operation | Time | Space |
|-----------|------|-------|
| memcpy | O(n) | O(1) |
| memmove | O(n) | O(1) |
| strcpy | O(n) | O(1) |
| strlen | O(n) | O(1) |
| strcmp | O(n) | O(1) |
| malloc | O(1) | O(n) |
| free | O(1) | - |

## 🚀 Quick Test

```bash
# Test memory functions
cd 06_memory_functions
gcc approach1_memcpy_byte.c -o test && ./test

# Test string functions
cd ../07_string_functions
gcc approach1_strcpy_strlen.c -o test && ./test

# Test custom allocator
cd ../12_custom_allocator
gcc approach1_simple.c -o test && ./test
```

## 📚 Study Order

1. **Const pointers** - Fundamental
2. **Pointer types** - Safety
3. **Memory functions** - Must-know
4. **String functions** - Common
5. **Function pointers** - Advanced
6. **Dynamic allocation** - Critical
7. **Custom allocators** - Expert level

## 🎓 Learning Path

### Beginner
- Const pointers
- Basic pointer types
- Simple memory functions

### Intermediate
- Function pointers
- String functions
- 2D array passing
- malloc/calloc/realloc

### Advanced
- Custom allocators
- Complex declarations
- Optimization techniques

---

**Total**: 30 C programs, 7 READMEs
**Status**: 100% Complete ✅
**Ready**: Interview preparation

# Pointer Types

## 1. Dangling Pointer

**Definition**: Pointer pointing to memory that has been freed or deallocated.

### Causes:
```c
// Case 1: After free
int *ptr = malloc(sizeof(int));
free(ptr);
// ptr is now dangling

// Case 2: Returning local variable address
int *func() {
    int x = 10;
    return &x;  // DANGER: x destroyed after return
}

// Case 3: Variable goes out of scope
int *ptr;
{
    int x = 10;
    ptr = &x;
}
// ptr is dangling here
```

### Prevention:
```c
free(ptr);
ptr = NULL;  // Set to NULL after free
```

## 2. Wild Pointer

**Definition**: Uninitialized pointer containing garbage value.

```c
int *wild;  // Contains random address
*wild = 10; // UNDEFINED BEHAVIOR
```

### Prevention:
```c
int *ptr = NULL;  // Always initialize
```

## 3. Void Pointer

**Definition**: Generic pointer that can point to any data type.

```c
void *ptr;
int x = 10;
ptr = &x;  // OK
printf("%d\n", *(int *)ptr);  // Must cast before dereferencing
```

### Characteristics:
- Cannot be dereferenced directly
- Must be cast to specific type
- Used in generic functions (malloc, qsort, etc.)
- No pointer arithmetic without casting

### Use Cases:
```c
void *malloc(size_t size);
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
```

## 4. NULL Pointer

**Definition**: Pointer explicitly set to point to nothing (address 0).

```c
int *ptr = NULL;
```

### Characteristics:
- Guaranteed to not point to valid memory
- Safe to check: `if (ptr != NULL)`
- Dereferencing causes segmentation fault
- Defined as `((void *)0)` or `0`

### Best Practices:
```c
// Always check before use
if (ptr != NULL) {
    *ptr = 10;
}

// Initialize pointers
int *ptr = NULL;

// Set to NULL after free
free(ptr);
ptr = NULL;
```

## Comparison Table

| Type | Initialized | Points To | Safe to Dereference |
|------|-------------|-----------|---------------------|
| Dangling | Yes | Freed memory | No |
| Wild | No | Random | No |
| Void | Yes | Any type | No (must cast) |
| NULL | Yes | Nothing (0) | No |

## Interview Tips

1. **Dangling**: Always set to NULL after free
2. **Wild**: Always initialize pointers
3. **Void**: Used for generic programming
4. **NULL**: Check before dereferencing

## Common Mistakes

```c
// Mistake 1: Using after free
int *p = malloc(sizeof(int));
free(p);
*p = 10;  // WRONG

// Mistake 2: Uninitialized
int *p;
*p = 10;  // WRONG

// Mistake 3: Void pointer arithmetic
void *p = malloc(10);
p++;  // WRONG (need to cast first)

// Mistake 4: Not checking NULL
int *p = malloc(sizeof(int));
*p = 10;  // WRONG (malloc can return NULL)
```

## Safe Patterns

```c
// Pattern 1: Safe allocation
int *p = malloc(sizeof(int));
if (p != NULL) {
    *p = 10;
    free(p);
    p = NULL;
}

// Pattern 2: Safe initialization
int *p = NULL;
if (condition) {
    p = &some_variable;
}
if (p != NULL) {
    *p = 10;
}
```

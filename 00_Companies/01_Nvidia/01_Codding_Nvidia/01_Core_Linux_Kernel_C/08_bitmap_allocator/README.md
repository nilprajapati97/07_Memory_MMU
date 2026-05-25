# Q8: Implement a bitmap allocator

## Code
See solution.c for the implementation.

## In-depth Explanation (Nvidia Interview Style)

A bitmap allocator is a compact way to manage resources (e.g., memory pages, IDs) using a bit array. Each bit represents the allocation state of a resource.

### How it works
- `alloc_bit()` scans for a 0 bit, sets it, and returns the index.
- `free_bit()` clears the bit at the given index.

### Example Usage
```c
int idx = alloc_bit(); // allocate a resource
free_bit(idx); // free the resource
```

### Why is this important for Nvidia/Linux Kernel?
- Used in memory management, ID allocation, and device drivers.
- Efficient, low-overhead, and easy to scale.

### Interview Tips
- Be ready to discuss concurrency and atomicity for multi-threaded use.
- Know how to extend to larger bitmaps (arrays of words).

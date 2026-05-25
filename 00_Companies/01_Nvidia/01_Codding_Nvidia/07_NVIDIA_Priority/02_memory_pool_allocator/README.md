# Memory pool allocator

## In-depth Explanation (Nvidia Interview Style)

- Memory pools pre-allocate a fixed-size block of memory for fast allocation/deallocation.
- Used in kernel for objects with predictable lifetimes (e.g., slab allocator).

### Interview Tips
- Be ready to discuss fragmentation, free lists, and concurrency.

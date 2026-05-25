# Implement your own malloc() and free()

## Code
See solution.c for a simple bump allocator example.

## In-depth Explanation (Nvidia Interview Style)

- Real malloc/free implementations use complex data structures (free lists, bins, etc.).
- This example uses a static buffer and a bump pointer for simplicity.
- In interviews, discuss fragmentation, thread safety, and how to implement free.

### Interview Tips
- Be ready to discuss memory fragmentation, alignment, and thread safety.
- Know how real allocators (glibc, kernel slab) work at a high level.

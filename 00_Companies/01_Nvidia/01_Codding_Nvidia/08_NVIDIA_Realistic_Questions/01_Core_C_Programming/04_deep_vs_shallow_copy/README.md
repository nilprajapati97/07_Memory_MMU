# Deep copy vs shallow copy (implement both)

## Code
See solution.c for both implementations.

## In-depth Explanation (Nvidia Interview Style)

- Shallow copy: Copies pointers as-is; both structs share the same memory for pointer fields.
- Deep copy: Allocates new memory and copies the data, so each struct is independent.

### Interview Tips
- Be ready to discuss memory management, double-free bugs, and when to use each approach.

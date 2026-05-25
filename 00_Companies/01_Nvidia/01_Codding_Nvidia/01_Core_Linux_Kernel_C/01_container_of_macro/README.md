# Q1: Implement container_of() macro

## Code
See solution.c for the implementation.

## In-depth Explanation (Nvidia Interview Style)

The `container_of()` macro is a powerful C construct used extensively in the Linux kernel. It allows you to retrieve a pointer to the parent structure from a pointer to one of its members. This is crucial for implementing generic data structures (like linked lists) where you only have a pointer to a member, but need access to the containing structure.

### How it works
- `offsetof(type, member)` computes the byte offset of `member` within `type`.
- Subtracting this offset from the member pointer gives the base address of the structure.
- The macro uses GCC extensions (statement expressions and typeof) for type safety and flexibility.

### Example Usage
```c
struct node {
    int data;
    struct list_head list;
};

struct list_head *ptr = ...;
struct node *n = container_of(ptr, struct node, list);
```

### Why is this important for Nvidia/Linux Kernel?
- Enables generic, reusable data structures.
- Avoids code duplication.
- Used in list management, device drivers, and more.

### Interview Tips
- Be ready to explain pointer arithmetic and type safety.
- Understand why `container_of` is preferred over casting.
- Know where it’s used in the kernel (e.g., linked lists, file operations).

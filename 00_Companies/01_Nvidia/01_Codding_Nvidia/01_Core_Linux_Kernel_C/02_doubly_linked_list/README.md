# Q2: Implement a doubly linked list like Linux `list_head`

## Code
See solution.c for a minimal implementation and example usage.

## In-depth Explanation (Nvidia Interview Style)

### What is `list_head`?
- `list_head` is a generic, circular, doubly linked list structure used throughout the Linux kernel.
- It allows embedding a list node inside any structure, enabling generic list operations.

### Key Operations
- `INIT_LIST_HEAD`: Initializes a list head.
- `list_add`: Adds a new entry after the head.
- `list_add_tail`: Adds a new entry before the head (at the tail).
- `list_del`: Removes an entry from the list.

### Why is this important for Nvidia/Linux Kernel?
- Used for managing lists of devices, tasks, memory blocks, etc.
- Enables efficient insertion and deletion from any position in O(1) time.

### Interview Tips
- Be ready to explain pointer arithmetic and container_of usage.
- Know how to traverse and modify the list safely.
- Understand the benefits of circular vs. linear lists in the kernel.

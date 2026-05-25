# Q23: Write safe user-kernel buffer copy code

## Code
See solution.c for the implementation.

## In-depth Explanation (Nvidia Interview Style)

- Use `copy_from_user()` and `copy_to_user()` for safe memory access between user and kernel space.
- Always check the return value for errors (non-zero means failure).
- Never trust user pointers; validate and handle errors gracefully.

### Interview Tips
- Be ready to discuss security implications of user-kernel memory access.
- Know how to avoid kernel panics and data leaks.

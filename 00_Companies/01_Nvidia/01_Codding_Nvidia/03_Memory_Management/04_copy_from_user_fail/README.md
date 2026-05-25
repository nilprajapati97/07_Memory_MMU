# Q22: What happens if copy_from_user() fails?

## In-depth Explanation (Nvidia Interview Style)

- Returns number of bytes not copied (non-zero means failure).
- Can fail if user pointer is invalid or memory is not accessible.
- Kernel code must check the return value and handle errors gracefully.

### Interview Tips
- Be ready to discuss safe user-kernel memory access.
- Know how to avoid kernel panics from bad user pointers.

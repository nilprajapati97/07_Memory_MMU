# Q7: Detect integer overflow in kernel C

## Code
See solution.c for the implementation.

## In-depth Explanation (Nvidia Interview Style)

Integer overflow bugs can lead to security vulnerabilities and system crashes. In kernel code, it’s critical to check for overflow before performing arithmetic operations.

### How it works
- For addition, check if the result would exceed INT_MAX or go below INT_MIN before adding.
- Return a boolean indicating overflow, and only set the result if safe.

### Example Usage
```c
int res;
bool of = add_overflow(a, b, &res);
```

### Why is this important for Nvidia/Linux Kernel?
- Prevents buffer overflows, memory corruption, and security issues.
- Linux kernel provides helpers like `check_add_overflow()` for this purpose.

### Interview Tips
- Be ready to discuss signed/unsigned overflow.
- Know how to handle other operations (subtraction, multiplication).

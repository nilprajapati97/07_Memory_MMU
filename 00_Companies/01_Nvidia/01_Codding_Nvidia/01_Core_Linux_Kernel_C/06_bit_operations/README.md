# Q6: Implement bit operations: set, clear, test bit

## Code
See solution.c for the implementation.

## In-depth Explanation (Nvidia Interview Style)

Bit manipulation is fundamental in kernel and driver code for managing flags, hardware registers, and efficient data structures. The three basic operations are:
- **Set bit:** Use bitwise OR to set a specific bit.
- **Clear bit:** Use bitwise AND with the negated bit mask to clear a bit.
- **Test bit:** Use bitwise AND to check if a bit is set.

### Example Usage
```c
unsigned int flags = 0;
set_bit(&flags, 2); // sets bit 2
clear_bit(&flags, 2); // clears bit 2
bool is_set = test_bit(flags, 2); // tests bit 2
```

### Why is this important for Nvidia/Linux Kernel?
- Used in managing device state, interrupt masks, and memory management.
- Efficient and atomic bit operations are critical for performance and correctness.

### Interview Tips
- Be ready to discuss atomicity and concurrency issues with bit operations.
- Know how these are implemented in the Linux kernel (e.g., `set_bit`, `clear_bit`, `test_bit` macros/functions).

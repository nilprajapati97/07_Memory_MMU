# Q14: Fix a race condition in shared counter code

## Code
See solution.c for the implementation.

## In-depth Explanation (Nvidia Interview Style)

Race conditions occur when multiple threads access and modify shared data concurrently. Protect shared counters with spinlocks or atomic operations to ensure correctness.

### Interview Tips
- Be ready to discuss atomic_t and alternatives to spinlocks.
- Know when to use spinlocks vs. mutexes.

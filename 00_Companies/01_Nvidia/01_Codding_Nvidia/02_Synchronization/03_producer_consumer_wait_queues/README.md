# Q13: Implement producer-consumer using wait queues

## Code
See solution.c for the implementation.

## In-depth Explanation (Nvidia Interview Style)

Wait queues are used in the Linux kernel to block processes until a condition becomes true. They are essential for implementing producer-consumer patterns without busy-waiting.

### How it works
- Producer adds data and wakes up consumers.
- Consumer waits for data using `wait_event()` and then consumes it.
- Spinlocks protect the queue data structure.

### Interview Tips
- Be ready to discuss race conditions and wake-up mechanisms.
- Know the difference between wait queues and completion variables.

# Q12: When can you not sleep in kernel code?

## In-depth Explanation (Nvidia Interview Style)

- Cannot sleep in atomic context (e.g., interrupt handlers, code holding spinlocks).
- Sleeping is only allowed in process context.
- Sleeping in atomic context can cause kernel panics or deadlocks.

### Interview Tips
- Be ready to explain the difference between process and atomic context.
- Know how to check context (e.g., `in_atomic()`, `in_interrupt()`).

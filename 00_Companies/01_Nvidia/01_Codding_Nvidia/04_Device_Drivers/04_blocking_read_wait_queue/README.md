# Q29: Implement blocking read using wait queue

## In-depth Explanation (Nvidia Interview Style)

- Use wait queues to block a process until data is available.
- `wait_event_interruptible()` is commonly used in read operations.
- Wake up waiting processes when data arrives.

### Interview Tips
- Be ready to discuss race conditions and wake-up mechanisms.
- Know the difference between blocking and non-blocking I/O.

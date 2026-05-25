***Circular Buffer: Implement a thread-safe circular buffer (lock-free or using mutexes) for producer-consumer scenarios.***

# Thread-Safe Circular Buffer (Producer-Consumer)

This directory contains a C implementation of a thread-safe circular buffer using mutexes and condition variables for producer-consumer scenarios.

## Features
- Fixed-size buffer (BUFFER_SIZE)
- Multiple producers/consumers possible
- Mutex for mutual exclusion
- Condition variables for blocking on full/empty

## How it works
- **Producer** waits if buffer is full, then inserts item and signals consumer.
- **Consumer** waits if buffer is empty, then removes item and signals producer.
- All operations are thread-safe.

## Usage
- See `circular_buffer_mutex.c` for a demo with one producer and one consumer thread.

## Lock-Free Variant
A lock-free version can be implemented using atomic operations (e.g., C11 atomics), but is more complex and platform-dependent.

---

For most use cases, the mutex-based version is robust and easy to understand.

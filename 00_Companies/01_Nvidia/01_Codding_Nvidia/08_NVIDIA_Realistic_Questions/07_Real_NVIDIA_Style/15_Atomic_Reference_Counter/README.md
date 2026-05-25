# Atomic Reference Counters in the Linux Kernel

## Problem
How do you implement an atomic reference counter in the Linux kernel? Why is it important?

## Solution Overview
- Use `atomic_t` for the reference count.
- Use `atomic_inc()` to increment and `atomic_dec_and_test()` to decrement and check for zero.

## Importance
- Ensures safe object lifetime management in concurrent code.
- Prevents use-after-free and memory leaks.

## Interview Notes
- Discuss race conditions, memory barriers, and object cleanup.
- Be ready to explain when to use atomic_t vs refcount_t.

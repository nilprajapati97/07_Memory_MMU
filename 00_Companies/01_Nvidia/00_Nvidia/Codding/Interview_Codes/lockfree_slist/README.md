# Lock-Free Singly Linked List (Linux Kernel Style)

## Overview
This directory contains a conceptual implementation of a lock-free singly linked list suitable for use in the Linux kernel. It demonstrates how to handle the ABA problem using pointer tagging (version counters).

## Files
- `lockfree_slist.c`: Core implementation of the lock-free singly linked list.
- `README.md`: This documentation.

## ABA Problem Handling
The ABA problem occurs when a memory location is changed from value A to B and back to A, making it appear unchanged to a thread. To prevent this, we use a version counter (tag) alongside the pointer. Every update increments the version, so even if the pointer is the same, the version will differ if an intermediate change occurred.

## Key Concepts
- **atomic64_t**: Used to store both the pointer and a version counter.
- **Pointer Tagging**: Lower bits store the pointer, upper bits store the version.
- **atomic64_cmpxchg**: Ensures lock-free, atomic updates.

## Usage
This code is for educational purposes and is not production-ready. In real kernel code, use kernel-provided lock-free primitives and memory barriers as appropriate.

## References
- [Linux Kernel Documentation: Lockless Algorithms](https://www.kernel.org/doc/html/latest/locking/lockless-design.html)
- [The ABA Problem](https://en.wikipedia.org/wiki/ABA_problem)

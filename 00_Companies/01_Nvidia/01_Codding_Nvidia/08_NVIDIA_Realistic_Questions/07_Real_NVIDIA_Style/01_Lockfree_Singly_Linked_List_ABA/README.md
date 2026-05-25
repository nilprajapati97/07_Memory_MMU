# Lock-Free Singly Linked List (Linux Kernel, ABA-aware)

## Problem
Implement a lock-free singly linked list for use in the Linux kernel. How do you handle the ABA problem?

## Solution Overview
- Uses atomic operations and a tagged pointer (pointer + version tag) to avoid the ABA problem.
- Each update increments the tag, so even if a pointer is reused, the tag will differ.
- No global locks; safe for concurrent push/pop.

## ABA Problem
- ABA occurs when a pointer is removed, freed, and then reused, making CAS succeed incorrectly.
- Solution: Use a tag/counter with the pointer (tagged pointer) or hazard pointers.

## Kernel Context
- Uses `atomic_long_t` for atomic head updates.
- Uses `kmalloc`/`kfree` for node allocation.
- Suitable for lock-free queues, stacks, and freelists in kernel modules.

## Interview Notes
- Be ready to discuss memory reclamation (hazard pointers, RCU), and why simple CAS is not enough.
- Discuss tradeoffs: tagged pointers vs hazard pointers vs RCU.

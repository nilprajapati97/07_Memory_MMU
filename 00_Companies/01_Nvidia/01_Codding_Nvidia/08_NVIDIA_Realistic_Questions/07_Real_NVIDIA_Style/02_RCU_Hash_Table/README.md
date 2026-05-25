# Kernel-Space Hash Table with RCU and Spinlocks

## Problem
Write a kernel-space hash table supporting concurrent RCU-protected lookups and spinlock-protected updates.

## Solution Overview
- Lookups use RCU read-side critical sections for lockless concurrency.
- Updates (insert/delete) use per-bucket spinlocks for safety.
- Uses hlist and RCU primitives from the Linux kernel.

## RCU in the Kernel
- RCU allows readers to traverse data structures without locks.
- Writers synchronize with readers using synchronize_rcu().
- Used in routing tables, process lists, and more.

## Interview Notes
- Discuss RCU vs lock-free vs lock-based designs.
- Be ready to explain memory reclamation and why synchronize_rcu() is needed after deletion.

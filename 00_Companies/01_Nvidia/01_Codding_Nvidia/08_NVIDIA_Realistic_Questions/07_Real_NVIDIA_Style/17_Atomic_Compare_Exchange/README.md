# Atomic Compare-and-Exchange in the Linux Kernel

## Problem
How do you perform an atomic compare-and-exchange in the Linux kernel? What are the use cases?

## Solution Overview
- Use `atomic_cmpxchg()` to atomically update a variable if it matches an expected value.
- Returns the old value; update occurs only if old matches.

## Use Cases
- Lock-free data structures, reference counters, and state machines.
- Avoids race conditions in concurrent code.

## Interview Notes
- Discuss ABA problem, memory barriers, and atomic API details.
- Be ready to explain lock-free algorithms and pitfalls.

# Per-CPU Counters in the Linux Kernel

## Problem
How do you implement a scalable per-CPU counter in the Linux kernel? Why is it better than a global atomic counter?

## Solution Overview
- Use `DEFINE_PER_CPU` to declare a per-CPU variable.
- Use `this_cpu_inc()` for fast, lockless increments.
- Aggregate with `for_each_possible_cpu()` and `per_cpu()`.

## Advantages
- Avoids cacheline bouncing and contention on SMP systems.
- Scales well with many CPUs.

## Interview Notes
- Discuss tradeoffs: accuracy vs scalability, aggregation cost.
- Be ready to explain per-CPU data structures and SMP primitives.

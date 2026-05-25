# Seqlocks in the Linux Kernel

## Problem
How do you use a seqlock for protecting shared data in the Linux kernel? What are the tradeoffs?

## Solution Overview
- Writers use `write_seqlock()`/`write_sequnlock()` to update data.
- Readers use `read_seqbegin()`/`read_seqretry()` to read data safely.
- Readers may retry if a concurrent write occurs.

## Tradeoffs
- Seqlocks are efficient for many readers, few writers.
- Readers may starve if writers are frequent.

## Interview Notes
- Discuss use cases: timekeeping, statistics, etc.
- Be ready to explain starvation, livelock, and seqlock API details.

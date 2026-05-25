
# Simple ARM64 Memory Barrier Demo

This directory contains a simple but powerful C program that demonstrates the use and necessity of memory barriers in a multi-threaded context on ARM64/Linux. This is the kind of code you might be asked to explain or modify in a systems interview at Nvidia or similar companies.

## Requirements
- Linux system (preferably ARM64, but also works on x86)
- GCC with pthread support

## Build
```sh
gcc -O2 -pthread -o simple_mb_demo simple_mb_demo.c
```

## Run
```sh
./simple_mb_demo
```

## What Does the Demo Do?
- Launches two threads that each write to one shared variable and read from the other, with a memory barrier in between.
- Repeats this test many times, looking for the rare case where both threads read zero—indicating a reordering that should not happen with proper barriers.
- Demonstrates the importance of memory barriers for correct synchronization on weakly ordered systems like ARM64.

## Code Walkthrough
```c
// Thread 1
x = 1;
__sync_synchronize(); // Full memory barrier
r1 = y;

// Thread 2
y = 1;
__sync_synchronize(); // Full memory barrier
r2 = x;
```
If both `r1` and `r2` are zero in the same iteration, it means both threads saw the other's write as not yet visible—a classic reordering bug.

## Expected Output
```
Test finished.
```
If you ever see:
```
Reordering observed at iteration X! r1=0, r2=0
```
It means the barrier failed (should not happen with proper barriers).

## Learning Objectives
- Understand how weak memory ordering can break seemingly correct code.
- See the effect of adding and removing memory barriers.
- Gain intuition for why barriers are essential in concurrent programming on ARM64.

## Interview-Ready Insights
- Be able to explain why this test is important, what it demonstrates, and how it relates to real-world kernel or driver code.
- Try removing the `__sync_synchronize()` calls to see the effect of no barriers—this is a classic interview exercise.

---

**Try removing the `__sync_synchronize()` calls to see the effect of no barriers! Be ready to explain the results in detail.**

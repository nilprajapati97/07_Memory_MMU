#   ***Custom Memory Allocator: Implement a basic malloc and free using sbrk. How do you handle fragmentation?***
==============================================================================================


# Custom Memory Allocator (malloc/free using sbrk)

This directory contains a simple implementation of a custom memory allocator in C, using `sbrk` for heap management. It also includes a deep explanation of fragmentation and strategies to handle it.

## Files
- `allocator.c`: Source code for the custom allocator (`malloc`, `free`, and supporting functions)
- `README.md`: This documentation

## Overview
- Implements basic `malloc` and `free` using `sbrk`.
- Handles fragmentation using a free list and block coalescing.
- Explains fragmentation and mitigation strategies in detail below.

---

## Fragmentation in Memory Allocators

### 1. What is Fragmentation?
Fragmentation occurs when free memory is broken into small, non-contiguous blocks, making it difficult to allocate large contiguous memory even if the total free memory is sufficient.

- **External Fragmentation:** Free memory is split into small blocks scattered throughout the heap.
- **Internal Fragmentation:** Allocated memory blocks are larger than requested, wasting space inside blocks.

### 2. How This Allocator Handles Fragmentation
- **Free List:** Keeps track of all free blocks.
- **Coalescing:** When a block is freed, adjacent free blocks are merged to form a larger block, reducing external fragmentation.
- **First-fit Allocation:** Allocates the first sufficiently large block found in the free list.

### 3. Limitations
- This simple allocator does not implement splitting of large blocks on allocation, but this can be added for better memory utilization.
- No thread safety (single-threaded only).

---

See `allocator.c` for code and comments explaining each step.
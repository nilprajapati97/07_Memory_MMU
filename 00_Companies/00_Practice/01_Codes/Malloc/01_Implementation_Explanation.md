# Custom Malloc Implementation - In-Depth Explanation

This document provides a detailed explanation of the custom `malloc` implementation, starting from scratch, tailored for an NVIDIA interview.

## Overview
The custom `malloc` implementation is a memory allocator that manages memory manually without relying on the standard library's `malloc`. It uses a linked list to track memory blocks and system calls like `mmap` to allocate memory from the operating system.

### Key Components
1. **Block Structure**: Each memory block is represented by a `struct Block`, which contains metadata such as size, free status, and a pointer to the next block.
2. **Linked List**: The memory blocks are organized as a linked list, with the `head` pointing to the first block.
3. **System Calls**: The `mmap` system call is used to request memory from the OS.

### Functions
#### 1. `find_free_block`
This function traverses the linked list to find a free block of sufficient size. If no such block exists, it returns `NULL`.

#### 2. `request_memory`
This function uses `mmap` to allocate a new memory block from the OS. The block's metadata is initialized, and it is added to the linked list.

#### 3. `my_malloc`
This function allocates memory by:
- Searching for a free block using `find_free_block`.
- Requesting a new block using `request_memory` if no free block is found.
- Returning a pointer to the usable memory area (excluding metadata).

#### 4. `my_free`
This function marks a block as free by setting its `free` flag to `1`.

#### 5. `my_realloc`
This function resizes a memory block by:
- Allocating a new block if the current block is too small.
- Copying data to the new block.
- Freeing the old block.

### Test Case
The `main` function demonstrates the usage of `my_malloc`, `my_free`, and `my_realloc`.

## Memory Management Details
- **Alignment**: Ensures memory alignment for platform compatibility.
- **Fragmentation**: Minimizes fragmentation by reusing free blocks.
- **Error Handling**: Handles allocation failures gracefully.

## Optimization Opportunities
- Implementing a best-fit or first-fit strategy for block allocation.
- Adding thread safety for multithreaded environments.
- Using `sbrk` as an alternative to `mmap` for simpler systems.

This implementation is designed to be simple yet effective, making it suitable for embedded systems and high-performance applications.
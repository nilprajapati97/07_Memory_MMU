# Linux Kernel & NVIDIA Interview Coding Questions

## Layered Directory Structure

This workspace is organized in a layered, topic-wise manner for clarity and ease of navigation. Each directory represents a major topic or layer, with subdirectories for specific questions or concepts. Every question directory contains:
- `solution.c`: C code solution (kernel/system style)
- `README.md`: In-depth explanation (Nvidia interview style)

---

## Directory Overview

### 1. Core Linux Kernel C
- container_of macro
- doubly linked list
- circular buffer
- reference counting
- ...

### 2. Synchronization
- mutex, spinlock, semaphore, rwlock
- race conditions, deadlocks, RCU
- ...

### 3. Memory Management
- kmalloc, vmalloc, alloc_pages
- page alignment, GFP flags, user/kernel copy
- ...

### 4. Device Drivers
- char device, ioctl, wait queues
- poll/select/epoll
- ...

### 5. Debugging Questions
- kernel panic, dmesg, ftrace, perf, kgdb
- use-after-free, race condition debugging
- ...

### 6. NVIDIA GPU Relevant Topics
- IOMMU, DMA, PCI BAR, MMIO, cache coherency
- ...

### 7. NVIDIA Priority (High-Yield)
- memmove, memory pool allocator, producer-consumer, circular buffer, spinlock
- atomic refcount, lock-free stack (ABA), SPSC ring buffer, deadlock debugging
- memory barriers, use-after-free, DMA buffer ownership

### 8. NVIDIA Realistic Questions
- Core C, Data Structures, OS/Kernel Coding, System Programming
- Kernel Concepts, Algorithmic Problems, Real NVIDIA Style, Debugging Round
- Each subdirectory contains realistic, interview-style problems

---

## How to Use
- Start from the topic layer you want to study.
- Dive into each question for code and explanations.
- Use this structure for systematic interview prep or revision.

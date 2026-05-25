# Custom Slab Allocator for Fixed-Size GPU Buffers

## Problem
Implement a custom slab allocator for fixed-size GPU buffer objects. How do you handle cache coloring and fragmentation?

## Solution Overview
- Pre-allocates a pool of fixed-size objects (slabs) for fast allocation/deallocation.
- Uses a free list protected by a spinlock.
- Implements cache coloring by assigning a color to each object to reduce cache conflicts.

## Cache Coloring
- Assigns objects to different cache lines/sets to minimize cache contention.
- Color is cycled for each allocation.

## Fragmentation Handling
- Internal fragmentation is minimized by using fixed-size objects.
- External fragmentation is avoided by pooling.

## Kernel Context
- Uses `kmalloc`/`kfree` for slab and object allocation.
- Suitable for GPU buffer management, DMA pools, etc.

## Interview Notes
- Discuss tradeoffs of slab vs buddy vs page allocators.
- Be ready to explain cache coloring, fragmentation, and concurrency.

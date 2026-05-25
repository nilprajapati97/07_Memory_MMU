# Custom Slab Allocator for Fixed-Size GPU Buffer Objects
# 3. Implement a custom slab allocator for fixed-size GPU buffer objects. How do you handle cache coloring and fragmentation?


## Problem
Implement a custom slab allocator for fixed-size GPU buffer objects. How do you handle cache coloring and fragmentation?

## Solution Overview
- **Slab Allocator:** Allocates memory in slabs (pages) divided into fixed-size objects.
- **Cache Coloring:** Each slab is assigned a color to distribute objects across different cache lines, reducing cache conflicts.
- **Fragmentation Handling:**
  - Free objects are tracked per slab.
  - Slabs are categorized as full, partial, or free.
  - When all objects in a slab are freed, the slab can be released or reused.

## Key Structures
- `slab_obj`: Represents a single buffer object.
- `slab_page`: Represents a slab (page) containing multiple objects and a color.
- `slab_cache`: Manages all slabs and provides locking for concurrency.

## Allocation/Freeing
- **Allocation:**
  - Try to allocate from a partial slab with the desired color.
  - If none, create a new slab with that color.
- **Free:**
  - Return the object to its slab.
  - If the slab becomes fully free, move it to the free list.

## Cache Coloring
- The color is chosen based on CPU or round-robin to spread allocations across cache lines.

## Fragmentation
- By grouping objects and tracking free/partial/full slabs, fragmentation is minimized.
- Free slabs can be released to the system if memory pressure is high (not shown in demo code).

## Note
- This is a simplified demonstration. Production code should track which slab owns each object for safe freeing and support NUMA, huge pages, etc.

# Q19: Difference between kmalloc, vmalloc, alloc_pages

## In-depth Explanation (Nvidia Interview Style)

- **kmalloc:** Allocates physically contiguous memory, suitable for DMA and hardware buffers.
- **vmalloc:** Allocates virtually contiguous but physically non-contiguous memory, used for large allocations.
- **alloc_pages:** Allocates one or more whole pages, returns struct page pointer.

### Interview Tips
- Know when to use each allocator.
- Be ready to discuss performance and hardware constraints.

# Q20: Implement page-aligned allocation logic

## Code
See solution.c for the implementation.

## In-depth Explanation (Nvidia Interview Style)

Page alignment is required for DMA, hardware buffers, and some kernel APIs. Use `PAGE_ALIGN()` macro and `kmalloc` for page-aligned allocations.

### Interview Tips
- Be ready to discuss alignment requirements for different hardware.

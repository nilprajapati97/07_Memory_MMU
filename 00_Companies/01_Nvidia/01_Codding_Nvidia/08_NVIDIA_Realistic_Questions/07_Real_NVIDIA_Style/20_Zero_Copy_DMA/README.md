# Zero-Copy DMA Buffer Mapping in the Linux Kernel

## Problem
How do you implement zero-copy DMA between user space and a device in the Linux kernel? What are the APIs and considerations?

## Solution Overview
- Use `get_user_pages_fast()` to pin user pages.
- Use `dma_map_page()` to map the page for device DMA.
- Unmap and release the page after DMA is done.

## Key Considerations
- Properly handle pinning, mapping, and unmapping.
- Ensure cache coherency and error handling.

## Interview Notes
- Discuss security, lifetime of user buffers, and DMA direction.
- Be ready to explain zero-copy tradeoffs and pitfalls.

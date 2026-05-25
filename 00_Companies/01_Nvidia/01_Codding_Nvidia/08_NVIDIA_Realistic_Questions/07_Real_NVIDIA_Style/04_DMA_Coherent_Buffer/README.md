# DMA-Coherent Buffer Allocation in Linux Device Drivers

## Problem
How do you allocate and use a DMA-coherent buffer in a Linux device driver? What are the key considerations?

## Solution Overview
- Use `dma_alloc_coherent()` to allocate a buffer accessible by both CPU and device without explicit cache management.
- Always free with `dma_free_coherent()`.
- Requires a valid `struct device *`.

## Key Considerations
- DMA-coherent buffers are mapped for both CPU and device, avoiding cache flush/invalidate.
- Allocation may fail if memory is fragmented or not available.
- Device must support DMA and be properly initialized.

## Interview Notes
- Discuss differences between coherent and streaming DMA APIs.
- Be ready to explain cache management, alignment, and device/CPU synchronization.

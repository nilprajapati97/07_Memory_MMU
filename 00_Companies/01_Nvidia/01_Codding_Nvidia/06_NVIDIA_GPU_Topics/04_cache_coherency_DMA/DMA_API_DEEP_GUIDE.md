# Linux DMA API Deep Guide

This document summarizes and explains the official Linux DMA API documentation for interviews and practical driver development.

---

## 1. DMA API Basics
- **Header:** `#include <linux/dma-mapping.h>`
- **dma_addr_t:** Address type for DMA, not directly usable by CPU.
- **Why:** DMA addresses may differ from CPU physical addresses due to IOMMU or bus translation.

---

## 2. Allocating DMA-Coherent Buffers
- **dma_alloc_coherent():** Allocates memory visible and consistent for both CPU and device (no cache issues).
  - Returns both a CPU pointer and a DMA address.
  - Use for buffers accessed frequently by both CPU and device.
  - Example:
    ```c
    void *cpu_addr = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
    ```
- **dma_free_coherent():** Frees memory allocated by `dma_alloc_coherent()`.
- **dma_pool_create()/dma_pool_alloc():** For many small, aligned, coherent buffers (e.g., descriptors).

---

## 3. DMA Addressing Limitations
- **DMA mask:** Devices may not be able to address all of RAM. Use `dma_set_mask()` to set the addressable range.
- **dma_max_mapping_size():** Returns the largest buffer size the device can map in one go.

---

## 4. Streaming DMA Mappings
- **dma_map_single():** Maps an existing buffer for DMA (not necessarily coherent).
  - Must be physically contiguous (e.g., from kmalloc).
  - Specify direction: `DMA_TO_DEVICE`, `DMA_FROM_DEVICE`, or `DMA_BIDIRECTIONAL`.
- **dma_unmap_single():** Unmaps the buffer after DMA is done.
- **dma_sync_single_for_cpu()/dma_sync_single_for_device():** Synchronize CPU and device views of memory (flush/invalidate caches as needed).

**Key Interview Point:** Always align buffers to cache line or page boundaries to avoid partial cache line issues.

---

## 5. Scatter/Gather DMA
- **dma_map_sg():** Maps a scatterlist for DMA (for non-contiguous memory).
- **dma_unmap_sg():** Unmaps the scatterlist.
- **sg_dma_address()/sg_dma_len():** Get DMA address/length for each segment.

---

## 6. Non-Coherent DMA Allocations
- For platforms where CPU and device caches are not coherent.
- Use `dma_alloc_noncoherent()` and manage cache explicitly with sync calls.

---

## 7. Debugging DMA API Usage
- **DMA API Debugging:** Kernel can check for correct use of DMA API (enable in kernel config).
- **dma_debug:** Debugfs interface for tracking DMA mappings and errors.

---

## 8. Best Practices
- Always use the DMA API, not physical addresses or manual cache management.
- Check return values for mapping functions (they can fail).
- Use the correct direction and sync calls.
- Free/unmap with the same size and parameters as allocation/mapping.
- For portable drivers, avoid architecture-specific tricks.

---

## 9. Example Interview Q&A
- **Q:** Why use `dma_alloc_coherent()` instead of `kmalloc()`?
  - **A:** Ensures memory is cache-coherent for both CPU and device, avoiding stale data.
- **Q:** What happens if you forget to sync before/after DMA?
  - **A:** Data corruption or stale data due to unsynchronized caches.
- **Q:** How do you debug DMA mapping errors?
  - **A:** Enable DMA API debugging in the kernel, use debugfs, and check kernel logs for warnings.

---

## 10. Further Reading
- [Linux DMA API Documentation](https://www.kernel.org/doc/html/latest/core-api/dma-api.html)
- [Dynamic DMA mapping Guide](https://www.kernel.org/doc/html/latest/core-api/dma-api-howto.html)

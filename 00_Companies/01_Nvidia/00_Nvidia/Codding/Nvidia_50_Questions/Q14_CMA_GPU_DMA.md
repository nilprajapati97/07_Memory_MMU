# Q14: CMA (Contiguous Memory Allocator) for GPU DMA

**Section:** Memory Management | **Difficulty:** Hard | **Topics:** CMA, `dma_alloc_coherent`, scatter-gather, `sg_table`, DMA streaming, IOVA, pinned pages

---

## Question

Implement CMA (Contiguous Memory Allocator) usage for GPU DMA.

---

## Answer

```c
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/mm.h>

#define DMA_BUF_SIZE (64 * 1024 * 1024)  /* 64 MB coherent DMA buffer */

/* ─── Coherent DMA buffer (for GPU command rings, descriptor tables) ───── */
struct gpu_coherent_buf {
    void       *cpu_addr;    /* CPU virtual address */
    dma_addr_t  dma_addr;    /* IOVA programmed into GPU DMA engine */
    size_t      size;
};

int gpu_coherent_alloc(struct device *dev, struct gpu_coherent_buf *buf)
{
    buf->size     = DMA_BUF_SIZE;
    /*
     * dma_alloc_coherent:
     *  1. Allocates physically contiguous memory (from CMA pool or DMA zone)
     *  2. Maps it through the IOMMU — returns a DMA address (IOVA)
     *  3. Ensures CPU/device cache coherency (write-through or uncacheable)
     */
    buf->cpu_addr = dma_alloc_coherent(dev, buf->size,
                                        &buf->dma_addr, GFP_KERNEL);
    if (!buf->cpu_addr) {
        dev_err(dev, "Failed to allocate %zu MB coherent DMA buffer\n",
                buf->size >> 20);
        return -ENOMEM;
    }

    dev_info(dev, "Coherent DMA: cpu=%p dma=%pad size=%zuMB\n",
             buf->cpu_addr, &buf->dma_addr, buf->size >> 20);
    return 0;
}

void gpu_coherent_free(struct device *dev, struct gpu_coherent_buf *buf)
{
    dma_free_coherent(dev, buf->size, buf->cpu_addr, buf->dma_addr);
    memset(buf, 0, sizeof(*buf));
}

/* ─── Streaming DMA (for data transfer buffers — one-shot DMA ops) ────── */
struct gpu_stream_buf {
    void       *cpu_addr;
    size_t      size;
    dma_addr_t  dma_addr;
};

int gpu_stream_map(struct device *dev, struct gpu_stream_buf *buf)
{
    buf->cpu_addr = kmalloc(buf->size, GFP_KERNEL);
    if (!buf->cpu_addr)
        return -ENOMEM;

    /*
     * dma_map_single: map a kernel virtual address for DMA.
     * DMA_TO_DEVICE: CPU writes, device reads (writeback flush before DMA).
     * DMA_FROM_DEVICE: device writes, CPU reads (invalidate before CPU read).
     * DMA_BIDIRECTIONAL: both directions.
     */
    buf->dma_addr = dma_map_single(dev, buf->cpu_addr,
                                    buf->size, DMA_TO_DEVICE);
    if (dma_mapping_error(dev, buf->dma_addr)) {
        kfree(buf->cpu_addr);
        return -EIO;
    }
    return 0;
}

void gpu_stream_unmap(struct device *dev, struct gpu_stream_buf *buf)
{
    dma_unmap_single(dev, buf->dma_addr, buf->size, DMA_TO_DEVICE);
    kfree(buf->cpu_addr);
}

/* ─── Scatter-Gather DMA (for non-contiguous user buffers) ─────────────── */
struct gpu_sg_buf {
    struct sg_table sgt;
    struct page   **pages;
    unsigned int    num_pages;
};

int gpu_sg_map_user(struct device *dev, void __user *uaddr,
                     size_t size, struct gpu_sg_buf *sgbuf)
{
    int ret;
    unsigned int np = DIV_ROUND_UP(
        (unsigned long)uaddr % PAGE_SIZE + size, PAGE_SIZE);

    sgbuf->num_pages = np;
    sgbuf->pages     = kvmalloc_array(np, sizeof(*sgbuf->pages), GFP_KERNEL);
    if (!sgbuf->pages)
        return -ENOMEM;

    /* Pin user pages — prevent them from being swapped or moved */
    ret = pin_user_pages_fast((unsigned long)uaddr, np,
                               FOLL_WRITE, sgbuf->pages);
    if (ret < 0)
        goto err_pages;
    if ((unsigned int)ret < np) {
        ret = -EFAULT;
        goto err_unpin;
    }

    /* Build scatter-gather table from pinned pages */
    ret = sg_alloc_table_from_pages(&sgbuf->sgt, sgbuf->pages, np,
                                     (unsigned long)uaddr % PAGE_SIZE,
                                     size, GFP_KERNEL);
    if (ret)
        goto err_unpin;

    /* Map the SG table through IOMMU for DMA */
    ret = dma_map_sg(dev, sgbuf->sgt.sgl,
                      sgbuf->sgt.nents, DMA_TO_DEVICE);
    if (!ret) {
        ret = -EIO;
        goto err_sg;
    }

    return 0;

err_sg:    sg_free_table(&sgbuf->sgt);
err_unpin: unpin_user_pages(sgbuf->pages, np);
err_pages: kvfree(sgbuf->pages);
    return ret;
}

void gpu_sg_unmap(struct device *dev, struct gpu_sg_buf *sgbuf)
{
    dma_unmap_sg(dev, sgbuf->sgt.sgl, sgbuf->sgt.nents, DMA_TO_DEVICE);
    sg_free_table(&sgbuf->sgt);
    unpin_user_pages(sgbuf->pages, sgbuf->num_pages);
    kvfree(sgbuf->pages);
}
```

---

## Explanation

### Core Concept

**Three DMA allocation strategies:**

```
┌──────────────────────────────────────────────────────────────────┐
│  dma_alloc_coherent                                               │
│  • Physically contiguous, cache-coherent, permanent IOVA mapping │
│  • Use for: command rings, descriptor tables, firmware regions    │
│  • Source: CMA pool or GFP_DMA32 zone                            │
├──────────────────────────────────────────────────────────────────┤
│  dma_map_single / dma_map_sg (streaming)                          │
│  • Temporary IOVA mapping for a single DMA operation             │
│  • Call sync_for_device before DMA, sync_for_cpu after           │
│  • Use for: data transfer buffers, one-shot uploads/downloads     │
├──────────────────────────────────────────────────────────────────┤
│  pin_user_pages + dma_map_sg (zero-copy user DMA)                 │
│  • Pins userspace pages, builds scatter-gather, maps via IOMMU   │
│  • Use for: cuMemcpyHtoD from user buffer, RDMA-style transfers  │
└──────────────────────────────────────────────────────────────────┘
```

**CMA Setup** (via kernel command line or device tree):
```
cma=256M@0x40000000   # Reserve 256MB at address 0x40000000 for CMA
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `dma_alloc_coherent(dev, size, dma_addr, gfp)` | Allocate coherent DMA buffer |
| `dma_free_coherent(dev, size, cpu_addr, dma_addr)` | Free coherent DMA buffer |
| `dma_map_single(dev, ptr, size, dir)` | Map single buffer for streaming DMA |
| `dma_unmap_single(dev, addr, size, dir)` | Unmap streaming DMA buffer |
| `dma_mapping_error(dev, dma_addr)` | Check if DMA mapping failed |
| `dma_map_sg(dev, sgl, nents, dir)` | Map scatter-gather list |
| `dma_unmap_sg(dev, sgl, nents, dir)` | Unmap scatter-gather list |
| `sg_alloc_table_from_pages(sgt, pages, n, offset, size, gfp)` | Build SG table from page array |
| `pin_user_pages_fast(addr, n, flags, pages)` | Pin user pages (FOLL_WRITE for write access) |
| `unpin_user_pages(pages, n)` | Release pinned user pages |
| `dma_sync_single_for_device(dev, addr, size, dir)` | Flush CPU caches before device read |
| `dma_sync_single_for_cpu(dev, addr, size, dir)` | Invalidate caches before CPU read |

### Trade-offs & Pitfalls

- **Forgetting `dma_sync_*` for streaming DMA.** On non-coherent architectures (ARM), the CPU caches dirty data that the device hasn't seen. `dma_sync_single_for_device` flushes the CPU cache. After DMA completes, `dma_sync_single_for_cpu` invalidates the cache so the CPU reads fresh device data.
- **`dma_map_sg` return value.** `dma_map_sg` returns the number of mapped SG entries — may be less than `nents` if the IOMMU merges adjacent entries. Always use the returned count, not the original `nents`, when programming DMA hardware.
- **`pin_user_pages` vs `get_user_pages`.** `pin_user_pages` is the correct API for DMA — it prevents page migration and sets the `PG_pin` flag that CMA respects. `get_user_pages` allows migration, which can silently corrupt DMA operations.

### NVIDIA / GPU Context

- **CUDA `cuMemcpyHtoD`:** internally calls `pin_user_pages` + `dma_map_sg` to DMA user buffer to GPU without CPU copy (zero-copy)
- **GPU Command Ring:** `dma_alloc_coherent` for the ring buffer — CPU writes commands, GPU reads them; coherency guaranteed without explicit sync
- **NVLink buffers:** Use `dma_alloc_coherent` with CMA to guarantee physically contiguous large buffers for NVLink DMA engines which lack scatter-gather support

---

## Cross Questions & Answers

**CQ1: What is the difference between coherent DMA and streaming DMA?**
> **Coherent DMA:** The buffer is always coherent between CPU and device — no explicit cache management needed. Typically uncacheable or write-through. Slower for CPU access but simpler. Used for small, frequently-accessed control structures. **Streaming DMA:** A normal cached buffer that is temporarily mapped for one DMA operation. The driver must call `dma_sync_*` to maintain coherency. Faster CPU access between DMA ops. Used for large data transfer buffers.

**CQ2: What is an IOVA (I/O Virtual Address) and how does the IOMMU generate it?**
> An IOVA is the address the DMA device uses — a virtual address in the device's address space, translated by the IOMMU to a physical address. The kernel's IOMMU DMA layer maintains an IOVA allocator (red-black tree of free IOVA ranges). `dma_map_single` allocates an IOVA, programs the IOMMU page table to map `IOVA → physical_page`, and returns the IOVA to the driver. The driver programs this IOVA into the DMA engine. This gives per-device address isolation — one device cannot DMA to another's memory.

**CQ3: How does `dma_set_mask_and_coherent(dev, DMA_BIT_MASK(48))` affect DMA allocation?**
> It tells the DMA subsystem that the device can address 48-bit DMA addresses (256 TB). Without this call, the kernel assumes the device can only handle 32-bit DMA (4 GB limit) and allocates from `GFP_DMA32` zone. With 48-bit mask, the full physical memory range is available for DMA buffers. NVIDIA A100/H100 GPUs use 48-bit addressing for their PCIe DMA engines, so this call is mandatory in their probe function.

**CQ4: What is the danger of calling `kfree` on a `dma_alloc_coherent` buffer?**
> `dma_alloc_coherent` returns a virtual address but does NOT allocate through `kmalloc`/slab. Calling `kfree` on it is undefined behavior — it will attempt to free an untracked pointer, corrupting the slab allocator and likely causing a kernel oops or silent data corruption. Always use the matching `dma_free_coherent` with the same `device`, `size`, `cpu_addr`, and `dma_addr` that were returned by `dma_alloc_coherent`.

**CQ5: What is the `SWIOTLB` and when does it activate for GPU DMA?**
> `SWIOTLB` (Software I/O TLB) is a kernel bounce buffer mechanism activated when a DMA device cannot address the physical memory containing a buffer (e.g., device has 32-bit DMA mask but the buffer is above 4GB). The kernel copies data to/from a low-memory "bounce buffer" for the DMA operation. For NVIDIA GPUs with 48-bit DMA capability, `SWIOTLB` should never activate. If it does (shown by `swiotlb: coherent allocation failed` messages), check that `dma_set_mask_and_coherent` was called correctly.

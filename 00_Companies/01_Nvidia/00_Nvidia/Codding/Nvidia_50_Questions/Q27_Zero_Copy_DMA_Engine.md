# Q27: Zero-Copy DMA Engine for GPU Data Transfer

**Section:** System Design | **Difficulty:** Hard | **Topics:** `pin_user_pages_fast`, `sg_alloc_table_from_pages`, `dma_map_sg`, zero-copy, PCIe DMA, GPU P2P transfer

---

## Question

Implement a zero-copy DMA engine for transferring user data directly to a GPU without CPU copy.

---

## Answer

```c
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/* ─── Transfer descriptor ─────────────────────────────────────────────────*/
struct zero_copy_transfer {
    struct page       **pages;       /* pinned user pages                    */
    unsigned long       num_pages;   /* number of pinned pages               */
    struct sg_table     sgt;         /* scatterlist table after DMA map       */
    dma_addr_t          gpu_dma_va;  /* GPU DMA VA (IOVA) of the mapped mem  */
    u64                 gpu_dest_va; /* GPU virtual address for CUDA access  */
    size_t              size;        /* total transfer size in bytes          */
    struct gpu_fence   *completion;  /* fence to signal on transfer done      */
};

/* ─── Step 1: Pin user pages (prevent them from being swapped) ────────────*/
static int zcopy_pin_pages(struct zero_copy_transfer *t,
                             unsigned long user_addr, size_t size)
{
    long nr_pinned;

    t->num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
    t->pages = kvmalloc_array(t->num_pages, sizeof(*t->pages), GFP_KERNEL);
    if (!t->pages)
        return -ENOMEM;

    /*
     * pin_user_pages_fast: pin user pages for DMA.
     * FOLL_WRITE: request write access (GPU will write to these pages).
     * Returns number of pages pinned (may be < num_pages on fault).
     * Unlike get_user_pages, pin_user_pages sets page PinCount — 
     * prevents page from being migrated or swapped during DMA.
     */
    nr_pinned = pin_user_pages_fast(user_addr,
                                     t->num_pages,
                                     FOLL_WRITE,
                                     t->pages);
    if (nr_pinned < 0) {
        kvfree(t->pages);
        return nr_pinned;
    }
    if (nr_pinned < (long)t->num_pages) {
        /* Partial pin — unpin what we got and fail */
        unpin_user_pages(t->pages, nr_pinned);
        kvfree(t->pages);
        return -EFAULT;
    }

    t->size = size;
    return 0;
}

/* ─── Step 2: Build scatter-gather table from pinned pages ────────────────*/
static int zcopy_build_sgt(struct zero_copy_transfer *t,
                             unsigned long user_addr, size_t size)
{
    int ret;
    unsigned int offset = offset_in_page(user_addr);

    /*
     * sg_alloc_table_from_pages: creates a scatterlist from page array.
     * Merges contiguous pages into single SG entries (reduces DMA descriptors).
     * offset: byte offset within first page.
     * size: total transfer size.
     * chunk_size: max SG entry size (0 = no limit).
     */
    ret = sg_alloc_table_from_pages(&t->sgt,
                                     t->pages,
                                     t->num_pages,
                                     offset,    /* offset in first page */
                                     size,
                                     GFP_KERNEL);
    if (ret) {
        pr_err("zcopy: sg_alloc_table_from_pages failed: %d\n", ret);
        return ret;
    }

    return 0;
}

/* ─── Step 3: Map SG for GPU DMA (IOMMU assigns IOVA range) ──────────────*/
static int zcopy_dma_map(struct zero_copy_transfer *t, struct device *gpu_dev)
{
    int nents;

    /*
     * dma_map_sg: programs IOMMU to map physical pages in SG list
     * to a contiguous (or fragmented) IOVA range visible to the GPU.
     * Returns number of DMA entries after IOMMU merging.
     * DMA_TO_DEVICE: CPU writes, GPU reads (CPU→GPU direction).
     * DMA_FROM_DEVICE: GPU writes, CPU reads (GPU→CPU direction).
     */
    nents = dma_map_sg(gpu_dev, t->sgt.sgl, t->sgt.nents, DMA_TO_DEVICE);
    if (!nents) {
        pr_err("zcopy: dma_map_sg failed\n");
        return -ENOMEM;
    }

    /* First DMA address = IOVA start for GPU DMA engine */
    t->gpu_dma_va = sg_dma_address(t->sgt.sgl);

    pr_debug("zcopy: %zu bytes mapped at IOVA 0x%llx (%d SG entries)\n",
             t->size, (u64)t->gpu_dma_va, nents);
    return 0;
}

/* ─── Step 4: Program GPU DMA engine ─────────────────────────────────────*/
static void zcopy_program_gpu_dma(struct zero_copy_transfer *t,
                                   struct gpu_device *gpu,
                                   u64 gpu_dest_va)
{
    struct scatterlist *sg;
    int i;
    u64 src_iova, dest_va = gpu_dest_va;

    /*
     * Program GPU's copy engine with a chain of DMA descriptors:
     * one descriptor per SG entry → src (IOVA) to dst (GPU VA).
     */
    for_each_sg(t->sgt.sgl, sg, t->sgt.nents, i) {
        src_iova = sg_dma_address(sg);
        u32 len  = sg_dma_len(sg);

        /* Write DMA descriptor to GPU MMIO copy engine */
        gpu_ce_submit_copy(gpu,
                           src_iova,  /* source: host IOVA (DRAM)  */
                           dest_va,   /* dest:   GPU VRAM VA       */
                           len);
        dest_va += len;
    }

    /* Doorbell: kick GPU DMA engine */
    gpu_ce_ring_doorbell(gpu);
}

/* ─── Step 5: Cleanup ─────────────────────────────────────────────────────*/
void zcopy_cleanup(struct zero_copy_transfer *t, struct device *gpu_dev)
{
    /* Unmap from IOMMU (must be done before unpin) */
    dma_unmap_sg(gpu_dev, t->sgt.sgl, t->sgt.nents, DMA_TO_DEVICE);
    sg_free_table(&t->sgt);

    /* Unpin user pages (allow migration/swap again) */
    unpin_user_pages(t->pages, t->num_pages);
    kvfree(t->pages);
}

/* ─── Full transfer API ───────────────────────────────────────────────────*/
int gpu_zero_copy_transfer(struct gpu_device *gpu,
                             unsigned long user_src,
                             u64 gpu_dest_va,
                             size_t size)
{
    struct zero_copy_transfer *t;
    int ret;

    t = kzalloc(sizeof(*t), GFP_KERNEL);
    if (!t)
        return -ENOMEM;

    ret = zcopy_pin_pages(t, user_src, size);
    if (ret) goto free_t;

    ret = zcopy_build_sgt(t, user_src, size);
    if (ret) goto unpin;

    ret = zcopy_dma_map(t, &gpu->pdev->dev);
    if (ret) goto free_sgt;

    zcopy_program_gpu_dma(t, gpu, gpu_dest_va);
    /* GPU DMA runs asynchronously; caller waits on fence */
    /* t freed in DMA completion interrupt: zcopy_cleanup + kfree(t) */
    return 0;

free_sgt:
    sg_free_table(&t->sgt);
unpin:
    unpin_user_pages(t->pages, t->num_pages);
    kvfree(t->pages);
free_t:
    kfree(t);
    return ret;
}
```

### Transfer Architecture

```
User Buffer (DRAM)        CPU Address Space           GPU VRAM
 page0 [0x1000_0000]       user_addr = 0x7fff0000
 page1 [0x1000_1000]
 page2 [0x2000_0000]   ─── pin_user_pages_fast ──► page list
    │                  ─── sg_alloc_table ────────► scatter-gather table
    │                  ─── dma_map_sg ────────────► IOVA: 0x9000_0000
    │                                                  │
    │    PCIe Bus: src=IOVA → GPU DMA engine ──────────► dest=GPU VA
    │                                                  │
    └──────────── NO CPU COPY ─────────────────────────►
```

---

## Explanation

### Core Concept

Zero-copy eliminates the `memcpy(kernel_buf, user_buf)` step by pinning user pages and mapping them directly for GPU DMA. The data flows: user DRAM → PCIe bus → GPU VRAM, without any CPU touching the data.

**Performance comparison:**
- Traditional: `cudaMemcpy(gpu, data, size)` = user→kernel copy + kernel→GPU DMA = 2× memory bandwidth
- Zero-copy: pin + map + GPU DMA = 1× memory bandwidth (50% better bandwidth utilization)

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `pin_user_pages_fast(addr, npages, flags, pages)` | Pin user pages for DMA (RDMA-safe) |
| `unpin_user_pages(pages, npages)` | Release pinned pages |
| `sg_alloc_table_from_pages(sgt, pages, npages, off, size, gfp)` | Build SG from page array |
| `sg_free_table(sgt)` | Free SG table |
| `dma_map_sg(dev, sgl, nents, dir)` | Map SG through IOMMU → get IOVA |
| `dma_unmap_sg(dev, sgl, nents, dir)` | Unmap from IOMMU |
| `sg_dma_address(sg)` | Get IOVA of SG entry |
| `sg_dma_len(sg)` | Get length of SG entry after DMA mapping |
| `for_each_sg(sgl, sg, nents, i)` | Iterate SG entries |
| `kvmalloc_array(n, size, gfp)` | Allocate large arrays (vmalloc fallback) |

### Trade-offs & Pitfalls

- **`pin_user_pages` vs `get_user_pages`.** `pin_user_pages` sets a stable "pin" count that prevents page migration and signals to the kernel that DMA is in progress. `get_user_pages` only increments normal refcount — pages can still be migrated (e.g., by NUMA balancing), causing the physical address to change mid-DMA. Always use `pin_user_pages` for DMA.
- **Must `dma_unmap_sg` before `unpin_user_pages`.** Unmapping triggers IOMMU TLB invalidation and cache flushes — must complete before the physical pages are released.
- **SWIOTLB bounce buffers.** On systems where the GPU's DMA mask doesn't cover the user pages' physical addresses, `dma_map_sg` silently bounces through a kernel buffer — negating zero-copy. Ensure `dma_set_mask_and_coherent(dev, DMA_BIT_MASK(48))` is called.

### NVIDIA / GPU Context

NVIDIA GPUDirect RDMA uses this exact pattern:
- NIC DMA engine maps user pages via `pin_user_pages_fast` + `dma_map_sg`
- GPU DMA engine (copy engine) reads from the same IOVA range
- Data flows NIC → DRAM → GPU, with the NIC and GPU accessing the same IOMMU-mapped pages
- GPUDirect Storage extends this for NVMe directly to GPU VRAM

---

## Cross Questions & Answers

**CQ1: What is GPUDirect RDMA and how does it differ from standard zero-copy?**
> GPUDirect RDMA allows network cards to DMA directly to GPU VRAM, bypassing system DRAM entirely. Standard zero-copy (above) moves data: CPU DRAM → PCIe → GPU VRAM. GPUDirect RDMA: NIC PCIe → GPU PCIe (peer-to-peer) → GPU VRAM — no DRAM touch. The NIC gets the GPU VRAM's physical address (via `pci_p2pdma_virt_to_bus`), programs its DMA engine with it, and data flows over PCIe without hitting system memory. Bandwidth bottleneck becomes PCIe link bandwidth, not DRAM.

**CQ2: What is a scatter-gather list and why is it important for DMA?**
> User memory is physically non-contiguous — pages 0, 1, 2 of a user buffer might be at physical addresses 0x1000, 0x5000, 0x2000 (any order). A single DMA transfer requires contiguous physical addresses (or IOVA if IOMMU is used). A scatter-gather list describes multiple physically non-contiguous memory segments. The GPU DMA engine processes SG entries sequentially, copying each segment to the next GPU VRAM offset. `sg_alloc_table_from_pages` automatically merges adjacent physical pages into larger segments for efficiency.

**CQ3: What is the SWIOTLB and when does it activate for GPU DMA?**
> SWIOTLB (Software IO Translation Lookaside Buffer) is a kernel bounce buffer used when: (1) a DMA device cannot address the physical location of data (DMA mask mismatch), or (2) IOMMU is disabled. When `dma_map_sg` finds a page outside the device's DMA mask, it copies the data to a SWIOTLB buffer in the DMA-addressable zone and returns the SWIOTLB's physical address. This defeats zero-copy. Fix: set `dma_set_mask_and_coherent(dev, DMA_BIT_MASK(48))` and ensure the kernel is booted with `iommu=on`.

**CQ4: How does `sg_alloc_table_from_pages` merge pages and why is merging important?**
> Physically contiguous pages are merged into single SG entries: if pages 0-3 are at consecutive physical addresses, they form one SG entry of 4 pages (16KB). Merging reduces: (1) the number of IOMMU page table entries (less IOMMU TLB pressure), (2) the number of DMA descriptors programmed to the GPU copy engine (less command overhead). Without merging, a 16MB buffer with 4096 non-contiguous pages would require 4096 DMA descriptors — with merging, perhaps 100-200, dramatically reducing copy engine setup overhead.

**CQ5: What is the `FOLL_WRITE` flag in `pin_user_pages_fast` and why is it needed?**
> `FOLL_WRITE` signals that the caller intends to write to the pages (the GPU DMA writes data to them). This triggers a Copy-on-Write (COW) break if the page is shared: if the user buffer is a COW page (e.g., after a `fork()`), `FOLL_WRITE` forces allocation of a private copy before pinning. Without `FOLL_WRITE`, the GPU would DMA into a shared COW page, potentially corrupting the parent process's memory. For GPU-to-CPU DMA (reading GPU data to user buffer), also use `FOLL_WRITE` since the DMA engine writes to the user pages.

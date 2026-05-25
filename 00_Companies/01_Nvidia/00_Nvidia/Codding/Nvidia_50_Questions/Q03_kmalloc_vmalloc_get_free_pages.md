# Q03: kmalloc vs vmalloc vs get_free_pages

**Section:** Linux Kernel Internals | **Difficulty:** Medium | **Topics:** memory allocation, physical contiguity, DMA, GFP flags, page allocator

---

## Question

What is the difference between `kmalloc`, `vmalloc`, and `get_free_pages`? When do you use each?

---

## Answer

```c
/* ─── kmalloc ─────────────────────────────────────────────────────────────
 * Physically contiguous. Backed by slab allocator. Max ~4MB on most configs.
 * Safe for DMA (address is contiguous in both virtual and physical memory).
 */
void *buf = kmalloc(256, GFP_KERNEL);
if (!buf)
    return -ENOMEM;
kfree(buf);

/* ─── vmalloc ─────────────────────────────────────────────────────────────
 * Virtually contiguous only — pages may be scattered in physical memory.
 * Can allocate large regions (hundreds of MB). NOT safe for DMA.
 * Overhead: sets up kernel page table entries for each page.
 */
void *vbuf = vmalloc(10 * 1024 * 1024); /* 10 MB */
if (!vbuf)
    return -ENOMEM;
vfree(vbuf);

/* ─── __get_free_pages ────────────────────────────────────────────────────
 * Raw page allocator. order = log2(number of pages).
 * Physically contiguous. Returns a kernel virtual address.
 * order 0 = 4KB, order 1 = 8KB, order 2 = 16KB, ...
 */
unsigned long page = __get_free_pages(GFP_KERNEL, 2); /* 4 pages = 16KB */
if (!page)
    return -ENOMEM;
free_pages(page, 2);

/* ─── dma_alloc_coherent ──────────────────────────────────────────────────
 * For GPU/PCIe DMA: physically contiguous, coherent (CPU+device see same data).
 * Maps through IOMMU. Returns both CPU virtual address and DMA address.
 */
dma_addr_t dma_handle;
void *coherent = dma_alloc_coherent(dev, 4096, &dma_handle, GFP_KERNEL);
dma_free_coherent(dev, 4096, coherent, dma_handle);
```

### Comparison Table

| API | Physically Contiguous | Max Size | DMA Safe | IOMMU Aware | Overhead |
|-----|-----------------------|----------|----------|-------------|----------|
| `kmalloc` | **Yes** | ~4 MB | Yes (with care) | No | Low |
| `vmalloc` | No | ~GB | **No** | No | High (page tables) |
| `__get_free_pages` | **Yes** | ~4 MB | Yes (with care) | No | Medium |
| `dma_alloc_coherent` | **Yes** | Platform limit | **Yes** | **Yes** | High |

---

## Explanation

### Core Concept

The Linux kernel has a layered memory allocator:

```
dma_alloc_coherent         ← DMA-safe, device-aware
       │
kmalloc / kzalloc          ← General-purpose, small (<4MB), slab-backed
       │
__get_free_pages           ← Raw page allocator (buddy system)
       │
vmalloc                    ← Large, virtually-contiguous only
```

**Physical contiguity** matters because hardware DMA engines use physical addresses. A DMA engine given a "virtual address" would read garbage — it needs a single contiguous physical range (or a scatter-gather list).

### Key APIs / Macros Used

| API | Notes |
|-----|-------|
| `kmalloc(size, gfp)` | Returns physically contiguous region up to 4MB |
| `kzalloc(size, gfp)` | Same as `kmalloc` but zero-initializes |
| `kfree(ptr)` | Free a `kmalloc`/`kzalloc` allocation |
| `vmalloc(size)` | Large virtual-contiguous allocation |
| `vfree(ptr)` | Free a `vmalloc` allocation |
| `__get_free_pages(gfp, order)` | Allocate `2^order` contiguous pages |
| `free_pages(addr, order)` | Free pages from `__get_free_pages` |
| `dma_alloc_coherent(dev, size, dma_addr, gfp)` | DMA + IOMMU aware allocation |

### Trade-offs & Pitfalls

- **`vmalloc` for DMA:** A common mistake. `vmalloc` pages are physically scattered. Passing a `vmalloc` address to a DMA engine results in data corruption or a machine check.
- **`kmalloc` for large allocations:** `kmalloc` can fail for large sizes (> 4MB) due to the buddy allocator requiring contiguous physical pages. Use `vmalloc` for large non-DMA allocations, or CMA for large DMA allocations.
- **Fragmentation:** Over time, `__get_free_pages` with high order (e.g., order 9 = 2MB) can fail due to memory fragmentation. This is why NVIDIA uses CMA (Contiguous Memory Allocator) reservations at boot time for large GPU DMA buffers.

### NVIDIA / GPU Context

- **Command buffers, descriptor rings:** `dma_alloc_coherent` — GPU must DMA-read them directly
- **Kernel module data structures:** `kmalloc` — small, frequent allocations like context descriptors, fence objects
- **Large GPU firmware images:** `vmalloc` — loaded once, CPU-only access for patching
- **GPU framebuffer mappings:** `ioremap` (separate from these allocators — maps physical GPU VRAM)

---

## Cross Questions & Answers

**CQ1: Can you use `kmalloc` memory as a DMA buffer without `dma_alloc_coherent`?**
> Yes, but you must explicitly use `dma_map_single(dev, ptr, size, direction)` to get a `dma_addr_t` that is safe to program into the hardware. This handles IOMMU mapping and cache coherency on non-coherent architectures. On x86 with a coherent IOMMU, `kmalloc` address ≈ physical address, but this is an architecture assumption you should never hard-code.

**CQ2: What is the GFP_KERNEL vs GFP_ATOMIC distinction and why does it matter for DMA allocation?**
> `GFP_KERNEL` allows the allocator to sleep (block waiting for memory reclaim, compaction). `GFP_ATOMIC` never sleeps — it fails immediately if memory is not available. DMA allocations from interrupt context must use `GFP_ATOMIC`. Failing to do so causes a `might_sleep()` BUG assertion in the kernel.

**CQ3: What is CMA (Contiguous Memory Allocator) and when does NVIDIA use it?**
> CMA reserves a region of physically contiguous memory at boot time (configured via `cma=<size>` kernel parameter or device tree). At runtime, `dma_alloc_coherent` can draw from this pool for large contiguous allocations that would otherwise fail due to fragmentation. NVIDIA GPU drivers use CMA for large persistent DMA buffers (e.g., 64–256 MB) used for GPU command rings and scatter-gather staging areas.

**CQ4: How does `vmalloc` set up virtual-to-physical mappings?**
> `vmalloc` allocates individual pages from the buddy allocator (they can be anywhere in physical memory), then creates PTEs in the kernel's `vmalloc` address space (`VMALLOC_START` to `VMALLOC_END`) mapping those pages contiguously. This costs one TLB entry per page (not one for the whole range), which is why `vmalloc` can increase TLB pressure and cause more frequent TLB misses compared to `kmalloc`.

**CQ5: Why does NVIDIA's GPU driver use `dma_alloc_coherent` instead of `__get_free_pages` + `dma_map_pages`?**
> `dma_alloc_coherent` ensures the allocation is coherent (CPU and GPU see the same data without explicit cache flushes) and properly registered with the IOMMU. `__get_free_pages` + `dma_map_pages` gives you DMA mappings but not cache coherency on non-coherent architectures (e.g., ARM-based SoCs with NVIDIA Tegra). Coherent memory is essential for command buffers that are written by CPU and read by GPU without explicit sync points.

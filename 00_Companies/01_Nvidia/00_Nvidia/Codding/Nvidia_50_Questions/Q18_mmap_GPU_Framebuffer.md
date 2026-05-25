# Q18: Custom mmap Handler for GPU Framebuffer Exposure

**Section:** Memory Management | **Difficulty:** Hard | **Topics:** `mmap`, `vm_area_struct`, `remap_pfn_range`, `pgprot_writecombine`, `VM_IO`, `VM_PFNMAP`, GPU framebuffer

---

## Question

Implement a custom mmap handler for exposing GPU framebuffer to userspace.

---

## Answer

```c
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#define GPU_FRAMEBUF_PA   0xE0000000ULL  /* GPU framebuffer physical base */
#define GPU_FRAMEBUF_SIZE (256 * 1024 * 1024) /* 256 MB */

/* ─── GPU VMA operations ──────────────────────────────────────────────────
 * These callbacks are invoked by the kernel on VMA lifecycle events.
 */
static void gpu_vma_open(struct vm_area_struct *vma)
{
    pr_debug("GPU mmap: vma open [0x%lx – 0x%lx]\n",
             vma->vm_start, vma->vm_end);
    /* Increment reference count if tracking VMA opens */
}

static void gpu_vma_close(struct vm_area_struct *vma)
{
    pr_debug("GPU mmap: vma close [0x%lx – 0x%lx]\n",
             vma->vm_start, vma->vm_end);
    /* Decrement reference count, flush GPU caches if needed */
}

static const struct vm_operations_struct gpu_vm_ops = {
    .open  = gpu_vma_open,
    .close = gpu_vma_close,
};

/* ─── Main mmap handler ───────────────────────────────────────────────────
 * Called when userspace does: mmap(NULL, size, PROT_READ|PROT_WRITE,
 *                                   MAP_SHARED, gpu_fd, offset)
 */
static int gpu_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size     = vma->vm_end - vma->vm_start;
    unsigned long pfn_base = GPU_FRAMEBUF_PA >> PAGE_SHIFT;
    unsigned long pfn_off  = vma->vm_pgoff; /* page offset from mmap(offset) */

    /* ── Validate request ─────────────────────────────────────────────── */
    if (size > GPU_FRAMEBUF_SIZE) {
        pr_err("GPU mmap: requested size %lu > framebuffer size\n", size);
        return -EINVAL;
    }
    if (pfn_off >= (GPU_FRAMEBUF_SIZE >> PAGE_SHIFT)) {
        pr_err("GPU mmap: page offset 0x%lx out of bounds\n", pfn_off);
        return -EINVAL;
    }
    if ((pfn_off << PAGE_SHIFT) + size > GPU_FRAMEBUF_SIZE)
        return -EINVAL;

    /* ── Set memory type flags ────────────────────────────────────────── */

    /*
     * pgprot_writecombine: write-combining memory type.
     * Coalesces CPU writes into larger PCIe transactions for best
     * framebuffer write bandwidth. Required for GPU VRAM mappings.
     *
     * Alternative for strict register access: pgprot_noncached (UC)
     * Alternative for normal RAM: leave as pgprot_kernel (WB)
     */
    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

    /*
     * VM_IO:       This VMA maps I/O memory (not normal RAM).
     *              Prevents this region from being included in core dumps.
     * VM_PFNMAP:   Mapping is backed by raw PFNs, not struct page.
     *              Required for remap_pfn_range on MMIO regions.
     * VM_DONTEXPAND: Prevent mremap() from expanding this VMA.
     * VM_DONTDUMP:  Exclude from coredump (privacy for GPU memory).
     */
    vm_flags_set(vma, VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP);

    /* ── Install VMA operations ───────────────────────────────────────── */
    vma->vm_ops = &gpu_vm_ops;

    /* ── Map physical GPU framebuffer into userspace VMA ─────────────── */
    /*
     * remap_pfn_range: directly install PTEs mapping physical pages
     * into the user VMA without going through struct page.
     * pfn_base + pfn_off: physical page frame number of the mapping start.
     */
    if (remap_pfn_range(vma,
                         vma->vm_start,         /* user virtual address */
                         pfn_base + pfn_off,     /* physical PFN */
                         size,
                         vma->vm_page_prot)) {
        return -EAGAIN;
    }

    return 0;
}

/* ─── mmap for GPU system memory (DMA-allocated, struct page backed) ──────*/
static int gpu_sysmem_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct gpu_coherent_buf *buf = file->private_data;
    unsigned long size = vma->vm_end - vma->vm_start;

    if (size > buf->size)
        return -EINVAL;

    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
    vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);

    /*
     * dma_mmap_coherent: maps a dma_alloc_coherent buffer into userspace.
     * Handles IOMMU-aware VA mappings and correct page protection.
     * Preferred over remap_pfn_range for coherent DMA memory.
     */
    return dma_mmap_coherent(buf->dev, vma,
                              buf->cpu_addr, buf->dma_addr, size);
}

/* ─── Fault handler for demand-paged GPU buffers ─────────────────────────*/
static vm_fault_t gpu_vm_fault(struct vm_fault *vmf)
{
    struct page *page;

    /*
     * For GPU buffers backed by real struct pages (not MMIO),
     * use vm_fault to install pages on-demand (demand paging).
     */
    page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM);
    if (!page)
        return VM_FAULT_OOM;

    /* Zero the page before giving it to userspace (security) */
    clear_page(page_address(page));
    get_page(page);
    vmf->page = page;
    return 0;
}

static const struct vm_operations_struct gpu_demand_vm_ops = {
    .fault = gpu_vm_fault,
};

/* ─── file_operations ─────────────────────────────────────────────────────*/
static const struct file_operations gpu_fops = {
    .owner = THIS_MODULE,
    .mmap  = gpu_mmap,
};
```

---

## Explanation

### Core Concept

`mmap` maps hardware memory (GPU VRAM) or kernel-allocated buffers directly into userspace virtual address space. This enables **zero-copy access** — the CUDA runtime can read/write GPU memory directly from user processes without any kernel copy.

**Two types of mmap in GPU drivers:**

```
1. MMIO mmap (remap_pfn_range):
   GPU VRAM (physical) ──────── remap_pfn_range ────── user VA
   No struct page, raw PFN mapping, write-combining cache type

2. DMA coherent mmap (dma_mmap_coherent):
   dma_alloc_coherent buffer ── dma_mmap_coherent ─── user VA
   struct page backed, coherent cache, IOMMU-aware
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `remap_pfn_range(vma, vaddr, pfn, size, prot)` | Map physical PFN range into user VMA |
| `dma_mmap_coherent(dev, vma, cpu, dma, size)` | Map coherent DMA buffer into user VMA |
| `pgprot_writecombine(prot)` | Set write-combining cache policy |
| `pgprot_noncached(prot)` | Set uncacheable (UC) policy (for registers) |
| `vm_flags_set(vma, flags)` | Add VMA flags (replaces `vma->vm_flags |=`) |
| `VM_IO` | Marks VMA as I/O memory |
| `VM_PFNMAP` | VMA backed by raw PFNs (no struct page) |
| `VM_DONTEXPAND` | Prevent `mremap` expansion |
| `VM_DONTDUMP` | Exclude from coredump |
| `struct vm_operations_struct` | Callbacks for open/close/fault on VMA |
| `vmf->page` | Set in fault handler to install a struct page |
| `get_page(page)` | Increment page refcount (required before vmf->page assignment) |

### Trade-offs & Pitfalls

- **`remap_pfn_range` for MMIO.** Physical framebuffer addresses don't have `struct page` backing — `vm_insert_page` won't work. Use `remap_pfn_range` with `VM_PFNMAP`.
- **Security: validate offset and size.** Without bounds checking, a malicious user can `mmap` with a large `offset`, wrapping around and mapping kernel memory. Always check `(pfn_off << PAGE_SHIFT) + size <= GPU_FRAMEBUF_SIZE`.
- **No `mremap` without care.** `VM_DONTEXPAND` prevents `mremap` from extending the VMA. Allow `mremap` only if your `vm_operations.mremap` callback validates the new range.
- **Write-combining vs uncached:** Use write-combining (`pgprot_writecombine`) for framebuffers and large streaming writes — multiple small writes are merged into one PCIe transaction. Use uncached (`pgprot_noncached`) for control registers where intermediate write values must be immediately observable.

### NVIDIA / GPU Context

NVIDIA GPU driver `mmap` paths:
- **`/dev/nvidia0` mmap:** Maps GPU VRAM via `remap_pfn_range` with `VM_IO | VM_PFNMAP | pgprot_writecombine` — CUDA runtime uses this for `cuMemcpy` staging
- **Userspace command buffer mmap:** Maps `dma_alloc_coherent` command ring buffer so CUDA can write GPU commands directly without a kernel copy
- **Doorbell mmap:** Maps GPU doorbell register page (`pgprot_noncached`) so `cuStreamSynchronize` can ring the GPU doorbell from userspace without an ioctl syscall — ultra-low latency submission

---

## Cross Questions & Answers

**CQ1: What is the difference between `VM_IO` and `VM_PFNMAP`? Can you have one without the other?**
> `VM_IO` marks the VMA as device I/O memory — prevents it from being included in core dumps, tells the kernel not to try to swap or migrate the pages. `VM_PFNMAP` signals that the VMA is backed by raw PFN mappings (no `struct page`) — prevents the kernel from doing page refcount operations on the mapped pages. For `remap_pfn_range` on MMIO, both flags are typically set together. You can have `VM_IO` without `VM_PFNMAP` (for device memory that does have struct pages), but `VM_PFNMAP` without `VM_IO` is unusual.

**CQ2: What is the `nopage`/`fault` vm_operation and how does it differ from `remap_pfn_range`?**
> `remap_pfn_range` pre-populates all PTEs at `mmap` time — no page faults after mapping. `vm_operations.fault` is a demand-paging approach: no PTEs are installed at `mmap` time; when the user accesses any page, a fault fires, the driver allocates and returns the page. Demand paging saves memory when not all pages are accessed. CUDA uses pre-population (`remap_pfn_range`) for framebuffers (all accessed) and demand paging (`fault`) for sparse GPU address spaces.

**CQ3: How does `dma_mmap_coherent` differ from `remap_pfn_range` for DMA buffers?**
> `dma_mmap_coherent` is IOMMU-aware: it correctly maps the CPU virtual addresses of `dma_alloc_coherent` buffers into the VMA, respecting the platform's memory type requirements. On ARM with non-coherent IOMMU, it sets up special CPU cache attributes. `remap_pfn_range` with a raw physical address bypasses IOMMU awareness — safe on x86 with coherent IOMMU, but incorrect on ARM. Always use `dma_mmap_coherent` for coherent DMA buffers.

**CQ4: How do you safely unmap GPU VRAM when the process exits before calling munmap?**
> When the process exits, the kernel calls `do_exit` → `mmput` → destroys all VMAs → calls `vm_ops->close` for each VMA. The `gpu_vma_close` callback receives the VMA being destroyed and can perform cleanup (decrement a reference count, flush GPU caches). The driver must not free the GPU VRAM in `vma_close` if other VMAs or kernel references still exist — use reference counting.

**CQ5: What is `mmap_sem`/`mmap_lock` and when must a GPU driver acquire it?**
> `mmap_lock` (`mm_struct.mmap_lock`) is an `rw_semaphore` protecting the process's VMA tree. The kernel acquires it in `read` mode for most VMA lookups and in `write` mode for VMA modifications. A GPU driver must acquire `mmap_lock` (read mode via `mmap_read_lock(mm)`) before calling `find_vma`, `pin_user_pages`, or any function that walks the VMA tree. Failing to hold `mmap_lock` during VMA tree access is a race condition — another thread could simultaneously modify the VMA tree via `munmap` or `brk`.

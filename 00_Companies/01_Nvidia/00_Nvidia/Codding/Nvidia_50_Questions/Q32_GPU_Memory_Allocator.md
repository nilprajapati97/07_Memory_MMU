# Q32: GPU Memory Allocator with VRAM and System Memory Pools

**Section:** System Design | **Difficulty:** Hard | **Topics:** `gen_pool`, `vmalloc`, GPU memory descriptor, VRAM pool, system memory pool, unified memory

---

## Question

Implement a GPU memory allocator supporting VRAM, system memory, and unified memory allocations with a `gen_pool`-based VRAM pool.

---

## Answer

```c
#include <linux/genalloc.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/kref.h>

/* ─── Memory location ─────────────────────────────────────────────────────*/
enum mem_location {
    MEM_VIDMEM  = 0,  /* GPU VRAM: fast, GPU-local                     */
    MEM_SYSMEM  = 1,  /* System RAM: accessible by CPU and GPU via DMA */
    MEM_UNIFIED = 2,  /* Demand-paged, accessible from both CPU and GPU */
};

/* ─── GPU Memory Descriptor ───────────────────────────────────────────────
 * Tracks a single GPU memory allocation.
 */
struct gpu_mem_descriptor {
    u64                 gpu_va;      /* GPU virtual address (GMMU mapped)  */
    void               *cpu_va;      /* CPU virtual address (NULL if VRAM) */
    dma_addr_t          dma_addr;    /* IOMMU IOVA for DMA access          */
    size_t              size;        /* allocation size in bytes           */
    enum mem_location   location;    /* where the memory lives             */
    u32                 flags;       /* PROT_READ | PROT_WRITE | cache type */
    struct kref         ref;         /* reference counting                 */
    struct list_head    list;        /* link in allocator's alloc_list      */
};

/* ─── GPU Memory Pool ─────────────────────────────────────────────────────*/
struct gpu_mem_pool {
    struct gen_pool    *vram_pool;   /* gen_pool for GPU VRAM               */
    struct device      *dev;         /* GPU PCI device (for DMA ops)        */
    u64                 vram_base;   /* physical base of GPU VRAM           */
    u64                 vram_size;   /* total VRAM size                     */
    spinlock_t          lock;
    struct list_head    alloc_list;  /* all active allocations              */
};

/* ─── Initialize VRAM pool ────────────────────────────────────────────────*/
int gpu_mem_pool_init(struct gpu_mem_pool *pool,
                       struct device *dev,
                       u64 vram_phys_base, u64 vram_size)
{
    int ret;

    pool->dev        = dev;
    pool->vram_base  = vram_phys_base;
    pool->vram_size  = vram_size;
    spin_lock_init(&pool->lock);
    INIT_LIST_HEAD(&pool->alloc_list);

    /*
     * gen_pool: generic memory pool for managing a fixed-size memory region.
     * min_alloc_order=12 = minimum allocation is one page (4KB).
     * Alternative: order=21 for 2MB GPU pages.
     */
    pool->vram_pool = gen_pool_create(PAGE_SHIFT, -1 /* no NUMA node */);
    if (!pool->vram_pool)
        return -ENOMEM;

    /* Add the entire VRAM range to the pool */
    ret = gen_pool_add(pool->vram_pool, vram_phys_base, vram_size, -1);
    if (ret) {
        gen_pool_destroy(pool->vram_pool);
        return ret;
    }

    /* Use best-fit allocation algorithm for VRAM */
    gen_pool_set_algo(pool->vram_pool, gen_pool_best_fit, NULL);

    pr_info("GPU: VRAM pool initialized: %llu MB at PA 0x%llx\n",
            vram_size >> 20, vram_phys_base);
    return 0;
}

/* ─── Allocate GPU memory ─────────────────────────────────────────────────*/
struct gpu_mem_descriptor *gpu_mem_alloc(struct gpu_mem_pool *pool,
                                          size_t size,
                                          enum mem_location location,
                                          u32 flags)
{
    struct gpu_mem_descriptor *desc;

    desc = kzalloc(sizeof(*desc), GFP_KERNEL);
    if (!desc)
        return ERR_PTR(-ENOMEM);

    desc->size     = size;
    desc->location = location;
    desc->flags    = flags;
    kref_init(&desc->ref);

    switch (location) {
    case MEM_VIDMEM:
        /*
         * gen_pool_alloc: allocate from GPU VRAM pool.
         * Returns physical address (GPU PA), not a CPU VA.
         * CPU cannot directly access this memory.
         */
        desc->gpu_va = gen_pool_alloc(pool->vram_pool, size);
        if (!desc->gpu_va) {
            pr_err("GPU: VRAM OOM for %zuMB allocation\n", size >> 20);
            kfree(desc);
            return ERR_PTR(-ENOMEM);
        }
        desc->cpu_va  = NULL; /* no CPU mapping by default */
        desc->dma_addr = desc->gpu_va; /* VRAM is directly DMA-addressable */
        break;

    case MEM_SYSMEM:
        /*
         * vmalloc: allocate virtually contiguous system memory.
         * Physically non-contiguous — needs scatterlist for DMA.
         * CPU can access via cpu_va.
         */
        desc->cpu_va = vmalloc(size);
        if (!desc->cpu_va) {
            kfree(desc);
            return ERR_PTR(-ENOMEM);
        }
        /*
         * For DMA access: would use dma_map_single or sg_alloc_table_from_pages.
         * Simplified here — in production, call dma_map_sg on the vmalloc pages.
         */
        desc->dma_addr = 0; /* populated after DMA mapping */
        break;

    case MEM_UNIFIED:
        /*
         * Unified memory: allocate VA range in both CPU and GPU VA spaces.
         * Physical pages allocated on demand via page fault handling (Q29).
         * Use vmalloc for CPU backing; gpu_va_alloc for GPU VA range.
         */
        desc->cpu_va = vmalloc(size);
        if (!desc->cpu_va) {
            kfree(desc);
            return ERR_PTR(-ENOMEM);
        }
        /* GPU VA allocated via gpu_va_alloc (Q28) */
        desc->gpu_va = gpu_va_alloc(pool->gpu_va_space, size, PAGE_SIZE);
        if (!desc->gpu_va) {
            vfree(desc->cpu_va);
            kfree(desc);
            return ERR_PTR(-ENOMEM);
        }
        break;
    }

    spin_lock(&pool->lock);
    list_add(&desc->list, &pool->alloc_list);
    spin_unlock(&pool->lock);

    pr_debug("GPU: allocated %zuMB loc=%d gpu_va=0x%llx\n",
             size >> 20, location, desc->gpu_va);
    return desc;
}

/* ─── Free GPU memory ─────────────────────────────────────────────────────*/
static void gpu_mem_release(struct kref *ref)
{
    struct gpu_mem_descriptor *desc =
        container_of(ref, struct gpu_mem_descriptor, ref);

    switch (desc->location) {
    case MEM_VIDMEM:
        gen_pool_free(pool->vram_pool, desc->gpu_va, desc->size);
        break;
    case MEM_SYSMEM:
        if (desc->dma_addr)
            dma_unmap_single(pool->dev, desc->dma_addr, desc->size,
                              DMA_BIDIRECTIONAL);
        vfree(desc->cpu_va);
        break;
    case MEM_UNIFIED:
        vfree(desc->cpu_va);
        gpu_va_free(pool->gpu_va_space, desc->gpu_va);
        break;
    }

    kfree(desc);
}

void gpu_mem_put(struct gpu_mem_pool *pool, struct gpu_mem_descriptor *desc)
{
    spin_lock(&pool->lock);
    list_del(&desc->list);
    spin_unlock(&pool->lock);

    kref_put(&desc->ref, gpu_mem_release);
}

void gpu_mem_get(struct gpu_mem_descriptor *desc)
{
    kref_get(&desc->ref);
}

/* ─── Pool statistics ─────────────────────────────────────────────────────*/
void gpu_mem_pool_stats(struct gpu_mem_pool *pool)
{
    pr_info("VRAM: total=%llu MB, free=%zu MB, used=%zu MB\n",
            pool->vram_size >> 20,
            gen_pool_avail(pool->vram_pool) >> 20,
            gen_pool_size(pool->vram_pool) >> 20 -
            gen_pool_avail(pool->vram_pool) >> 20);
}
```

---

## Explanation

### Core Concept

GPU memory allocation has three fundamentally different backing stores:

```
MEM_VIDMEM:  GPU VRAM (HBM)
  ┌─────────────────────────────────┐
  │  gen_pool (physical PA space)   │  Fast for GPU, invisible to CPU
  └─────────────────────────────────┘

MEM_SYSMEM:  System DRAM
  ┌─────────────────────────────────┐
  │  vmalloc / dma_alloc_coherent   │  CPU accessible, DMA mapped for GPU
  └─────────────────────────────────┘

MEM_UNIFIED: Demand-paged
  ┌─────────────────────────────────┐
  │  Page fault on GPU/CPU access   │  Transparent migration (UVM)
  └─────────────────────────────────┘
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `gen_pool_create(order, nid)` | Create a generic memory pool |
| `gen_pool_add(pool, base, size, nid)` | Add memory region to pool |
| `gen_pool_alloc(pool, size)` | Allocate from pool (returns address) |
| `gen_pool_free(pool, addr, size)` | Return memory to pool |
| `gen_pool_avail(pool)` | Available bytes |
| `gen_pool_set_algo(pool, algo, data)` | Set allocation algorithm |
| `gen_pool_best_fit` | Best-fit algorithm callback |
| `vmalloc(size)` / `vfree(p)` | Virtually-contiguous system memory |
| `kref_init` / `kref_get` / `kref_put` | Reference counting |

### Trade-offs & Pitfalls

- **VRAM fragmentation with gen_pool.** `gen_pool` with `gen_pool_best_fit` reduces fragmentation but is O(n) for allocation. For a 40GB HBM2e pool, use a buddy allocator instead (O(log n)). NVIDIA's VRAM pool uses a custom buddy allocator.
- **vmalloc for large sysmem allocations.** `vmalloc` is O(n/page) for mapping. For allocations > 1GB, use `alloc_pages_bulk` or `cma_alloc` for physically contiguous memory.
- **kref for shared allocations.** Multiple GPU contexts can share the same descriptor (e.g., shared CUDA IPC memory). `kref` ensures the physical memory is freed only when the last reference drops.

### NVIDIA / GPU Context

NVIDIA CUDA memory allocation paths:
- `cuMemAlloc(size)` → `MEM_VIDMEM` allocation from `gen_pool`-like VRAM pool
- `cuMemAllocHost(size)` → `MEM_SYSMEM` pinned via `dma_alloc_coherent`
- `cudaMallocManaged(size)` → `MEM_UNIFIED` with UVM fault handler
- `cuMemAllocAsync(size, stream)` → Pool allocator (stream-ordered memory)

---

## Cross Questions & Answers

**CQ1: What is `gen_pool` and when is it preferred over `kmalloc`?**
> `gen_pool` is a generic allocator for managing a fixed memory range (not kernel heap). It's used when: (1) the memory has a specific physical address range (GPU VRAM, reserved memory, MMIO-adjacent region), (2) the driver needs to hand out portions of this range to users, (3) the memory is not backed by `struct page` (no `kmalloc/vmalloc` support). `kmalloc` is for kernel heap (managed by SLUB); `gen_pool` is for external memory regions the kernel knows nothing about.

**CQ2: What is CUDA stream-ordered memory allocation (`cuMemAllocAsync`) and why is it faster?**
> Stream-ordered allocation maintains a per-stream memory pool. Instead of freeing memory back to the OS on `cuMemFree`, it returns it to the stream pool for immediate reuse on the next `cuMemAllocAsync` in the same stream. This eliminates: (1) GMMU page table update latency (pages stay mapped), (2) `gen_pool_alloc/free` overhead for allocation metadata, (3) IOMMU mapping/unmapping overhead. For iterative neural network forward/backward passes, stream-ordered allocation reduces allocation overhead by ~10-100x vs `cuMemAlloc/Free` per iteration.

**CQ3: How would you implement GPU memory compaction to defragment a fragmented VRAM pool?**
> Memory compaction requires: (1) find all live allocations in fragmented regions, (2) allocate new contiguous space, (3) GPU DMA copy (copy engine) from old to new location, (4) update all GMMU PTEs pointing to old addresses to point to new addresses, (5) free the old space. Steps 3-4 must be atomic from the perspective of GPU kernels using the memory — requires pausing all GPU contexts while PTEs are updated (GPU preemption + fence wait). This is expensive and typically only done when VRAM is critically fragmented.

**CQ4: What is the difference between `dma_alloc_coherent` and `vmalloc` for system memory GPU buffers?**
> `dma_alloc_coherent`: allocates physically contiguous memory that's DMA-coherent (cache-coherent between CPU and GPU). Returns both a CPU VA and a DMA address. No scatter-gather needed. Best for small buffers (< 4MB), command rings, descriptors. `vmalloc`: allocates virtually contiguous but physically scattered pages. CPU accessible but requires scatter-gather for DMA. Better for large buffers where physical contiguity is not available.

**CQ5: How does CUDA Unified Memory handle the case where GPU accesses a CPU-cached page?**
> When GPU accesses a page currently resident in CPU DRAM (and cached in CPU L1/L2): (1) GPU GMMU faults, (2) UVM fault handler runs, (3) if `access_counter > migration_threshold`, UVM migrates the page from DRAM to HBM (via PCIe DMA + `cudaMemPrefetchAsync` logic), (4) updates both CPU page tables (marks page not-present, causing CPU fault on next access) and GPU GMMU PTE (maps to new HBM address), (5) replays GPU fault. CPU later faults on the page and UVM migrates it back to DRAM. This page thrashing is a known UVM pitfall for pages accessed alternately by CPU and GPU.

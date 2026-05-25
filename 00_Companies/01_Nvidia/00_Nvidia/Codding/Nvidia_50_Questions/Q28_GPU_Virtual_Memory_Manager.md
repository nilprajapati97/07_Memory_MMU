# Q28: GPU Virtual Memory Manager

**Section:** System Design | **Difficulty:** Hard | **Topics:** VA space management, `rb_tree`, best-fit allocation, GPU GMMU, `vm_area_struct` analogy, GPU page tables

---

## Question

Design a GPU Virtual Memory Manager (VMM) that manages GPU VA space allocation using a red-black tree.

---

## Answer

```c
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/types.h>

#define GPU_VA_START    0x0000100000000000ULL  /* 1TB start */
#define GPU_VA_END      0x0000800000000000ULL  /* 128TB end */
#define GPU_PAGE_SIZE   (2 * 1024 * 1024ULL)  /* 2MB GPU pages */
#define GPU_PAGE_MASK   (~(GPU_PAGE_SIZE - 1))

/* ─── VA Range descriptor ─────────────────────────────────────────────────
 * Represents one allocation in the GPU VA space.
 */
struct va_range {
    u64             va_start;   /* inclusive start virtual address */
    u64             va_end;     /* exclusive end virtual address   */
    u64             phys_addr;  /* backing physical address (VRAM or sysmem) */
    u32             flags;      /* PROT_READ/WRITE/EXEC, cache type, etc.    */
    struct rb_node  node;       /* position in va_tree                       */
};

/* ─── GPU VA Space ────────────────────────────────────────────────────────*/
struct gpu_va_space {
    struct rb_root  va_tree;    /* RB tree of allocated va_range nodes       */
    spinlock_t      lock;
    u64             va_start;   /* beginning of managed VA space             */
    u64             va_end;     /* end of managed VA space                   */
    u64             total_size; /* va_end - va_start                         */
    u64             used_bytes; /* currently allocated bytes                 */
};

void gpu_va_space_init(struct gpu_va_space *vas)
{
    vas->va_tree   = RB_ROOT;
    spin_lock_init(&vas->lock);
    vas->va_start  = GPU_VA_START;
    vas->va_end    = GPU_VA_END;
    vas->total_size = GPU_VA_END - GPU_VA_START;
    vas->used_bytes = 0;
}

/* ─── Insert allocation into RB tree (ordered by va_start) ───────────────*/
static void va_insert(struct gpu_va_space *vas, struct va_range *new_va)
{
    struct rb_node **link = &vas->va_tree.rb_node;
    struct rb_node  *parent = NULL;

    while (*link) {
        struct va_range *cur = rb_entry(*link, struct va_range, node);
        parent = *link;

        if (new_va->va_start < cur->va_start)
            link = &(*link)->rb_left;
        else
            link = &(*link)->rb_right;
    }

    rb_link_node(&new_va->node, parent, link);
    rb_insert_color(&new_va->node, &vas->va_tree);
}

/* ─── Best-fit VA allocation ──────────────────────────────────────────────
 * Scans the gaps between allocated ranges to find the smallest gap that fits.
 * Falls back to first-fit if no best-fit found (for simplicity).
 * Returns allocated va_start, or 0 on failure.
 */
u64 gpu_va_alloc(struct gpu_va_space *vas, u64 size, u64 align)
{
    struct rb_node *node;
    struct va_range *va, *best_va = NULL;
    u64 prev_end, gap_start, aligned_start;
    u64 best_gap = U64_MAX;
    u64 result = 0;

    /* Align size to GPU page boundary */
    size = ALIGN(size, GPU_PAGE_SIZE);
    align = max(align, GPU_PAGE_SIZE);

    spin_lock(&vas->lock);

    prev_end = vas->va_start;

    /* Scan all allocated ranges in VA order, check gaps between them */
    for (node = rb_first(&vas->va_tree); node; node = rb_next(node)) {
        va = rb_entry(node, struct va_range, node);

        /* Gap is [prev_end, va->va_start) */
        gap_start = ALIGN(prev_end, align);

        if (gap_start + size <= va->va_start) {
            u64 gap_size = va->va_start - gap_start;
            /* Best-fit: prefer smallest gap that fits */
            if (gap_size >= size && gap_size < best_gap) {
                best_gap = gap_size;
                result = gap_start;
            }
        }

        prev_end = va->va_end;
    }

    /* Check the gap after the last allocation (up to va_end) */
    gap_start = ALIGN(prev_end, align);
    if (gap_start + size <= vas->va_end) {
        u64 gap_size = vas->va_end - gap_start;
        if (gap_size < best_gap)
            result = gap_start;
    }

    if (!result) {
        spin_unlock(&vas->lock);
        return 0; /* OOM: no VA space available */
    }

    /* Allocate and insert the va_range */
    struct va_range *new_va = kzalloc(sizeof(*new_va), GFP_ATOMIC);
    if (!new_va) {
        spin_unlock(&vas->lock);
        return 0;
    }

    new_va->va_start = result;
    new_va->va_end   = result + size;
    va_insert(vas, new_va);
    vas->used_bytes += size;

    spin_unlock(&vas->lock);
    return result;
}

/* ─── VA lookup: find va_range containing a given GPU VA ──────────────────*/
struct va_range *gpu_va_find(struct gpu_va_space *vas, u64 gpu_va)
{
    struct rb_node *node = vas->va_tree.rb_node;

    while (node) {
        struct va_range *va = rb_entry(node, struct va_range, node);

        if (gpu_va < va->va_start)
            node = node->rb_left;
        else if (gpu_va >= va->va_end)
            node = node->rb_right;
        else
            return va; /* gpu_va is within [va_start, va_end) */
    }
    return NULL;
}

/* ─── VA free: remove allocation and update GPU page tables ───────────────*/
int gpu_va_free(struct gpu_va_space *vas, u64 gpu_va)
{
    struct va_range *va;

    spin_lock(&vas->lock);
    va = gpu_va_find(vas, gpu_va);
    if (!va) {
        spin_unlock(&vas->lock);
        return -EINVAL;
    }

    rb_erase(&va->node, &vas->va_tree);
    vas->used_bytes -= (va->va_end - va->va_start);
    spin_unlock(&vas->lock);

    /* Invalidate GPU TLB for this VA range */
    gpu_gmmu_unmap(vas, va->va_start, va->va_end - va->va_start);

    kfree(va);
    return 0;
}
```

### VA Space Layout

```
GPU VA Space: 0x0001_0000_0000_0000 ── 0x0008_0000_0000_0000 (128TB)

 [VA_START]──[alloc1]──[gap1]──[alloc2]──[gap2]──[alloc3]──[VA_END]
              RB node           RB node            RB node

 RB Tree (ordered by va_start):
           alloc2 (root)
          /              \
      alloc1           alloc3

 Best-fit algorithm examines: gap0=[VA_START, alloc1.start),
                               gap1=[alloc1.end, alloc2.start),
                               gap2=[alloc2.end, alloc3.start),
                               gap3=[alloc3.end, VA_END)
 → picks smallest gap ≥ requested_size
```

---

## Explanation

### Core Concept

GPU VA space management mirrors the kernel's `mmap` / `vm_area_struct` VMA tree but for GPU virtual addresses. The GPU GMMU (Graphics Memory Management Unit) has its own page tables separate from the CPU MMU. The driver manages the GPU VA space independently.

**Why best-fit?** Prevents VA space fragmentation — keeping large contiguous free regions for large CUDA allocations (model tensors, frame buffers). First-fit is faster but leads to small scattered free regions.

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `ALIGN(x, align)` | Round up to alignment boundary |
| `rb_first(root)` | Leftmost node (smallest va_start) |
| `rb_next(node)` | Next node in sorted order |
| `rb_entry(node, type, member)` | `container_of` wrapper for RB nodes |
| `rb_link_node` + `rb_insert_color` | Insert into RB tree (two-step) |
| `rb_erase(node, root)` | Remove from RB tree |
| `GFP_ATOMIC` | Allocation under spinlock (no sleep) |
| `kzalloc` / `kfree` | Heap allocation for `va_range` nodes |

### Trade-offs & Pitfalls

- **VA fragmentation.** Best-fit reduces external fragmentation but not internal (allocated more than needed due to alignment). Periodically compact the VA space during idle periods or use a buddy-style VA allocator.
- **`GFP_ATOMIC` under spinlock.** The spinlock prevents sleeping — use `GFP_ATOMIC` for any allocation inside the lock. Better: pre-allocate `va_range` nodes outside the lock and use them inside (eliminates `GFP_ATOMIC` failure risk).
- **Sparse tree for huge VA spaces.** 128TB VA space could hypothetically have millions of small allocations. The RB tree scan during best-fit is O(n) in the number of allocations. For production, use a dedicated VA allocator (`gen_pool` or `iova_domain`) which maintains a free-list for O(log n) allocation.

### NVIDIA / GPU Context

NVIDIA GPU driver maintains per-process `NvU64 va_space` structures:
- Each CUDA process has its own 128TB GPU VA space (mirroring CPU's 128TB user VA)
- `cuMemAlloc(size)` → GPU VASpace alloc → `iommu_map()` → GPU GMMU page table update
- Per-context 2-level VA split: 64GB low = UVM managed (demand-paged), rest = explicit `cuMemAlloc`

---

## Cross Questions & Answers

**CQ1: What is the GPU GMMU and how does it differ from the CPU MMU?**
> The GMMU (Graphics MMU) is the GPU's hardware memory management unit. Like the CPU MMU, it translates GPU virtual addresses → physical addresses using page tables in GPU VRAM. Differences: GPU pages are typically 4KB, 64KB, or 2MB (large pages for TLB efficiency). The GMMU is programmed by the driver (via MMIO writes), not the OS. Each GPU context has its own GMMU page table (CR3-equivalent), allowing process isolation. The GMMU supports sparse mappings efficiently (most GPU VA space is unmapped).

**CQ2: How does UVM (Unified Virtual Memory) change the VA space design?**
> Without UVM: CPU and GPU have separate VA spaces; `cudaMemcpy` copies between them. With UVM: CPU and GPU share the same virtual address range (e.g., `cudaMallocManaged`). The driver maps the same physical pages into both CPU page tables (via `remap_pfn_range`) and GPU GMMU page tables, with demand-paging on fault. When the CPU accesses a GPU-resident page, a CPU page fault triggers migration to DRAM. When GPU accesses a CPU-resident page, a GPU fault triggers migration to VRAM. The VA space manager must track which device currently "owns" each range.

**CQ3: How would you implement VA space merging (coalescing adjacent free regions)?**
> After `gpu_va_free`, check if the freed range is adjacent to its RB predecessor or successor. Use `rb_prev(node)` and `rb_next(node)` before erasing. If `prev->va_end == freed->va_start`, merge them into one. Then check if the merged range is adjacent to the next. This two-pass merge keeps the free list clean. Alternatively, maintain a separate free-range RB tree in addition to the allocated-range tree — free ranges are also in a tree ordered by size (for O(log n) best-fit queries).

**CQ4: What is the difference between `gpu_va_space` and a kernel `vm_area_struct` VMA?**
> Both represent a virtual address range in an address space, but: `vm_area_struct` is in the CPU's virtual address space (managed by `mm_struct`), backed by real kernel VM infrastructure (page faults, swap, NUMA migration). `gpu_va_space` is in the GPU's virtual address space, managed entirely by the driver in software. There's no kernel-level support for GPU VMAs — no automatic page-fault handling, no swap support, no NUMA migration. The driver must handle all GPU TLB invalidation, page table updates, and fault recovery manually.

**CQ5: How do you handle VA space exhaustion when a CUDA process needs more than 128TB?**
> 128TB is the 47-bit address space limit. Solutions: (1) Use 56-bit or 64-bit GPU VA mode (modern GPUs like H100 support this, mapped via extra GMMU levels), (2) VA space multiplexing — reuse VA ranges by unmapping old allocations when new ones are needed (explicit eviction), (3) Sparse residency — only map GPU pages when actually accessed (demand-paging via GMMU fault handler). NVIDIA A100/H100 supports `NV_LARGE_ADDRESS` mode with 56-bit GPU VA, giving 64PB of addressable GPU memory.

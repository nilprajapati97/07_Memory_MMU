# Q12: Huge Pages / THP — Allocate 2MB Page

**Section:** Memory Management | **Difficulty:** Medium | **Topics:** huge pages, THP, `HPAGE_PMD_ORDER`, `alloc_pages`, TLB pressure, GPU performance

---

## Question

What is huge pages / THP in Linux? Write kernel code to allocate a 2MB huge page.

---

## Answer

```c
#include <linux/mm.h>
#include <linux/huge_mm.h>
#include <linux/slab.h>

/* ─── Allocate a 2MB huge page from the kernel ────────────────────────────
 * HPAGE_PMD_ORDER = 9 on x86_64 (512 pages × 4KB = 2MB)
 * __GFP_COMP: allocate a "compound page" (head + tail pages as one unit)
 */
struct page *alloc_huge_page_kernel(void)
{
    struct page *page;

    page = alloc_pages(GFP_KERNEL | __GFP_COMP | __GFP_NOWARN,
                        HPAGE_PMD_ORDER);
    if (!page) {
        pr_warn("Failed to allocate 2MB huge page\n");
        return NULL;
    }

    pr_info("Huge page: PA=0x%llx, size=%lu KB\n",
            (u64)page_to_phys(page),
            (PAGE_SIZE << HPAGE_PMD_ORDER) >> 10);

    return page;
}

void free_huge_page_kernel(struct page *page)
{
    __free_pages(page, HPAGE_PMD_ORDER);
}

/* ─── Allocate a 1GB huge page (x86_64 with 1G pages support) ─────────────
 * HPAGE_PUD_ORDER = 18 (262144 pages × 4KB = 1GB)
 */
struct page *alloc_1gb_page(void)
{
    if (!cpu_has_feature(X86_FEATURE_PDPE1GB))
        return NULL;

    return alloc_pages(GFP_KERNEL | __GFP_COMP | __GFP_NOWARN,
                        HPAGE_PUD_ORDER);
}

/* ─── Map a huge page into a user VMA (GPU driver vm_fault handler) ───────*/
static vm_fault_t gpu_huge_vm_fault(struct vm_fault *vmf)
{
    struct page *page;

    /* Try 2MB huge page first */
    page = alloc_pages(GFP_KERNEL | __GFP_COMP | __GFP_NOWARN,
                        HPAGE_PMD_ORDER);
    if (!page) {
        /* Fall back to 4KB page */
        page = alloc_page(GFP_KERNEL);
        if (!page)
            return VM_FAULT_OOM;
    }

    get_page(page);
    vmf->page = page;
    return 0;
}

/* ─── Enable THP for a VMA (userspace GPU buffer) ─────────────────────────*/
void gpu_enable_thp_for_vma(struct vm_area_struct *vma)
{
    /*
     * VM_HUGEPAGE: hint that THP should be used for this VMA.
     * The kernel's khugepaged daemon will opportunistically
     * collapse 512 consecutive 4KB pages into one 2MB THP.
     */
    vma->vm_flags |= VM_HUGEPAGE;

    /*
     * VM_NOHUGEPAGE: explicitly disable THP (for DMA-coherent regions
     * where huge page splitting during unmap would be problematic).
     */
    /* vma->vm_flags |= VM_NOHUGEPAGE; */
}

/* ─── Check if a page is part of a compound (huge) page ──────────────────*/
void inspect_page(struct page *page)
{
    if (PageCompound(page)) {
        struct page *head = compound_head(page);
        pr_info("Compound page: order=%u, head PA=0x%llx\n",
                compound_order(head),
                (u64)page_to_phys(head));
    }
}
```

---

## Explanation

### Core Concept

Modern x86_64 supports three page sizes:
- **4 KB** — standard, always available
- **2 MB** — "huge pages" (PMD-level mapping), most commonly used
- **1 GB** — "gigantic pages" (PUD-level mapping), requires hardware support

**Why huge pages matter for GPU workloads:**

Each 4KB page requires one TLB entry. A 4GB GPU buffer mapped with 4KB pages needs 1,048,576 TLB entries — far exceeding the TLB capacity (~1024 entries). TLB misses require page table walks that cost 100+ cycles each.

With 2MB pages, the same 4GB buffer needs only 2,048 TLB entries — a 512× reduction in TLB pressure.

```
TLB Pressure for 4GB buffer:
  4KB pages:  4GB / 4KB  = 1,048,576 TLB entries  → constant TLB thrash
  2MB pages:  4GB / 2MB  = 2,048    TLB entries   → TLB stays warm
  1GB pages:  4GB / 1GB  = 4        TLB entries   → essentially zero overhead
```

**THP (Transparent Huge Pages):** The kernel automatically promotes groups of 512 contiguous 4KB pages to a single 2MB THP. The process `khugepaged` scans memory and performs the promotion. No application changes needed — hence "transparent."

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `HPAGE_PMD_ORDER` | Order of a 2MB huge page (= 9 on x86_64) |
| `HPAGE_PMD_SIZE` | Size of a 2MB huge page in bytes |
| `HPAGE_PUD_ORDER` | Order of a 1GB huge page (= 18 on x86_64) |
| `alloc_pages(gfp, order)` | Allocate `2^order` physically contiguous pages |
| `__GFP_COMP` | Allocate as a compound page (treated as one unit) |
| `__free_pages(page, order)` | Free a multi-order allocation |
| `PageCompound(page)` | Test if a page is part of a compound page |
| `compound_head(page)` | Get the head page of a compound page |
| `compound_order(head)` | Get the order of a compound page |
| `VM_HUGEPAGE` | VMA flag: encourage THP for this region |
| `VM_NOHUGEPAGE` | VMA flag: disable THP for this region |

### Trade-offs & Pitfalls

- **Allocation failure:** High-order page allocations frequently fail on fragmented systems. Always fall back to 4KB pages if 2MB allocation fails. Use `__GFP_NOWARN` to suppress the "page allocation failure" printk.
- **THP splitting:** When a 2MB THP needs to be partially unmapped or remapped, the kernel "splits" it back into 512 × 4KB pages. This is expensive (invalidates TLB, page table restructuring). GPU drivers that do partial unmaps should disable THP with `VM_NOHUGEPAGE` for those VMAs.
- **`__GFP_COMP` is required.** Without it, `alloc_pages(order > 0)` returns the head page of an array of individual pages, not a compound page. Compound pages are needed for `put_page` reference counting to work correctly on the entire compound.
- **NUMA:** On NUMA systems, huge pages allocated without `__GFP_THISNODE` may come from a remote NUMA node, incurring remote memory access latency. Use `alloc_pages_node(nid, gfp, order)` for NUMA-local huge page allocation.

### NVIDIA / GPU Context

NVIDIA GPU drivers use huge pages for:
- **CUDA Unified Virtual Memory (UVM):** GPU and CPU share a VA space; 2MB pages dramatically reduce TLB misses on GPU TLB
- **cuBLAS/cuDNN workspace allocations:** Large matrix buffers mapped with 2MB pages for GPU kernel throughput
- **NVLink P2P mappings:** Peer GPU memory mapped into local GPU GMMU with 2MB PTEs for lower PTE count and better TLB performance
- NVIDIA's `nvidia-uvm.ko` module manages huge page promotion/demotion based on access patterns

---

## Cross Questions & Answers

**CQ1: What is `khugepaged` and how does it promote pages to THP?**
> `khugepaged` is a kernel thread that periodically scans anonymous VMAs for regions where all 512 × 4KB pages are present, anonymous, and writable. When found, it acquires all 512 pages, allocates one 2MB compound page, copies the data, installs the PMD-level PTE, and frees the 512 individual pages. The process is transparent to userspace. It is controlled via `/sys/kernel/mm/transparent_hugepage/` — can be set to `always`, `madvise`, or `never`.

**CQ2: What is `madvise(MADV_HUGEPAGE)` and how does it differ from the system-wide THP setting?**
> `madvise(addr, len, MADV_HUGEPAGE)` marks a specific VMA range as THP-eligible, even if the system-wide setting is `madvise` (only promote when explicitly requested). CUDA allocates compute buffers with `MADV_HUGEPAGE` to ensure GPU work buffers benefit from 2MB pages regardless of system policy. Conversely, `MADV_NOHUGEPAGE` disables THP for regions where splitting would be costly (e.g., DMA streaming buffers).

**CQ3: How does the GMMU (GPU MMU) benefit from 2MB pages compared to 4KB pages?**
> The GPU's TLB (Translation Lookaside Buffer) is smaller than the CPU's — typically 512–4096 entries. A large CUDA kernel accessing a 1GB buffer with 4KB pages generates 262,144 TLB misses per full traversal, each requiring a GPU page table walk (200+ GPU cycles). With 2MB pages, the same access needs only 512 TLB entries. NVIDIA measures 2–4× GPU memory bandwidth improvement when switching from 4KB to 2MB GPU page mappings for large matrix operations.

**CQ4: What is a "compound page" and why is `__GFP_COMP` needed for huge page allocations?**
> A compound page groups multiple physically contiguous pages under a single "head" page with a reference count. The head page's `order` field records how many tail pages follow it. `put_page(head)` decrements the ref count for the entire compound, freeing all pages atomically. Without `__GFP_COMP`, you get an array of independent pages — `put_page` on each tail page frees it individually, breaking the contiguity guarantee and causing double-free bugs.

**CQ5: What happens to THP during `fork()` — does the child inherit the huge page or get individual pages?**
> The child inherits the PMD-level PTE pointing to the same 2MB huge page (Copy-on-Write). When either parent or child writes to any byte in the 2MB range, the kernel must split the THP: it allocates 512 × 4KB pages, copies the 2MB content into them, and installs 512 individual PTEs for the writing process. This "THP split on CoW" is a known performance pitfall for GPU processes that `fork()` after allocating large CUDA buffers — NVIDIA recommends allocating CUDA resources after `fork()`.

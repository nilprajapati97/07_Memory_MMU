# Q17: GFP Flags — GFP_KERNEL vs GFP_ATOMIC vs GFP_DMA

**Section:** Memory Management | **Difficulty:** Easy-Medium | **Topics:** GFP flags, memory zones, allocation context, IRQ, DMA zone, NUMA

---

## Question

Explain the difference between `GFP_KERNEL`, `GFP_ATOMIC`, `GFP_DMA`, `GFP_NOWAIT`.

---

## Answer

```c
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>

/* ─── GFP_KERNEL ─────────────────────────────────────────────────────────
 * Normal allocation. CAN sleep (block for reclaim, compaction, swapping).
 * Allocates from ZONE_NORMAL (or ZONE_HIGH on 32-bit).
 * Use in: process context, driver init, ioctl handlers.
 */
void *process_context_alloc(size_t size)
{
    void *p = kmalloc(size, GFP_KERNEL);
    if (!p)
        return NULL; /* allocation failed after reclaim attempt */
    return p;
}

/* ─── GFP_ATOMIC ─────────────────────────────────────────────────────────
 * CANNOT sleep. Uses reserved emergency pool.
 * Never triggers reclaim or compaction.
 * Higher failure rate than GFP_KERNEL.
 * Use in: IRQ handlers, softirqs, tasklets, spinlock-held sections.
 */
irqreturn_t gpu_irq_handler(int irq, void *dev_id)
{
    /* WRONG: GFP_KERNEL in IRQ context causes BUG (might_sleep assertion) */
    // void *p = kmalloc(64, GFP_KERNEL);  /* BUG! */

    /* CORRECT */
    void *p = kmalloc(64, GFP_ATOMIC);
    if (!p) {
        pr_warn_ratelimited("GPU: failed to alloc in IRQ\n");
        return IRQ_HANDLED; /* handle gracefully */
    }
    /* ... use p ... */
    return IRQ_HANDLED;
}

/* ─── GFP_DMA ─────────────────────────────────────────────────────────── 
 * Allocates from ZONE_DMA (0–16MB on x86).
 * For legacy ISA DMA controllers that can only address 16MB.
 * Almost never needed on modern hardware.
 */
void *isa_dma_alloc(size_t size)
{
    return kmalloc(size, GFP_KERNEL | GFP_DMA);
}

/* ─── GFP_DMA32 ───────────────────────────────────────────────────────── 
 * Allocates from ZONE_DMA32 (0–4GB on x86_64).
 * For 32-bit DMA devices (older PCIe cards with 32-bit DMA mask).
 * Use: dma_set_mask(dev, DMA_BIT_MASK(32)) devices.
 */
void *pcie_32bit_dma_alloc(size_t size)
{
    return kmalloc(size, GFP_KERNEL | GFP_DMA32);
}

/* ─── GFP_NOWAIT ──────────────────────────────────────────────────────── 
 * Like GFP_ATOMIC but does NOT use the emergency reserve pool.
 * Returns NULL immediately if no memory available.
 * Use: optional optimization paths where failure is acceptable.
 */
void *try_prealloc_buffer(size_t size)
{
    return kmalloc(size, GFP_NOWAIT);
}

/* ─── GFP_HIGHUSER_MOVABLE ───────────────────────────────────────────────
 * Allocates from ZONE_HIGHMEM (32-bit) or anywhere (64-bit).
 * Page is movable (memory compaction can migrate it).
 * Used for user-space GPU buffer pages that benefit from THP compaction.
 */
struct page *alloc_movable_user_page(void)
{
    return alloc_page(GFP_HIGHUSER_MOVABLE);
}

/* ─── Modifier flags (combined with base GFP) ────────────────────────────*/
void demonstrate_modifiers(void)
{
    /* __GFP_ZERO: zero-initialize the allocated memory */
    void *zeroed = kmalloc(256, GFP_KERNEL | __GFP_ZERO);

    /* __GFP_NOWARN: suppress "page allocation failure" kernel log message */
    void *silent = kmalloc(4096 * 1024, GFP_KERNEL | __GFP_NOWARN);

    /* __GFP_RETRY_MAYFAIL: retry harder, but don't loop indefinitely */
    void *large  = kmalloc(512 * 1024, GFP_KERNEL | __GFP_RETRY_MAYFAIL);

    /* __GFP_NOFAIL: keep retrying until success (dangerous — can deadlock) */
    /* Only use when allocation MUST succeed and failure means kernel panic */
    void *must   = kmalloc(256, GFP_KERNEL | __GFP_NOFAIL);

    /* __GFP_THISNODE: allocate from the current NUMA node only */
    void *local  = kmalloc_node(256, GFP_KERNEL | __GFP_THISNODE,
                                  numa_node_id());

    kfree(zeroed); kfree(silent); kfree(large); kfree(must); kfree(local);
}
```

### GFP Flags Summary Table

| Flag | Can Sleep | Zone | Emergency Pool | Use Case |
|------|-----------|------|---------------|---------|
| `GFP_KERNEL` | Yes | NORMAL | No | Process context, general |
| `GFP_ATOMIC` | No | NORMAL | **Yes** | IRQ/softirq/spinlock |
| `GFP_DMA` | Yes | DMA (0-16MB) | No | Legacy ISA DMA |
| `GFP_DMA32` | Yes | DMA32 (0-4GB) | No | 32-bit PCI devices |
| `GFP_NOWAIT` | No | NORMAL | No | Optional optimization |
| `GFP_HIGHUSER_MOVABLE` | Yes | HIGH | No | User page allocation |
| `GFP_NOIO` | Yes* | NORMAL | No | Block I/O contexts (no I/O during reclaim) |
| `GFP_NOFS` | Yes* | NORMAL | No | Filesystem contexts (no fs during reclaim) |

*Can schedule but restricts what reclaim can do.

---

## Explanation

### Core Concept

**Memory zones** on x86_64:
```
Physical Address Space:
  0 ── 16MB:    ZONE_DMA      ← legacy ISA DMA
  0 ── 4GB:     ZONE_DMA32    ← 32-bit PCI DMA
  above 4GB:    ZONE_NORMAL   ← general purpose (64-bit systems)
```

**GFP flag = zone selector + behavioral flags:**

```c
GFP_KERNEL = __GFP_RECLAIM | __GFP_IO | __GFP_FS
          // can reclaim     can do I/O  can call FS
```

The behavioral flags tell the allocator what actions it can take to find memory:
- `__GFP_RECLAIM`: run page reclaim (calls shrinkers, kswapd)
- `__GFP_IO`: allowed to do block I/O (swap pages out)
- `__GFP_FS`: allowed to call filesystem (writeback dirty pages)

`GFP_ATOMIC` sets none of these — it either finds free memory immediately or fails. It uses an emergency reserve pool to guarantee a small number of atomic allocations always succeed (for network packet headers, IRQ data, etc.).

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `kmalloc(size, gfp)` | Allocate with GFP flags |
| `kzalloc(size, gfp)` | Zero-initialized `kmalloc` |
| `kmalloc_node(size, gfp, node)` | NUMA-local allocation |
| `alloc_page(gfp)` | Allocate a single page |
| `alloc_pages_node(node, gfp, order)` | NUMA + order allocation |
| `in_interrupt()` | Returns true if in IRQ/softirq context (use `GFP_ATOMIC`) |
| `in_atomic()` | Returns true if in atomic context (spinlock held, IRQ disabled) |

### Trade-offs & Pitfalls

- **`GFP_ATOMIC` can fail more often.** Never assume it succeeds. Always check the return value and have a fallback (drop the allocation, reuse a pre-allocated buffer, defer to a workqueue).
- **`GFP_NOFAIL` can cause system lockup.** If the system is genuinely OOM, `__GFP_NOFAIL` causes the allocator to loop forever waiting for memory — the process spins in the allocator, holding resources that might be needed to free memory. Use only for truly critical allocations with no fallback.
- **Wrong GFP in wrong context** triggers `might_sleep()` BUG in debug kernels. Enable `CONFIG_DEBUG_ATOMIC_SLEEP` during development.

### NVIDIA / GPU Context

| Context | GFP Flag | Reason |
|---------|----------|--------|
| GPU IRQ handler (fence complete) | `GFP_ATOMIC` | Cannot sleep in IRQ |
| CUDA `cuMemAlloc` (ioctl) | `GFP_KERNEL` | Process context, can sleep |
| Tasklet (completion processing) | `GFP_ATOMIC` | Softirq context |
| GPU driver probe | `GFP_KERNEL` | Init, no restrictions |
| GPU DMA descriptor ring | `GFP_KERNEL | GFP_DMA32` | 32-bit DMA address |
| CMA huge page for NVLink | `GFP_KERNEL | __GFP_COMP` | Compound page |

---

## Cross Questions & Answers

**CQ1: What happens if you call `kmalloc(GFP_KERNEL)` while holding a spinlock?**
> The kernel has a `might_sleep()` check at the start of the GFP_KERNEL allocation path. If called while a spinlock is held (preemption disabled), this assertion fires: `BUG: sleeping function called from invalid context`. This is caught at development time with `CONFIG_DEBUG_ATOMIC_SLEEP=y`. In production kernels without the check, the behavior is undefined — the allocator may try to sleep, causing a deadlock or kernel corruption.

**CQ2: What is `GFP_NOIO` and why is it used in block device drivers?**
> `GFP_NOIO` allows the allocator to sleep and reclaim memory, but forbids performing I/O during reclaim. Block device drivers use this to prevent a deadlock where: (1) the block driver tries to allocate memory, (2) memory reclaim tries to write dirty pages back to disk, (3) writeback tries to allocate memory from the same block driver — a circular dependency. `GFP_NOIO` breaks the cycle by prohibiting I/O in the reclaim path.

**CQ3: What is the `ZONE_MOVABLE` zone and how does it support memory hotplug?**
> `ZONE_MOVABLE` is a virtual zone containing only movable pages (user pages, file cache pages). It exists to support memory hotplug — unplugging a physical memory DIMM requires migrating all pages away from it. If the DIMM contains unmovable kernel data (kernel text, non-movable slab objects), hotplug fails. By isolating kernel allocations to `ZONE_NORMAL` and user/movable allocations to `ZONE_MOVABLE`, the kernel ensures DIMMs containing only movable pages can be safely removed. NVIDIA's HBM (High Bandwidth Memory) pools use a similar concept for GPU memory hotplug in future architectures.

**CQ4: How does the emergency pool for `GFP_ATOMIC` work?**
> The kernel reserves a small fraction of each memory zone (controlled by `min_free_kbytes`) as an emergency reserve. `GFP_ATOMIC` allocations are allowed to dip into this reserve — regular `GFP_KERNEL` allocations cannot. This guarantees that IRQ handlers can always allocate a small amount of memory even when the system is nearly out of memory. The reserve size is typically 64KB–4MB depending on total RAM.

**CQ5: What is `mempool` and when does a GPU driver use it instead of plain `kmalloc`?**
> A `mempool_t` is a pre-allocated pool of objects that guarantees allocation success even under memory pressure. The pool pre-allocates N objects at init time. `mempool_alloc(pool, GFP_ATOMIC)` first tries `kmalloc(GFP_ATOMIC)`. If that fails, it returns a pre-allocated object from the pool. GPU drivers use mempools for: fence objects (must succeed in interrupt context), command batch headers (allocated on GPU submission hot path). Without mempool, a GPU command submission could fail under memory pressure, causing GPU stall.

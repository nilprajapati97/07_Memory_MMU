# Q29: GPU Fault Handler for On-Demand GMMU Mapping

**Section:** System Design | **Difficulty:** Hard | **Topics:** GPU page fault, GMMU, `alloc_page`, `irqwork`, MMIO fault registers, fault replay, demand paging

---

## Question

Implement a GPU fault handler that allocates pages on demand and replays faulted GPU accesses.

---

## Answer

```c
#include <linux/mm.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/io.h>

#define FAULT_ADDR_REG    0x500   /* MMIO: GPU faulted virtual address    */
#define FAULT_TYPE_REG    0x504   /* MMIO: fault type (read/write/atomic) */
#define FAULT_CTX_REG     0x508   /* MMIO: faulting GPU context ID        */
#define FAULT_STATUS_REG  0x50C   /* MMIO: fault status / clear register  */
#define FAULT_REPLAY_REG  0x510   /* MMIO: write 1 to replay faulted inst */
#define FAULT_CLEAR_BIT   BIT(0)
#define FAULT_REPLAY_BIT  BIT(0)

/* ─── GPU Fault Types ─────────────────────────────────────────────────────*/
#define GPU_FAULT_READ    0
#define GPU_FAULT_WRITE   1
#define GPU_FAULT_ATOMIC  2

/* ─── Per-fault work item ─────────────────────────────────────────────────*/
struct gpu_fault_work {
    struct work_struct  work;
    u64                 fault_addr;   /* GPU VA that faulted            */
    u32                 fault_type;   /* read/write/atomic              */
    u32                 ctx_id;       /* GPU context that faulted       */
    struct gpu_device  *gpu;
};

/* ─── GPU GMMU mapping helper ─────────────────────────────────────────────
 * Maps a physical page into the GPU's GMMU page tables at fault_va.
 */
static int gpu_gmmu_map_page(struct gpu_device *gpu,
                              u64 fault_va, struct page *page,
                              u32 ctx_id, u32 prot)
{
    phys_addr_t phys = page_to_phys(page);

    /* Write to GPU GMMU page table update registers (device-specific) */
    /* In real drivers: insert PTE into multi-level GMMU page table */
    writel(upper_32_bits(fault_va),   gpu->regs + GMMU_PTE_HI_REG);
    writel(lower_32_bits(fault_va),   gpu->regs + GMMU_PTE_LO_REG);
    writel(upper_32_bits(phys),       gpu->regs + GMMU_PA_HI_REG);
    writel(lower_32_bits(phys) | prot, gpu->regs + GMMU_PA_LO_REG);
    writel(ctx_id,                    gpu->regs + GMMU_CTX_REG);
    writel(GMMU_UPDATE_CMD,           gpu->regs + GMMU_CTRL_REG);

    /* Flush GMMU TLB for this VA */
    writel(GMMU_TLB_FLUSH,            gpu->regs + GMMU_TLB_REG);

    /* Wait for GMMU update to complete */
    if (readl_poll_timeout(gpu->regs + GMMU_STATUS_REG,
                            val, val & GMMU_IDLE, 10, 100000)) {
        pr_err("GPU: GMMU page table update timeout\n");
        return -EIO;
    }

    return 0;
}

/* ─── Fault worker: runs in kernel thread context ─────────────────────────
 * Deferred from ISR to allow memory allocation (GFP_KERNEL).
 */
static void gpu_fault_worker(struct work_struct *work)
{
    struct gpu_fault_work *fw =
        container_of(work, struct gpu_fault_work, work);
    struct gpu_device *gpu = fw->gpu;
    struct page *page;
    u32 prot;
    int ret;

    pr_debug("GPU: fault at VA=0x%llx type=%u ctx=%u\n",
             fw->fault_addr, fw->fault_type, fw->ctx_id);

    /* ── Allocate a physical page to back the faulted VA ──────────────── */
    page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
    if (!page) {
        pr_err("GPU: OOM — cannot service fault at VA=0x%llx\n",
               fw->fault_addr);
        /* Signal the faulted context with OOM error */
        gpu_ctx_signal_oom(gpu, fw->ctx_id);
        goto out;
    }

    /* ── Determine page protection flags ──────────────────────────────── */
    prot = GMMU_PTE_VALID;
    if (fw->fault_type == GPU_FAULT_WRITE ||
        fw->fault_type == GPU_FAULT_ATOMIC)
        prot |= GMMU_PTE_WRITE;
    if (fw->fault_type != GPU_FAULT_READ)
        prot |= GMMU_PTE_READ; /* read+write */

    /* ── Map the page into GPU GMMU at the faulted address ────────────── */
    ret = gpu_gmmu_map_page(gpu, fw->fault_addr & PAGE_MASK,
                             page, fw->ctx_id, prot);
    if (ret) {
        __free_page(page);
        gpu_ctx_signal_fault_error(gpu, fw->ctx_id);
        goto out;
    }

    /* ── Replay: tell GPU to re-execute the faulted instruction ─────────
     * Without replay, the GPU thread that faulted is stuck forever.
     * Writing to FAULT_REPLAY_REG restarts the faulted warp from the
     * instruction that caused the fault.
     */
    writel(FAULT_REPLAY_BIT, gpu->regs + FAULT_REPLAY_REG);

out:
    kfree(fw);
}

/* ─── GPU Fault ISR: called from GPU interrupt handler ───────────────────
 * Runs in interrupt context — CANNOT allocate memory, CANNOT sleep.
 * Reads fault info from MMIO, queues work to handle it in process context.
 */
irqreturn_t gpu_fault_isr(int irq, void *dev_id)
{
    struct gpu_device *gpu = dev_id;
    struct gpu_fault_work *fw;
    u32 status;

    /* Read fault status register */
    status = readl(gpu->regs + FAULT_STATUS_REG);
    if (!(status & GPU_FAULT_PENDING))
        return IRQ_NONE;

    /* Allocate work item — must use GFP_ATOMIC in IRQ context */
    fw = kmalloc(sizeof(*fw), GFP_ATOMIC);
    if (!fw) {
        pr_err_ratelimited("GPU: failed to allocate fault work item\n");
        /* Acknowledge fault to re-enable interrupt, but drop the fault */
        writel(FAULT_CLEAR_BIT, gpu->regs + FAULT_STATUS_REG);
        return IRQ_HANDLED;
    }

    /* Read fault details from GPU MMIO registers (while interrupt is active) */
    fw->fault_addr = ((u64)readl(gpu->regs + FAULT_ADDR_REG + 4) << 32) |
                      readl(gpu->regs + FAULT_ADDR_REG);
    fw->fault_type = readl(gpu->regs + FAULT_TYPE_REG);
    fw->ctx_id     = readl(gpu->regs + FAULT_CTX_REG);
    fw->gpu        = gpu;

    INIT_WORK(&fw->work, gpu_fault_worker);

    /* Acknowledge the interrupt (allow GPU to continue queuing faults) */
    writel(FAULT_CLEAR_BIT, gpu->regs + FAULT_STATUS_REG);

    /* Queue work to handle fault in process context (can sleep/alloc) */
    queue_work(gpu->fault_wq, &fw->work);

    return IRQ_HANDLED;
}
```

---

## Explanation

### Core Concept

GPU demand paging mirrors CPU demand paging (`do_page_fault`):

```
GPU warp accesses unmapped VA:
                │
                ▼
    GPU GMMU fault interrupt fires
                │
                ▼
    gpu_fault_isr (IRQ context)
    ─ reads fault VA/type from MMIO
    ─ acknowledges interrupt
    ─ queues fault_worker
                │
                ▼
    gpu_fault_worker (process context)
    ─ alloc_page (can sleep)
    ─ gpu_gmmu_map_page (update GMMU PTE)
    ─ FAULT_REPLAY_REG = 1
                │
                ▼
    GPU warp re-executes faulted instruction → succeeds
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `alloc_page(gfp)` | Allocate one physical page |
| `page_to_phys(page)` | Get physical address of `struct page` |
| `__free_page(page)` | Free an allocated page |
| `readl(addr)` | Read 32-bit MMIO register |
| `writel(val, addr)` | Write 32-bit MMIO register |
| `readl_poll_timeout(addr, var, cond, sleep, timeout)` | Poll register with timeout |
| `INIT_WORK(work, fn)` | Initialize work item |
| `queue_work(wq, work)` | Submit work to workqueue |
| `container_of(ptr, type, member)` | Get parent struct from member |
| `GFP_ATOMIC` | Non-sleeping allocation for IRQ context |
| `pr_err_ratelimited(...)` | Rate-limited error log |

### Trade-offs & Pitfalls

- **Race: two faults for the same page.** Two GPU warps can fault on the same VA simultaneously. The second fault worker must check if the page is already mapped before allocating a new one. Use a per-VA-range lock or `atomic_cmpxchg` on the GMMU PTE.
- **Fault replay must be written after GMMU update completes.** If replay is triggered before the GMMU TLB flush completes, the GPU re-executes the instruction and faults again. Always wait for `GMMU_IDLE` before writing to `FAULT_REPLAY_REG`.
- **ISR must acknowledge before queuing work.** If the ISR acknowledges the fault interrupt after queuing work, another fault on the same address can arrive before the worker runs — potentially overwriting the first fault info. Acknowledge immediately.

### NVIDIA / GPU Context

NVIDIA UVM (Unified Virtual Memory) is built on GPU fault handling:
- `cudaMallocManaged` allocates VA range but no physical pages
- First GPU access to any page in the range triggers a GMMU fault
- `nvUvmGpuFaultHandler` → allocates page → updates GMMU PTE → replays
- Subsequent accesses to the same page are fast (TLB hit)
- Page migration between GPU VRAM and system DRAM uses the same fault mechanism

---

## Cross Questions & Answers

**CQ1: How does the GPU fault handler handle access control violations (not just missing mappings)?**
> Access control faults (write to read-only page, no-execute violation) are signaled with a different `fault_type` value in `FAULT_TYPE_REG`. The fault worker reads the existing GMMU PTE for the faulted VA. If the page is mapped but the access mode (R/W/X) is incorrect, the worker checks if the GPU context has permission to upgrade (e.g., read→write). If not, it signals the GPU context with `SIGBUS` equivalent, terminates the context, and does NOT replay. This mirrors the CPU MMU's segfault handling.

**CQ2: What is fault coalescing and why is it important for GPU page fault performance?**
> When a GPU warp accesses 32 threads × 128 bytes = 4KB of data, all 32 threads may fault simultaneously on adjacent addresses. Without coalescing, this generates 32 separate fault interrupts and 32 separate page allocations. Fault coalescing batches all faults from the same "fault group" (same page, same time window) into a single handler call, allocates the page once, and replays all warps together. NVIDIA's fault buffer collects multiple fault entries before firing the interrupt, enabling batch processing in the fault worker.

**CQ3: How do you handle the case where `alloc_page` fails in the fault worker (OOM)?**
> Options: (1) Invoke the GPU shrinker (Q16) to reclaim cached GPU pages and retry, (2) Evict a less-recently-used page from another GPU context's mapping (steal-on-fault), (3) Signal the faulted GPU context with an error (CUDA `cudaErrorMemoryAllocation`) and terminate it. The choice depends on the use case: interactive workload → evict, batch compute → fail fast. NVIDIA UVM defaults to retry with eviction before failing.

**CQ4: How does the fault handler interact with GPU context teardown?**
> When a GPU context is destroyed (user calls `cuCtxDestroy`), the driver must: (1) stop accepting new fault workers for that context, (2) wait for in-flight fault workers to complete (use a `completion` or `kthread_flush_work`), (3) then destroy the GMMU page tables. Without synchronization, a fault worker could write a GMMU PTE for a context that's being destroyed — a use-after-free. Use a `ctx_refcount` or RCU to ensure the context outlives all pending fault workers.

**CQ5: What is ATS (Address Translation Services) and how does it change the fault model?**
> With ATS (PCIe Address Translation Services), the GPU can cache CPU page table entries in its own TLB. Instead of the GPU faulting on unmapped addresses and the driver allocating pages, the GPU queries the CPU MMU directly via PCIe ATC requests. The CPU MMU responds with the physical address (or "no mapping" for faults). This eliminates most GPU page faults for UVM memory — the GPU accesses user pages directly without driver intervention. Faults only occur for truly unmapped ranges, not merely for pages not yet cached in GPU TLB.

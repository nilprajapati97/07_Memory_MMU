# Q37: NUMA-Aware GPU Memory Allocation

**Section:** System Design | **Difficulty:** Hard | **Topics:** NUMA, `kmalloc_node`, `alloc_pages_node`, `dev_to_node`, PCIe topology, CPU affinity, `set_mempolicy`

---

## Question

Implement NUMA-aware memory allocation for a GPU driver to minimize PCIe latency.

---

## Answer

```c
#include <linux/numa.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/topology.h>
#include <linux/pci.h>

/* ─── NUMA Topology for GPU Driver ───────────────────────────────────────
 *
 *  NUMA Node 0:  CPU0–CPU15,  DRAM 0 (128GB),  GPU0 (PCIe)
 *  NUMA Node 1:  CPU16–CPU31, DRAM 1 (128GB),  GPU1 (PCIe)
 *
 *  Accessing GPU0 from CPU16 (Node 1) traverses the QPI/UPI interconnect.
 *  Accessing GPU0 from CPU0  (Node 0) uses the local PCIe root complex.
 *  Latency difference: ~3× for cross-NUMA PCIe DMA vs local-NUMA.
 */

/* ─── GPU device with NUMA node binding ───────────────────────────────────*/
struct gpu_numa_context {
    struct pci_dev  *pdev;
    int              numa_node;   /* NUMA node closest to this GPU */
    struct device   *dev;
};

/* ─── Query GPU's NUMA node from PCIe topology ────────────────────────────*/
int gpu_get_numa_node(struct pci_dev *pdev)
{
    int node;

    /*
     * dev_to_node(&pdev->dev): queries the NUMA node for the PCIe device.
     * This uses the PCIe topology: which CPU socket/root complex the GPU
     * is connected to. Returns NUMA_NO_NODE if not known.
     *
     * Internally reads /sys/bus/pci/devices/<BDF>/numa_node or the
     * ACPI/SRAT table for PCIe device NUMA affinity.
     */
    node = dev_to_node(&pdev->dev);

    if (node == NUMA_NO_NODE) {
        /*
         * Fallback: use pcibus_to_node() — derives NUMA node from
         * the PCIe root complex's NUMA association.
         */
        node = pcibus_to_node(pdev->bus);
    }

    if (node == NUMA_NO_NODE) {
        pr_warn("GPU: cannot determine NUMA node, using node 0\n");
        node = 0;
    }

    pr_info("GPU %s: NUMA node = %d\n", pci_name(pdev), node);
    return node;
}

/* ─── NUMA-aware metadata allocation ─────────────────────────────────────*/
struct gpu_channel *gpu_channel_alloc_numa(struct gpu_numa_context *ctx)
{
    struct gpu_channel *chan;

    /*
     * kmalloc_node: allocate from the specified NUMA node's memory.
     * Equivalent to kmalloc but with NUMA locality guarantee.
     * The channel struct (used by CPU to submit commands) lives on
     * the same NUMA node as the GPU — minimizes PCIe latency.
     */
    chan = kmalloc_node(sizeof(*chan), GFP_KERNEL | __GFP_NOWARN,
                         ctx->numa_node);
    if (!chan) {
        /* Fallback: allow any node if preferred node is out of memory */
        chan = kmalloc(sizeof(*chan), GFP_KERNEL);
        if (!chan)
            return ERR_PTR(-ENOMEM);
        pr_warn("GPU: channel allocated on non-local NUMA node\n");
    }
    return chan;
}

/* ─── NUMA-aware DMA buffer allocation ────────────────────────────────────
 * Allocates pages pinned to the GPU's NUMA node for minimum PCIe latency.
 */
struct page **gpu_alloc_dma_pages_numa(struct gpu_numa_context *ctx,
                                        size_t num_pages)
{
    struct page **pages;
    size_t i;

    pages = kvmalloc_array(num_pages, sizeof(*pages), GFP_KERNEL);
    if (!pages)
        return NULL;

    for (i = 0; i < num_pages; i++) {
        /*
         * alloc_pages_node: allocate a page from the specified NUMA node.
         * __GFP_THISNODE: strictly allocate from this node — no fallback.
         * Without __GFP_THISNODE: kernel may fall back to remote nodes.
         * order=0: single page allocation.
         */
        pages[i] = alloc_pages_node(ctx->numa_node,
                                     GFP_KERNEL | __GFP_THISNODE,
                                     0 /* order = single page */);
        if (!pages[i]) {
            /* Fallback allocation without node restriction */
            pages[i] = alloc_page(GFP_KERNEL);
            if (!pages[i])
                goto err_free_pages;
            pr_debug("GPU: page %zu allocated from non-local node\n", i);
        }
    }
    return pages;

err_free_pages:
    while (i--)
        __free_page(pages[i]);
    kvfree(pages);
    return NULL;
}

/* ─── NUMA-aware interrupt affinity ─────────────────────────────────────
 * Pin GPU IRQ to CPUs on the same NUMA node as the GPU.
 */
int gpu_set_irq_affinity_numa(struct gpu_numa_context *ctx, int irq)
{
    struct cpumask *node_cpus;
    int ret;

    /*
     * cpumask_of_node: returns the cpumask of all CPUs on a NUMA node.
     * By pinning the IRQ handler to local CPUs, we ensure:
     * 1. IRQ handler runs on a CPU with low-latency PCIe access to GPU
     * 2. IRQ handler data (struct) is in DRAM local to both CPU and GPU
     */
    node_cpus = (struct cpumask *)cpumask_of_node(ctx->numa_node);

    ret = irq_set_affinity_hint(irq, node_cpus);
    if (ret)
        pr_warn("GPU: failed to set IRQ %d NUMA affinity: %d\n", irq, ret);
    else
        pr_info("GPU: IRQ %d pinned to NUMA node %d CPUs\n",
                irq, ctx->numa_node);
    return ret;
}

/* ─── NUMA-aware per-CPU workqueue ───────────────────────────────────────*/
struct workqueue_struct *gpu_create_numa_wq(struct gpu_numa_context *ctx,
                                              const char *name)
{
    /*
     * alloc_workqueue with WQ_NUMA: creates a workqueue where work items
     * prefer to run on CPUs belonging to the NUMA node where they were queued.
     * For GPU driver: queue from CPU on same node as GPU → work runs locally.
     */
    return alloc_workqueue(name,
                            WQ_UNBOUND | WQ_NUMA | WQ_HIGHPRI,
                            0 /* max_active: 0 = default */);
}

/* ─── Full NUMA context initialization ───────────────────────────────────*/
int gpu_numa_context_init(struct gpu_numa_context *ctx,
                            struct pci_dev *pdev)
{
    ctx->pdev      = pdev;
    ctx->dev       = &pdev->dev;
    ctx->numa_node = gpu_get_numa_node(pdev);
    return 0;
}

/* ─── NUMA statistics for a GPU allocation ───────────────────────────────*/
void gpu_print_numa_stats(struct gpu_numa_context *ctx,
                            struct page **pages, size_t num_pages)
{
    size_t local = 0, remote = 0;

    for (size_t i = 0; i < num_pages; i++) {
        if (page_to_nid(pages[i]) == ctx->numa_node)
            local++;
        else
            remote++;
    }

    pr_info("GPU NUMA: %zu/%zu pages on local node %d, %zu remote\n",
            local, num_pages, ctx->numa_node, remote);
}
```

---

## Explanation

### Core Concept

```
  ┌─────────────────────┐      ┌─────────────────────┐
  │    NUMA Node 0      │      │    NUMA Node 1      │
  │                     │      │                     │
  │  CPU0–15  DRAM0     │      │  CPU16–31  DRAM1    │
  │       │             │      │        │             │
  │  PCIe Root Complex 0│      │  PCIe Root Complex 1│
  │       │             │      │        │             │
  └───────┼─────────────┘      └────────┼─────────────┘
          │    ◄── QPI/UPI ──►          │
          ▼                             ▼
        GPU0                          GPU1
  (DRAM0 access: fast)          (DRAM1 access: fast)
  (DRAM1 access: ~3× slower)    (DRAM0 access: ~3× slower)
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `dev_to_node(&pdev->dev)` | Get NUMA node for a PCIe device |
| `pcibus_to_node(pdev->bus)` | Get NUMA node from PCIe bus |
| `kmalloc_node(size, gfp, node)` | Allocate kernel memory from NUMA node |
| `alloc_pages_node(node, gfp, order)` | Allocate pages from NUMA node |
| `__GFP_THISNODE` | Strict: fail if requested node has no memory |
| `cpumask_of_node(node)` | CPUs on a specific NUMA node |
| `irq_set_affinity_hint(irq, mask)` | Pin IRQ to CPU set |
| `page_to_nid(page)` | Get NUMA node of a page |
| `WQ_NUMA` | NUMA-aware workqueue flag |
| `alloc_workqueue(name, flags, max)` | Create NUMA-aware workqueue |

### Trade-offs & Pitfalls

- **`__GFP_THISNODE` can fail.** If the local NUMA node is memory-pressured, `alloc_pages_node` with `__GFP_THISNODE` returns NULL rather than falling back to remote nodes. Always have a fallback path without `__GFP_THISNODE`.
- **GPU NUMA node may be -1 on old BIOSes.** If ACPI tables don't encode PCIe NUMA affinity, `dev_to_node` returns `NUMA_NO_NODE`. In this case, use PCIe bus topology (which root complex port the GPU is behind) to infer the local CPU socket.

### NVIDIA / GPU Context

NVIDIA's CUDA runtime calls `numa_node_of_cpu(cudaGetDevice())` to determine local NUMA node before allocating pinned memory. `cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync)` on multi-socket systems includes NUMA-aware memory allocation. `numactl --cpunodebind=0 --membind=0 ./cuda_app` — pin a CUDA application to NUMA node 0 for predictable performance.

---

## Cross Questions & Answers

**CQ1: What is the performance impact of non-NUMA-aligned GPU driver buffers?**
> DMA buffers (command rings, descriptor tables, interrupt status pages) accessed by both CPU and GPU. If the buffer is on NUMA node 1 but the GPU is on node 0: (1) GPU DMA reads traverse QPI/UPI to reach remote DRAM — adds ~80ns latency per cache line. (2) CPU writes to command ring are also remote — adds ~80ns. For a 1M GPU commands/second rate: 80ns × 1M = 80ms of extra latency per second. Benchmarks show 15–30% throughput reduction for cross-NUMA GPU driver buffers.

**CQ2: How does `__GFP_THISNODE` differ from `__GFP_PREFERRED_MOVABLE` in NUMA allocation?**
> `__GFP_THISNODE`: strictly allocate from the requested node. If that node is full, return NULL (fail). No cross-node fallback. `__GFP_PREFERRED_MOVABLE`: hint to prefer the node but allow cross-node fallback using movable pages (which can be migrated back to the preferred node later). For GPU DMA buffers: use `__GFP_THISNODE` if strict locality is required (performance-critical paths). Use without it when GPU DMA buffers must always succeed even under memory pressure.

**CQ3: What is `cpumask_of_node` and why is IRQ affinity important for GPU performance?**
> `cpumask_of_node(n)` returns a `cpumask_t` of all CPUs on NUMA node n. IRQ affinity pins the GPU interrupt (completion, fault, ECC interrupt) handler to CPUs on the same NUMA node as the GPU. Benefit: the interrupt handler accesses GPU memory-mapped registers and driver data structures — if these are on the same NUMA node as the CPU running the handler, L3 cache hits are more likely and memory access latency is lower. Misaligned IRQ affinity can add 100–200ns to interrupt latency.

**CQ4: How do you handle GPU NUMA topology in multi-socket systems with NVLink?**
> NVLink creates a new "peer distance" dimension: GPU-to-GPU via NVLink may be faster than GPU-to-local-DRAM via PCIe. `cudaDeviceGetP2PAttribute(result, cudaDevP2PAttrPerformanceRank, dev0, dev1)` returns a performance rank for GPU pairs. For driver data placement: (1) always use the CPU-local NUMA node for CPU-accessed buffers, (2) use the GPU's local VRAM for GPU-only buffers, (3) for GPU-to-GPU shared buffers: place on the NUMA node with the best aggregate bandwidth to both GPUs.

**CQ5: What is `move_pages()` and can it be used to fix misaligned GPU driver memory?**
> `move_pages()` is a Linux syscall that migrates physical pages to a specified NUMA node while preserving virtual address mappings. Used by the `numactl` tool. For GPU driver allocations: kernel code can use `migrate_pages()` (kernel equivalent) to migrate misaligned DMA buffers to the GPU's local NUMA node after initial allocation. However, DMA-mapped buffers cannot be migrated while DMA is in flight — must `dma_unmap`, migrate, then `dma_map` again. This is expensive and typically only done during driver initialization, not at runtime.





ARM / ARM64 Linux Kernel
Memory Management
PART 4
DMA Coherency & Cache Management on ARM64
Complete Technical Reference for Kernel Developers
ARM64 (AArch64)  •  Qualcomm SoC  •  Linux Kernel 6.x  •  SMMU v2/v3



# Table of Contents
1.  The Core Problem: CPU Cache vs DMA Engine
1.1  What Happens Without Coherency
1.2  Two Fundamental Approaches
2.  ARM64 Cache Hierarchy & Architecture
2.1  Cache Levels on Qualcomm SoC
2.2  Cache Line Size and Alignment
3.  Cache Maintenance Assembly Instructions
3.1  DC CVAC / CIVAC / IVAC
3.2  DSB / DMB / ISB Barriers
3.3  In Linux Kernel C Code
4.  Hardware Coherent vs Non-Coherent Approaches
4.1  HW Coherent (CCI/CMN Interconnect)
4.2  Non-Coherent (Software Cache Maintenance)
5.  Complete Linux DMA API
5.1  Coherent DMA Mapping
5.2  Streaming DMA Mapping
5.3  Scatter-Gather DMA
5.4  Sync Operations (Partial Sync)
5.5  DMA Pools for Small Allocations
6.  Memory Types and MAIR_EL1
6.1  Memory Type Attributes
6.2  ioremap Variants
7.  SMMU/IOMMU Architecture
7.1  What Is an IOMMU/SMMU?
7.2  SMMU Architecture Diagram
7.3  Device Tree Bindings
7.4  SMMU Driver Interaction
7.5  SMMU Fault Handling
8.  DMA Coherency Decision Tree
9.  Five Practical Rules with Code
10.  SMMUv3 and SVA (Shared Virtual Addressing)
11.  Debugging DMA and Cache Issues
12.  Reference Tables & Summary

# 1. The Core Problem: CPU Cache vs DMA Engine
Modern ARM64 SoCs have multi-level caches (L1/L2 per core, L3 shared). The CPU reads/writes go through these caches. But a DMA engine (camera ISP, GPU, codec, network controller) accesses physical memory directly — it bypasses the CPU cache entirely.

This creates a cache coherency problem that every driver developer working on ARM64 SoCs must understand and handle correctly.
## 1.1 What Happens Without Coherency
Scenario A: CPU writes data, DMA reads it

CPU Core
  |
  v
[L1 Cache] -> [L2 Cache] -> [L3 Cache] -> [DRAM]
                                           ^
                                        DMA Engine reads here

Problem:
  CPU wrote to cache -> data is in L1/L2 but NOT yet in DRAM
  DMA reads DRAM     -> reads STALE/OLD data!

Scenario B: DMA writes data, CPU reads it

DMA Engine writes to DRAM
  CPU reads -> hits L1 cache -> reads OLD cached data!
  (Cache has stale copy from before DMA wrote)
## 1.2 Two Fundamental Approaches
| Approach | Mechanism | Driver Impact |
| HW Coherent | CCI/CMN interconnect snoops CPU caches automatically | No explicit cache ops needed; dma_alloc_coherent() is fully safe |
| Non-Coherent | DMA bypasses cache entirely; software must maintain coherency | Must use DMA streaming API with correct direction flags for every transfer |


⚠️  KEY PRINCIPLE: The DMA API abstracts both approaches. Always use the DMA API — never perform raw physical address DMA without it.
# 2. ARM64 Cache Hierarchy & Architecture
## 2.1 Cache Levels on Qualcomm SoC
ARM64 Cache Hierarchy (typical Qualcomm SoC):

Core 0                    Core 1
+------------------+       +------------------+
| L1-I$ (64KB)     |       | L1-I$ (64KB)     |
| L1-D$ (64KB)     |       | L1-D$ (64KB)     |
| L2$  (512KB)     |       | L2$  (512KB)     |
+--------+---------+       +--------+---------+
         |                          |
         +----------+  +-----------+
                    |  |
              +-----+--+------+
              |  L3$ (8MB shared) |
              +------------------+
                    |
              +-----+-------+
              | System Cache |  (LLC / DSU)
              +-----+-------+
                    |
                 [DRAM]

Each cache level has different characteristics:
| Level | Typical Size | Latency | Per-Core? | Notes |
| L1-D$ | 32–64 KB | ~4 cycles | Yes | Write-back, write-allocate |
| L1-I$ | 32–64 KB | ~4 cycles | Yes | Instruction cache only |
| L2$ | 256 KB–1 MB | ~12 cycles | Yes | Unified; PIPT |
| L3$ | 4–16 MB | ~30 cycles | Shared | Shared across cluster (DSU) |
| DRAM | ≥ 4 GB | ~100+ cycles | Shared | DMA engines access this directly |

## 2.2 Cache Line Size and Alignment
On ARM64, the cache line size is typically 64 bytes (verified via CTR_EL0 register). This is critical for DMA buffer alignment.

// Get cache line size in kernel
unsigned int cls = cache_line_size();  // typically returns 64

// Check CTR_EL0 (Cache Type Register)
// Bits [19:16] = DminLine = log2(words in smallest D-cache line)
// Bits [3:0]   = IminLine = log2(words in smallest I-cache line)

⚠️  CRITICAL: DMA buffers must be cache-line aligned (64 bytes on ARM64). A buffer that shares a cache line with other kernel data risks cache corruption during DMA.
// WRONG: may share cache line with adjacent data
char buf[100];
dma_map_single(dev, buf, 100, DMA_FROM_DEVICE);  // BUG!

// RIGHT: cache-line aligned allocation
size_t aligned_size = ALIGN(100, dma_get_cache_alignment());
char *buf = kmalloc(aligned_size, GFP_KERNEL | GFP_DMA);
// Or use dma_alloc_coherent() which handles alignment automatically
# 3. Cache Maintenance Assembly Instructions
Understanding the underlying ARM64 cache maintenance instructions is essential for kernel developers, even though you rarely call them directly from drivers. The DMA API uses these internally.
## 3.1 DC (Data Cache) Operations
| Instruction | Full Name | Effect & Use Case |
| DC CVAC, Xn | Clean by VA to PoC | Write dirty cache line to DRAM. Use before DMA read (DMA_TO_DEVICE). |
| DC CIVAC, Xn | Clean+Invalidate by VA to PoC | Write back AND discard cache line. Most complete operation for cache management. |
| DC IVAC, Xn | Invalidate by VA to PoC | Discard cache line (no write-back!). Use after DMA write (DMA_FROM_DEVICE). WARNING: data loss if line is dirty. |
| DC CVAU, Xn | Clean by VA to PoU | Clean to Point of Unification (for I-cache / D-cache coherency). Used after patching code. |
| DC CVAP, Xn | Clean by VA to Persistence | For persistent memory (NVDIMM). Ensures data reaches persistent storage. |
| DC ZVA, Xn | Zero by VA | Zero an aligned block without reading from DRAM. Efficient memset() for large buffers. |

### Point of Coherency (PoC) vs Point of Unification (PoU)

PoU (Point of Unification) = where I-cache and D-cache are coherent
  Typically the L2 cache
  Use DC CVAU + IC IVAU for JIT / module loading

PoC (Point of Coherency) = where all observers (CPU, DMA) are coherent
  Typically main memory (DRAM)
  Use DC CVAC / CIVAC / IVAC for DMA operations
## 3.2 DSB / DMB / ISB — Memory Barriers
Cache maintenance instructions MUST be followed by memory barriers to ensure ordering and completion:

| Instruction | Name | Behavior & Use |
| DSB SY | Data Sync Barrier | Waits until ALL preceding memory accesses (including cache ops) complete. Required after DC* instructions before DMA starts. |
| DMB SY | Data Memory Barrier | Ensures ordering of memory accesses. Does not wait for completion like DSB. Use between CPU writes that must be seen in order. |
| ISB | Instruction Sync Barrier | Flushes pipeline. Use after changing system registers (TTBR, SCTLR, etc.) or after instruction cache maintenance. |
| DSB ISHST | DSB Inner Share Store | Lighter barrier: only stores, inner-shareable domain. Used before TLBI instructions. |
| DSB ISH | DSB Inner Shareable | All accesses, inner-shareable domain. Used after TLBI for TLB invalidation completion. |

## 3.3 Cache Maintenance in Linux Kernel C Code
Linux provides C wrappers for ARM64 barrier instructions:

/* arch/arm64/include/asm/barrier.h */

/* Full system barriers */
dsb(sy);      // DSB SY  - wait for all memory accesses
dmb(sy);      // DMB SY  - memory ordering
isb();        // ISB     - pipeline flush

/* Inner-shareable variants (for SMP) */
dsb(ish);     // DSB ISH
dsb(ishst);   // DSB ISHST
dmb(ish);     // DMB ISH

/* Example: Manual cache clean for DMA (don't do this in drivers!) */
/* Use DMA API instead - this is what it does internally:       */
static inline void arm64_sync_cache_range(void *addr, size_t size)
{
    void *end = addr + size;
    u64 cls = cache_line_size();

    /* Clean + Invalidate each cache line in the range */
    do {
        asm volatile("dc civac, %0" : : "r"(addr));
        addr += cls;
    } while (addr < end);

    dsb(sy);  /* Wait for all DC CIVAC to complete */
}

### Instruction vs Data Cache Coherency
/* After writing code (JIT compiler, eBPF) to memory: */
/* 1. Clean D-cache to PoU */
void flush_icache_range(unsigned long start, unsigned long end)
{
    /* Clean D-cache lines to PoU */
    do { asm("dc cvau, %0" : : "r"(start)); start += cls; }
    while (start < end);
    dsb(ish);

    /* Invalidate I-cache lines to PoU */
    do { asm("ic ivau, %0" : : "r"(start)); start += cls; }
    while (start < end);
    dsb(ish);
    isb();  /* Flush pipeline to see new instructions */
}
# 4. Hardware Coherent vs Non-Coherent Approaches
## 4.1 Hardware Coherent DMA (CCI/CMN Interconnect)
On ARM64 SoCs with a Cache Coherent Interconnect (CCI) or Coherent Mesh Network (CMN), DMA masters participate in the hardware coherency protocol. When a DMA engine reads memory, it can snoop the CPU cache directly.

ARM64 SoC with Hardware Coherency:

CPU Cluster A              CPU Cluster B
[Core0][Core1]             [Core2][Core3]
[  L1$  ][  L1$  ]         [  L1$  ][  L1$  ]
[    L2$ (1MB)    ]         [    L2$ (1MB)    ]
        |                           |
        +----------- CMN -----------+
                      |
              +-------+-------+
              |    L3$ (16MB) |
              +-------+-------+
                      |
           +----------+-----------+
           |  Coherent Interconnect|
           |  (CCI-550 / CMN-700) |
           +--+----------+-------++
              |          |       |
         [GPU DMA]  [Camera ISP] [GPU]
          (coherent master)
              |          |
              +----+-----+
                   |
                 [DRAM]

With hardware coherency: DMA reads snoop the CPU cache — if the data is dirty in L1, the cache line is forwarded directly to the DMA engine without needing a write-back to DRAM first.

### Detecting Hardware Coherency in Driver Code
/* Check if device is hardware-coherent at runtime */
if (dev_is_dma_coherent(dev)) {
    /* No explicit cache maintenance needed */
    /* dma_alloc_coherent() maps as cached */
    pr_info("Device %s is HW coherent
", dev_name(dev));
} else {
    /* Must use streaming DMA API with explicit sync */
    pr_info("Device %s requires SW cache management
",
            dev_name(dev));
}

### Device Tree: Marking a Device as DMA-Coherent
/* In Device Tree Source (.dts): */

/* Option 1: Mark entire SoC as coherent */
soc {
    dma-coherent;  /* All DMA masters are HW coherent */
};

/* Option 2: Mark individual device */
camera_isp: isp@1a00000 {
    compatible = "qcom,camera-isp";
    reg = <0x1a00000 0x10000>;
    dma-coherent;   /* this device is HW coherent */
};
## 4.2 Non-Coherent DMA (Software Cache Maintenance)
When the DMA master is not connected to the cache coherency fabric, software is responsible for ensuring the cache and memory are synchronized before/after each DMA transfer.

ARM64 SoC without full coherency:

CPU Cluster                Simple DMA Peripheral
[Core0][Core1]                 [DMA Engine]
[  L1$ | L1$  ]                    |
[   L2$ (512KB)  ]                 |
        |                           |
   [Bus Matrix] <----(no snoop)-----+
        |
     [DRAM]  <--- DMA reads/writes directly

Result: CPU cache and DRAM can be out of sync!

The DMA API handles this automatically when you use the correct direction flags:

| Direction Flag | When to Use | Cache Operation Performed |
| DMA_TO_DEVICE | CPU wrote data, DMA engine will READ it | DC CIVAC: Clean + Invalidate cache lines (write dirty data to DRAM) |
| DMA_FROM_DEVICE | DMA engine will WRITE data, CPU will READ it | DC IVAC: Invalidate cache lines (force CPU to re-fetch from DRAM after DMA writes) |
| DMA_BIDIRECTIONAL | Both CPU and DMA read/write (use sparingly) | DC CIVAC: Clean + Invalidate (safe for both directions, but more expensive) |


⚠️  IMPORTANT: Using the wrong direction flag is a common bug. DMA_TO_DEVICE then DMA_FROM_DEVICE — always specify what will actually happen next, not what happened before.
# 5. Complete Linux DMA API
The Linux DMA API provides a portable, architecture-agnostic interface for DMA buffer management. Under the hood, it calls the appropriate ARM64 cache maintenance instructions.
## 5.1 Coherent DMA Mapping
Coherent DMA memory is always coherent between CPU and DMA. No explicit cache operations are required before or after each DMA transfer. This is the simplest API but may have performance trade-offs.

### Allocation
/* Allocate coherent DMA memory */
void *cpu_addr = dma_alloc_coherent(
    dev,          /* struct device *  (your platform/PCI device) */
    size,         /* size in bytes                               */
    &dma_handle,  /* output: DMA (bus/IOVA) address for HW      */
    GFP_KERNEL    /* allocation flags                            */
);

/* cpu_addr  -> CPU virtual address (for kernel to read/write)  */
/* dma_handle -> physical/bus/IOVA address (program into DMA HW) */

if (!cpu_addr) {
    dev_err(dev, "Failed to allocate DMA coherent buffer
");
    return -ENOMEM;
}

### What Happens Internally on ARM64
/* On NON-hardware-coherent SoC: */
/*   Maps memory as MT_NORMAL_NC (Non-Cached)              */
/*   CPU writes bypass cache -> go directly to DRAM        */
/*   DMA reads DRAM -> always sees latest data             */
/*   Trade-off: CPU access is SLOW (no cache benefit)      */

/* On HARDWARE-coherent SoC: */
/*   Maps memory as MT_NORMAL (Cached)                     */
/*   Hardware coherency protocol ensures consistency       */
/*   CPU access is FAST (cache is used)                    */

### Freeing
dma_free_coherent(dev, size, cpu_addr, dma_handle);
/* Releases the DMA mapping and returns memory to the system */

### Write-Combining Variant
/* For framebuffers and GPU VRAM: CPU writes are batched */
void *cpu_addr = dma_alloc_wc(dev, size, &dma_handle, GFP_KERNEL);
dma_free_wc(dev, size, cpu_addr, dma_handle);

Use dma_alloc_wc() for framebuffers and memory where the CPU will write sequentially and read rarely. Write-combining batches CPU writes for better throughput to DRAM.
## 5.2 Streaming DMA Mapping
Streaming DMA is used for large data buffers (video frames, audio, network packets) where you want CPU cache benefits between DMA transfers. The cache is managed explicitly at map/unmap time.

### dma_map_single / dma_unmap_single
/* === DMA_TO_DEVICE: CPU wrote data, DMA engine will read === */
dma_addr_t dma_addr = dma_map_single(
    dev,               /* struct device *                 */
    cpu_ptr,           /* kernel virtual address          */
    size,              /* bytes to map                    */
    DMA_TO_DEVICE      /* direction                       */
);
/* Internally: DC CIVAC on all cache lines -> DSB            */
/* CPU cache lines written back to DRAM; DMA can now read   */

/* Always check for mapping error! */
if (dma_mapping_error(dev, dma_addr)) {
    dev_err(dev, "DMA mapping failed
");
    return -ENOMEM;
}

/* Program dma_addr into hardware DMA controller register */
writel(lower_32_bits(dma_addr), base + DMA_ADDR_LO_REG);
writel(upper_32_bits(dma_addr), base + DMA_ADDR_HI_REG);
writel(size, base + DMA_SIZE_REG);
writel(DMA_START, base + DMA_CTRL_REG);

/* Wait for DMA completion (interrupt, polling, etc.) */
wait_for_completion(&dma_done);

/* Unmap: now safe to access cpu_ptr again */
dma_unmap_single(dev, dma_addr, size, DMA_TO_DEVICE);

/* === DMA_FROM_DEVICE: DMA writes, CPU reads === */
dma_addr_t dma_addr = dma_map_single(
    dev, cpu_ptr, size, DMA_FROM_DEVICE
);
/* Internally: DC IVAC on all cache lines -> DSB             */
/* CPU cache lines invalidated; after DMA writes, CPU will  */
/* fetch fresh data from DRAM on next access                */

start_dma_receive();       /* Start DMA transfer */
wait_for_completion(&rx_done);

/* Unmap before CPU reads: ensures cache is invalidated */
dma_unmap_single(dev, dma_addr, size, DMA_FROM_DEVICE);

/* NOW safe to read cpu_ptr: cache is clean */
process_received_data(cpu_ptr, size);

⚠️  RULE: Between dma_map_single() and dma_unmap_single(), the buffer BELONGS to the DMA engine. Do NOT access it from the CPU during this window!
## 5.3 Scatter-Gather DMA
Scatter-gather is used when the data buffer is not physically contiguous. The DMA controller (or SMMU) handles the non-contiguous segments. Common in network drivers, storage drivers, and video pipelines.

#include <linux/scatterlist.h>

#define NUM_ENTRIES 4
struct scatterlist sg[NUM_ENTRIES];

/* Initialize scatter-gather list */
sg_init_table(sg, NUM_ENTRIES);

/* Set entries: each points to a CPU buffer */
sg_set_buf(&sg[0], buf0, len0);  /* first fragment  */
sg_set_buf(&sg[1], buf1, len1);  /* second fragment */
sg_set_buf(&sg[2], buf2, len2);  /* third fragment  */
sg_set_buf(&sg[3], buf3, len3);  /* fourth fragment */

/* Map entire scatter-gather list for DMA */
int nents = dma_map_sg(dev, sg, NUM_ENTRIES, DMA_TO_DEVICE);
if (!nents) {
    dev_err(dev, "scatter-gather DMA mapping failed
");
    return -ENOMEM;
}
/* Note: nents may be < NUM_ENTRIES if adjacent pages are merged */

/* Iterate mapped entries and program descriptor ring */
struct scatterlist *s;
int i;
for_each_sg(sg, s, nents, i) {
    dma_addr_t addr = sg_dma_address(s);  /* IOVA/DMA address */
    unsigned int len = sg_dma_len(s);     /* length           */
    /* Program addr + len into DMA descriptor ring           */
    tx_desc[i].addr = cpu_to_le64(addr);
    tx_desc[i].len  = cpu_to_le32(len);
}

start_dma_transfer();
wait_for_completion(&dma_done);

/* Unmap the scatter-gather list */
dma_unmap_sg(dev, sg, NUM_ENTRIES, DMA_TO_DEVICE);

### Scatter-Gather with Allocated Pages
/* For dynamically allocated page buffers: */
struct page *pages[NUM_PAGES];
struct scatterlist sg[NUM_PAGES];
sg_init_table(sg, NUM_PAGES);
for (i = 0; i < NUM_PAGES; i++) {
    pages[i] = alloc_page(GFP_KERNEL);
    sg_set_page(&sg[i], pages[i], PAGE_SIZE, 0);
}
## 5.4 Sync Operations (Partial Sync Without Unmap)
When a DMA buffer needs to be accessed from both CPU and DMA alternately WITHOUT unmapping (e.g., ping-pong buffers, descriptor rings), use the sync operations:

/* === Sync for CPU access (after DMA wrote to buffer) === */
dma_sync_single_for_cpu(dev, dma_addr, size, DMA_FROM_DEVICE);
/* -> Invalidates CPU cache lines for this range            */
/* -> CPU will fetch fresh data from DRAM on next access   */
/* -> Buffer is now safe to READ from CPU                  */

process_data(cpu_ptr);  /* CPU reads the DMA-written data  */
prepare_next_buffer(cpu_ptr);  /* CPU writes new data      */

/* === Sync for device access (CPU wrote, DMA will read) === */
dma_sync_single_for_device(dev, dma_addr, size, DMA_TO_DEVICE);
/* -> Cleans CPU cache lines (writes dirty data to DRAM)   */
/* -> Buffer is now safe for DMA to READ                   */

start_next_dma_transfer();

### Ping-Pong Buffer Pattern
/* Classic ping-pong: DMA fills buffer A while CPU processes B */
struct my_buffer {
    void       *cpu_addr;
    dma_addr_t  dma_addr;
    size_t      size;
};

struct my_buffer bufs[2];
int active = 0;

/* In DMA completion interrupt: */
irqreturn_t dma_done_irq(int irq, void *data)
{
    int done = active;
    int next = 1 - active;

    /* Sync completed buffer for CPU access */
    dma_sync_single_for_cpu(dev, bufs[done].dma_addr,
                            bufs[done].size, DMA_FROM_DEVICE);

    /* Sync next buffer for device (CPU wrote descriptors) */
    dma_sync_single_for_device(dev, bufs[next].dma_addr,
                               bufs[next].size, DMA_TO_DEVICE);

    /* Start next DMA into next buffer */
    program_dma(bufs[next].dma_addr, bufs[next].size);
    active = next;

    /* Process completed buffer (can sleep in workqueue) */
    queue_work(wq, &bufs[done].work);
    return IRQ_HANDLED;
}

## 5.5 DMA Pools for Small Repeated Allocations
DMA pools are used for small, frequently allocated DMA-capable objects (e.g., DMA descriptor rings, USB transfer descriptors, command buffers). Each allocation in a pool has the same size and alignment.

#include <linux/dmapool.h>

/* Create a DMA pool at driver initialization */
struct dma_pool *pool = dma_pool_create(
    "my_desc_pool",            /* name (shown in /proc/dma_pools) */
    dev,                        /* struct device *                 */
    sizeof(struct my_desc),     /* object size                     */
    32,                         /* alignment (bytes, power of 2)   */
    0                           /* boundary (0 = no boundary)      */
);
if (!pool) {
    dev_err(dev, "Failed to create DMA pool
");
    return -ENOMEM;
}

/* Allocate a descriptor from the pool */
dma_addr_t dma_handle;
struct my_desc *desc = dma_pool_alloc(pool, GFP_KERNEL, &dma_handle);
if (!desc) {
    dev_err(dev, "DMA pool allocation failed
");
    return -ENOMEM;
}

/* Use: cpu side = desc, DMA side = dma_handle */
desc->control = CMD_FLAGS;
desc->src_addr = src_dma;
desc->dst_addr = dst_dma;
desc->length   = transfer_size;
/* program dma_handle into hardware ring */

/* Return descriptor to pool when done */
dma_pool_free(pool, desc, dma_handle);

/* Destroy pool at driver exit */
dma_pool_destroy(pool);

DMA pools are coherent mappings internally. Items in a pool are always coherent — no cache flushing needed. Ideal for small, frequently-used DMA objects like SDMA descriptors, USB TDs, network ring entries.
### DMA API Summary
| API | Coherency | Cache Ops | Use Case |
| dma_alloc_coherent() | Always coherent | Auto (non-cached on non-HW-coherent) | DMA control structs, descriptors |
| dma_map_single() | On map/unmap | Clean or Invalidate at map | Large data buffers, one-shot DMA |
| dma_map_sg() | On map/unmap | Clean or Invalidate per SG entry | Scatter-gather, non-contiguous data |
| dma_sync_single_for_cpu() | On sync call | Invalidate (FROM_DEVICE) | Ping-pong, streaming without unmap |
| dma_sync_single_for_device() | On sync call | Clean (TO_DEVICE) | Ping-pong, streaming without unmap |
| dma_pool_alloc() | Always coherent | None (coherent pool) | Small repeated DMA objects |

# 6. Memory Types and MAIR_EL1
ARM64 page table entries encode the memory type via a 3-bit AttrIndx field that indexes into the MAIR_EL1 (Memory Attribute Indirection Register). This controls cacheability, shareability, and ordering behavior.
## 6.1 Memory Type Attributes
/* MAIR_EL1 register: 8 slots x 8 bits = 64 bits total */
/* Linux ARM64 typical setup (arch/arm64/mm/proc.S):    */

Index 0: MT_DEVICE_nGnRnE   /* Strongly ordered MMIO    */
Index 1: MT_DEVICE_nGnRE    /* Device memory (PCIe)     */
Index 2: MT_DEVICE_GRE      /* Write-combining (FB)     */
Index 3: MT_NORMAL_NC        /* Non-cacheable normal     */
Index 4: MT_NORMAL           /* Normal cached WB/WA      */  <- most RAM
Index 5: MT_NORMAL_WT        /* Write-through            */
Index 6: MT_NORMAL_TAGGED    /* Tagged normal (MTE)      */
Index 7: (reserved)

| Memory Type | Cache | Ordering | Use Case |
| MT_NORMAL | WB/WA | Relaxed | Normal kernel/user RAM, kmalloc, vmalloc |
| MT_NORMAL_NC | None | Normal | Coherent DMA buffers (non-HW-coherent SoC) |
| MT_NORMAL_WT | Write-through | Normal | Rarely used, special cache behavior |
| MT_DEVICE_nGnRnE | None | Strictly ordered | MMIO registers, I/O ports (ioremap default) |
| MT_DEVICE_nGnRE | None | Device | PCIe MMIO regions (ioremap_np) |
| MT_DEVICE_GRE | None | Relaxed | Write-combining: framebuffers, GPU VRAM |
| MT_NORMAL_TAGGED | WB/WA | Normal | Memory Tagging Extension (MTE) for KASAN |


### Memory Attribute Bit Encoding
/* Each MAIR slot encodes 2 x 4-bit attributes: */
/* Bits [7:4] = outer (system cache) attributes  */
/* Bits [3:0] = inner (CPU cache) attributes     */

/* MT_NORMAL: 0xFF = 1111_1111                   */
/*   Outer: Write-Back, Read-Allocate, Write-Allocate */
/*   Inner: Write-Back, Read-Allocate, Write-Allocate */

/* MT_NORMAL_NC: 0x44 = 0100_0100               */
/*   Outer: Non-Cacheable                        */
/*   Inner: Non-Cacheable                        */

/* MT_DEVICE_nGnRnE: 0x00 = 0000_0000           */
/*   Outer: Device memory                        */
/*   Inner: nG (non-Gathering), nR (non-Reordering), nE (no Early write ack)*/

## 6.2 ioremap Variants
The ioremap() family of functions maps physical I/O memory into the kernel virtual address space with the appropriate memory type:

#include <linux/io.h>

/* Standard MMIO mapping (MT_DEVICE_nGnRE) */
/* Use for hardware registers, PCIe MMIO    */
void __iomem *base = ioremap(phys_addr, size);
if (!base) return -ENOMEM;

/* Access registers using accessor macros:  */
u32 val = readl(base + REG_STATUS);         /* 32-bit read  */
writel(0x1, base + REG_CTRL);               /* 32-bit write */
writew(0xFF, base + REG_BYTE);              /* 16-bit write */
u64 v64 = readq(base + REG_64BIT);          /* 64-bit read  */

/* Unmap when done (e.g., in remove()) */
iounmap(base);

/* Write-combining mapping (MT_NORMAL_NC or WC)       */
/* Use for framebuffers, GPU VRAM, write-heavy regions */
void __iomem *fb = ioremap_wc(phys_addr, size);
/* CPU writes are buffered/batched -> better DRAM throughput */
/* Do NOT use for control registers! (ordering not guaranteed) */

/* Cached mapping (MT_NORMAL) */
/* Rarely used for MMIO - only for SoC peripherals    */
/* that explicitly support cached CPU access           */
void __iomem *cached = ioremap_cache(phys_addr, size);

| Function | Memory Type | Cache | Use Case |
| ioremap() | MT_DEVICE_nGnRE | None | General MMIO, hardware registers |
| ioremap_np() | MT_DEVICE_nGnRnE | None | Strictly ordered (no prefetch/gather) |
| ioremap_wc() | MT_DEVICE_GRE | Write-comb. | Framebuffers, GPU VRAM, display memory |
| ioremap_cache() | MT_NORMAL | WB/WA | Cached peripheral access (unusual) |
| ioremap_uc() | MT_NORMAL_NC | None | Non-cached without device semantics |
| iounmap() | N/A | N/A | Unmap any ioremap() mapping |


⚠️  NEVER use readl()/writel() on ioremap_wc() regions — they expect ordered device memory. Use memcpy_toio() / memcpy_fromio() for write-combining regions instead.
# 7. SMMU/IOMMU Architecture
The SMMU (System Memory Management Unit) is ARM's IOMMU — an MMU for DMA masters. It translates device virtual addresses (IOVAs) to physical addresses, providing isolation and security for DMA operations.
## 7.1 What Is an IOMMU/SMMU?
Without SMMU (dangerous):
  DMA Engine -> PHYSICAL ADDRESS -> DRAM
  DMA can access ANY physical memory - security risk!

With SMMU (secure):
  DMA Engine -> IOVA -> [SMMU translates] -> PHYSICAL ADDRESS -> DRAM
  DMA can ONLY access IOVAs mapped in its SMMU page table.
  Each device has its own isolated address space (domain).

## 7.2 SMMU Architecture Diagram
ARM64 SoC with SMMU:

CPU Cluster          Camera ISP          GPU             PCIe NIC
[Core0][Core1]       [DMA Engine]    [DMA Engine]    [DMA Engine]
    |                    |               |               |
    |              [SMMU Stream 5] [SMMU Stream 12] [SMMU Stream 25]
    |              | IOVA->PA |     | IOVA->PA |    | IOVA->PA |
    |              | (Domain5)|     |(Domain12)|    |(Domain25)|
    |                    |               |               |
    +--------------------+---------------+---------------+
                         |
                  [System Bus / NoC]
                         |
                      [DRAM]

| Benefit | Description |
| Security | Compromised DMA master cannot access arbitrary physical memory. Each device is isolated in its own IOVA space. |
| IOVA Space | Devices see a flat 64-bit address space. Physical fragmentation is hidden from hardware. |
| HW Scatter-Gather | Non-contiguous physical pages appear as contiguous IOVA to the device. DMA descriptors become simpler. |
| Fault Isolation | SMMU faults are per-device. A DMA fault does not crash the system - it kills only the faulting device. |
| Virtualization | Two-stage translation (SMMUv3): Stage 1 (IOVA->IPA) + Stage 2 (IPA->PA). Enables secure VM DMA. |

## 7.3 Device Tree Bindings
/* ARM SMMU v2 Device Tree (Qualcomm SoC style) */
smmu: iommu@15000000 {
    compatible = "arm,smmu-v2";
    reg = <0x15000000 0x10000>;
    #iommu-cells = <1>;
    interrupts = <GIC_SPI 48 IRQ_TYPE_LEVEL_HIGH>;
    clocks = <&gcc SMMU_CLK>, <&gcc SMMU_AHB_CLK>;
    clock-names = "bus", "iface";
};

/* SMMUv3 (modern ARM64 SoCs) */
smmu_v3: iommu@30000000 {
    compatible = "arm,smmu-v3";
    reg = <0x30000000 0x20000>;
    #iommu-cells = <1>;
    interrupts = <GIC_SPI 60 IRQ_TYPE_EDGE_RISING>,
                 <GIC_SPI 61 IRQ_TYPE_EDGE_RISING>;
    msi-parent = <&its>;
};

/* Bind devices to SMMU streams */
camera_isp: isp@1a00000 {
    compatible = "qcom,camera-isp";
    reg = <0x1a00000 0x10000>;
    iommus = <&smmu 0x200>;   /* stream ID 0x200 */
};

gpu: gpu@1c00000 {
    compatible = "qcom,adreno-640";
    iommus = <&smmu 0x300>, <&smmu 0x301>;
    /* GPU has 2 streams: command + data */
};

## 7.4 SMMU Driver Interaction
When a driver calls DMA API functions, the kernel's DMA layer automatically programs the SMMU. The driver never directly touches SMMU registers.

/* When driver calls dma_map_single(): internally Linux: */
/* 1. Allocates an IOVA from the device IOVA allocator  */
/* 2. Creates SMMU page table entries: IOVA -> PA        */
/* 3. Performs cache maintenance (DC CIVAC + DSB)        */
/* 4. Returns IOVA to driver                             */

dma_addr_t iova = dma_map_single(dev, cpu_ptr, size, DMA_TO_DEVICE);
/* Program iova (NOT physical address) into hardware:    */
writel(lower_32_bits(iova), base + ISP_SRC_ADDR_LO);
writel(upper_32_bits(iova), base + ISP_SRC_ADDR_HI);

## 7.5 SMMU Fault Handling
/* Typical SMMU fault in dmesg: */
arm-smmu 15000000.iommu: Unhandled context fault
arm-smmu 15000000.iommu:   IOVA   = 0x12345678
arm-smmu 15000000.iommu:   FSYNR0 = 0x00000002
arm-smmu 15000000.iommu:   device = camera_isp (SID=0x200)

| Fault Cause | Symptom | Solution |
| Wrong address in HW register | Fault at 0 or a physical address | Use dma_handle, not virt_to_phys() |
| Buffer not mapped before DMA | Translation fault at valid-looking IOVA | Call dma_map_single() before programming HW |
| Buffer overflow past map size | Fault at IOVA just past end of buffer | Check size param in dma_map_single() |
| Using physical addr instead of IOVA | Translation fault at physical address | Never use virt_to_phys() for DMA on SMMU SoCs |

# 8. DMA Coherency Decision Tree
Use this decision tree to determine which DMA API and approach to use for any given situation:

Need to DMA to/from a buffer?
|
+-- Is the SoC hardware coherent?
|   (check DT: "dma-coherent" property on device or soc node)
|   |
|   +-- YES --> dma_alloc_coherent() OR dma_map_single()
|               SMMU handles address translation
|               Hardware maintains cache coherency automatically
|               No explicit cache maintenance needed
|
|   +-- NO --> continue below
|
+-- Is the buffer ALWAYS shared between CPU and DMA simultaneously?
|   (e.g., DMA descriptor ring both CPU and HW access at same time)
|   |
|   +-- YES --> dma_alloc_coherent()
|               Non-cached mapping on non-HW-coherent SoC
|               Always coherent, no explicit cache ops
|               Cost: CPU access is SLOW (no cache)
|
+-- Is the buffer LARGE (video frame, audio, network packet)?
|   (want CPU cache benefits between DMA transfers)
|   |
|   +-- YES + single contiguous buffer:
|   |         dma_map_single() before DMA starts
|   |         (CPU cannot touch buffer while mapped!)
|   |         dma_unmap_single() after DMA completes
|   |
|   +-- YES + need CPU access without unmap:
|             dma_sync_single_for_cpu() -> CPU reads
|             dma_sync_single_for_device() -> DMA reads
|
+-- Is the buffer physically NON-CONTIGUOUS?
|   (scattered pages, gather-list of buffers)
|   |
|   +-- YES --> dma_map_sg() / dma_unmap_sg()
|               SMMU presents as contiguous IOVA to device
|
+-- Is it a SMALL, FREQUENTLY ALLOCATED DMA object?
    (DMA descriptor, command buffer, USB TD)
    |
    +-- YES --> dma_pool_create() + dma_pool_alloc()
                Coherent, pre-allocated pool
                Very fast allocation from hot pool

| Situation | API to Use | Cache Behavior | CPU Speed |
| HW coherent SoC (dma-coherent DT) | Any DMA API | Handled by hardware | Fast |
| Control/descriptor buffer, always shared | dma_alloc_coherent() | Non-cached (non-HW-coh.) | Slow |
| Large data buffer, one-shot DMA | dma_map/unmap_single() | Clean/Invalidate at map | Fast |
| Ping-pong, repeated DMA + CPU | dma_sync_single_for_*() | Sync at each handoff | Fast |
| Non-contiguous pages/buffers | dma_map/unmap_sg() | Clean/Invalidate per SGL | Fast |
| Small frequent DMA objects | dma_pool_alloc() | Non-cached coherent pool | Very fast |

# 9. Five Practical Rules with Code
These five rules prevent the most common DMA coherency bugs in ARM64 driver development:
## Rule 1: Never Access a DMA Buffer from CPU While DMA Is Active
/* WRONG: CPU reads while DMA may be writing */
dma_map_single(dev, buf, size, DMA_FROM_DEVICE);
start_dma();
printk("value: %x
", buf[0]);  /* BUG! DMA writing, cache invalid! */

/* RIGHT: wait for DMA, then unmap before CPU access */
dma_addr_t dma_addr = dma_map_single(dev, buf, size, DMA_FROM_DEVICE);
if (dma_mapping_error(dev, dma_addr)) return -ENOMEM;
start_dma();
wait_for_completion(&dma_done);        /* wait for HW */
dma_unmap_single(dev, dma_addr, size, DMA_FROM_DEVICE);
printk("value: %x
", buf[0]);  /* SAFE: cache invalidated by unmap */

## Rule 2: Always Check dma_mapping_error()
/* WRONG: ignoring mapping failure */
dma_addr_t addr = dma_map_single(dev, buf, size, DMA_TO_DEVICE);
writel(addr, hw_reg);  /* BUG: addr may be DMA_MAPPING_ERROR! */

/* RIGHT: check for error before using the address */
dma_addr_t addr = dma_map_single(dev, buf, size, DMA_TO_DEVICE);
if (dma_mapping_error(dev, addr)) {
    dev_err(dev, "DMA mapping failed for buffer
");
    return -ENOMEM;
}
writel(lower_32_bits(addr), hw_reg_lo);
writel(upper_32_bits(addr), hw_reg_hi);

## Rule 3: Match map/unmap Directions Exactly
/* WRONG: using wrong direction on unmap */
dma_addr_t addr = dma_map_single(dev, buf, size, DMA_TO_DEVICE);
/* ... DMA reads buf ... */
dma_unmap_single(dev, addr, size, DMA_FROM_DEVICE); /* BUG! Direction mismatch */

/* RIGHT: use same direction for map and unmap */
dma_addr_t addr = dma_map_single(dev, buf, size, DMA_TO_DEVICE);
/* ... DMA reads buf ... */
dma_unmap_single(dev, addr, size, DMA_TO_DEVICE);  /* CORRECT */

## Rule 4: DMA Buffers Must Be Cache-Line Aligned
/* WRONG: unaligned buffer on non-HW-coherent SoC */
struct my_data {
    u8  header;           /* offset 0 */
    u32 dma_payload[16];  /* offset 1 - NOT cache-line aligned! */
};
/* If header and dma_payload share a cache line, invalidating  */
/* for DMA_FROM_DEVICE will discard the CPU's header write!    */

/* RIGHT: ensure DMA buffers are cache-line aligned */
struct my_data {
    u8  header;
    u8  __pad[63];        /* pad to next cache line */
    u32 dma_payload[16];  /* cache-line aligned     */
} __attribute__((aligned(64)));

/* Or: use separate allocations */
void *dma_buf = kmalloc(ALIGN(size, cache_line_size()), GFP_KERNEL | GFP_DMA);

## Rule 5: Never Use vmalloc() for DMA
/* WRONG: vmalloc pages are not physically contiguous */
void *buf = vmalloc(size);
dma_addr_t dma_addr = virt_to_phys(buf);  /* WRONG! */
/* virt_to_phys on vmalloc = random physical page (not contiguous!) */
/* On SMMU systems: SMMU will fault                                 */
/* On non-SMMU systems: DMA reads random physical pages (corruption) */

/* RIGHT: use dma_alloc_coherent() for DMA buffers */
dma_addr_t dma_addr;
void *buf = dma_alloc_coherent(dev, size, &dma_addr, GFP_KERNEL);
if (!buf) return -ENOMEM;
/* dma_addr is valid for hardware programming */
/* buf is the CPU virtual address             */

# 10. SMMUv3 and SVA (Shared Virtual Addressing)
SMMUv3 is the latest ARM SMMU specification, used in modern Qualcomm Snapdragon 8 Gen series and other high-end ARM64 SoCs. It adds significant new capabilities over SMMUv2.
## 10.1 SMMUv3 New Features
| Feature | Description |
| Two-Stage Translation | Stage 1 (IOVA -> IPA) + Stage 2 (IPA -> PA). Stage 1 managed by guest OS, Stage 2 by hypervisor. Enables secure device assignment to VMs. |
| PCIe ATS Support | Address Translation Services: device-side TLB. Device caches its own SMMU translations, reducing SMMU lookup overhead for frequently used IOVAs. |
| PRI (Page Request Interface) | Device can request on-demand page mapping (similar to CPU page faults). Device accesses non-mapped IOVA -> SMMU reports to OS -> OS maps page -> device retries. Enables SVA. |
| SVA (Shared Virtual Addressing) | Device shares the CPU process virtual address space. Device uses same virtual addresses as the CPU process. Critical for ML inference on NPU/DSP. |
| Command Queue (CMDQ) | Ring buffer for SMMU configuration commands. Software writes commands; SMMU executes asynchronously. Eliminates costly MMIO register writes for each mapping change. |
| MSI Support | Uses PCIe-style MSI interrupts for fault reporting instead of level-triggered interrupts. Scales to large numbers of contexts. |

## 10.2 SMMUv3 SVA — Shared Virtual Addressing
SVA allows a device to share the same virtual address space as a CPU process. This is used in Qualcomm NPU/DSP drivers for ML inference: the NPU can access user-space tensors using the same virtual addresses as the application.

#include <linux/iommu.h>

/* Bind device to current process's address space */
struct iommu_sva *handle = iommu_sva_bind_device(dev,
                                                   current->mm,
                                                   NULL /* priv */);
if (IS_ERR(handle)) {
    dev_err(dev, "SVA bind failed: %ld
", PTR_ERR(handle));
    return PTR_ERR(handle);
}

/* Get PASID (Process Address Space ID) */
u32 pasid = iommu_sva_get_pasid(handle);
if (pasid == IOMMU_PASID_INVALID) {
    dev_err(dev, "Failed to get PASID
");
    iommu_sva_unbind_device(handle);
    return -EINVAL;
}

/* Program PASID into device hardware */
/* Device now uses PASID to select which process page table to use */
writel(pasid, npu_base + NPU_PASID_REG);
writel(NPU_ENABLE_SVA, npu_base + NPU_CTRL_REG);

/* Device can now access any virtual address in current->mm */
/* using PASID-tagged transactions through SMMU             */
/* Example: NPU accesses user tensor at 0x7fff_1234_0000   */
/*          SMMU translates using process page table        */
/*          No dma_map() needed for userspace buffers!      */

/* Submit inference job using user virtual addresses */
struct npu_job job = {
    .input_va  = (u64)user_input_tensor,   /* userspace VA */
    .output_va = (u64)user_output_tensor,  /* userspace VA */
    .model_va  = (u64)user_model_buffer,   /* userspace VA */
    .pasid     = pasid,
};
npu_submit_job(&job);

/* Clean up: unbind when done */
iommu_sva_unbind_device(handle);

### SVA Benefits vs Traditional DMA
| Aspect | Traditional DMA | SVA |
| Userspace buffer | Must pin pages + dma_map() before each transfer | Device uses userspace VA directly, no mapping needed |
| Page fault handling | Allocation must be complete before DMA | PRI: device requests page on demand (like CPU page fault) |
| Zero-copy | Often requires copy to DMA-safe buffer | True zero-copy: device accesses userspace memory directly |
| Complexity | Driver manages IOVA space explicitly | Driver only manages PASID binding |
| Use case | General peripherals, legacy devices | NPU, DSP, AI accelerators, GPU compute |


SMMUv3 SVA requires CONFIG_IOMMU_SVA=y, CONFIG_ARM_SMMU_V3_SVA=y in the kernel config. Check if your Qualcomm SoC's SMMU firmware supports PRI (Page Request Interface).
# 11. Debugging DMA and Cache Issues
DMA coherency bugs are among the most difficult kernel bugs to diagnose. They manifest as data corruption, random crashes, and intermittent failures. Here are the tools and techniques.
## 11.1 Kernel Configuration for DMA Debugging
/* Enable in kernel config (menuconfig or .config): */
CONFIG_DMA_API_DEBUG=y          # Catches unmapped access, wrong direction
CONFIG_DMA_API_DEBUG_SG=y       # Extend to scatter-gather operations
CONFIG_IOMMU_DEBUG=y            # Verbose SMMU/IOMMU logging
CONFIG_IOMMU_FAULT_INJECTION=y  # Inject SMMU faults for testing

/* DMA API debug catches common mistakes: */
/* - Accessing a buffer while it is still mapped for DMA           */
/* - Calling dma_unmap without a matching dma_map                 */
/* - Using wrong DMA size in unmap                                */
/* - DMA mappings that are never unmapped (leaks)                 */

## 11.2 Runtime Debugging Commands
# Check if SMMU is active and devices are assigned
ls /sys/kernel/iommu_groups/
cat /sys/kernel/iommu_groups/*/type

# SMMU fault messages
dmesg | grep -i smmu
dmesg | grep -i iommu
dmesg | grep -i "unhandled context fault"

# DMA mapping statistics
cat /sys/kernel/debug/dma-api/dump   # All active DMA mappings
cat /sys/kernel/debug/dma-api/stats  # Mapping statistics
cat /sys/kernel/debug/dma-api/error_count

# ARM64 cache information
cat /sys/devices/system/cpu/cpu0/cache/index0/size   # L1-D
cat /sys/devices/system/cpu/cpu0/cache/index1/size   # L1-I
cat /sys/devices/system/cpu/cpu0/cache/index2/size   # L2
cat /sys/devices/system/cpu/cpu0/cache/index3/size   # L3
cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size

# IOMMU domain for a specific device
cat /sys/bus/platform/devices/<device_name>/iommu_group/type

# CMA (Contiguous Memory Allocator) stats
cat /proc/meminfo | grep Cma
cat /sys/kernel/debug/cma/<cma_name>/alloc_count
cat /sys/kernel/debug/cma/<cma_name>/fail_count

## 11.3 Diagnosing DMA Coherency Bugs
| Symptom | Likely Cause | Debug Steps |
| Random data corruption in received buffers | Cache not invalidated before CPU reads DMA_FROM_DEVICE buffer | Check: dma_unmap_single() called before CPU reads. Enable CONFIG_DMA_API_DEBUG. |
| Device receives wrong/stale data | Cache not cleaned before DMA_TO_DEVICE transfer | Check: dma_map_single() called AFTER CPU fills buffer, not before. |
| SMMU fault: translation fault at 0x0 | Programming 0 or NULL into DMA address register | Check dma_mapping_error() return value. Verify buffer pointer. |
| SMMU fault: translation fault at physical address | Using virt_to_phys() instead of DMA API on SMMU system | Replace virt_to_phys() with dma_map_single(). Never bypass DMA API. |
| DMA works without SMMU but fails with SMMU enabled | Missing iommus = property in DT, or SMMU not programmed correctly | Check DT iommus property. Check SMMU clock/power domain init. |
| Occasional coherency failures on SMP system | Missing memory barrier between DMA setup and DMA start | Add wmb() / dsb(sy) between programming DMA registers and starting DMA. |


## 11.4 Using ftrace to Debug DMA Operations
# Trace DMA map/unmap calls for a specific device
echo "dma_map_single dma_unmap_single" > /sys/kernel/debug/tracing/set_ftrace_filter
echo function > /sys/kernel/debug/tracing/current_tracer
echo 1 > /sys/kernel/debug/tracing/tracing_on
# ... trigger the issue ...
echo 0 > /sys/kernel/debug/tracing/tracing_on
cat /sys/kernel/debug/tracing/trace | grep -i dma

## 11.5 Kernel Memory Sanitizers
For cache coherency bugs caused by out-of-bounds writes that corrupt adjacent DMA buffers:

/* Enable KASAN (Kernel Address Sanitizer) - detects OOB writes */
CONFIG_KASAN=y
CONFIG_KASAN_INLINE=y   /* or CONFIG_KASAN_OUTLINE */
CONFIG_KASAN_SW_TAGS=y  /* for MTE-less ARM64 */

/* Enable KFENCE (sampling-based memory safety) */
CONFIG_KFENCE=y         /* low overhead, production-safe */

/* These catch:                                          */
/* - Write overflow past DMA buffer into adjacent memory  */
/* - Use-after-free of DMA buffers                        */
/* - Cache aliasing bugs (wrong alignment)                */
# 12. Reference Tables & Summary
## 12.1 Complete DMA API Quick Reference
| Function | Header | Purpose |
| dma_alloc_coherent(dev, sz, &dma, gfp) | linux/dma-mapping.h | Allocate always-coherent DMA buffer. Non-cached on non-HW-coherent SoC. |
| dma_free_coherent(dev, sz, cpu, dma) | linux/dma-mapping.h | Free coherent DMA buffer. |
| dma_alloc_wc(dev, sz, &dma, gfp) | linux/dma-mapping.h | Write-combining DMA buffer. For framebuffers. |
| dma_map_single(dev, ptr, sz, dir) | linux/dma-mapping.h | Map existing buffer for streaming DMA. Returns IOVA. |
| dma_unmap_single(dev, dma, sz, dir) | linux/dma-mapping.h | Unmap streaming DMA. Safe for CPU access after this. |
| dma_mapping_error(dev, dma) | linux/dma-mapping.h | Check if dma_map_single() failed. Always call this! |
| dma_map_sg(dev, sg, n, dir) | linux/dma-mapping.h | Map scatter-gather list. Returns number of mapped entries. |
| dma_unmap_sg(dev, sg, n, dir) | linux/dma-mapping.h | Unmap scatter-gather list. |
| dma_sync_single_for_cpu(dev,dma,sz,dir) | linux/dma-mapping.h | Sync DMA buffer for CPU access (invalidate on FROM_DEVICE). |
| dma_sync_single_for_device(dev,dma,sz,dir) | linux/dma-mapping.h | Sync DMA buffer for device access (clean on TO_DEVICE). |
| dma_pool_create(name, dev, sz, align, boundary) | linux/dmapool.h | Create a pool of small DMA-coherent objects. |
| dma_pool_alloc(pool, gfp, &dma) | linux/dmapool.h | Allocate one object from DMA pool. |
| dma_pool_free(pool, vaddr, dma) | linux/dmapool.h | Return object to DMA pool. |
| dma_pool_destroy(pool) | linux/dmapool.h | Destroy DMA pool and free all memory. |
| dev_is_dma_coherent(dev) | linux/dma-mapping.h | Returns true if device is hardware DMA-coherent. |

## 12.2 ARM64 Cache Maintenance Quick Reference
| Instruction | Linux C Wrapper | Use Case |
| DC CIVAC, Xn | (internal to DMA API) | DMA_TO_DEVICE: clean cache before DMA reads from buffer |
| DC IVAC, Xn | (internal to DMA API) | DMA_FROM_DEVICE: invalidate cache after DMA writes to buffer |
| DC CVAC, Xn | (internal to DMA API) | Clean to PoC without invalidating (rare, SW-only use) |
| DC CVAU, Xn + IC IVAU | flush_icache_range() | After JIT code generation / eBPF / kernel module loading |
| DSB SY | dsb(sy) | After cache ops: wait for completion before continuing |
| DMB SY | dmb(sy) | Memory ordering barrier between CPU writes |
| ISB | isb() | After sysreg change or I-cache maintenance; flush pipeline |


## 12.3 Key Kernel Source Files
| File / Path | Contents |
| kernel/dma/mapping.c | Core DMA API: dma_map_single, dma_alloc_coherent, etc. |
| arch/arm64/mm/dma-mapping.c | ARM64-specific DMA mapping (SWIOTLB, bounce buffers) |
| arch/arm64/include/asm/cacheflush.h | ARM64 cache flush macros and inline functions |
| arch/arm64/include/asm/barrier.h | dsb(), dmb(), isb() wrappers and memory barrier macros |
| drivers/iommu/arm/arm-smmu.c | ARM SMMU v1/v2 driver |
| drivers/iommu/arm/arm-smmu-v3/ | ARM SMMUv3 driver (arm-smmu-v3.c, arm-smmu-v3-sva.c) |
| drivers/iommu/iommu.c | Core IOMMU subsystem (domain management, DMA ops) |
| arch/arm64/include/asm/memory.h | PAGE_OFFSET, phys_to_virt, virt_to_phys macros |
| include/linux/dma-mapping.h | DMA API declarations, DMA_TO_DEVICE, DMA_FROM_DEVICE |
| include/linux/dmapool.h | DMA pool API declarations |


## 12.4 Topic Summary
| Concept | Key Point |
| Cache coherency problem | CPU uses cache; DMA reads/writes DRAM directly -> stale data without coherency management |
| HW coherent | CCI/CMN interconnect snoops CPU caches. No SW cache ops needed. Enabled by "dma-coherent" DT property. |
| Non-coherent | SW must clean cache before device reads (DC CIVAC) and invalidate before CPU reads (DC IVAC). |
| dma_alloc_coherent() | Always coherent. Non-cached (MT_NORMAL_NC) on non-HW-coherent SoC. Use for control/descriptor structures. |
| dma_map_single() | Streaming DMA. Performs cache clean (TO_DEVICE) or invalidate (FROM_DEVICE) at map/unmap time. |
| SMMU | ARM IOMMU. Translates IOVA -> PA per device. Provides isolation and scatter-gather capability. |
| MAIR_EL1 | 8-slot memory type register. AttrIndx in PTE selects slot. MT_NORMAL=cached, MT_NORMAL_NC=uncached, MT_DEVICE_nGnRnE=MMIO. |
| ioremap() | Maps MMIO as MT_DEVICE_nGnRE. Use ioremap_wc() for framebuffers. Always use readl/writel, not dereference. |
| SMMUv3 SVA | Device shares process page table via PASID. True zero-copy for NPU/DSP ML inference workloads. |
| Debugging | CONFIG_DMA_API_DEBUG, dmesg SMMU faults, /sys/kernel/debug/dma-api, KASAN for OOB detection. |


## 12.5 Interview Quick-Reference Card
Key facts for ARM64 DMA interview questions:

- ARM64 cache line = 64 bytes (CTR_EL0 register)
- DC CIVAC = Clean + Invalidate by VA to PoC (used for DMA_TO_DEVICE)
- DC IVAC = Invalidate by VA to PoC (used for DMA_FROM_DEVICE)
- dma_alloc_coherent() = always coherent, non-cached on non-HW-coherent SoC
- dma_map_single() = cached buffer, cache ops at map/unmap, direction matters
- SMMU = ARM IOMMU; translates device IOVA to physical address
- SMMUv3 adds: two-stage translation, ATS, PRI, SVA (PASID-based)
- MT_DEVICE_nGnRnE = strongly ordered (for MMIO); MT_NORMAL = cached (for RAM)
- dma-coherent DT = hardware coherency enabled; no SW cache maintenance
- iommus = <&smmu SID> in DT = device behind SMMU with given stream ID
- Never use virt_to_phys() for DMA on SMMU systems; use DMA API only
- DMA_API_DEBUG=y catches: double-unmap, wrong size, pre-map CPU access

  • End of Part 4: DMA Coherency & Cache Management on ARM64  •
  ARM/ARM64 Linux Kernel Memory Management Reference Guide

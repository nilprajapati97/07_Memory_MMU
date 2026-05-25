# Q10: Kernel Memory-Mapped I/O with ioremap and Barriers

**Section:** Linux Kernel Internals | **Difficulty:** Medium | **Topics:** MMIO, `ioremap`, `readl`/`writel`, `wmb`, `rmb`, PCIe BAR, cache coherency

---

## Question

Implement kernel memory-mapped I/O with `ioremap` and barrier usage.

---

## Answer

```c
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>

/* GPU register offsets (BAR0) */
#define GPU_BAR0_BASE  0xF0000000ULL
#define GPU_BAR0_SIZE  (16 * 1024 * 1024)    /* 16 MB */

#define REG_CTRL       0x000    /* control register */
#define REG_STATUS     0x004    /* status register */
#define REG_DMA_ADDR_LO 0x100   /* DMA lower 32-bit address */
#define REG_DMA_ADDR_HI 0x104   /* DMA upper 32-bit address */
#define REG_DMA_SIZE   0x108    /* DMA transfer size in bytes */

#define CTRL_DMA_START BIT(0)
#define CTRL_RESET     BIT(1)
#define STATUS_READY   BIT(0)
#define STATUS_BUSY    BIT(1)
#define STATUS_ERROR   BIT(2)

static void __iomem *bar0;  /* ioremap'd MMIO base pointer */

/* ─── Map GPU BAR ─────────────────────────────────────────────────────────*/
int gpu_mmio_init(struct pci_dev *pdev)
{
    /* Use PCI API for proper IOMMU/BAR handling */
    bar0 = pci_iomap(pdev, 0, GPU_BAR0_SIZE);
    if (!bar0)
        return -ENOMEM;

    /* For non-PCI use: bar0 = ioremap(GPU_BAR0_BASE, GPU_BAR0_SIZE); */
    return 0;
}

/* ─── Write a 64-bit DMA address to paired 32-bit registers ───────────────*/
void gpu_set_dma_addr(dma_addr_t addr)
{
    writel((u32)(addr & 0xFFFFFFFF), bar0 + REG_DMA_ADDR_LO);
    writel((u32)(addr >> 32),        bar0 + REG_DMA_ADDR_HI);
}

/* ─── Start DMA Transfer ──────────────────────────────────────────────────*/
void gpu_start_dma(dma_addr_t addr, u32 size)
{
    /* Program DMA address and size registers */
    gpu_set_dma_addr(addr);
    writel(size, bar0 + REG_DMA_SIZE);

    /*
     * wmb(): write memory barrier.
     * Ensures the DMA address/size register writes are VISIBLE
     * to the device BEFORE we write the DMA_START command.
     * Without this, the CPU (or PCIe write combining) might
     * reorder the CTRL write before the address writes.
     */
    wmb();

    /* Kick the DMA engine */
    writel(CTRL_DMA_START, bar0 + REG_CTRL);
}

/* ─── Poll for DMA Completion ─────────────────────────────────────────────*/
int gpu_wait_dma_done(unsigned int timeout_us)
{
    u32 status;

    /*
     * readl() already includes an rmb() equivalent on x86.
     * On ARM/POWER, explicit rmb() may be needed between reads
     * that must observe a hardware-updated sequence.
     */
    return readl_poll_timeout(bar0 + REG_STATUS, status,
                               !(status & STATUS_BUSY),
                               10,          /* poll interval: 10 μs */
                               timeout_us); /* total timeout */
}

/* ─── Read status register ────────────────────────────────────────────────*/
u32 gpu_read_status(void)
{
    u32 status;

    rmb();                              /* ensure prior reads are complete */
    status = readl(bar0 + REG_STATUS);
    return status;
}

/* ─── Reset the GPU ───────────────────────────────────────────────────────*/
void gpu_reset(void)
{
    writel(CTRL_RESET, bar0 + REG_CTRL);
    wmb();
    /* Wait for reset to complete */
    udelay(100);
    writel(0, bar0 + REG_CTRL);  /* deassert reset */
    wmb();
}

/* ─── Write-combining (WC) mapping for framebuffer ───────────────────────*/
void __iomem *gpu_map_framebuffer(phys_addr_t fb_base, size_t size)
{
    /*
     * ioremap_wc: write-combining mapping.
     * Multiple writes are coalesced into larger PCIe transactions.
     * Much faster than ioremap() for framebuffer pixel writes.
     * NOT coherent — do not mix reads and writes without barriers.
     */
    return ioremap_wc(fb_base, size);
}

/* ─── Unmap ───────────────────────────────────────────────────────────────*/
void gpu_mmio_exit(struct pci_dev *pdev)
{
    if (bar0) {
        pci_iounmap(pdev, bar0);
        bar0 = NULL;
    }
}
```

---

## Explanation

### Core Concept

**Memory-Mapped I/O (MMIO)** maps GPU hardware registers into the CPU's virtual address space. The CPU reads/writes these addresses using load/store instructions, which are routed by the PCIe bus to the GPU's register space.

`ioremap` creates a non-cacheable kernel virtual mapping for a physical address range. The `void __iomem *` annotation tells both the compiler and sparse checker that this is a device address — direct C pointer dereferencing is forbidden; use `readl`/`writel` accessors.

**Memory Ordering Problem:**

Modern CPUs and PCIe controllers can reorder write transactions for performance. Without barriers:
```
CPU writes: addr_lo, addr_hi, size, CTRL_START
PCIe sees:  CTRL_START, addr_lo, addr_hi, size  ← DMA to wrong address!
```

With `wmb()` between the setup writes and the CTRL_START write, the CPU guarantees ordering.

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `ioremap(phys, size)` | Map MMIO region (non-cached) |
| `ioremap_wc(phys, size)` | Map MMIO with write-combining (framebuffer) |
| `ioremap_np(phys, size)` | Map MMIO with no prefetch (strict ordering) |
| `iounmap(addr)` | Unmap an ioremap'd region |
| `pci_iomap(pdev, bar, size)` | PCI-aware ioremap for a BAR |
| `readl(addr)` | 32-bit MMIO read |
| `writel(val, addr)` | 32-bit MMIO write |
| `readq(addr)` | 64-bit MMIO read |
| `writeq(val, addr)` | 64-bit MMIO write |
| `wmb()` | Write memory barrier — all prior writes visible before subsequent writes |
| `rmb()` | Read memory barrier — all prior reads complete before subsequent reads |
| `mb()` | Full memory barrier |
| `readl_poll_timeout(addr, val, cond, sleep_us, timeout_us)` | Poll register with timeout |

### Trade-offs & Pitfalls

- **Direct pointer dereference of `__iomem` memory is forbidden.** `*(bar0 + offset) = val` compiles but may not reach the hardware due to CPU/compiler caching. Use `writel`/`readl`.
- **`ioremap` vs `ioremap_wc`:** `ioremap` (UC — uncacheable) is correct for control registers. `ioremap_wc` (write-combining) is faster for streaming writes (framebuffer, DMA descriptors) but coalesces writes — do not use for registers where intermediate values matter.
- **64-bit register writes:** On 32-bit hardware paths (PCIe 32-bit), write the LOW 32 bits first, then HIGH 32 bits. The device latches the full 64-bit value when HIGH is written. Always write low before high.
- **`iounmap` on module exit.** Failing to call `iounmap` wastes kernel virtual address space (VMALLOC area). In long-running systems, this causes kernel virtual address space exhaustion.

### NVIDIA / GPU Context

NVIDIA GPU BAR0 (typically 16MB–32MB) contains:
- **Registers:** GPU control, status, interrupt, timer registers
- **Doorbell region:** Per-channel doorbells that the CPU writes to notify the GPU of new commands
- **BAR1:** GPU framebuffer (VRAM) aperture — mapped with `ioremap_wc` for best write bandwidth

The NVIDIA driver programs GPU engines by writing to MMIO registers through `ioremap`'d BAR0. Before enabling a GPU engine (e.g., starting a Copy Engine DMA), all descriptor writes must be completed with `wmb()` before writing the "kick" register. This is a hard requirement verified in NVIDIA's hardware validation tests.

---

## Cross Questions & Answers

**CQ1: What is the difference between `wmb()`, `rmb()`, `mb()`, and `mmiowb()` in the Linux kernel?**
> - `wmb()`: Ensures all preceding stores are visible before subsequent stores (write fence)
> - `rmb()`: Ensures all preceding loads complete before subsequent loads (read fence)
> - `mb()`: Full barrier — both read and write ordering
> - `mmiowb()` (deprecated in Linux 5.2): Was required on some architectures (IA64) to ensure MMIO write ordering across CPUs. Replaced by implicit ordering in `spin_unlock` and `writel` on modern kernels.
> On x86, `writel` already includes an implicit sfence equivalent, but on ARM and POWER, explicit barriers are critical.

**CQ2: What is write-combining (WC) memory and why is it faster for GPU framebuffer writes?**
> Write-combining buffers merge multiple small writes into fewer large PCIe TLPs (Transaction Layer Packets). A GPU framebuffer filled pixel-by-pixel generates thousands of 4-byte writes. With WC mapping, the CPU's write-combining hardware coalesces up to 64 bytes into a single PCIe write transaction, reducing PCIe overhead by 16x. The trade-off: WC writes may not be immediately visible to the device; a `wmb()` or write to a non-WC register flushes the WC buffer.

**CQ3: Can two different kernel modules ioremap the same physical address?**
> Technically yes, but it is incorrect behavior. Both modules get separate kernel virtual addresses mapping the same physical MMIO region, leading to potential concurrent uncoordinated register access. The proper design is for one "owner" driver to `ioremap` the region and provide an accessor API to other modules, or use the PCIe device's BAR access via the `pci_dev` struct which is managed by the PCI subsystem.

**CQ4: What is `readl_poll_timeout` and how is it different from a busy-wait loop?**
> `readl_poll_timeout(addr, var, condition, sleep_us, timeout_us)` is a kernel macro that polls a register until a condition is true or a timeout elapses. When `sleep_us > 0`, it calls `usleep_range` between polls, yielding the CPU. When `sleep_us == 0`, it uses `udelay` (busy-wait). For GPU register polling in interrupt context, `sleep_us=0` is required. In a workqueue, `sleep_us=10` yields the CPU and reduces power consumption.

**CQ5: What is BAR (Base Address Register) in PCIe and how does the kernel discover GPU register addresses?**
> Each PCIe device has up to 6 BARs (BAR0–BAR5) in its PCI configuration space. Each BAR describes a memory region (physical base address and size) that the device exposes. The BIOS/OS assigns physical addresses to BARs during enumeration. The GPU driver reads BAR0 physical address with `pci_resource_start(pdev, 0)` and size with `pci_resource_len(pdev, 0)`, then calls `pci_iomap(pdev, 0, size)` to create the `ioremap`'d mapping. `pci_request_regions` must be called first to claim exclusive access to the BARs.

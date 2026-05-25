# Q35: PCIe Peer-to-Peer DMA Between GPUs

**Section:** System Design | **Difficulty:** Hard | **Topics:** `pci_p2pdma`, PCIe P2P, GPU–GPU DMA, `dma_map_sg`, BAR memory, NVLink, ACS

---

## Question

Implement PCIe peer-to-peer DMA to copy data directly between two GPUs without involving host memory.

---

## Answer

```c
#include <linux/pci-p2pdma.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>

/* ─── GPU-to-GPU P2P DMA Architecture ────────────────────────────────────
 *
 *  GPU0 VRAM (src)                GPU1 VRAM (dst)
 *  ┌──────────┐                   ┌──────────┐
 *  │ BAR1 mem │◄──── PCIe P2P ───►│ BAR1 mem │
 *  └──────────┘      (TLP)        └──────────┘
 *       │                               │
 *       └─── pci_alloc_p2pmem ──────────┘
 *                                (intermediate buffer)
 */

/* ─── P2P resource registration (called during driver probe) ─────────────*/
int gpu_p2p_init(struct gpu_device *gpu0, struct gpu_device *gpu1)
{
    int ret;

    /*
     * pci_p2pdma_distance: checks if P2P is possible between two PCI devices.
     * Returns the estimated latency (PCIe hops) or negative error if not possible.
     * P2P requires: same PCIe root complex, or a PCIe switch that supports P2P.
     * ACS (Access Control Services) on PCIe switches can BLOCK P2P — must be disabled.
     */
    ret = pci_p2pdma_distance(gpu0->pdev, gpu1->pdev, true /* verbose */);
    if (ret < 0) {
        pr_err("GPU P2P: distance check failed: %d\n", ret);
        return ret;
    }
    pr_info("GPU P2P: PCIe hops = %d between GPU0 and GPU1\n", ret);

    /*
     * pci_p2pdma_add_resource: register GPU VRAM (BAR1) as a P2P DMA resource.
     * BAR1 is the large BAR that exposes GPU VRAM to the PCIe fabric.
     * offset: start offset within BAR1; size: how many bytes to expose.
     */
    ret = pci_p2pdma_add_resource(gpu0->pdev,
                                   PCI_BAR_1,        /* BAR index */
                                   gpu0->vram_size,  /* size */
                                   0                 /* offset within BAR */);
    if (ret) {
        pr_err("GPU P2P: failed to add GPU0 VRAM as P2P resource: %d\n", ret);
        return ret;
    }

    ret = pci_p2pdma_add_resource(gpu1->pdev,
                                   PCI_BAR_1,
                                   gpu1->vram_size,
                                   0);
    if (ret) {
        pr_err("GPU P2P: failed to add GPU1 VRAM as P2P resource: %d\n", ret);
        return ret;
    }

    pr_info("GPU P2P: initialized GPU0 <-> GPU1 P2P DMA\n");
    return 0;
}

/* ─── Allocate P2P memory buffer ─────────────────────────────────────────
 * Allocates a buffer from GPU0's VRAM that is accessible via P2P by GPU1.
 * Typically used as a staging buffer for GPU-to-GPU transfers.
 */
void *gpu_p2p_alloc(struct gpu_device *src_gpu,
                     struct gpu_device *dst_gpu,
                     size_t size,
                     dma_addr_t *dma_addr_out)
{
    void *p2p_buf;

    /*
     * pci_alloc_p2pmem: allocate DMA-coherent memory from src_gpu's BAR.
     * Returns a CPU-accessible virtual address to the P2P buffer.
     * Memory is physically in GPU VRAM (fast for GPU), accessible to dst_gpu via PCIe.
     */
    p2p_buf = pci_alloc_p2pmem(src_gpu->pdev, size);
    if (!p2p_buf) {
        pr_err("GPU P2P: failed to allocate %zu bytes P2P memory\n", size);
        return NULL;
    }

    /*
     * pci_p2pmem_virt_to_bus: convert virtual P2P address to a bus address.
     * This bus address is the address that dst_gpu's DMA engine must use.
     */
    *dma_addr_out = pci_p2pmem_virt_to_bus(src_gpu->pdev, p2p_buf);
    if (!*dma_addr_out) {
        pci_free_p2pmem(src_gpu->pdev, p2p_buf, size);
        return NULL;
    }

    pr_debug("GPU P2P: allocated %zuMB P2P buffer at virt=%p bus=0x%llx\n",
             size >> 20, p2p_buf, (u64)*dma_addr_out);
    return p2p_buf;
}

/* ─── GPU-to-GPU DMA transfer via P2P ────────────────────────────────────
 * Initiates a DMA transfer from GPU0 VRAM to GPU1 VRAM via PCIe P2P.
 * Neither src nor dst data passes through host DRAM.
 */
int gpu_p2p_memcpy(struct gpu_device *src_gpu,
                    struct gpu_device *dst_gpu,
                    u64 src_vram_pa, u64 dst_vram_pa,
                    size_t size)
{
    struct scatterlist sg;

    /*
     * Build a scatterlist from the P2P source buffer.
     * sg_set_page is not used here because P2P memory is not backed by
     * struct page — we use sg_set_buf from the P2P virtual address.
     */
    sg_init_one(&sg, phys_to_virt(src_vram_pa), size);

    /*
     * Map the scatterlist for DMA from dst_gpu's perspective.
     * dma_map_sg: translates CPU physical addresses to IOVA bus addresses
     * that dst_gpu can use to DMA from src_gpu's BAR1.
     *
     * In P2P: the IOMMU translates the scatterlist to PCIe BAR addresses
     * on src_gpu, allowing dst_gpu to DMA directly from src_gpu's VRAM.
     */
    if (dma_map_sg(dst_gpu->dev, &sg, 1, DMA_FROM_DEVICE) != 1) {
        pr_err("GPU P2P: DMA map failed\n");
        return -EIO;
    }

    /* Program dst_gpu's DMA engine with the mapped address and size */
    gpu_dma_start(dst_gpu,
                   sg_dma_address(&sg), /* src: BAR address of src_gpu VRAM */
                   dst_vram_pa,          /* dst: dst_gpu VRAM PA             */
                   size);

    /* Wait for DMA completion (via interrupt / polling) */
    gpu_dma_wait(dst_gpu);

    dma_unmap_sg(dst_gpu->dev, &sg, 1, DMA_FROM_DEVICE);

    pr_debug("GPU P2P: copied %zuMB from GPU0 PA=0x%llx to GPU1 PA=0x%llx\n",
             size >> 20, src_vram_pa, dst_vram_pa);
    return 0;
}

/* ─── P2P memory free ─────────────────────────────────────────────────────*/
void gpu_p2p_free(struct gpu_device *src_gpu, void *p2p_buf, size_t size)
{
    pci_free_p2pmem(src_gpu->pdev, p2p_buf, size);
}
```

---

## Explanation

### Core Concept

```
Without P2P:
  GPU0 VRAM → PCIe → Host DRAM → PCIe → GPU1 VRAM   (2× PCIe bandwidth used)

With P2P:
  GPU0 VRAM → PCIe → GPU1 VRAM                       (1× PCIe bandwidth, ~2× faster)

Requirement: PCIe switch between GPU0 and GPU1 that allows P2P TLPs
             (Transaction Layer Packets) to pass without reaching the CPU.
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `pci_p2pdma_distance(dev1, dev2, verbose)` | Check if P2P is possible, get hop count |
| `pci_p2pdma_add_resource(pdev, bar, size, offset)` | Expose BAR as P2P DMA resource |
| `pci_alloc_p2pmem(pdev, size)` | Allocate from BAR P2P region |
| `pci_free_p2pmem(pdev, buf, size)` | Free P2P allocation |
| `pci_p2pmem_virt_to_bus(pdev, buf)` | Get bus address for DMA use |
| `dma_map_sg(dev, sg, n, dir)` | Map scatterlist for DMA (IOMMU) |
| `sg_dma_address(sg)` | Get IOVA address after DMA mapping |
| `sg_init_one(sg, buf, len)` | Initialize single-element scatterlist |

### Trade-offs & Pitfalls

- **ACS (Access Control Services) blocks P2P.** Many enterprise systems enable ACS on PCIe switches to enforce isolation between PCIe endpoints. ACS routes all TLPs via the root complex (host), negating P2P benefits. Must disable ACS via `pci=noacs` kernel parameter or per-device ACS override.
- **Large BAR (Resizable BAR / ReBAR) required.** BAR1 must be large enough to cover the VRAM region exposed for P2P. Older systems (BIOS) may not allocate 40GB BAR regions. Use `pci_resize_resource()` if available.

### NVIDIA / GPU Context

NVLink: NVIDIA's proprietary GPU interconnect (NVLink 4.0: 900 GB/s bidirectional, vs PCIe 5.0: ~128 GB/s). For A100/H100 NVSwitch systems, NVIDIA uses NVLink instead of PCIe P2P. P2P DMA is used when GPUs are connected only via PCIe (consumer/workstation GPUs without NVLink).

---

## Cross Questions & Answers

**CQ1: What is ACS and why does it block PCIe P2P?**
> ACS (Access Control Services) is a PCIe feature on switches that enforces TLP routing rules. With ACS enabled, a TLP from GPU0 to GPU1 through a PCIe switch is redirected to the Root Complex (host CPU) and back down to GPU1, rather than being forwarded directly. This ensures all traffic is visible/filterable by the host. For P2P DMA, this means data traverses DRAM (negating P2P benefits). Disable: `echo 0 > /sys/bus/pci/devices/<switch>/acs_ctrl` or use the `pci=noacs` kernel boot parameter.

**CQ2: What is NVLink and how is it different from PCIe P2P?**
> NVLink is NVIDIA's proprietary high-speed interconnect for GPU-to-GPU communication. NVLink 4.0 provides 900 GB/s bidirectional bandwidth vs. PCIe 5.0 x16's ~128 GB/s. NVLink uses a different physical layer (optimized for GPU-to-GPU) and doesn't require going through a PCIe switch. In the kernel, NVLink DMA appears as a virtual PCI bus — the driver uses `gpu_nvlink_memcpy()` instead of `pci_p2pdma`. NVLink also supports direct access to peer GPU memory (GPU can load/store to remote GPU memory like local memory — no DMA engine needed).

**CQ3: What is Resizable BAR (ReBAR) and why is it needed for P2P?**
> GPU VRAM is exposed to the PCIe fabric via BAR1 (Base Address Register). By default, BAR1 is limited to 256MB (PCIe spec default). ReBAR allows BAR1 to be resized to the full VRAM size (e.g., 40GB for A100). Without ReBAR: only 256MB of VRAM is accessible via P2P. With ReBAR: the full VRAM is accessible. BIOS must support 64-bit BARs. Linux kernel >= 5.11 enables ReBAR automatically if BIOS allows it. For NVIDIA GPUs: `nvidia-smi -q | grep BAR1` shows current BAR1 size.

**CQ4: How does the IOMMU affect PCIe P2P DMA?**
> With IOMMU enabled (Intel VT-d or AMD-Vi), the IOMMU validates all DMA transactions. For P2P: when GPU1 DMA-reads from GPU0's BAR address, the IOMMU checks that GPU1 is allowed to access that IOVA. `pci_p2pdma_add_resource` registers the BAR memory in the IOMMU page tables with permissions for P2P access. Without this registration, the IOMMU blocks the P2P transaction (DMAR fault). The `pci_p2pdma_distance()` check also verifies IOMMU compatibility.

**CQ5: What happens during GPU warm reset when there are active P2P DMA transfers?**
> Active P2P DMA transfers must be drained before GPU reset. The reset sequence: (1) mark GPU as "resetting" — new P2P initiations return error, (2) wait for all in-flight DMA completions (`gpu_dma_wait` with timeout), (3) `dma_unmap_sg` for all pending scatterlists, (4) invalidate IOMMU entries for the resetting GPU's BARs, (5) perform GPU reset (PCIe Function Level Reset), (6) re-initialize BARs and IOMMU mappings, (7) re-register P2P resources with `pci_p2pdma_add_resource`. If a P2P DMA is stuck (GPU hung), use `pci_reset_function()` which triggers FLR even without driver cooperation.

# Q15: IOMMU and GPU DMA Group Management

**Section:** Memory Management | **Difficulty:** Hard | **Topics:** IOMMU, `iommu_domain`, IOVA, DMA isolation, SR-IOV, GPU virtualization, PCIe

---

## Question

What is IOMMU and how does it affect GPU DMA? Implement IOMMU group management.

---

## Answer

```c
#include <linux/iommu.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>

/* ─── IOMMU Domain Setup for a GPU PCIe device ────────────────────────────*/
struct gpu_iommu_domain {
    struct iommu_domain *domain;
    struct pci_dev      *pdev;
    struct iommu_group  *group;
};

int gpu_iommu_domain_create(struct pci_dev *pdev,
                              struct gpu_iommu_domain *gd)
{
    int ret;

    gd->pdev  = pdev;

    /* Get the IOMMU group this device belongs to */
    gd->group = iommu_group_get(&pdev->dev);
    if (!gd->group) {
        dev_err(&pdev->dev, "No IOMMU group — IOMMU not present or not enabled\n");
        return -ENODEV;
    }

    /* Allocate a private IOMMU domain for this GPU */
    gd->domain = iommu_domain_alloc(pdev->dev.bus);
    if (!gd->domain) {
        ret = -ENOMEM;
        goto err_group;
    }

    /* Attach the device to our custom domain */
    ret = iommu_attach_device(gd->domain, &pdev->dev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to attach to IOMMU domain: %d\n", ret);
        goto err_domain;
    }

    dev_info(&pdev->dev, "IOMMU domain attached successfully\n");
    return 0;

err_domain:
    iommu_domain_free(gd->domain);
err_group:
    iommu_group_put(gd->group);
    return ret;
}

/* ─── Map GPU VRAM into the IOMMU domain (for P2P DMA access) ─────────────*/
int gpu_iommu_map_vram(struct gpu_iommu_domain *gd,
                        u64 iova_start, phys_addr_t phys_base,
                        size_t size)
{
    int ret;

    /*
     * Map physical GPU VRAM into device-visible IOVA space.
     * This allows peer GPU devices to DMA directly to this GPU's VRAM
     * using the IOVA address range.
     */
    ret = iommu_map(gd->domain,
                    iova_start,           /* device-side virtual address */
                    phys_base,            /* physical address of GPU VRAM */
                    size,
                    IOMMU_READ | IOMMU_WRITE | IOMMU_CACHE,
                    GFP_KERNEL);
    if (ret) {
        pr_err("IOMMU map failed: IOVA=0x%llx PA=0x%llx size=%zuMB ret=%d\n",
               iova_start, (u64)phys_base, size >> 20, ret);
        return ret;
    }

    pr_info("IOMMU: IOVA 0x%llx → PA 0x%llx, size %zuMB\n",
            iova_start, (u64)phys_base, size >> 20);
    return 0;
}

/* ─── Unmap from IOMMU domain ─────────────────────────────────────────────*/
void gpu_iommu_unmap_vram(struct gpu_iommu_domain *gd,
                           u64 iova_start, size_t size)
{
    size_t unmapped = iommu_unmap(gd->domain, iova_start, size);
    if (unmapped != size)
        pr_warn("IOMMU unmap: requested %zu, unmapped %zu\n", size, unmapped);
}

/* ─── Translate IOVA back to physical (for debugging) ─────────────────────*/
phys_addr_t gpu_iommu_iova_to_phys(struct gpu_iommu_domain *gd, u64 iova)
{
    return iommu_iova_to_phys(gd->domain, iova);
}

/* ─── Teardown ────────────────────────────────────────────────────────────*/
void gpu_iommu_domain_destroy(struct gpu_iommu_domain *gd)
{
    iommu_detach_device(gd->domain, &gd->pdev->dev);
    iommu_domain_free(gd->domain);
    iommu_group_put(gd->group);
    memset(gd, 0, sizeof(*gd));
}

/* ─── Check IOMMU capabilities ────────────────────────────────────────────*/
void gpu_iommu_query_caps(struct pci_dev *pdev)
{
    struct iommu_domain *domain;

    domain = iommu_get_domain_for_dev(&pdev->dev);
    if (!domain) {
        pr_info("Device uses default IOMMU domain (passthrough or DMA)\n");
        return;
    }

    pr_info("IOMMU domain type: %u\n", domain->type);
    /* IOMMU_DOMAIN_BLOCKED, IOMMU_DOMAIN_IDENTITY, IOMMU_DOMAIN_DMA,
     * IOMMU_DOMAIN_UNMANAGED */

    pr_info("IOMMU geometry: aperture=0x%llx..0x%llx\n",
            domain->geometry.aperture_start,
            domain->geometry.aperture_end);
}
```

---

## Explanation

### Core Concept

The **IOMMU (Input-Output Memory Management Unit)** is the hardware MMU for DMA devices. Just as a CPU MMU translates virtual → physical for CPU accesses, the IOMMU translates **IOVA → physical** for DMA accesses.

```
Without IOMMU:
  GPU DMA engine → directly accesses physical RAM → can corrupt any memory

With IOMMU:
  GPU DMA engine → IOVA → IOMMU page table → physical RAM (only mapped pages)
                                                  ↑ Access to unmapped IOVA = fault
```

**IOMMU Groups:** PCIe devices that can DMA to each other's memory (e.g., GPU + GPU on same PCIe switch) must be in the same IOMMU group and share or coordinate their IOMMU domains. This is critical for SR-IOV virtualization — each virtual function gets its own domain.

**NVIDIA GPU IOMMU Flow:**

```
CUDA process: cuMemcpyHtoD(dst, src, size)
                     │
                     ▼
Driver: dma_map_sg(dev, sgt) → IOMMU allocates IOVA
                                  IOMMU maps IOVA → user pages
                     │
                     ▼
GPU DMA engine: reads IOVA range → IOMMU translates → user pages
                     │
                     ▼
Driver: dma_unmap_sg() → IOMMU removes mapping
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `iommu_group_get(dev)` | Get the IOMMU group for a device |
| `iommu_group_put(group)` | Release group reference |
| `iommu_domain_alloc(bus)` | Allocate a new IOMMU translation domain |
| `iommu_domain_free(domain)` | Free an IOMMU domain |
| `iommu_attach_device(domain, dev)` | Attach device to domain |
| `iommu_detach_device(domain, dev)` | Detach device from domain |
| `iommu_map(domain, iova, phys, size, prot, gfp)` | Add IOVA → physical mapping |
| `iommu_unmap(domain, iova, size)` | Remove IOVA mapping |
| `iommu_iova_to_phys(domain, iova)` | Translate IOVA → physical (debug) |
| `iommu_get_domain_for_dev(dev)` | Get current domain for a device |
| `dma_map_sg(dev, sgl, n, dir)` | High-level: maps SG through IOMMU |

### Trade-offs & Pitfalls

- **IOMMU not always present/enabled.** On x86, IOMMU must be enabled via kernel param (`intel_iommu=on` or `amd_iommu=on`). Check `iommu_group_get` return value. Fallback: use DMA coherent API which handles both IOMMU and non-IOMMU cases.
- **IOMMU group constraints for SR-IOV.** All VFs of a PF must be in the same IOMMU group by hardware design. When assigning a VF to a VM, the entire group must be assigned — you cannot split VFs from the same PF to different VMs with different trust levels.
- **IOMMU overhead.** Each DMA access requires an IOMMU TLB lookup. On A100/H100 with ATS (Address Translation Services), the GPU can cache IOMMU translations in its own TLB, reducing overhead. Enable `pci_enable_ats(pdev, 0)` for this.

### NVIDIA / GPU Context

NVIDIA uses IOMMU for:
- **Security:** Prevent a buggy/malicious GPU kernel from writing to arbitrary host memory — critical in multi-tenant cloud (AWS/GCP/Azure)
- **GPU SR-IOV / MIG:** Each VF/MIG slice gets its own IOMMU domain — VMs cannot read each other's GPU memory via DMA
- **NVLink P2P:** Map one GPU's VRAM into another GPU's IOMMU domain for peer access without CPU involvement
- **ATS (Address Translation Services):** GPU caches CPU page table entries in GPU TLB, eliminating IOMMU lookup on subsequent DMA accesses to the same IOVA

---

## Cross Questions & Answers

**CQ1: What is the difference between `IOMMU_DOMAIN_DMA` and `IOMMU_DOMAIN_IDENTITY`?**
> `IOMMU_DOMAIN_DMA`: the kernel's default mode where IOVA ≠ physical address. The kernel manages the IOVA allocator and programs the IOMMU page tables. Provides full DMA isolation. `IOMMU_DOMAIN_IDENTITY` (passthrough): IOVA = physical address — the IOMMU is effectively transparent. Used for performance-critical devices in trusted environments where isolation is not needed. NVIDIA's bare-metal deployment uses `IDENTITY` for maximum DMA performance; cloud deployment uses `DMA` for multi-tenant security.

**CQ2: What is ATS (Address Translation Services) in PCIe and how does it benefit GPU DMA?**
> ATS allows a PCIe device to cache IOMMU translation results in its own translation cache (ATC). Instead of asking the IOMMU for every DMA transaction, the GPU TLB holds the physical address after the first access. This eliminates the latency of IOMMU TLB walk for repeated accesses to the same memory range. On A100 GPUs accessing a 1GB buffer, ATS means only the first 512 accesses (one per 2MB page) pay the IOMMU lookup cost.

**CQ3: How does IOMMU group membership affect PCIe pass-through in KVM virtualization?**
> KVM uses VFIO to pass PCIe devices to VMs. VFIO requires that all devices in an IOMMU group are either all passed to the same VM or all left in the host. If a GPU and a non-passthrough device (e.g., PCIe switch) are in the same IOMMU group, you cannot pass the GPU alone — the switch would be left in the host, allowing the VM's GPU to DMA to the switch's address space. NVIDIA's PCIe topology design ensures GPUs get their own isolated IOMMU groups.

**CQ4: What is the `SMMUv3` and where is it used with NVIDIA GPUs?**
> SMMUv3 is ARM's implementation of an IOMMU for ARM-based systems (SBSA). NVIDIA's Grace CPU-based servers (Grace Hopper Superchip) use SMMUv3 for DMA isolation between CPU and GPU. The GPU and CPU share memory via NVLink-C2C (Chip-to-Chip), and the SMMUv3 governs which physical addresses the GPU's DMA engine can access. The SMMU also supports PASID (Process Address Space ID), allowing per-process GPU DMA with different page table contexts.

**CQ5: How would you implement a per-process IOMMU domain for a multi-tenant GPU?**
> Allocate one `iommu_domain` per process/tenant. When a process calls `cuMemAlloc`, map the allocation into that process's IOMMU domain with a unique IOVA. Program the GPU's GMMU context to use the per-process IOVA space. When the GPU executes a process's command buffer, its DMA accesses go through the per-process IOMMU domain — it cannot access another process's IOVA range even if it has the physical address, because the IOMMU will fault the unmapped access. This is the basis of NVIDIA's MIG (Multi-Instance GPU) memory isolation.

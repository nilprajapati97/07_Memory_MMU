# Q36: SR-IOV Virtual Function (VF) Manager

**Section:** System Design | **Difficulty:** Hard | **Topics:** SR-IOV, PF/VF, `pci_enable_sriov`, GPU partitioning, VF resource allocation, `sysfs`

---

## Question

Implement an SR-IOV VF manager for GPU virtualization with per-VF resource allocation.

---

## Answer

```c
#include <linux/pci.h>
#include <linux/pci-ats.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

/* ─── SR-IOV Architecture ─────────────────────────────────────────────────
 *
 *   Physical Function (PF)          /dev/nvidia0 (host driver)
 *      ├── VF0  (VM1) ───────────► VRAM: 8GB, Compute: 25%, Max Ctx: 32
 *      ├── VF1  (VM2) ───────────► VRAM: 8GB, Compute: 25%, Max Ctx: 32
 *      ├── VF2  (VM3) ───────────► VRAM: 16GB, Compute: 50%, Max Ctx: 64
 *      └── VF3  (VM4) ───────────► VRAM: 8GB, Compute: 25%, Max Ctx: 32
 *
 *  Hardware: GPU PCIe config space exposes multiple VF PCI devices.
 *  Hypervisor: assigns each VF to a VM via IOMMU passthrough.
 */

#define MAX_GPU_VFS  16   /* H100 SXM supports up to 16 VFs */

/* ─── Per-VF resource configuration ──────────────────────────────────────*/
struct vf_config {
    u32 vram_mb;           /* VRAM allocation in MB                     */
    u32 compute_percent;   /* share of GPU SMs (0–100%)                 */
    u32 max_contexts;      /* max concurrent CUDA contexts               */
    u32 encoder_percent;   /* NVENC encoder bandwidth share (0–100%)    */
    u32 decoder_percent;   /* NVDEC decoder bandwidth share             */
    bool ecc_enabled;      /* per-VF ECC (requires ECC on PF)          */
};

/* ─── PF GPU device with VF management ───────────────────────────────────*/
struct gpu_pf_device {
    struct pci_dev      *pdev;
    struct gpu_device   *gpu;       /* underlying GPU hardware state     */
    int                  num_vfs;   /* currently enabled VF count        */
    struct vf_config     vf_cfg[MAX_GPU_VFS]; /* per-VF config           */

    /* Hardware registers for partitioning */
    void __iomem        *partition_regs; /* MMIO region for VF partitioning */
};

/* ─── VF MMIO register offsets (device-specific) ─────────────────────────*/
#define VF_VRAM_BASE_REG(vf_id)    (0x1000 + (vf_id) * 0x100)
#define VF_VRAM_SIZE_REG(vf_id)    (0x1004 + (vf_id) * 0x100)
#define VF_COMPUTE_QUOTA_REG(vf_id) (0x1008 + (vf_id) * 0x100)
#define VF_CTX_LIMIT_REG(vf_id)    (0x100C + (vf_id) * 0x100)
#define VF_ECC_CTRL_REG(vf_id)     (0x1010 + (vf_id) * 0x100)

/* ─── Program VF resource partition in hardware ──────────────────────────*/
static int gpu_partition_resources(struct gpu_pf_device *pf, int vf_id)
{
    struct vf_config *cfg = &pf->vf_cfg[vf_id];
    void __iomem *regs    = pf->partition_regs;
    u64 vram_base;
    u32 vram_pages;

    /* Calculate VRAM base for this VF:
     * Each VF gets a contiguous VRAM region. Base = sum of previous VFs' VRAM.
     */
    vram_base = 0;
    for (int i = 0; i < vf_id; i++)
        vram_base += (u64)pf->vf_cfg[i].vram_mb << 20;

    vram_pages = cfg->vram_mb * 256; /* 256 pages per MB (4KB pages) */

    /*
     * Write VF resource limits to GPU partition registers.
     * The GPU hardware enforces these limits: a VF that exceeds
     * its VRAM allocation gets a FAULT, not access to other VFs' memory.
     */
    writel((u32)(vram_base >> 12), regs + VF_VRAM_BASE_REG(vf_id));
    writel(vram_pages,              regs + VF_VRAM_SIZE_REG(vf_id));
    writel(cfg->compute_percent,    regs + VF_COMPUTE_QUOTA_REG(vf_id));
    writel(cfg->max_contexts,       regs + VF_CTX_LIMIT_REG(vf_id));
    writel(cfg->ecc_enabled ? 1 : 0, regs + VF_ECC_CTRL_REG(vf_id));

    /* Memory barrier: ensure all register writes are committed before
     * enabling the VF in PCI config space */
    wmb();

    pr_info("GPU SR-IOV VF%d: VRAM=%uMB@0x%llx compute=%u%% max_ctx=%u\n",
            vf_id, cfg->vram_mb, vram_base,
            cfg->compute_percent, cfg->max_contexts);
    return 0;
}

/* ─── Enable SR-IOV: create VF PCI devices ────────────────────────────────*/
int gpu_sriov_enable(struct gpu_pf_device *pf, int num_vfs)
{
    int ret, i;

    if (num_vfs > MAX_GPU_VFS) {
        pr_err("GPU SR-IOV: requested %d VFs, max %d\n", num_vfs, MAX_GPU_VFS);
        return -EINVAL;
    }

    /* Validate that resource allocations add up correctly */
    u32 total_vram = 0, total_compute = 0;
    for (i = 0; i < num_vfs; i++) {
        total_vram    += pf->vf_cfg[i].vram_mb;
        total_compute += pf->vf_cfg[i].compute_percent;
    }

    if (total_vram > (pf->gpu->total_vram_mb - 2048 /* reserve 2GB for PF */)) {
        pr_err("GPU SR-IOV: total VF VRAM %uMB exceeds available\n", total_vram);
        return -EINVAL;
    }
    if (total_compute > 100) {
        pr_err("GPU SR-IOV: total compute %u%% > 100%%\n", total_compute);
        return -EINVAL;
    }

    /* Program each VF's resource limits in hardware */
    for (i = 0; i < num_vfs; i++) {
        ret = gpu_partition_resources(pf, i);
        if (ret)
            return ret;
    }

    /*
     * pci_enable_sriov: instructs the PCIe hardware to expose VF PCI devices.
     * After this call, the hypervisor (KVM/Xen) can see num_vfs new PCI BDFs
     * (e.g., 0000:01:00.1, 0000:01:00.2, ...) and assign them to VMs via VFIO.
     */
    ret = pci_enable_sriov(pf->pdev, num_vfs);
    if (ret) {
        pr_err("GPU SR-IOV: pci_enable_sriov(%d) failed: %d\n", num_vfs, ret);
        return ret;
    }

    pf->num_vfs = num_vfs;
    pr_info("GPU SR-IOV: enabled %d VFs\n", num_vfs);
    return 0;
}

/* ─── Disable SR-IOV ──────────────────────────────────────────────────────*/
void gpu_sriov_disable(struct gpu_pf_device *pf)
{
    /* Must be called before removing the PF driver */
    pci_disable_sriov(pf->pdev);
    pf->num_vfs = 0;
    pr_info("GPU SR-IOV: disabled\n");
}

/* ─── VF driver probe (called in guest VM / VM's kernel) ─────────────────
 * The VF PCI device is passed to the VM via VFIO.
 * The guest VM's nvidia-vgpu driver calls this probe function.
 */
static int gpu_vf_probe(struct pci_dev *vf_pdev, const struct pci_device_id *id)
{
    struct gpu_vf_device *vf_dev;
    u32 vram_pages, vram_base_pfn;

    vf_dev = devm_kzalloc(&vf_pdev->dev, sizeof(*vf_dev), GFP_KERNEL);
    if (!vf_dev)
        return -ENOMEM;

    vf_dev->pdev = vf_pdev;

    if (pcim_enable_device(vf_pdev))
        return -EIO;
    pcim_iomap_regions(vf_pdev, BIT(0), "gpu_vf");

    vf_dev->regs = pcim_iomap_table(vf_pdev)[0];

    /* Read this VF's resource limits from BAR0 (set by PF driver) */
    vram_base_pfn = readl(vf_dev->regs + VF_VRAM_BASE_REG(0));
    vram_pages    = readl(vf_dev->regs + VF_VRAM_SIZE_REG(0));

    vf_dev->vram_pa   = (u64)vram_base_pfn << 12;
    vf_dev->vram_size = (u64)vram_pages * PAGE_SIZE;

    pr_info("GPU VF: probed, VRAM=%lluMB at PA=0x%llx\n",
            vf_dev->vram_size >> 20, vf_dev->vram_pa);

    pci_set_drvdata(vf_pdev, vf_dev);
    return 0;
}
```

---

## Explanation

### Core Concept

```
  ┌─────────────────────────────────────────────────────┐
  │                  H100 GPU (PF: 0000:01:00.0)        │
  │                                                     │
  │  Total VRAM: 80GB   Total Compute: 100%             │
  │                                                     │
  │  ┌──────────────────────────────────────────────┐  │
  │  │ Hardware Partition Controller                │  │
  │  │ VF0: VRAM [0..8GB]    compute=25%            │  │
  │  │ VF1: VRAM [8..16GB]   compute=25%            │  │
  │  │ VF2: VRAM [16..32GB]  compute=50%            │  │
  │  └──────────────────────────────────────────────┘  │
  │       │              │              │               │
  └───────┼──────────────┼──────────────┼───────────────┘
          ▼              ▼              ▼
       VF0 PCIe       VF1 PCIe       VF2 PCIe
    (passed to VM1) (passed to VM2) (passed to VM3)
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `pci_enable_sriov(pdev, num_vfs)` | Create VF PCI BDFs (Physical Function enable) |
| `pci_disable_sriov(pdev)` | Remove all VF devices |
| `pcim_enable_device(pdev)` | Enable VF PCI device (in guest) |
| `pcim_iomap_regions(pdev, mask, name)` | Map VF BAR regions |
| `readl(reg + VF_VRAM_BASE_REG(id))` | Read VF resource limits from MMIO |
| `writel(val, reg + VF_COMPUTE_QUOTA_REG(id))` | Write VF quota to partition HW |
| `wmb()` | Memory barrier before enabling VF |

### Trade-offs & Pitfalls

- **Hot-plug VF count change requires SR-IOV reset.** `pci_enable_sriov` can only be called once — to change the number of VFs, must call `pci_disable_sriov` then `pci_enable_sriov` again. This disrupts all running VMs.
- **Compute quota enforcement granularity.** Hardware SM time-slicing has a fixed scheduling quantum (typically 1ms in NVIDIA MIG). A VF set to 25% compute gets SMs for 25% of each quantum — this introduces latency variation for GPU kernels < 1ms in duration.

### NVIDIA / GPU Context

NVIDIA offers two GPU virtualization technologies:
- **MIG (Multi-Instance GPU)** — A100/H100 only. Hardware-level partitioning into up to 7 independent GPU instances. Complete memory isolation. Lower overhead than vGPU.
- **NVIDIA vGPU (SR-IOV)** — Full SR-IOV with 16+ VFs. Requires GRID license. Used in data center virtualization (VMware, KVM).

---

## Cross Questions & Answers

**CQ1: What is the difference between SR-IOV and MIG for GPU virtualization?**
> SR-IOV creates multiple virtual PCIe functions (VFs) that are time-multiplexed on the GPU hardware. Multiple VMs share the same physical SMs and memory controllers with time-slicing. MIG (Multi-Instance GPU) physically partitions the GPU hardware — each MIG instance gets dedicated SMs, L2 cache slices, HBM memory controllers, and a separate GPU engine. MIG instances are truly isolated (no timing side-channels). SR-IOV has lower overhead for workloads that fit in the time slice; MIG guarantees isolation for sensitive/regulated workloads.

**CQ2: How does the hypervisor assign a VF to a VM using VFIO?**
> VFIO (Virtual Function I/O) is the Linux kernel framework for userspace/VM access to PCI devices. Steps: (1) PF driver calls `pci_enable_sriov(8)` — 8 VF PCIe devices appear. (2) Host driver unbinds VF from any current driver. (3) VFIO kernel driver binds to VF: `echo vfio-pci > /sys/bus/pci/devices/0000:01:00.1/driver_override`. (4) IOMMU groups the VF (isolates its DMA access). (5) QEMU/KVM passes the VF to the VM via `qemu -device vfio-pci,host=0000:01:00.1`. The VM's nvidia-vgpu driver then calls `gpu_vf_probe()`.

**CQ3: What is PASID (Process Address Space ID) and how does it enable multi-process GPU sharing within a VF?**
> PASID is a PCIe extension that allows a single PCIe function (VF) to issue DMA transactions tagged with different process identifiers. The IOMMU uses the PASID to apply per-process page tables (SVM — Shared Virtual Memory). Without PASID: a VF has one IOMMU context shared by all guest processes. With PASID: each CUDA process in the VM gets its own IOMMU page table, allowing the GPU to DMA to/from virtual addresses in each process's address space independently. PASID is required for NVIDIA's SVM support (CUDA Unified Memory in VMs).

**CQ4: How do you handle a VF crash (GPU fault in VM) without affecting other VMs?**
> VF fault isolation: (1) GPU hardware generates a fault interrupt on the PF. (2) PF driver reads the fault register to identify the offending VF. (3) PF driver resets only the faulting VF: write VF reset bit to hardware, wait for reset complete, reinitialize VF resources, send a signal to the hypervisor that the VF was reset. (4) Other VFs continue running unaffected. (5) The VM using the faulted VF receives a PCI device error notification (AER). (6) The VM's nvidia-vgpu driver attempts GPU recovery (CUDA context error handling).

**CQ5: What is the SR-IOV `sysfs` interface and how is it used?**
> SR-IOV is controlled via sysfs: `echo 4 > /sys/bus/pci/devices/0000:01:00.0/sriov_numvfs` — enables 4 VFs. Read: `cat /sys/bus/pci/devices/0000:01:00.0/sriov_numvfs` — returns current VF count. `cat /sys/bus/pci/devices/0000:01:00.0/sriov_totalvfs` — maximum VFs supported. Disable: `echo 0 > /sys/bus/pci/devices/0000:01:00.0/sriov_numvfs`. The kernel calls the PF driver's `pci_driver.sriov_configure` callback on write, which calls `pci_enable_sriov()` or `pci_disable_sriov()`.

# Q31: NVIDIA Driver Architecture Overview

**Section:** System Design | **Difficulty:** Medium | **Topics:** NVIDIA driver stack, RM (Resource Manager), HAL, char device, PCIe, UVM, Open-Source driver

---

## Question

Describe the NVIDIA Linux GPU driver architecture with its key components and their interactions.

---

## Answer

```c
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/mm.h>

/*
 * NVIDIA Driver Architecture — Key Components
 *
 * ┌─────────────────────────────────────────────────────────┐
 * │                   User Space                             │
 * │  CUDA Runtime  │  OpenGL/Vulkan  │  nvidia-smi          │
 * └────────┬───────┴────────┬────────┴────────┬─────────────┘
 *          │ /dev/nvidia0   │ /dev/nvidiactl   │ /dev/nvidiaX
 *          ▼ ioctl/mmap     ▼                  ▼
 * ┌─────────────────────────────────────────────────────────┐
 * │              Kernel Space (nvidia.ko)                    │
 * │                                                          │
 * │  ┌─────────────┐   ┌──────────────┐  ┌───────────────┐  │
 * │  │  Char Device │   │  RM (Resource│  │  UVM Module   │  │
 * │  │  Interface   │──►│  Manager)    │  │  (nvidia-uvm) │  │
 * │  │  file_ops    │   │  alloc/free  │  │  fault handler│  │
 * │  │  ioctl/mmap  │   │  VA/channel  │  │  migration    │  │
 * │  └─────────────┘   └──────┬───────┘  └───────────────┘  │
 * │                            │                              │
 * │                   ┌────────▼────────┐                    │
 * │                   │  HAL (Hardware  │                    │
 * │                   │  Abstraction)   │                    │
 * │                   │  per-GPU-gen    │                    │
 * │                   └────────┬────────┘                    │
 * │                            │  MMIO readl/writel          │
 * └────────────────────────────┼────────────────────────────┘
 *                              │
 * ┌────────────────────────────▼────────────────────────────┐
 * │  GPU Hardware (via PCIe BAR0/BAR1)                       │
 * │  GPC  │  GMMU  │  Copy Engines  │  NVLink  │  PMU       │
 * └─────────────────────────────────────────────────────────┘
 */

/* ─── Char Device layer ───────────────────────────────────────────────────
 * /dev/nvidia0, /dev/nvidiactl, /dev/nvidia-uvm
 */
struct nvidia_file_private {
    struct gpu_device      *gpu;       /* associated GPU device  */
    struct gpu_va_space    *va_space;  /* per-process VA space   */
    struct list_head        allocations; /* list of allocations  */
    struct mutex            lock;
};

static int nvidia_open(struct inode *inode, struct file *file)
{
    struct nvidia_file_private *priv;
    struct gpu_device *gpu = container_of(inode->i_cdev,
                                            struct gpu_device, cdev);

    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->gpu = gpu;
    mutex_init(&priv->lock);
    INIT_LIST_HEAD(&priv->allocations);

    /* Allocate a GPU VA space for this process */
    priv->va_space = gpu_va_space_create(gpu);
    if (!priv->va_space) {
        kfree(priv);
        return -ENOMEM;
    }

    file->private_data = priv;
    return 0;
}

static long nvidia_ioctl(struct file *file, unsigned int cmd,
                          unsigned long arg)
{
    struct nvidia_file_private *priv = file->private_data;

    switch (cmd) {
    case NV_ESC_ALLOC_OS_EVENT:
        return rm_alloc_os_event(priv, arg);
    case NV_ESC_RM_ALLOC:
        return rm_alloc(priv, arg);   /* RM resource allocation */
    case NV_ESC_RM_CONTROL:
        return rm_control(priv, arg); /* RM control calls       */
    case NV_ESC_RM_MAP_MEMORY:
        return rm_map_memory(priv, arg);
    default:
        return -ENOTTY;
    }
}

static int nvidia_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct nvidia_file_private *priv = file->private_data;

    /* Delegate to appropriate mmap handler based on vma->vm_pgoff */
    if (vma->vm_pgoff == NV_MMAP_VIDMEM_OFFSET)
        return gpu_vram_mmap(priv->gpu, vma);
    else if (vma->vm_pgoff == NV_MMAP_SYSMEM_OFFSET)
        return gpu_sysmem_mmap(priv->gpu, vma);
    else
        return -EINVAL;
}

static int nvidia_release(struct inode *inode, struct file *file)
{
    struct nvidia_file_private *priv = file->private_data;

    /* Free all resources created by this file handle */
    rm_cleanup_client(priv);
    gpu_va_space_destroy(priv->va_space);
    kfree(priv);
    return 0;
}

static const struct file_operations nvidia_fops = {
    .owner          = THIS_MODULE,
    .open           = nvidia_open,
    .release        = nvidia_release,
    .unlocked_ioctl = nvidia_ioctl,
    .compat_ioctl   = nvidia_ioctl, /* 32-bit compat */
    .mmap           = nvidia_mmap,
};

/* ─── PCI probe ───────────────────────────────────────────────────────────*/
static int nvidia_pci_probe(struct pci_dev *pdev,
                              const struct pci_device_id *id)
{
    struct gpu_device *gpu;
    int ret;

    gpu = devm_kzalloc(&pdev->dev, sizeof(*gpu), GFP_KERNEL);
    if (!gpu) return -ENOMEM;

    ret = pcim_enable_device(pdev);         /* enable PCIe device          */
    if (ret) return ret;

    ret = pcim_iomap_regions(pdev, BIT(0) | BIT(1), "nvidia"); /* BAR0+BAR1 */
    if (ret) return ret;

    gpu->regs   = pcim_iomap_table(pdev)[0]; /* BAR0: registers */
    gpu->fb_bar = pcim_iomap_table(pdev)[1]; /* BAR1: framebuffer */

    dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(48));
    pci_set_master(pdev);  /* enable bus mastering for DMA */

    /* Read GPU chip ID from registers */
    gpu->chip_id = readl(gpu->regs + NV_PMC_BOOT_0);
    gpu->hal     = nvidia_hal_init(gpu->chip_id); /* select per-generation HAL */

    /* Initialize subsystems */
    gpu_vmm_init(gpu);
    gpu_ring_init(gpu);
    gpu_ce_init(gpu);  /* copy engine */

    /* Register char device */
    cdev_init(&gpu->cdev, &nvidia_fops);
    ret = cdev_add(&gpu->cdev, gpu->devno, 1);
    if (ret) return ret;

    pci_set_drvdata(pdev, gpu);
    pr_info("NVIDIA GPU 0x%x initialized at %s\n",
            gpu->chip_id, pci_name(pdev));
    return 0;
}

/* ─── PCI device table ────────────────────────────────────────────────────*/
static const struct pci_device_id nvidia_pci_table[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_NVIDIA, 0x2330) }, /* H100 SXM5 */
    { PCI_DEVICE(PCI_VENDOR_ID_NVIDIA, 0x2204) }, /* A100 SXM4 */
    { PCI_DEVICE(PCI_VENDOR_ID_NVIDIA, 0x20B2) }, /* A100 PCIe */
    { 0 }
};
MODULE_DEVICE_TABLE(pci, nvidia_pci_table);

static struct pci_driver nvidia_pci_driver = {
    .name     = "nvidia",
    .id_table = nvidia_pci_table,
    .probe    = nvidia_pci_probe,
    .remove   = nvidia_pci_remove,
    .suspend  = nvidia_pci_suspend,
    .resume   = nvidia_pci_resume,
};

module_pci_driver(nvidia_pci_driver);
```

### Component Table

| Component | File | Responsibility |
|-----------|------|----------------|
| Char device | `nv-frontend.c` | `/dev/nvidia*` open/ioctl/mmap |
| RM (Resource Manager) | `nv-rm.c` | Client/object resource lifecycle |
| HAL | `nv-hal.c` | Per-GPU-generation register programming |
| UVM | `nvidia-uvm.ko` | Unified Virtual Memory, fault handler |
| NVLink | `nv-nvlink.c` | NVLink topology, bandwidth management |
| DMA | `nv-dma.c` | DMA map/unmap, scatterlist management |
| IRQ | `nv-irq.c` | ISR, MSI-X setup, fault/completion dispatch |

---

## Explanation

### Core Concept

The NVIDIA driver is a **layered architecture**:

1. **Char device** — thin user-kernel interface: ioctl dispatch, mmap
2. **RM (Resource Manager)** — GPU resource lifecycle (channels, contexts, memory objects, events). Uses a client→object hierarchy similar to a capability system
3. **HAL (Hardware Abstraction Layer)** — per-GPU-generation register programming (Turing vs Ampere vs Hopper have different register layouts). HAL function pointers filled in at probe time based on `chip_id`
4. **Hardware** — accessed via MMIO on BAR0

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `pcim_enable_device(pdev)` | Enable PCIe device (managed, auto-disable on error) |
| `pcim_iomap_regions(pdev, mask, name)` | Map PCIe BARs (managed) |
| `dma_set_mask_and_coherent(dev, mask)` | Set DMA address mask |
| `pci_set_master(pdev)` | Enable PCIe bus mastering for DMA |
| `readl(regs + offset)` | Read GPU register |
| `cdev_init/cdev_add` | Register char device |
| `pci_set_drvdata/pci_get_drvdata` | Store/retrieve driver data per PCI device |
| `module_pci_driver(drv)` | Register PCI driver + module init/exit |

### Trade-offs & Pitfalls

- **Closed-source RM.** The RM module was historically closed-source binary blob. Since 2022, NVIDIA open-sourced the kernel modules for Turing+ GPUs (`open-gpu-kernel-modules`). The HAL is the most complex part — thousands of per-register accessor functions per GPU generation.
- **ioctl ABI compatibility.** The RM ioctl interface is a private ABI between the kernel module and CUDA runtime (`libcuda.so`). They must always match versions. Breaking the ioctl ABI requires updating both kernel and userspace atomically.

### NVIDIA / GPU Context

Real NVIDIA driver modules:
- `nvidia.ko` — main driver (RM, char device, HAL)
- `nvidia-uvm.ko` — UVM (Unified Virtual Memory)
- `nvidia-modeset.ko` — display subsystem (modesetting, HDMI/DP)
- `nvidia-drm.ko` — DRM (Direct Rendering Manager) integration for Wayland/X11

---

## Cross Questions & Answers

**CQ1: What is the RM (Resource Manager) and why is it the complexity center of the NVIDIA driver?**
> The RM manages a hierarchy of GPU resources: Client → Device → Channel → Memory Object. Each resource has a handle, lifecycle management, and access control. The RM implements a capability system: a client can only access resources it "owns" (via handles). The complexity comes from: (1) supporting 50+ GPU generations with the HAL, (2) managing shared resources between concurrent CUDA processes, (3) handling GPU resets without losing client state, (4) NVENC/NVDEC/Copy Engine resource management alongside compute. The RM is ~1M lines of C.

**CQ2: How does the HAL work in the NVIDIA driver?**
> The HAL is a struct of function pointers, one per hardware operation (e.g., `hal->gmmu_map_page = ga100_gmmu_map_page` for A100). At `pci_probe` time, `nvidia_hal_init(chip_id)` reads the GPU's chip ID from `NV_PMC_BOOT_0` and fills the HAL struct with the correct per-generation implementations. All MMIO accesses go through HAL function pointers — no direct `writel(reg, SPECIFIC_OFFSET)` outside the HAL. This allows the same RM code to run on Turing, Ampere, Hopper without #ifdef.

**CQ3: What are the three `/dev/nvidia*` devices and what is each used for?**
> `/dev/nvidia0..N`: per-GPU device files, one per physical GPU. CUDA runtime opens the device for its assigned GPU. `/dev/nvidiactl`: control device, opened by nvidia-smi and other management tools for global driver control (does not correspond to a specific GPU). `/dev/nvidia-uvm`: opened by the CUDA runtime to enable UVM; managed by the nvidia-uvm.ko module. `/dev/nvidia-modeset`: display path, opened by the X server and Wayland compositors.

**CQ4: How does the NVIDIA driver handle multiple CUDA processes sharing a GPU?**
> Each process opens `/dev/nvidia0` independently. `nvidia_open` creates a separate `nvidia_file_private` with its own VA space and RM client. The RM tracks all clients and their allocations separately. CUDA contexts from different processes are isolated at the GMMU level (different page table roots). The RM's resource manager handles contention: GPU time is shared via the GPU hardware scheduler's TSG (Time Slice Group) mechanism, one TSG per CUDA context.

**CQ5: What is the significance of `pci_set_master(pdev)` for GPU DMA?**
> `pci_set_master` enables the PCIe device to act as a **bus master** — initiating DMA transactions without CPU involvement. Without it, the GPU's DMA engine cannot autonomously transfer data; it can only respond to CPU-initiated transactions. For CUDA `cuMemcpyDtoH`, the GPU's copy engine initiates a DMA read from GPU VRAM and writes to system DRAM — this is only possible with bus mastering enabled. Modern PCIe devices cannot function without bus mastering for any DMA operation.

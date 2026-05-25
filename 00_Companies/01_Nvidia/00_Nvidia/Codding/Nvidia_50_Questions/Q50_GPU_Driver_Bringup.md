# Q50: GPU Driver Bring-Up (Full PCI Probe)

**Section:** System Design | **Difficulty:** Hard | **Topics:** PCI probe, `pcim_enable_device`, BAR mapping, DMA mask, MSI, interrupt handler, char device, `module_pci_driver`, error unwind

---

## Question

Implement a complete GPU driver bring-up sequence from `pci_probe` to first command submission, including all initialization subsystems and proper error unwind.

---

## Answer

```c
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>

/* ─── GPU PCI Device IDs ──────────────────────────────────────────────────
 * NVIDIA vendor ID = 0x10DE
 * H100 SXM5:   device ID 0x2330
 * A100 SXM4:   device ID 0x2204
 */
#define NVIDIA_VENDOR_ID  0x10DE
#define GPU_DEV_H100_SXM  0x2330
#define GPU_DEV_A100_SXM  0x2204

/* ─── GPU Device Structure ────────────────────────────────────────────────*/
#define GPU_MAX_IRQ       16
#define GPU_CHRDEV_MINOR  0

struct gpu_device {
    struct pci_dev     *pdev;

    /* BAR mappings */
    void __iomem       *bar0;   /* registers + push buffer ring */
    void __iomem       *bar1;   /* framebuffer (GPU VRAM)        */

    /* GPU identity */
    u32                 gpu_id;
    u32                 vbios_version;

    /* Subsystems */
    struct gpu_ring      ring;
    struct gpu_vmm       vmm;
    struct gpu_dma_engine dma;

    /* Interrupts */
    int                  num_irq;
    int                  irq[GPU_MAX_IRQ];

    /* Character device */
    struct cdev          cdev;
    dev_t                devno;

    /* State */
    bool                 initialized;
    atomic_t             ref_count;
};

/* ─── Forward declarations ────────────────────────────────────────────────*/
static int  gpu_vmm_init(struct gpu_device *gpu);
static int  gpu_ring_init_dev(struct gpu_device *gpu);
static int  gpu_dma_engine_init(struct gpu_device *gpu);
static int  gpu_chrdev_register(struct gpu_device *gpu);
static void gpu_chrdev_unregister(struct gpu_device *gpu);
static void gpu_dma_engine_fini(struct gpu_device *gpu);
static void gpu_ring_fini(struct gpu_device *gpu);
static void gpu_vmm_fini(struct gpu_device *gpu);

/* ─── Interrupt handler ───────────────────────────────────────────────────*/
static irqreturn_t gpu_irq_handler(int irq, void *data)
{
    struct gpu_device *gpu = data;
    u32 status;

    /* Read interrupt status register */
    status = readl(gpu->bar0 + 0x100);
    if (!status)
        return IRQ_NONE; /* spurious interrupt */

    /* Clear interrupts */
    writel(status, gpu->bar0 + 0x104);

    if (status & BIT(0))       /* command ring completion */
        gpu_ring_completion_irq(&gpu->ring);
    if (status & BIT(1))       /* DMA engine done */
        gpu_dma_completion_irq(&gpu->dma);
    if (status & BIT(2))       /* fault */
        schedule_work(&gpu->vmm.fault_work);

    return IRQ_HANDLED;
}

/* ─── Smoke test: submit NOP command ──────────────────────────────────────*/
#define GPU_NOP_CMD  0x0000000000000000ULL

static int gpu_smoke_test(struct gpu_device *gpu)
{
    int ret;

    ret = ring_submit(&gpu->ring, GPU_NOP_CMD);
    if (ret) {
        dev_err(&gpu->pdev->dev, "Smoke test: NOP submit failed: %d\n", ret);
        return ret;
    }

    dev_info(&gpu->pdev->dev, "Smoke test: NOP submitted OK\n");
    return 0;
}

/* ─── PCI Probe: full 12-step GPU bring-up ────────────────────────────────
 *
 * Error path uses goto labels for deterministic cleanup.
 * Each label undoes exactly the work done up to that point.
 */
static int gpu_pci_probe(struct pci_dev *pdev,
                          const struct pci_device_id *id)
{
    struct gpu_device *gpu;
    int ret;

    dev_info(&pdev->dev, "GPU probe: devid=0x%04x\n", pdev->device);

    /* ── Step 1: Allocate device structure ─────────────────────────────
     * devm_kzalloc: device-managed allocation; freed automatically on
     * device removal — no explicit kfree needed in error path.
     */
    gpu = devm_kzalloc(&pdev->dev, sizeof(*gpu), GFP_KERNEL);
    if (!gpu)
        return -ENOMEM;

    gpu->pdev = pdev;
    pci_set_drvdata(pdev, gpu);
    atomic_set(&gpu->ref_count, 1);

    /* ── Step 2: Enable PCI device ──────────────────────────────────────
     * pcim_enable_device: device-managed version of pci_enable_device.
     * Enables I/O and memory access, activates bus mastering.
     * Auto-disabled on driver detach.
     */
    ret = pcim_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "pcim_enable_device failed: %d\n", ret);
        return ret; /* devm frees gpu */
    }

    /* ── Step 3: Map BARs ───────────────────────────────────────────────
     * pcim_iomap_regions: request + iomap BARs 0 and 1.
     * BIT(0)|BIT(1) selects BAR0 and BAR1.
     * Returns an array of mapped addresses.
     */
    ret = pcim_iomap_regions(pdev, BIT(0) | BIT(1), "gpu_driver");
    if (ret) {
        dev_err(&pdev->dev, "pcim_iomap_regions failed: %d\n", ret);
        goto err_disable;
    }
    gpu->bar0 = pcim_iomap_table(pdev)[0];
    gpu->bar1 = pcim_iomap_table(pdev)[1];

    /* ── Step 4: Set DMA mask ───────────────────────────────────────────
     * GPU supports 48-bit physical addressing (DMA_BIT_MASK(48)).
     * dma_set_mask_and_coherent sets both streaming and coherent masks.
     * Must match GPU's actual IOV/ATS capabilities.
     */
    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(48));
    if (ret) {
        dev_err(&pdev->dev, "DMA mask set failed: %d\n", ret);
        goto err_unmap;
    }

    /* ── Step 5: Enable bus mastering ──────────────────────────────────
     * Allows GPU to initiate DMA transactions on PCIe bus.
     * Must be called before DMA transfers.
     */
    pci_set_master(pdev);

    /* ── Step 6: Read GPU identity from BAR0 ───────────────────────────
     * GPU chip ID register at BAR0 + 0x000 (BOOT_0).
     * Upper 8 bits = GPU family, lower bits = chip revision.
     */
    gpu->gpu_id = readl(gpu->bar0 + 0x000);
    dev_info(&pdev->dev, "GPU ID: 0x%08x\n", gpu->gpu_id);

    /* ── Step 7: Initialize Virtual Memory Manager ──────────────────────
     * Sets up GPU page tables, VA space allocator, fault handler.
     */
    ret = gpu_vmm_init(gpu);
    if (ret) {
        dev_err(&pdev->dev, "VMM init failed: %d\n", ret);
        goto err_unmap;
    }

    /* ── Step 8: Initialize Command Ring ────────────────────────────────
     * Allocates push buffer ring mapped to BAR0 MMIO.
     * Sets up doorbell register for command submission.
     */
    ret = gpu_ring_init_dev(gpu);
    if (ret) {
        dev_err(&pdev->dev, "Ring init failed: %d\n", ret);
        goto err_vmm;
    }

    /* ── Step 9: Initialize DMA Engine ─────────────────────────────────
     * Sets up copy engine channels for P2P and host DMA.
     */
    ret = gpu_dma_engine_init(gpu);
    if (ret) {
        dev_err(&pdev->dev, "DMA engine init failed: %d\n", ret);
        goto err_ring;
    }

    /* ── Step 10: Set up MSI interrupts ────────────────────────────────
     * pci_alloc_irq_vectors: allocate MSI/MSI-X vectors.
     * Request 1..MAX_IRQ vectors, prefer MSI-X.
     * Returns number of vectors allocated.
     */
    ret = pci_alloc_irq_vectors(pdev, 1, GPU_MAX_IRQ, PCI_IRQ_MSI | PCI_IRQ_MSIX);
    if (ret < 0) {
        dev_err(&pdev->dev, "IRQ vector alloc failed: %d\n", ret);
        goto err_dma;
    }
    gpu->num_irq = ret;

    /*
     * devm_request_irq: device-managed IRQ request.
     * IRQF_SHARED: allows sharing (for MSI: not needed but harmless).
     */
    for (int i = 0; i < gpu->num_irq; i++) {
        gpu->irq[i] = pci_irq_vector(pdev, i);
        ret = devm_request_irq(&pdev->dev, gpu->irq[i],
                                gpu_irq_handler, 0,
                                "gpu_irq", gpu);
        if (ret) {
            dev_err(&pdev->dev, "request_irq(%d) failed: %d\n",
                    gpu->irq[i], ret);
            goto err_irq;
        }
    }

    /* Enable GPU interrupt generation */
    writel(0xFFFFFFFF, gpu->bar0 + 0x108); /* interrupt enable mask */
    wmb();

    /* ── Step 11: Register character device ─────────────────────────────
     * Exposes /dev/gpuN to userspace for ioctl-based control.
     */
    ret = gpu_chrdev_register(gpu);
    if (ret) {
        dev_err(&pdev->dev, "chrdev register failed: %d\n", ret);
        goto err_irq;
    }

    /* ── Step 12: Smoke test ─────────────────────────────────────────────
     * Submit a NOP command to verify ring + interrupt path works.
     * This is the first command submitted to the GPU.
     */
    ret = gpu_smoke_test(gpu);
    if (ret)
        goto err_chrdev;

    gpu->initialized = true;
    dev_info(&pdev->dev, "GPU initialized: id=0x%08x, irqs=%d\n",
             gpu->gpu_id, gpu->num_irq);
    return 0;

/* ─── Error unwind (reverse order of initialization) ─────────────────── */
err_chrdev:
    gpu_chrdev_unregister(gpu);
err_irq:
    pci_free_irq_vectors(pdev);
err_dma:
    gpu_dma_engine_fini(gpu);
err_ring:
    gpu_ring_fini(gpu);
err_vmm:
    gpu_vmm_fini(gpu);
err_unmap:
    /* pcim_iomap_regions auto-unmapped by pcim_ framework */
err_disable:
    /* pcim_enable_device auto-disabled on driver detach */
    return ret;
}

/* ─── PCI Remove: reverse of probe ──────────────────────────────────────*/
static void gpu_pci_remove(struct pci_dev *pdev)
{
    struct gpu_device *gpu = pci_get_drvdata(pdev);

    if (!gpu || !gpu->initialized)
        return;

    /* Disable GPU interrupt generation first */
    writel(0, gpu->bar0 + 0x108);
    wmb();

    gpu_chrdev_unregister(gpu);
    pci_free_irq_vectors(pdev);   /* devm_request_irq auto-freed */
    gpu_dma_engine_fini(gpu);
    gpu_ring_fini(gpu);
    gpu_vmm_fini(gpu);
    /* BAR unmap and device disable handled by pcim_ managed resources */

    dev_info(&pdev->dev, "GPU removed\n");
}

/* ─── PCI Device ID Table ─────────────────────────────────────────────────
 * Matches H100 SXM5 and A100 SXM4.
 * Kernel uses this table to bind driver to device at probe time.
 */
static const struct pci_device_id gpu_pci_ids[] = {
    { PCI_DEVICE(NVIDIA_VENDOR_ID, GPU_DEV_H100_SXM) },
    { PCI_DEVICE(NVIDIA_VENDOR_ID, GPU_DEV_A100_SXM) },
    { 0, }  /* terminator */
};
MODULE_DEVICE_TABLE(pci, gpu_pci_ids);

/* ─── PCI Driver Structure ────────────────────────────────────────────────*/
static struct pci_driver gpu_pci_driver = {
    .name     = "gpu_driver",
    .id_table = gpu_pci_ids,
    .probe    = gpu_pci_probe,
    .remove   = gpu_pci_remove,
};

/* ─── Module Init/Exit ───────────────────────────────────────────────────
 * module_pci_driver: convenience macro that generates
 * module_init(gpu_pci_driver_init) and module_exit(gpu_pci_driver_exit)
 * which call pci_register_driver / pci_unregister_driver.
 */
module_pci_driver(gpu_pci_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GPU Driver Team");
MODULE_DESCRIPTION("NVIDIA GPU PCIe Driver");
MODULE_VERSION("1.0");
```

---

## Explanation

### Core Concept

```
Full GPU Bring-Up Sequence (12 steps):

  1. devm_kzalloc()            → allocate gpu_device struct
  2. pcim_enable_device()      → enable PCIe device
  3. pcim_iomap_regions()      → map BAR0 (regs) + BAR1 (VRAM)
  4. dma_set_mask_and_coherent() → configure 48-bit DMA
  5. pci_set_master()          → enable bus mastering (DMA)
  6. readl(bar0 + BOOT_0)      → read GPU chip ID
  7. gpu_vmm_init()            → page tables, VA allocator, fault handler
  8. gpu_ring_init()           → command push buffer + doorbell
  9. gpu_dma_engine_init()     → copy engines
 10. pci_alloc_irq_vectors()   → MSI/MSI-X + devm_request_irq
 11. gpu_chrdev_register()     → /dev/gpu0
 12. ring_submit(NOP)          → smoke test

Error unwind: reverse order — goto labels ensure deterministic cleanup.
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `devm_kzalloc` | Device-managed alloc; auto-freed on device remove |
| `pcim_enable_device` | Device-managed PCI enable; auto-disabled on detach |
| `pcim_iomap_regions` | Request + map BARs; auto-unmapped |
| `dma_set_mask_and_coherent` | Set DMA address limit for streaming + coherent |
| `pci_set_master` | Enable PCIe bus mastering for DMA |
| `readl(bar0 + offset)` | Read 32-bit MMIO register |
| `pci_alloc_irq_vectors` | Allocate MSI or MSI-X interrupt vectors |
| `devm_request_irq` | Device-managed IRQ registration; auto-freed |
| `pci_irq_vector(pdev, i)` | Get Linux IRQ number for vector i |
| `module_pci_driver` | Generates `module_init`/`module_exit` for PCI driver |
| `MODULE_DEVICE_TABLE` | Export device IDs for `modprobe` auto-loading |
| `PCI_DEVICE(vendor, device)` | Shorthand for PCI ID table entry |

### Trade-offs & Pitfalls

- **`devm_` vs manual cleanup.** Using `devm_kzalloc`, `pcim_enable_device`, `pcim_iomap_regions`, `devm_request_irq` eliminates most error-path cleanup — these are automatically unwound on `pci_dev` release. Only subsystems not using `devm_` need explicit cleanup labels.
- **DMA mask: 48-bit vs 64-bit.** Modern NVIDIA GPUs use IOVA (I/O virtual address) spaces that may be limited to 48 bits by the IOMMU or GPU firmware. Using `DMA_BIT_MASK(64)` when the hardware supports only 48-bit addressing can result in failed DMA mappings on systems with >256TB physical memory.
- **Smoke test placement.** Running the NOP smoke test before returning from probe is critical — it validates the full path: ring write → doorbell → GPU fetch → IRQ completion. Deferred testing (after probe returns) makes initialization failures harder to attribute to specific subsystems.

### NVIDIA / GPU Context

NVIDIA's open-source kernel module (r535+ drivers) follows exactly this pattern. The `nv_pci_probe` function in `kernel-open/nvidia/nv.c` allocates `nv_state_t`, calls `pcim_enable_device`, maps BARs, sets DMA masks, and registers char devices under `/dev/nvidia0`. MSI interrupts are allocated via `pci_alloc_irq_vectors`. The `module_init`/`module_exit` are wrapped by `module_pci_driver(nv_pci_driver)`.

---

## Cross Questions & Answers

**CQ1: What is the difference between `devm_` managed resources and manual cleanup?**
> `devm_` (device-managed) resources: registered with a devres list attached to the `struct device`. When the device is released (`device_release` or driver removal), all devres entries are unwound in reverse-registration order automatically. Manual cleanup: requires explicit calls in both the error path and `remove`. `devm_` reduces error-path bugs: missing cleanup on one of 15 goto labels is a common source of resource leaks in kernel drivers. However, `devm_` adds overhead (one kmalloc per resource registration) and prevents cleanup before device release (can't partially free early). For GPU drivers: use `devm_` for PCI setup, manual management for complex GPU subsystems that have inter-dependencies.

**CQ2: What does `pci_set_master` do and why is it required before DMA?**
> PCIe/PCI bus mastering: the CPU/bridge allows a device to initiate transactions on the bus (DMA). By default, a new PCI device cannot initiate bus transactions — it can only respond to CPU reads/writes. `pci_set_master(pdev)`: sets the `PCI_COMMAND_MASTER` bit in the device's PCI command register (config space offset 0x04). Without this: GPU DMA write transactions will be silently dropped or cause a PCIe error, resulting in data corruption (CPU sees zeros) or a machine check. Must be called after `pcim_enable_device` but before any DMA transfer.

**CQ3: What is `MODULE_DEVICE_TABLE` and how does `modprobe` use it?**
> `MODULE_DEVICE_TABLE(pci, gpu_pci_ids)`: embeds the PCI device ID table into the `.modalias` section of the kernel module `.ko` file. The `depmod` tool reads all `.ko` files and builds `/lib/modules/$(uname -r)/modules.alias` — a mapping from PCI device IDs to module names. When the kernel discovers a new PCI device (at boot or hot-plug), it generates a uevent with `MODALIAS=pci:v0000xxxx...`. `udev` reads this, searches `modules.alias`, and calls `modprobe` with the matching module name. The result: GPU is discovered → kernel auto-loads `gpu_driver.ko` without any manual configuration.

**CQ4: How do you handle PCI error recovery (AER) in a GPU driver?**
> AER (Advanced Error Reporting): PCIe mechanism for detecting and recovering from PCIe errors (uncorrectable errors: link failure, DMA read error; correctable errors: bit flips, retries exceeded). PCI driver error hooks: `struct pci_error_handlers { .error_detected, .mmio_enabled, .slot_reset, .resume }`. `error_detected(pdev, state)`: GPU driver must stop all DMA and IRQs, return `PCI_ERS_RESULT_NEED_RESET`. `slot_reset(pdev)`: called after hardware reset — reinitialize GPU (steps 7–12 of probe). `resume(pdev)`: notify users (CUDA contexts, UVM) that GPU is back. NVIDIA's driver implements full AER recovery to allow GPU reset without server reboot.

**CQ5: What is FLR (Function Level Reset) and when is it used for GPUs?**
> FLR: PCIe capability that resets a single PCI function (one GPU) without resetting the entire PCIe bus or other devices. Triggered by: writing `1` to `PCI_EXP_DEVCTL_BCR_FLR` bit in the PCIe device control register. Effect: GPU returns to power-on state (all registers reset, DMA stopped, IRQs cleared). Used for: (1) GPU error recovery (after a GPU hang), (2) SR-IOV: reset a VF before assigning it to a new VM, (3) driver unbind/rebind. After FLR: driver must re-run the full probe sequence (steps 1–12). Important: FLR requires bus mastering to be cleared before issuing reset, and the driver must quiesce all DMA before triggering FLR (otherwise in-flight DMA may corrupt host memory).

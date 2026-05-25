# Resource Mapping in a Linux Character Driver
*(How device registers / memory get mapped into a char driver, and then optionally into user space)*

When a character device sits on top of real hardware (UART, GPIO block, FPGA, PCI BAR, SoC IP block), the driver needs to reach the device's **registers** or **on-chip memory**. Those live at **physical addresses** the CPU cannot touch directly — the MMU only understands virtual addresses, and the kernel will refuse uncoordinated access. So the driver must:

1. **Discover** the resource (DT / ACPI / PCI / platform data).
2. **Claim** it (so two drivers can't fight over it).
3. **Map** it into the kernel's virtual address space (`ioremap`).
4. **Access** it via the proper accessors (`readl/writel`, `regmap`).
5. *(Optional)* Expose it to user space through the char device's `mmap` file op.

Below is the complete picture.

---

## 1. The Address Spaces Involved

```
 ┌──────────────────────────┐       ┌──────────────────────────┐
 │  User virtual address    │       │  Kernel virtual address  │
 │  (per process, via mmap) │       │  (global, via ioremap)   │
 └──────────────┬───────────┘       └─────────────┬────────────┘
                │ MMU                              │ MMU
                ▼                                  ▼
        ┌────────────────────────────────────────────────┐
        │       Physical address (bus address)           │
        │   e.g. 0xFE20_0000 = GPIO controller on a SoC  │
        └────────────────────────────────────────────────┘
                              │
                              ▼
                       Device registers
```

You never dereference a physical address in C. You either:

- Ask the kernel to **map physical → kernel-virtual** with `ioremap()`, then use `readl()/writel()`, **or**
- Ask the kernel to **map physical → user-virtual** through the char device's `mmap` op with `remap_pfn_range()` / `io_remap_pfn_range()`.

---

## 2. The `struct resource` Model

The kernel describes every hardware resource (MMIO range, I/O port range, IRQ, DMA channel, bus number) with:

```c
struct resource {
    resource_size_t start;
    resource_size_t end;     // inclusive
    const char     *name;
    unsigned long   flags;   // IORESOURCE_MEM, IORESOURCE_IO, IORESOURCE_IRQ ...
    struct resource *parent, *sibling, *child;
};
```

Resources form a tree rooted at `iomem_resource` (visible as `/proc/iomem`) and `ioport_resource` (`/proc/ioports`). Claiming a resource means inserting a child node into that tree, which is what prevents conflicts.

---

## 3. Where Resources Come From

| Source | How the driver gets them |
|--------|--------------------------|
| **Device Tree** (ARM/ARM64/RISC-V SoCs) | `platform_get_resource()`, `platform_get_mem_or_io()`, `of_iomap()` |
| **ACPI** (x86 embedded, ARM servers) | Same `platform_*` API; ACPI fills `struct resource` |
| **PCI / PCIe** | `pci_resource_start/len()`, `pci_iomap()`, BARs |
| **Legacy / board file** | Static `struct resource[]` in board code |
| **ISA / hard-coded** | `request_mem_region()` directly with a constant |

Example DT node consumed by a platform driver:

```dts
myip@fe200000 {
    compatible = "acme,myip-v1";
    reg = <0xfe200000 0x1000>;   /* phys base, size */
    interrupts = <GIC_SPI 42 IRQ_TYPE_LEVEL_HIGH>;
};
```

The platform core converts `reg` into a `struct resource` with `flags = IORESOURCE_MEM`, `start = 0xfe200000`, `end = 0xfe200fff`.

---

## 4. The Canonical Probe Sequence (Platform Driver + Char Device)

```c
struct mydev {
    void __iomem   *regs;        // kernel-virtual pointer to MMIO
    struct resource *mem;        // claimed region
    int             irq;
    struct cdev     cdev;
    dev_t           devt;
    struct device  *dev;
};

static int mydev_probe(struct platform_device *pdev)
{
    struct mydev *d;
    struct resource *res;

    d = devm_kzalloc(&pdev->dev, sizeof(*d), GFP_KERNEL);
    if (!d)
        return -ENOMEM;

    /* 1. Discover the MMIO resource */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res)
        return -ENODEV;

    /* 2 + 3. Claim AND map in one call (preferred modern API) */
    d->regs = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(d->regs))
        return PTR_ERR(d->regs);

    /* 4. IRQ */
    d->irq = platform_get_irq(pdev, 0);
    if (d->irq < 0)
        return d->irq;

    ret = devm_request_irq(&pdev->dev, d->irq, mydev_isr,
                           0, dev_name(&pdev->dev), d);
    if (ret)
        return ret;

    /* 5. Register the character device */
    ret = alloc_chrdev_region(&d->devt, 0, 1, "mydev");
    cdev_init(&d->cdev, &mydev_fops);
    cdev_add(&d->cdev, d->devt, 1);
    device_create(mydev_class, &pdev->dev, d->devt, d, "mydev0");

    platform_set_drvdata(pdev, d);
    return 0;
}
```

`devm_ioremap_resource()` is the modern one-shot that does **both**:

- `request_mem_region(start, size, name)` → inserts into `iomem_resource` (you'll see it in `/proc/iomem`).
- `ioremap(start, size)` → creates kernel page-table entries marking the range as **device memory** (uncached, no speculation, no write-combining unless asked).

It is also **device-managed** (`devm_`) so the kernel auto-releases it on driver detach or probe failure.

Manual equivalent (older code):

```c
if (!request_mem_region(res->start, resource_size(res), "mydev"))
    return -EBUSY;
d->regs = ioremap(res->start, resource_size(res));
if (!d->regs) { release_mem_region(...); return -ENOMEM; }
```

---

## 5. Accessing the Mapped Registers

After `ioremap`, **never** do `*(u32 *)d->regs = 1;` — it works on x86 but is wrong on ARM/PowerPC (endian, ordering, compiler reordering). Always use the accessors:

```c
#define REG_CTRL   0x00
#define REG_STATUS 0x04
#define REG_DATA   0x08

writel(0x1,  d->regs + REG_CTRL);     // 32-bit write, with barriers
u32 s = readl(d->regs + REG_STATUS);  // 32-bit read
writeb(0xaa, d->regs + REG_DATA);
```

Variants:

| Accessor | Meaning |
|----------|---------|
| `readb/w/l/q`, `writeb/w/l/q` | Little-endian, full memory barrier — safe default |
| `readl_relaxed`, `writel_relaxed` | No ordering — for hot loops where you add your own barriers |
| `ioread32`, `iowrite32` | Work on both MMIO and PIO (returned by `pci_iomap`) |
| `memcpy_toio` / `memcpy_fromio` | Bulk copy to/from MMIO |
| `__iomem` annotation | Sparse-checked tag that marks the pointer as device memory; mixing it with normal pointers triggers warnings |

### Why this matters

`ioremap()` maps the range with **PROT_DEVICE / strongly-ordered / non-cacheable** PTE attributes. The accessors add the right `dmb/dsb` (ARM) or `mfence`-equivalent barriers so:

- Writes hit the device in program order.
- The CPU doesn't speculatively load from MMIO.
- Compiler can't reorder or merge accesses (the accessors use `volatile` + barriers).

---

## 6. Variants of `ioremap`

| Function | Caching attribute | When to use |
|----------|------------------|-------------|
| `ioremap()` | Uncached, strongly ordered | **Registers** — the normal case |
| `ioremap_wc()` | Write-combining | Framebuffers, GPU command rings |
| `ioremap_wt()` | Write-through | Rare |
| `ioremap_cache()` | Cached | Reserved RAM you want as regular memory |
| `memremap(..., MEMREMAP_WB/WT/WC)` | RAM-like mappings | Persistent memory, reserved RAM regions (NOT for registers) |
| `pci_iomap(pdev, bar, len)` | PCI BAR; auto-picks MMIO vs PIO | PCI/PCIe drivers |
| `of_iomap(np, index)` | DT shortcut: parse `reg` + ioremap | Simple OF drivers |

Rule of thumb: **device registers → `ioremap`**, **RAM-backed regions → `memremap`**. Mixing them up causes machine checks on some platforms.

---

## 7. PCI Variant

```c
static int mypci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    void __iomem *regs;

    pci_enable_device(pdev);
    pci_request_regions(pdev, "mypci");        // claims all BARs
    regs = pci_iomap(pdev, 0, 0);              // map BAR0 (full length)
    pci_set_master(pdev);                      // enable bus-mastering DMA
    ...
}
```

`pci_iomap` returns an `__iomem` cookie usable with `ioread32`/`iowrite32`, regardless of whether the BAR is memory-mapped or port-mapped.

---

## 8. Exposing the Mapping to User Space — the char-device `mmap` Op

Sometimes you want user space to bang the registers directly (zero-copy, low-latency: video buffers, FPGA scratch RAM, performance counters). You implement `file_operations::mmap`:

```c
static int mydev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct mydev *d = filp->private_data;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn  = d->mem->start >> PAGE_SHIFT;   // physical page frame #
    unsigned long off  = vma->vm_pgoff << PAGE_SHIFT;   // user-requested offset

    /* --- Security: bounds-check! Never trust the user range --- */
    if (off + size > resource_size(d->mem))
        return -EINVAL;

    pfn += vma->vm_pgoff;

    /* Mark VMA as I/O memory: no swap, no core-dump, no merge */
    vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;

    /* Non-cacheable mapping — same attributes as ioremap() */
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    /* Build user PTEs that point at the device's physical pages */
    if (io_remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
        return -EAGAIN;

    return 0;
}

static const struct file_operations mydev_fops = {
    .owner   = THIS_MODULE,
    .open    = mydev_open,
    .release = mydev_release,
    .read    = mydev_read,
    .write   = mydev_write,
    .unlocked_ioctl = mydev_ioctl,
    .mmap    = mydev_mmap,
};
```

User side:

```c
int fd = open("/dev/mydev0", O_RDWR);
volatile uint32_t *regs = mmap(NULL, 0x1000,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, 0);
regs[0] = 1;                  // writes straight to device register
```

### What `io_remap_pfn_range` actually does

It walks the process's page tables and inserts PTEs that translate the requested user-virtual range to the device's **physical** page frames, with the protection bits given (`pgprot_noncached` here). From then on, CPU loads/stores from those virtual addresses go straight to the device without trapping into the kernel.

Two near-identical helpers:

| Helper | Use for |
|--------|---------|
| `remap_pfn_range()` | Reserved **RAM** pages (e.g., DMA-coherent buffer allocated by the driver) |
| `io_remap_pfn_range()` | **MMIO** / device memory (adds arch-specific attributes) |

### Security & correctness checklist for `mmap`

- Validate `vma->vm_pgoff` and `size` against the resource length (otherwise the user can map kernel/other-device memory — a classic CVE pattern).
- Set `VM_IO | VM_PFNMAP` so the VM core knows there's no `struct page` backing it (prevents `get_user_pages`, swap, fork-COW issues).
- Use `pgprot_noncached()` for registers, `pgprot_writecombine()` for framebuffer-like memory.
- Decide whether `MAP_SHARED` is required; reject `MAP_PRIVATE` if it makes no sense.
- Enforce capability checks (`capable(CAP_SYS_RAWIO)`) if exposing raw hardware to non-root.

---

## 9. DMA-Coherent Buffers (the other "memory mapping")

If the char driver allocates a buffer the device DMAs into, and you want user space to read it without a copy:

```c
/* In probe / open: */
d->dma_va = dma_alloc_coherent(&pdev->dev, SIZE, &d->dma_pa, GFP_KERNEL);

/* In mmap: */
return dma_mmap_coherent(&pdev->dev, vma, d->dma_va, d->dma_pa, SIZE);
```

`dma_mmap_coherent` is the DMA-API-aware equivalent of `remap_pfn_range` and uses the right cache attributes for that platform (some need non-cacheable, others coherent).

---

## 10. Teardown (Mirror Image)

```c
static void mydev_remove(struct platform_device *pdev)
{
    struct mydev *d = platform_get_drvdata(pdev);

    device_destroy(mydev_class, d->devt);
    cdev_del(&d->cdev);
    unregister_chrdev_region(d->devt, 1);
    /* devm_ioremap_resource + devm_request_irq auto-released here */
}
```

If you used manual APIs you must:

```c
iounmap(d->regs);
release_mem_region(res->start, resource_size(res));
free_irq(d->irq, d);
```

Order matters: unmap **after** the device is quiesced (no more ISRs, no in-flight DMA), otherwise you can crash on a stray interrupt that touches a freed mapping.

---

## 11. Visualizing the Whole Picture

```
Device Tree / ACPI / PCI ──► struct resource (phys range)
                                  │
                                  ▼
                  request_mem_region()   ─► /proc/iomem entry
                                  │
                                  ▼
                       ioremap(phys, size)
                                  │
                                  ▼
                  void __iomem *regs (kernel-virtual)
                                  │
                ┌─────────────────┴──────────────────┐
                ▼                                    ▼
   Kernel access via readl/writel        Char device fops->mmap
                                                    │
                                          io_remap_pfn_range
                                                    │
                                                    ▼
                                User process: volatile u32 *p = mmap(...)
                                p[0] = 1;   // straight to silicon
```

---

## 12. Quick API Cheat Sheet

| Need | Modern API |
|------|------------|
| Get MMIO resource | `platform_get_resource(pdev, IORESOURCE_MEM, idx)` |
| Claim + map | `devm_ioremap_resource(dev, res)` |
| Map a DT `reg` directly | `devm_platform_ioremap_resource(pdev, idx)` |
| Map PCI BAR | `pcim_iomap_regions()` + `pcim_iomap_table()` |
| Access registers | `readl/writel`, `ioread32/iowrite32` |
| Structured register access (bitfields, caching) | **regmap** subsystem (`regmap_init_mmio`) |
| Expose to user space | `.mmap = ...` with `io_remap_pfn_range` |
| DMA buffer to user | `dma_alloc_coherent` + `dma_mmap_coherent` |
| Verify it worked | `cat /proc/iomem`, `cat /proc/<pid>/maps`, `devmem2` (debug only) |

---

## 13. Common Pitfalls

1. **Dereferencing the `__iomem` pointer directly** — works on x86, breaks elsewhere.
2. **Forgetting `request_mem_region`** — two drivers can map the same range; chaos.
3. **Caching attributes mismatch** — mapping registers as cacheable causes reads to return stale CPU-cache data; mapping RAM as uncached destroys performance.
4. **`remap_pfn_range` on MMIO** — use `io_remap_pfn_range`.
5. **Not bounds-checking `vm_pgoff`/size in `mmap`** — privilege escalation.
6. **Tearing down in the wrong order** — `iounmap` before disabling IRQ/DMA leads to use-after-free in interrupt context.
7. **Holding a spinlock across MMIO that's behind a slow bus** — stalls the CPU; use mutexes if the operation can sleep, or `_relaxed` accessors + explicit barriers in tight paths.

---

### TL;DR

A character driver maps device resources by (a) discovering a `struct resource`, (b) claiming the physical range with `request_mem_region` (usually folded into `devm_ioremap_resource`), (c) producing a `void __iomem *` via `ioremap` for in-kernel access through `readl/writel`, and optionally (d) re-projecting those same physical pages into a user process by implementing `file_operations::mmap` with `io_remap_pfn_range` and the right `pgprot_*` attributes — always after validating the user-supplied offset and size.

If you'd like, I can drill into any one of: **regmap**, **PCI BAR mapping end-to-end**, **DMA-coherent buffers vs streaming DMA**, or a **complete compilable example driver** with a matching DT overlay and user test program. Just say which.
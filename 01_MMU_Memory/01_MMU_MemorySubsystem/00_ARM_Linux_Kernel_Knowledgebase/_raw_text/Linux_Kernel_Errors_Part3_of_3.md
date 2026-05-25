Linux Kernel & Userspace Runtime Errors
Part 3 of 3  —  Pages 41–60
Qualcomm Interview Preparation
Prepared for: Sandeep Kumar
Focus: ARM64 / Qualcomm SoC Platform Drivers

Coverage — Part 3
| # | Section | Topics Covered |
| 15 | Resource Leaks | Why critical on Qualcomm ARM, BAD/FIX probe examples, resource types table, devm_* APIs, PCIe hot-plug, detection tools |
| 16 | Incorrect Register Access | ARM MMIO basics, 6 scenarios (offset/size/endianness/alignment/RMW/barrier), access functions table, endianness quick-ref |
| 17 | Page Table Corruption | ARM64 PT hierarchy, PTE format, 6 corruption scenarios, bit fields table, symptoms table, detection tools |
| 18 | Incorrect mmap | Driver mmap flow, 6 scenarios (VM_IO/caching/validation/fault/permissions/fork), correct template, vm_flags table |
| 19 | Userspace Runtime Errors | 12 categories: Memory, Concurrency, File/IO, Signals, Integer, String, Linking, IPC, Resources, UB, ARM-specific, Security |
| 20 | Stack & Heap Overflow Detection | 5 stack detection methods, 6 heap detection methods, tool comparison table, recommended compile flags |
| 21 | Interview Quick-Reference | 18-error one-liner table, lock rules, teardown order, tool matrix, Q&A, ARM key points, kernel Kconfig, userspace flags |



# 15. Resource Leaks — Not Releasing I/O Memory, IRQs, DMA, GPIOs in Error Paths
Resource leaks occur when a driver allocates system resources (I/O memory, IRQs, DMA buffers, GPIOs, clocks, regulators) but fails to release them in error paths or in the remove/disconnect function. Over time, these unavailable resources cause probe failures, OOM, or system instability on the next plug cycle.
## Why It Is Critical on Qualcomm ARM Platforms
- Limited GPIOs/IRQs on IoT SoCs (QCS404, SDX55) — a leaked GPIO means no other driver can use it
- IOMMU/SMMU IOVA space is finite — leaked DMA mappings exhaust it quickly
- Hot-pluggable drivers (USB, PCIe) — every probe/remove cycle leaks on each plug event
- Suspend/resume cycles — leaks accumulate over thousands of cycles on automotive/IoT devices that run for years without a reboot
## BAD Example: Multi-Resource Probe with NO Error Cleanup (6 Resources Leaked)
⚠️  Every early return leaks ALL previously acquired resources. This is the most common driver bug in Qualcomm BSP bring-up.
/* ❌ BAD: Qualcomm SPI driver probe with NO error cleanup */
static int qcom_spi_probe(struct platform_device *pdev)
{
    struct qcom_spi *spi;
    struct resource *res;
    int ret;

    spi = kzalloc(sizeof(*spi), GFP_KERNEL);   /* Resource 1: memory */
    if (!spi)
        return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    spi->base = ioremap(res->start, resource_size(res)); /* Resource 2: MMIO */
    if (!spi->base)
        return -ENOMEM;   /* ❌ LEAK: spi not freed! */

    spi->clk = clk_get(&pdev->dev, "core_clk");           /* Resource 3: clock */
    if (IS_ERR(spi->clk))
        return PTR_ERR(spi->clk);  /* ❌ LEAK: spi + ioremap leaked! */

    ret = gpio_request(spi->cs_gpio, "spi-cs");            /* Resource 4: GPIO */
    if (ret)
        return ret;       /* ❌ LEAK: spi + ioremap + clk leaked! */

    spi->irq = platform_get_irq(pdev, 0);
    ret = request_irq(spi->irq, spi_isr, 0, "qcom-spi", spi); /* Resource 5: IRQ */
    if (ret)
        return ret;       /* ❌ LEAK: ALL previous resources leaked! */

    spi->dma_buf = dma_alloc_coherent(&pdev->dev, BUF_SZ,
                                      &spi->dma_addr, GFP_KERNEL); /* Resource 6: DMA */
    if (!spi->dma_buf)
        return -ENOMEM;   /* ❌ LEAK: 5 resources leaked! */

    return 0;
}
## FIX: Proper goto Cleanup Chain (Reverse Order Labels)
/* ✅ CORRECT: Reverse-order cleanup using goto chain */
static int qcom_spi_probe(struct platform_device *pdev)
{
    struct qcom_spi *spi;
    struct resource *res;
    int ret;

    spi = kzalloc(sizeof(*spi), GFP_KERNEL);
    if (!spi) return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    spi->base = ioremap(res->start, resource_size(res));
    if (!spi->base) { ret = -ENOMEM; goto err_free_mem; }

    spi->clk = clk_get(&pdev->dev, "core_clk");
    if (IS_ERR(spi->clk)) { ret = PTR_ERR(spi->clk); goto err_unmap; }

    ret = gpio_request(spi->cs_gpio, "spi-cs");
    if (ret) goto err_clk_put;

    spi->irq = platform_get_irq(pdev, 0);
    ret = request_irq(spi->irq, spi_isr, 0, "qcom-spi", spi);
    if (ret) goto err_gpio_free;

    spi->dma_buf = dma_alloc_coherent(&pdev->dev, BUF_SZ,
                                      &spi->dma_addr, GFP_KERNEL);
    if (!spi->dma_buf) { ret = -ENOMEM; goto err_free_irq; }

    platform_set_drvdata(pdev, spi);
    return 0;

/* Cleanup labels — REVERSE order of acquisition */
err_free_irq:    free_irq(spi->irq, spi);
err_gpio_free:   gpio_free(spi->cs_gpio);
err_clk_put:     clk_put(spi->clk);
err_unmap:       iounmap(spi->base);
err_free_mem:    kfree(spi);
    return ret;
}
## Complete Resource Types and Release Functions
| Resource | Acquire | Release | devm_ variant |
| Memory | kmalloc() / kzalloc() | kfree() | devm_kzalloc() |
| I/O Map | ioremap() | iounmap() | devm_ioremap_resource() |
| Clock (get) | clk_get() | clk_put() | devm_clk_get() |
| Clock (enable) | clk_prepare_enable() | clk_disable_unprepare() | (manual) |
| GPIO | gpio_request() | gpio_free() | devm_gpio_request() |
| IRQ | request_irq() | free_irq() | devm_request_irq() |
| DMA buffer | dma_alloc_coherent() | dma_free_coherent() | dmam_alloc_coherent() |
| DMA map | dma_map_single() | dma_unmap_single() | (manual) |
| Regulator | regulator_get() | regulator_put() | devm_regulator_get() |
| Regulator enable | regulator_enable() | regulator_disable() | (manual) |
| Pinctrl | pinctrl_get() | pinctrl_put() | devm_pinctrl_get() |
| Reset control | reset_control_get() | reset_control_put() | devm_reset_control_get() |
| PHY | phy_get() | phy_put() | devm_phy_get() |

## devm_* Managed APIs — Complete Correct Probe Example
Using devm_* APIs eliminates ALL goto cleanup chains. Resources are auto-freed on probe failure or driver detach.
/* ✅ BEST PRACTICE: All devm_* — no goto needed */
static int qcom_spi_probe(struct platform_device *pdev)
{
    struct qcom_spi *spi;
    int ret;

    spi = devm_kzalloc(&pdev->dev, sizeof(*spi), GFP_KERNEL);
    if (!spi) return -ENOMEM;

    spi->base = devm_platform_ioremap_resource(pdev, 0);
    if (IS_ERR(spi->base)) return PTR_ERR(spi->base);

    spi->clk = devm_clk_get(&pdev->dev, "core_clk");
    if (IS_ERR(spi->clk)) return PTR_ERR(spi->clk);

    ret = devm_gpio_request(&pdev->dev, spi->cs_gpio, "spi-cs");
    if (ret) return ret;

    ret = devm_request_irq(&pdev->dev, spi->irq, spi_isr,
                            0, "qcom-spi", spi);
    if (ret) return ret;

    spi->dma_buf = dmam_alloc_coherent(&pdev->dev, BUF_SZ,
                                        &spi->dma_addr, GFP_KERNEL);
    if (!spi->dma_buf) return -ENOMEM;

    platform_set_drvdata(pdev, spi);
    return 0;
}

/* Remove can be EMPTY — devm auto-cleans all resources */
static int qcom_spi_remove(struct platform_device *pdev)
{
    return 0;
}
## Real Scenario: Leak in Qualcomm PCIe Hot-Plug
PCIe on Qualcomm SM-series SoCs supports hot-plug. Every plug/unplug cycle calls probe() and remove(). A single leaked resource per cycle means hundreds of leaked resources after typical field usage.
/* ❌ Resource leak on every PCIe hot-plug cycle */
static int qcom_pcie_probe(struct platform_device *pdev)
{
    struct qcom_pcie *pcie = kzalloc(sizeof(*pcie), GFP_KERNEL);
    pcie->phy = phy_get(&pdev->dev, "pcie-phy");
    phy_power_on(pcie->phy);
    pcie->base = ioremap(res->start, SZ_4K);
    ret = clk_prepare_enable(pcie->aux_clk);
    if (ret)
        return ret;  /* ❌ phy + ioremap + kzalloc ALL leaked */
    /* Impact: after ~100 plug cycles → system becomes unstable */
}
## Asymmetric Remove Function Scenario
/* ❌ Probe acquires 5 resources, remove only frees 3 */
static int qcom_remove(struct platform_device *pdev)
{
    struct qcom_dev *dev = platform_get_drvdata(pdev);
    free_irq(dev->irq, dev);
    iounmap(dev->base);
    kfree(dev);
    /* ❌ FORGOT: clk_disable_unprepare(dev->clk)       */
    /* ❌ FORGOT: gpio_free(dev->reset_gpio)             */
    /* ❌ FORGOT: regulator_disable(dev->vdd)            */
    return 0;
}

/* ✅ FIX: Symmetric remove — reverse order of probe */
static int qcom_remove(struct platform_device *pdev)
{
    struct qcom_dev *dev = platform_get_drvdata(pdev);
    free_irq(dev->irq, dev);           /* Last acquired → first freed */
    regulator_disable(dev->vdd);
    clk_disable_unprepare(dev->clk);
    gpio_free(dev->reset_gpio);
    iounmap(dev->base);
    kfree(dev);                        /* First acquired → last freed */
    return 0;
}
## devm_* Gotchas — When NOT to Use
| Situation | Problem with devm_* | Solution |
| Runtime PM suspend | IRQ may fire after device suspended while devm handles teardown | Manual free_irq() in suspend callback |
| Order-sensitive shutdown | devm frees in reverse probe order, not your custom order | Use manual cleanup for specific ordering requirements |
| DMA still active | DMA engine may complete AFTER devm frees the buffer | Call dmaengine_terminate_sync() before resource free |
| Shared resources | devm lifetime tied to ONE device; shared resource freed too early | Use manual reference counting for cross-device resources |

## Detection Tools
| Tool / Command | How It Helps Detect Leaks |
| /proc/iomem | Shows all mapped I/O regions. Leaked ioremap regions persist after rmmod |
| /proc/interrupts | Leaked IRQs remain registered after driver remove. Check for stale entries. |
| /sys/kernel/debug/gpio | Shows GPIO ownership. Leaked GPIOs stay claimed preventing other drivers from using them |
| /sys/kernel/debug/clk/clk_summary | Leaked enabled clocks remain ON consuming power even after driver unload |
| modprobe / rmmod loop | Run "for i in $(seq 1 500); do modprobe drv; rmmod drv; done" and monitor resource counts |
| kmemleak | "echo scan > /sys/kernel/debug/kmemleak" finds orphaned kmalloc allocations |

⚠️  Key Rule: Every acquire must have a matching release in ALL error paths + remove(). Test with repeated modprobe/rmmod cycles.
# 16. Incorrect Register Access — Wrong Offsets, Sizes, or Endianness in MMIO/PIO
Incorrect register access means reading or writing hardware registers with wrong offsets, wrong sizes (8-bit vs 32-bit), or wrong endianness. This causes silent hardware misconfiguration, peripheral malfunction, or bus faults on ARM.
## ARM MMIO Access Basics — Qualcomm GENI UART Register Map Example
/* Qualcomm GENI UART Register Map (Base: 0x0A800000) */
/*
 * Offset 0x00: GENI_M_CMD0       (32-bit) — Main command register
 * Offset 0x04: GENI_M_CMD_CTRL   (32-bit) — Command control
 * Offset 0x08: GENI_M_IRQ_STATUS (32-bit) — Interrupt status (RO)
 * Offset 0x0C: GENI_M_IRQ_CLEAR  (32-bit) — Interrupt clear (WO)
 * Offset 0x10: SE_DMA_TX_LEN     (32-bit) — DMA TX length
 * Offset 0x18: SE_GENI_STATUS    (32-bit) — Engine status
 *
 * RULES:
 *   Registers are WORD-ALIGNED (4-byte boundary)
 *   Qualcomm ARM SoCs are LITTLE-ENDIAN
 *   Must use readl()/writel() — never dereference __iomem directly
 */

#define GENI_M_CMD0          0x00
#define GENI_M_CMD_CTRL      0x04
#define GENI_M_IRQ_STATUS    0x08
#define GENI_M_IRQ_CLEAR     0x0C
#define SE_DMA_TX_LEN        0x10
#define SE_GENI_STATUS       0x18
## Scenario 1: Wrong Register Offset (Typo / Copy-Paste Error)
/* ❌ BUG: Wrong offset! 0x10 is SE_DMA_TX_LEN, NOT IRQ_STATUS */
static irqreturn_t geni_uart_isr(int irq, void *data)
{
    struct geni_uart *uart = data;
    /* Reads SE_DMA_TX_LEN register instead of GENI_M_IRQ_STATUS */
    u32 status = readl(uart->base + 0x10);  /* ❌ Wrong offset! */
    /* IRQ never cleared → IRQ storm or missed data */
    if (status & M_RX_FIFO_WR)
        handle_rx(uart);
}

/* ✅ FIX: Always use named macros */
    u32 status = readl(uart->base + GENI_M_IRQ_STATUS);  /* Clear intent */
## Scenario 2: Wrong Access Size — 8-bit vs 32-bit (PCIe Config Space)
/* ❌ BUG: 32-bit read for a register that requires BYTE access */
static int qcom_pcie_read_config(struct pcie_port *pp, int where,
                                  int size, u32 *val)
{
    void __iomem *addr = pp->dbi_base + where;
    *val = readl(addr);  /* ❌ Wrong for 1-byte register! */
    /* Some PCIe config regs are 8-bit. readl() triggers Sync External Abort */
}

/* ✅ FIX: Match access function to register width */
static int qcom_pcie_read_config(struct pcie_port *pp, int where,
                                  int size, u32 *val)
{
    void __iomem *addr = pp->dbi_base + where;
    switch (size) {
    case 1: *val = readb(addr);   break;  /* 8-bit  */
    case 2: *val = readw(addr);   break;  /* 16-bit */
    case 4: *val = readl(addr);   break;  /* 32-bit */
    default: return PCIBIOS_BAD_REGISTER_NUMBER;
    }
    return 0;
}
ℹ️  ARM SError when wrong-size MMIO access hits hardware: "SError interrupt on CPU2, code 0xbf000002 -- Synchronous External Abort"
## Scenario 3: Endianness Mismatch — EMAC Ethernet Big-Endian Hardware
/* ❌ BUG: Hardware is BIG-ENDIAN; ARM CPU is LITTLE-ENDIAN */
struct emac_hw_desc { u32 status; u32 length; u32 addr_lo; u32 addr_hi; };

static void emac_setup_tx_desc(struct emac_dev *emac, dma_addr_t buf, int len)
{
    struct emac_hw_desc *desc = emac->tx_ring;
    desc->length   = len;                  /* 0x00000100 (256) in LE */
    desc->addr_lo  = lower_32_bits(buf);
    /* HW reads 0x00010000 (65536!) — corrupted DMA transfer length */
}

/* ✅ FIX: Convert CPU (LE) → hardware (BE) */
    desc->length  = cpu_to_be32(len);
    desc->addr_lo = cpu_to_be32(lower_32_bits(buf));

/* Reading back from hardware: */
    int hw_len = be32_to_cpu(desc->length);  /* ✅ Convert BE → CPU */
## Scenario 4: Non-Aligned Access on ARM
/* ❌ BUG: Accessing 32-bit register at non-aligned offset 0x05 */
static void qcom_read_status(void __iomem *base)
{
    u32 val = readl(base + 0x05);  /* ❌ Alignment fault on ARM! */
    /* ARM device memory (Device-nGnRnE) ALWAYS faults on unaligned access */
    /* Error: "Unhandled fault: alignment fault (0x96000021)" */
}

/* ✅ FIX: Access at 4-byte aligned offset */
    u32 val = readl(base + 0x04);
    u8 byte = (val >> 8) & 0xFF;  /* Extract specific byte from aligned read */
## Scenario 5: Read-Modify-Write Without Proper Masking (GCC Clock)
/*
 * Register: GCC_USB30_CTRL  (offset 0x1C)
 * Bits [3:0]  = CLK_DIV        Bits [7:4]  = RESERVED (read-only HW state)
 * Bit  [8]    = CLK_EN          Bits [31:9] = RESERVED
 */

/* ❌ BUG: Overwrites ENTIRE register — destroys CLK_EN and reserved bits! */
static void gcc_usb_set_div_bad(void __iomem *base, int div)
{
    writel(div, base + GCC_USB30_CTRL);  /* Clears CLK_EN! USB stops! */
}

/* ✅ FIX: Read-Modify-Write with proper masking */
static void gcc_usb_set_div(void __iomem *base, int div)
{
    u32 val = readl(base + GCC_USB30_CTRL);  /* Read current */
    val &= ~GENMASK(3, 0);                    /* Clear CLK_DIV bits only */
    val |= FIELD_PREP(GENMASK(3, 0), div);    /* Set new divider */
    writel(val, base + GCC_USB30_CTRL);        /* Write back */
}
## Scenario 6: Missing readl Barrier — Write Not Reaching Hardware (Watchdog)
/* ❌ BUG: ARM write buffer may reorder writes */
static void qcom_wdt_reset(struct qcom_wdt *wdt)
{
    writel(1, wdt->base + WDT_RST);    /* Trigger reset */
    writel(0, wdt->base + WDT_EN);     /* Disable WDT   */
    /* WDT_EN=0 might reach HW BEFORE WDT_RST=1 due to write buffer! */
}

/* ✅ FIX: Use wmb() or read-back as barrier */
static void qcom_wdt_reset(struct qcom_wdt *wdt)
{
    writel(1, wdt->base + WDT_RST);
    wmb();  /* Write memory barrier: RST must reach HW before EN */
    writel(0, wdt->base + WDT_EN);

    /* Alternative: readl() as implicit barrier */
    /* readl(wdt->base + WDT_RST);  forces prior write to complete */
}
## ARM MMIO Access Functions Table
| Function | Size | Use Case | Notes |
| readb() / writeb() | 8-bit | Byte registers (UART data register) | Implies memory barrier |
| readw() / writew() | 16-bit | Half-word registers | Implies memory barrier |
| readl() / writel() | 32-bit | Standard ARM SoC registers (most common) | Implies memory barrier |
| readq() / writeq() | 64-bit | 64-bit registers (ARM64 only) | Implies memory barrier |
| ioread32be() / iowrite32be() | 32-bit BE | Big-endian peripheral hardware | Performs byte-swap automatically |
| readl_relaxed() / writel_relaxed() | 32-bit | Performance-critical paths (no barrier) | Use carefully: no ordering guarantee |

## Endianness Conversion Quick Reference
| Direction | Macro | Example Use | Note |
| CPU → Little-Endian HW | cpu_to_le32(val) | Most Qualcomm registers | No-op on LE CPU |
| CPU → Big-Endian HW | cpu_to_be32(val) | Network hardware, some IPs | Byte-swaps on LE CPU |
| Little-Endian HW → CPU | le32_to_cpu(val) | Reading LE descriptor fields | No-op on LE CPU |
| Big-Endian HW → CPU | be32_to_cpu(val) | Reading BE hardware registers | Byte-swaps on LE CPU |
| 16-bit variants | cpu_to_le16() / le16_to_cpu() | USB descriptors, FAT | Same pattern as 32-bit |

ℹ️  On ARM (little-endian CPU), cpu_to_le32() is a no-op. Always use it anyway for portability and documentation of intent.
# 17. Page Table Corruption — Incorrect Manipulation of PTEs/PMDs/PUDs
Page table corruption occurs when kernel code incorrectly modifies page table entries (PTEs, PMDs, PUDs, PGDs) — the MMU translation structures that map virtual addresses to physical addresses. Corruption causes wrong memory mappings, permission faults, data corruption across processes, or complete system crash.
## ARM64 Page Table Hierarchy (4KB granule, 48-bit VA)
Virtual Address (48-bit) decomposition:
+---------+---------+---------+---------+--------------+
|  PGD    |  PUD    |  PMD    |  PTE    | Page Offset  |
| [47:39] | [38:30] | [29:21] | [20:12] |   [11:0]     |
|  9 bits |  9 bits |  9 bits |  9 bits |   12 bits    |
+---------+---------+---------+---------+--------------+

Translation walk:
PGD Table  -->  PUD Table  -->  PMD Table  -->  PTE Table  -->  Physical Page
(512 entries)  (512 entries)  (512 entries)  (512 entries)

PTE Entry (64-bit on ARM64):
+------------------+--------------------+---------------+
| [63:48] Attrs    | [47:12] Phys Addr  | [11:0] Flags  |
| XN,PXN,Contiguous| Output Address     | AP,SH,AF,Valid|
+------------------+--------------------+---------------+
## Scenario 1: Buffer Overflow Into Page Table Memory
/* ❌ BUG: Kernel module overflows into adjacent page table page */
struct my_data { char buf[4096]; };  /* Exactly one page */

static void bad_write(struct my_data *data)
{
    /* data is a kmalloc allocation. Adjacent SLAB object might be a page table */
    memset(data->buf, 0, 8192);  /* ❌ Writes 8KB into 4KB buffer! */
    /*
     * If adjacent SLAB object is a page table page:
     *   - PTEs overwritten with zeros -> pages become UNMAPPED
     *   - Processes using those mappings -> instant crash
     *   - Kernel page tables hit -> kernel panic
     */
}

/* Resulting ARM64 Oops: */
/* Unable to handle kernel paging request at va ffff000012340000 */
/* ESR = 0x96000007 (Translation fault, level 3) */
/* PTE was zeroed out by buffer overflow */
## Scenario 2: Double-Free of Page Table Pages
/* ❌ BUG: Freeing a page table page while other PTEs in it are still valid */
static void bad_cleanup(struct mm_struct *mm, unsigned long addr)
{
    pgd_t *pgd = pgd_offset(mm, addr);
    pud_t *pud = pud_offset(pgd, addr);
    pmd_t *pmd = pmd_offset(pud, addr);
    pte_t *pte = pte_offset_kernel(pmd, addr);

    set_pte(pte, __pte(0));           /* Clear one PTE entry */
    free_page((unsigned long)pte & PAGE_MASK);  /* ❌ Frees whole PTE page! */
    /*
     * Other PTEs in the SAME page still point to valid data.
     * Freed page gets reallocated for something else.
     * Those PTEs now map random data as "memory" -> CATASTROPHIC.
     */
}
## Scenario 3: Missing TLB Invalidation After PTE Modification
/* ❌ BUG: PTE updated but TLB not flushed */
static void remap_buffer(struct vm_area_struct *vma, unsigned long addr,
                          phys_addr_t new_phys)
{
    pte_t *pte = get_pte_for_addr(vma->vm_mm, addr);
    set_pte_at(vma->vm_mm, addr, pte,
               pfn_pte(new_phys >> PAGE_SHIFT, vma->vm_page_prot));
    /* ❌ No TLB invalidation! CPU uses stale TLB entry. */
    /* Result: CPU reads OLD physical page -> silent data corruption */
}

/* ✅ FIX: Always flush TLB after PTE modification */
    set_pte_at(vma->vm_mm, addr, pte,
               pfn_pte(new_phys >> PAGE_SHIFT, vma->vm_page_prot));
    flush_tlb_page(vma, addr);     /* Flush on all CPUs that may have cached it */
    /* For a range: flush_tlb_range(vma, start, end); */
    /* ARM64 sequence: dsb(ishst) + tlbi vae1is + dsb(ish) + isb */
## Scenario 4: Wrong Permission Bits in PTE — Security Vulnerability
/* ❌ BUG: Mapping kernel memory as user-writable (security hole!) */
static int bad_map_shared(struct vm_area_struct *vma, phys_addr_t phys)
{
    /* Sets user-writable bits without checking if this is kernel memory */
    pgprot_t prot = __pgprot(PTE_VALID | PTE_USER | PTE_WRITE | PTE_AF);
    return remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
                           vma->vm_end - vma->vm_start, prot);
    /* Userspace can now overwrite kernel code/data! */
}

/* ✅ FIX: Validate physical address and use safe pgprot */
    if (phys < PHYS_OFFSET) {  /* Never map kernel phys memory to user */
        pr_err("Refusing to map kernel memory to userspace\n");
        return -EPERM;
    }
    pgprot_t prot = vm_get_page_prot(vma->vm_flags);
    return remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
                           vma->vm_end - vma->vm_start, prot);
## Scenario 5: Race Condition in Page Table Update (SMP)
/* ❌ BUG: Two CPUs handling page fault for same address simultaneously */
/* CPU0: handle_fault_cpu0() allocates PTE page and starts filling it */
/* CPU1: handle_fault_cpu1() reads PMD which is mid-update by CPU0   */
/*       CPU1 follows garbage PMD pointer -> accesses random memory   */
/*       as page table -> catastrophic corruption                      */

/* ✅ FIX: Kernel uses atomic operations for page table updates on ARM64 */
static inline void set_pmd_safe(pmd_t *pmdp, pmd_t pmd)
{
    /* ARM64: WRITE_ONCE ensures single-copy atomicity */
    WRITE_ONCE(*pmdp, pmd);
    dsb(ishst);  /* Data Sync Barrier: visible to all inner-shareable CPUs */
}
## Scenario 6: Corrupting Page Tables via DMA
/* ❌ BUG: DMA hardware writes more data than allocated buffer */
static int setup_dma(struct my_dev *dev)
{
    dev->dma_buf = kmalloc(256, GFP_KERNEL);
    dev->dma_addr = dma_map_single(dev->dev, dev->dma_buf,
                                   256, DMA_FROM_DEVICE);
    /* Hardware misconfigured: writes 4KB to 256-byte buffer */
    start_dma_transfer(dev, 4096);  /* ❌ Should be 256! */
    /*
     * DMA overwrites adjacent SLAB objects which may be page table pages.
     * If SMMU/IOMMU is not protecting: silent PT corruption.
     * If SMMU catches it: "arm-smmu: Unhandled context fault"
     */
}
## ARM64 PTE Bit Fields Table
| Bit(s) | Field | Meaning | Effect if Corrupted |
| [1:0] | Valid+Type | 0b11 = valid page desc, 0b01 = block, 0b00 = invalid | Translation fault (page appears unmapped) |
| [6] | AP[1] | EL0 access: 0=EL1 only, 1=EL0 accessible | Kernel-only page accessible to userspace = security hole |
| [7] | AP[2] | 0=RW, 1=Read-Only | Permission fault on write OR wrong read-only enforcement |
| [9:8] | SH | Shareability: 00=Non, 10=Outer, 11=Inner-shareable | Cache coherency issues on SMP (stale data between cores) |
| [10] | AF | Access Flag: must=1, or hardware sets it on first access | Access flag fault on every access (major performance hit) |
| [47:12] | OA | Output Address (physical page frame number) | Maps to WRONG physical page: silent data corruption |
| [53] | PXN | Privileged Execute Never: EL1 cannot execute this page | If cleared: kernel code injection vulnerability |
| [54] | UXN/XN | Execute Never for EL0 (user) | If cleared: user code execution of non-code pages |

## Symptoms of Page Table Corruption
| Symptom Observed | Likely Cause | ESR Code |
| Random "Translation fault" on valid addresses | PTE zeroed or corrupted by overflow | 0x96000007 (L3) |
| Process sees another process's data | PTE points to wrong physical page | No fault (silent!) |
| "Permission fault" on previously accessible memory | AP/RO bits accidentally flipped | 0x9600000D (L3 perm) |
| Kernel panic after fork() | PGD/PUD level corruption during copy-on-write | Varies |
| Data corruption without any crash | Wrong PA mapped but valid bit still set | None (hardest to detect!) |
| SMMU context faults from GPU/DSP | Corrupted IOMMU page tables or DMA overrun | SMMU FSR register |

## Detection Tools
| Tool | Kernel Config | What It Detects |
| KASAN | CONFIG_KASAN=y | Buffer overflows that spill into adjacent page table pages |
| DEBUG_PAGEALLOC | CONFIG_DEBUG_PAGEALLOC=y | Unmaps freed pages; catches use-after-free of PT pages |
| PAGE_TABLE_CHECK | CONFIG_PAGE_TABLE_CHECK=y | Validates PT entries on each modification (Linux 5.17+) |
| PAGE_POISONING | CONFIG_PAGE_POISONING=y | Fills freed pages with 0xAA pattern; makes corruption visible |
| T32 mmu.info | Lauterbach JTAG | mmu.info 0xVA: dumps page table walk, shows validity and attributes |
| ARM PAC/BTI | ARMv8.3+ HW feature | Pointer Authentication catches corrupted code pointers at runtime |

⚠️  Key Rule: Never directly modify PTEs. Always use kernel APIs (set_pte_at(), pte_mkwrite(), etc.). Always flush TLB after any page table change with flush_tlb_page() or flush_tlb_range().
# 18. Incorrect mmap Implementation — Wrong vm_flags or Fault Handler
Incorrect mmap occurs when a driver's mmap file operation sets wrong vm_flags (permissions, caching), implements a broken fault handler, or maps wrong physical addresses. This leads to security vulnerabilities, data corruption, bus errors, or cache incoherency on ARM.
## How mmap Works in a Driver (ARM64 Diagram)
Userspace:                         Kernel Driver (.mmap callback):
-----------                        --------------------------------
buf = mmap(NULL, size,             my_driver_mmap(file, vma)
           PROT_READ|PROT_WRITE,     |
           MAP_SHARED, fd, 0);       v
                                   1. Validate size and offset
  [User Virtual Addr]              2. Set vma->vm_flags  (VM_IO | VM_PFNMAP)
  (e.g. 0x7F000000)  <----------  3. Set vma->vm_page_prot  (caching)
        |                          4a. remap_pfn_range() for fixed mapping
        v  (mapped to)             4b. or set vm_ops->fault for on-demand
  [Physical Address]
  (e.g. 0x0A800000 = HW register)
## Scenario 1: Missing VM_IO / VM_PFNMAP for Device Memory
/* ❌ BUG: Missing VM_IO for device register mapping */
static int dsp_mmap(struct file *f, struct vm_area_struct *vma)
{
    phys_addr_t phys = DSP_SRAM_BASE;  /* 0x8B000000 */
    unsigned long size = vma->vm_end - vma->vm_start;
    /* No VM_IO set: kernel treats this as normal RAM
     * - Core dump tries to read it -> bus fault
     * - get_user_pages() tries to manage these "pages"
     * - Page migration may try to move non-existent page structs */
    return remap_pfn_range(vma, vma->vm_start,
                           phys >> PAGE_SHIFT, size, vma->vm_page_prot);
}

/* ✅ FIX: Always set VM_IO | VM_PFNMAP for device memory */
static int dsp_mmap(struct file *f, struct vm_area_struct *vma)
{
    phys_addr_t phys = DSP_SRAM_BASE;
    unsigned long size = vma->vm_end - vma->vm_start;
    /* Mark as I/O: no page struct, no swap, excluded from core dump */
    vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
    return remap_pfn_range(vma, vma->vm_start,
                           phys >> PAGE_SHIFT, size, vma->vm_page_prot);
}
## Scenario 2: Wrong Caching Attributes — Cache Incoherency (Camera Buffer)
/* ❌ BUG: Default vm_page_prot is write-back CACHED on ARM */
/*   Camera DMA buffer is written by hardware, not CPU.     */
/*   CPU cache holds STALE data -> corrupted camera frames  */
static int cam_mmap(struct file *f, struct vm_area_struct *vma)
{
    return remap_pfn_range(vma, vma->vm_start,
                           buf->phys >> PAGE_SHIFT, buf->size,
                           vma->vm_page_prot);  /* ❌ CACHED! */
}

/* ✅ FIX: Set write-combine (non-cacheable) for DMA buffers */
static int cam_mmap(struct file *f, struct vm_area_struct *vma)
{
    /* Write-combine: CPU bypasses cache for writes; reads from memory */
    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
    vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND;
    return remap_pfn_range(vma, vma->vm_start,
                           buf->phys >> PAGE_SHIFT, buf->size,
                           vma->vm_page_prot);
}
## Scenario 3: Missing Size/Offset Validation — Security Vulnerability
/* ❌ SECURITY BUG: No validation of user-provided offset */
/*   User can map ANY physical address, including kernel memory! */
static int insecure_mmap(struct file *f, struct vm_area_struct *vma)
{
    /* vma->vm_pgoff is directly from userspace mmap() call */
    unsigned long phys = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long size = vma->vm_end - vma->vm_start;
    return remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
                           size, vma->vm_page_prot);  /* ❌ No check! */
}
/* EXPLOIT: int fd = open("/dev/vuln", O_RDWR);
            mmap(NULL, 4096, PROT_RW, MAP_SHARED, fd, 0x80000000);
            /* Now attacker reads/writes kernel memory at 0x80000000 */

/* ✅ FIX: Validate offset is within device's own memory region */
static int secure_mmap(struct file *f, struct vm_area_struct *vma)
{
    struct my_device *dev = f->private_data;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long size   = vma->vm_end - vma->vm_start;
    if (offset + size > dev->mem_size)  /* Bounds check */
        return -EINVAL;
    unsigned long pfn = (dev->phys_base + offset) >> PAGE_SHIFT;
    vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND;
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    return remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
}
## Scenario 4: Broken Fault Handler — Wrong Page Returned (GPU Driver)
/* ❌ BUG: No bounds check, no get_page() */
static vm_fault_t gpu_fault_handler(struct vm_fault *vmf)
{
    struct gpu_bo *bo = vmf->vma->vm_private_data;
    /* No bounds check: user can fault beyond buffer -> OOB array access */
    struct page *page = bo->pages[vmf->pgoff];  /* ❌ Array OOB! */
    vmf->page = page;  /* ❌ No get_page(): refcount wrong -> UAF */
    return 0;
}

/* ✅ FIX: Bounds check + get_page() */
static vm_fault_t gpu_fault_handler(struct vm_fault *vmf)
{
    struct gpu_bo *bo = vmf->vma->vm_private_data;
    unsigned long offset = vmf->pgoff - vmf->vma->vm_pgoff;
    if (offset >= bo->num_pages)  /* Bounds check */
        return VM_FAULT_SIGBUS;
    struct page *page = bo->pages[offset];
    if (!page) return VM_FAULT_SIGBUS;
    get_page(page);  /* VM will call put_page() when unmapping */
    vmf->page = page;
    return 0;
}
## Scenario 5: Allowing VM_WRITE on Read-Only Device Registers
/* ❌ BUG: Not checking if user requested WRITE access to RO registers */
static int pmic_status_mmap(struct file *f, struct vm_area_struct *vma)
{
    return remap_pfn_range(vma, vma->vm_start,
                           PMIC_STATUS_PHYS >> PAGE_SHIFT,
                           PAGE_SIZE, vma->vm_page_prot);
}
/* User: void *r = mmap(NULL, 4096, PROT_RW, MAP_SHARED, fd, 0); */
/* *(int*)r = 0x1234;  -> ARM64 Synchronous External Abort! */

/* ✅ FIX: Enforce read-only */
    if (vma->vm_flags & VM_WRITE) { pr_err("RO registers\n"); return -EPERM; }
    vma->vm_flags  &= ~VM_WRITE;
    vma->vm_page_prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));
    vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND;
## Scenario 6: Missing VM_DONTCOPY — Child Inherits Device Mapping on fork()
/* ❌ BUG: Child process inherits GPU command buffer after fork() */
static int gpu_mmap(struct file *f, struct vm_area_struct *vma)
{
    remap_pfn_range(vma, vma->vm_start,
                    gpu_cmd_phys >> PAGE_SHIFT, size, vma->vm_page_prot);
    /* Missing VM_DONTCOPY: fork() child inherits this mapping */
    /* Two processes write to same GPU buffer -> GPU crash/corruption */
}

/* ✅ FIX: Add VM_DONTCOPY */
    vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTCOPY | VM_DONTEXPAND;
## Complete Correct mmap Implementation Template
/* ✅ COMPLETE CORRECT TEMPLATE */
static int qcom_device_mmap(struct file *f, struct vm_area_struct *vma)
{
    struct qcom_dev *dev = f->private_data;
    unsigned long size   = vma->vm_end - vma->vm_start;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;

    /* 1. Validate size and offset */
    if (offset + size > dev->mem_size) {
        pr_err("mmap: offset+size exceeds device memory\n");
        return -EINVAL;
    }

    /* 2. Validate permissions */
    if ((vma->vm_flags & VM_WRITE) && !dev->allow_write) {
        pr_err("mmap: write access not permitted\n");
        return -EPERM;
    }

    /* 3. Set proper vm_flags */
    vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;

    /* 4. Set proper caching (CRITICAL on ARM!) */
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    /* Use pgprot_writecombine() for DMA/framebuffer, noncached for MMIO */

    /* 5. Map physical memory */
    return remap_pfn_range(vma, vma->vm_start,
                           (dev->phys_base + offset) >> PAGE_SHIFT,
                           size, vma->vm_page_prot);
}
## vm_flags Quick Reference
| Flag | Purpose | When to Set |
| VM_IO | I/O memory; no page struct, no swap, no OOM kill | Device registers, MMIO mappings |
| VM_PFNMAP | PFN-mapped, not backed by struct page | remap_pfn_range() mappings |
| VM_DONTEXPAND | Cannot be expanded with mremap() | Fixed-size device memory regions |
| VM_DONTDUMP | Excluded from core dumps (unreadable device memory) | All device memory mappings |
| VM_DONTCOPY | Not inherited by child on fork() | Per-process device state (GPU contexts) |
| VM_WRITE | User can write to this mapping | Check and clear if HW is read-only |
| VM_SHARED | Shared between multiple processes | Shared DMA buffers, IPC regions |
| VM_MIXEDMAP | Mixed page/PFN mapping | vm_insert_page() combined with remap_pfn |

## ARM64 Caching Options for mmap
| pgprot Macro | ARM64 MAIR Attr | CPU Cache Behavior | Use Case |
| pgprot_cached() | Normal WB (write-back) | Full write-back cache | CPU-only memory (no DMA sharing) |
| pgprot_writecombine() | Normal NC (non-cached) | Write-combining, reads memory | DMA buffers, framebuffers, video |
| pgprot_noncached() | Device-nGnRnE | No cache, no reorder, no gather | Hardware registers (MMIO) |
| pgprot_device() | Device-nGnRE | No cache, some reordering OK | Device MMIO (more performance) |

# 19. Userspace Runtime Errors — Complete List (12 Categories)
The following table-organized reference covers all runtime error categories that can occur in userspace applications on ARM/Linux platforms, particularly relevant for Qualcomm embedded targets.
## Category 1: Memory Errors
| Error | Description | Signal / Symptom |
| Segmentation Fault | Accessing unmapped or protected memory | SIGSEGV |
| Bus Error | Unaligned access or non-existent physical address | SIGBUS |
| Heap buffer overflow | Writing beyond malloc'd region into adjacent object | Crash or silent corruption |
| Stack buffer overflow | Writing beyond local array bounds on the call stack | SIGSEGV or SIGABRT (canary) |
| Use-after-free | Accessing memory after free() — same root as kernel UAF | Crash / data corruption |
| Double free | free() called twice on the same pointer | Heap corruption, crash |
| Memory leak | malloc() without matching free() | OOM over time |
| NULL pointer dereference | Dereferencing a NULL or uninitialized pointer | SIGSEGV at address 0x0 |
| Stack overflow | Deep recursion or large locals exceed thread stack size | SIGSEGV at stack guard page |
| Invalid free | free() on stack variable, global, or middle of allocation | Heap corruption / SIGABRT |

## Category 2: Concurrency / Threading Errors
| Error | Description | Detection Tool |
| Data race | Two threads access shared data without sync, one writes | TSan (ThreadSanitizer) |
| Deadlock | Two or more threads wait on locks held by each other (ABBA) | Valgrind Helgrind, GDB |
| Priority inversion | Low-priority thread holds lock needed by high-priority thread | Priority-inheritance mutex |
| Double-lock (self deadlock) | Same thread locks a non-recursive mutex twice | Lockdep equivalent / PTHREAD_MUTEX_ERRORCHECK |
| Thread-unsafe function | strtok(), localtime(), rand() called from multiple threads | Code review, strtok_r(), etc. |
| Spurious wakeup | pthread_cond_wait without while-loop predicate check | Code review |
| Signal loss (cond var) | Signal sent before waiter sleeps — event permanently lost | Careful ordering, mutexes |

## Category 3: File & I/O Errors
| Error | Description | Fix |
| File descriptor leak | open() without matching close() | Always close(fd), use strace to find leaks |
| Read/write on closed fd | Using fd after close() returns EBADF | Set fd = -1 after close, check before use |
| TOCTOU race | access() check then open() — file changes between the two calls | Use O_CREAT|O_EXCL, openat() with dirfd |
| Broken pipe (SIGPIPE) | Writing to pipe/socket with no reader | Handle SIGPIPE or set MSG_NOSIGNAL/SO_NOSIGPIPE |
| Partial read/write | read()/write() may return fewer bytes than requested | Loop until all bytes transferred |
| FD exhaustion | Exceeding per-process limit (default 1024) | ulimit -n, close fds promptly, use epoll |

## Category 4: Signal Handling Errors
| Error | Description | Fix |
| Async-signal-unsafe functions | printf(), malloc(), mutex_lock() called in signal handler | Use only async-signal-safe functions (write, _exit, etc.) |
| Race with main thread | Shared data modified in handler without sig_atomic_t | Use volatile sig_atomic_t for handler-main shared flags |
| Deadlock via signal | Handler tries to lock mutex already held by interrupted code | Block signals before locking (sigprocmask) |
| SA_RESTART not set | System calls return EINTR instead of auto-restarting after signal | Set SA_RESTART in sigaction flags |

## Category 5: Integer & Arithmetic Errors
| Error | Description | Detection |
| Integer overflow | INT_MAX + 1 wraps to INT_MIN (signed undefined behavior in C) | UBSan -fsanitize=integer |
| Unsigned underflow | 0U - 1 wraps to 0xFFFFFFFF (size_t huge positive value) | Compiler warnings -Wsign-compare |
| Division by zero | Triggers SIGFPE (also for modulo by zero) | UBSan, code review |
| Sign conversion bug | Negative value assigned to size_t causes huge allocation | -Wsign-conversion GCC flag |
| Shift overflow | 1 << 32 on 32-bit int is undefined behavior | UBSan -fsanitize=shift |

## Category 6: String & Buffer Errors
| Error | Description | Fix |
| strcpy overflow | No bounds check; overwrites memory after buffer end | Use strscpy() or strlcpy() |
| Missing NULL terminator | String functions read beyond buffer end | strncpy fills with NULs; ensure space for \0 |
| Format string vulnerability | printf(user_input) lets attacker read/write stack via %n | Always: printf("%s", user_input) |
| Off-by-one | strncpy(buf, src, sizeof(buf)) leaves no room for \0 | Use sizeof(buf)-1 or snprintf |

## Category 7: Dynamic Linking / Library Errors
| Error | Description | Symptom |
| Symbol not found at runtime | Missing .so file or missing EXPORT_SYMBOL equivalent | Immediate crash before main() |
| ABI mismatch | Header says struct is 16 bytes, .so compiled with 24 bytes | Silent data corruption or crash |
| dlopen/dlsym failures | Plugin loading fails; return value unchecked | NULL dereference on plugin use |
| Static init order fiasco | C++ globals depending on each other across translation units | Crash or wrong values at startup |

## Category 8: IPC / Socket Errors
| Error | Description | errno / Fix |
| Connection refused | Server not listening when client connects | ECONNREFUSED — retry with backoff |
| Connection reset | Peer process crashes; next read/write gets error | ECONNRESET — handle gracefully |
| Address already in use | Binding same port without SO_REUSEADDR | EADDRINUSE — setsockopt SO_REUSEADDR |
| Shared memory race | Two processes access shm without semaphore | Data corruption — use sem_post/sem_wait |
| Pipe deadlock | Both sides write without reading; both block on full buffer | Use threads or select()/epoll() |

## Category 9: Resource Exhaustion Errors
| Error | Description | Detection |
| Out of Memory (OOM) | malloc() returns NULL; or OOM killer fires under pressure | Check malloc return; valgrind --leak-check |
| FD exhaustion | Exceeds per-process limit ulimit -n (default 1024) | strace, /proc/PID/fd count |
| Thread limit | pthread_create fails with EAGAIN (too many threads) | /proc/sys/kernel/threads-max |
| Address space exhaustion | 32-bit process exhausts 3GB virtual address space | Use 64-bit build for large datasets |

## Category 10: Undefined Behavior (UB)
| Error | Description | Detection |
| Uninitialized variable | Reading allocated but never written memory — garbage values | MSan, -Wuninitialized |
| Strict aliasing violation | Casting int* to float* — compiler misoptimizes memory accesses | UBSan, -fno-strict-aliasing as workaround |
| Signed overflow | Compiler assumes no overflow; eliminates bounds checks! | UBSan -fsanitize=signed-integer-overflow |
| Misaligned access | Casting char* to int* on ARM32 strict alignment mode | UBSan -fsanitize=alignment |

## Category 11: ARM-Specific Runtime Errors
| Error | Description | ARM Specific Detail |
| Alignment fault | Unaligned 32/64-bit access on ARMv5/v6 or device memory | SIGBUS; ESR alignment fault code 0x92000001 |
| Illegal instruction | Running ARMv8-A instruction on ARMv7 binary | SIGILL; check -march= compile flag vs runtime CPU |
| MTE tag mismatch | Memory Tagging Extension (ARMv8.5+) detects UAF or overflow | SIGSEGV with tag-check-fault code; HW-accelerated ASan |
| PAC/BTI trap | Pointer Authentication fails (corrupted pointer detected) | SIGILL; pointer integrity check at function return/indirect call |
| Cache coherency issue | Self-modifying code without __clear_cache() / cacheflush() | Executes stale instructions from I-cache |

## Category 12: Security-Related Runtime Errors
| Error | Description | Mitigation |
| Stack smashing detected | Canary overwritten; __stack_chk_fail fires | -fstack-protector-strong; SIGABRT |
| Return-to-libc / ROP | Stack overflow exploited to redirect to libc function | ASLR + PIE + stack canary + NX |
| Format string attack | %n in printf lets attacker write to arbitrary address | Never printf(user_input); use -Wformat-security |
| Heap metadata corruption | Overflow of malloc header corrupts allocator metadata | ASan, hardened glibc, electric fence |
| GOT/PLT overwrite | Heap overflow rewrites function pointer in Global Offset Table | -Wl,-z,relro,-z,now (RELRO) |

# 20. Detecting Stack Overflow & Heap Overflow in Userspace
## ARM Process Stack Layout
Process Virtual Address Space (ARM64):
+----------------------------------+  High Address (0x0000_FFFF_FFFF_FFFF)
|         STACK                    |  Grows DOWNWARD
|           |                      |
|           v                      |
|    [Guard Page - UNMAPPED]       |  Triggers SIGSEGV on overflow
|           ^                      |
|           |                      |
|         HEAP                     |  Grows UPWARD
|                                  |
|  .bss  /  .data  /  .text       |
+----------------------------------+  Low Address (0x0000_0000_0000_0000)
## Stack Overflow Detection Methods
### Method 1: GCC Stack Protector (Canary) — Production-Safe
Compiler inserts a canary value between local variables and the return address. If overwritten, detected at function return with "*** stack smashing detected ***".
/* Compile: gcc -fstack-protector-all -o app app.c */
void vulnerable_function(char *input)
{
    char buf[64];
    strcpy(buf, input);  /* ❌ If input > 64 bytes -> overwrites canary */
}
/* At function return: "*** stack smashing detected ***: terminated" */
/* Sends SIGABRT -> core dump */

/* ARM64 Assembly with canary:
 * func:
 *   LDR  X8, [X18, #__stack_chk_guard]  ; Load canary from TLS
 *   STR  X8, [SP, #56]                  ; Store on stack frame
 *   ... function body ...
 *   LDR  X8, [SP, #56]                  ; Reload from stack
 *   LDR  X9, [X18, #__stack_chk_guard]  ; Reload reference
 *   CMP  X8, X9                         ; Compare
 *   B.NE __stack_chk_fail               ; Mismatch = SIGABRT
 */

/* GCC flags:
 * -fstack-protector        : char arrays > 8 bytes only
 * -fstack-protector-strong : arrays + address-taken locals (recommended)
 * -fstack-protector-all    : ALL functions (max coverage, <1% overhead)
 */
### Method 2: AddressSanitizer (ASan) — Best Tool for Development
ASan inserts shadow memory (1 byte per 8 app bytes) to track every allocation. Catches stack overflow, heap overflow, UAF, double-free. Reports exact file/line.
/* Compile: gcc -fsanitize=address -g -o app app.c */
/* Run:     ./app                                   */

/* ASan output example: */
/* ==12345==ERROR: AddressSanitizer: stack-buffer-overflow                  */
/* WRITE of size 100 at 0x7ffd12345678 thread T0                             */
/*     #0 0x400a23 in vulnerable_function app.c:5                            */
/*     #1 0x400b56 in main app.c:12                                          */
/* Address is located in stack of thread T0 at offset 96 in frame           */
/*     #0 0x4009f0 in vulnerable_function app.c:3                            */
/*   This frame has 1 object(s):
/*     [32, 96) 'buf' <== Memory access at offset 96 overflows this variable */

/* ASan Shadow Memory Layout (ARM64):                     */
/* App memory   Shadow byte (1:8 ratio)                   */
/* [allocated]  0x00 = accessible                         */
/* [red zone ]  0xFA = heap right redzone (overflow guard)*/
/* [freed    ]  0xFD = freed heap region                  */
/* [stack    ]  0xF1 = stack left redzone                 */
### Method 3: Valgrind — Deep Analysis (No Recompile Needed)
/* Run: valgrind --tool=memcheck --stack-protection=yes ./app */
/* No recompilation needed (but -g helps with line numbers)   */
/* Overhead: 20-50x (too slow for production, great for dev)  */

/* Output example: */
/* ==9876== Invalid write of size 1          */
/* ==9876==    at 0x4C30F45: memset          */
/* ==9876==    by 0x400610: main (test.c:7)  */
/* ==9876== Address 0x5205080 is 0 bytes     */
/* ==9876==  after a block of size 64 alloc'd*/
### Method 4: ulimit + Signal Handler
#include <signal.h>
#include <sys/resource.h>

/* 1. Set stack size limit */
struct rlimit rl = { .rlim_cur = 1024*1024, .rlim_max = 1024*1024 };
setrlimit(RLIMIT_STACK, &rl);

/* 2. Catch SIGSEGV on stack overflow with alternate signal stack */
static char altstack[SIGSTKSZ];
stack_t ss = { .ss_sp = altstack, .ss_size = SIGSTKSZ, .ss_flags = 0 };
sigaltstack(&ss, NULL);

void handler(int sig, siginfo_t *info, void *ctx) {
    write(2, "Stack overflow detected!\n", 25);
    _exit(1);
}

struct sigaction sa = {
    .sa_sigaction = handler,
    .sa_flags = SA_SIGINFO | SA_ONSTACK  /* Use alternate stack! */
};
sigaction(SIGSEGV, &sa, NULL);
### Method 5: pthread Guard Pages for Threads
pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setstacksize(&attr, 64 * 1024);   /* 64KB stack */
pthread_attr_setguardsize(&attr, 4096);         /* 4KB guard page */
pthread_create(&tid, &attr, thread_func, NULL);
/* When thread overflows stack -> hits guard page -> SIGSEGV */
## Heap Overflow Detection Methods
### Method 1: ASan — Primary Recommendation
ASan places red zones (poisoned guard regions) around every heap allocation. Any write into a red zone is caught immediately at the time of write.
/* gcc -fsanitize=address -g -o test test.c && ./test */
char *buf = malloc(64);
memset(buf, 'A', 128);  /* ❌ Write 128 bytes to 64-byte buffer */
/* ASan immediately reports: */
/* ERROR: AddressSanitizer: heap-buffer-overflow on address 0x60200000ef80 */
/* WRITE of size 128 at 0x60200000ef80 */
/* 0x60200000ef80 is located 0 bytes to the RIGHT of 64-byte region        */
### Method 2: Valgrind Memcheck
### Method 3: Electric Fence (libefence) — Immediate SIGSEGV
/* Install: sudo apt install electric-fence */
/* Compile: gcc -g -o test test.c -lefence  */
/* Electric Fence places each allocation at page boundary: */
/*                                              */
/* +------------------+------------------+      */
/* | malloc'd buffer  | [GUARD PAGE]     |      */
/* | (64 bytes)       |  UNMAPPED        |      */
/* | ends at page end | Access = SIGSEGV |      */
/* +------------------+------------------+      */
/* Overflow = immediate SIGSEGV (not delayed) */
### Method 4: fsanitize=bounds — Compile-Time Array Bounds
/* gcc -fsanitize=bounds -g -o test test.c */
int arr[10];
arr[15] = 42;
/* Runtime error: index 15 out of bounds for type 'int [10]' */
### Method 5: Custom Debug Allocator with Guard Patterns
#define MAGIC_GUARD  0xDEADBEEF

void *debug_malloc(size_t size)
{
    void *raw = malloc(sizeof(size_t) + size + sizeof(uint32_t));
    *(size_t *)raw = size;                            /* Store size */
    uint32_t *tail = (uint32_t *)(raw + sizeof(size_t) + size);
    *tail = MAGIC_GUARD;                              /* Place guard */
    return raw + sizeof(size_t);
}

void debug_free(void *ptr)
{
    void *raw = ptr - sizeof(size_t);
    size_t size = *(size_t *)raw;
    uint32_t *tail = (uint32_t *)(ptr + size);
    if (*tail != MAGIC_GUARD) {
        fprintf(stderr, "HEAP OVERFLOW DETECTED at %p\n", ptr);
        abort();
    }
    free(raw);
}
### Method 6: mprotect() Based Detection
/* Place unmapped guard page immediately after allocation */
void *guarded_alloc(size_t size)
{
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t total = ((size + page_size - 1) / page_size + 1) * page_size;
    void *mem = mmap(NULL, total + page_size,
                     PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    mprotect(mem + total, page_size, PROT_NONE); /* Guard page */
    return mem + total - size; /* Buffer ends exactly at guard page */
}
## Tool Comparison Table
| Tool | Overhead | Stack OOB | Heap OOB | Use-After-Free | Uninit Reads | ARM Support |
| ASan | 2–3x | ✅ Yes | ✅ Yes | ✅ Yes | ❌ No | ✅ Full |
| MSan | 3x | ❌ No | ❌ No | ❌ No | ✅ Yes | ✅ Limited |
| TSan | 5–10x | ❌ No | ❌ No | ❌ No | ❌ No | ✅ Full |
| Valgrind | 20–50x | ❌ Limited | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Slow |
| Electric Fence | High mem | ❌ No | ✅ Yes | ❌ No | ❌ No | ✅ Yes |
| Stack Protector | <1% | ✅ Canary | ❌ No | ❌ No | ❌ No | ✅ Yes |
| UBSan bounds | 5–10% | ✅ Arrays | ❌ No | ❌ No | ❌ No | ✅ Yes |

## Recommended ARM Compile Flags
# Development (maximum detection):
aarch64-linux-gnu-gcc \
    -fsanitize=address,undefined \
    -fstack-protector-all      \
    -D_FORTIFY_SOURCE=2        \
    -g -O1                     \
    -o app app.c

# Production (hardened binary):
aarch64-linux-gnu-gcc \
    -fstack-protector-strong   \
    -D_FORTIFY_SOURCE=2        \
    -fPIE -pie                 \
    -Wl,-z,relro,-z,now        \
    -O2                        \
    -o app app.c

# CI/Automated testing (combined sanitizers):
aarch64-linux-gnu-gcc -fsanitize=address,undefined -g -O1 -o app app.c
# 21. Interview Quick-Reference Summary
ℹ️  Memorize this section! These are the most commonly asked questions in Qualcomm Linux Kernel driver interviews.
## Kernel Errors One-Liner Table (All 18 Errors)
| # | Error | What It Is | Fix in One Line |
| 1 | NULL Pointer Dereference | Access via NULL/uninit pointer | Always check return of of_*(), platform_get_*(), devm_*() |
| 2 | Use-After-Free | Access after kfree() | Sync IRQ, kill tasklet, terminate DMA BEFORE freeing |
| 3 | Double Free | kfree() called twice | Set ptr = NULL after every kfree() |
| 4 | Memory Leaks | kmalloc without kfree | Use devm_* APIs or goto cleanup chains in reverse order |
| 5 | Buffer Overflow | Write beyond allocation | Validate len before memcpy; use FORTIFY_SOURCE |
| 6 | Stack Overflow | Exceed kernel stack (16KB) | No large local arrays (>256B); use kmalloc for large bufs |
| 7 | Slab Corruption | Corrupt SLUB freelist | Secondary symptom; fix root overflow/UAF/DMA overrun |
| 8 | Uninitialized Memory | Read garbage from alloc'd mem | Use kzalloc(); memset structs; enable INIT_STACK_ALL_ZERO |
| 9 | Invalid Memory Access | Access unmapped/protected VA | Always ioremap(); never deref __user directly (use copy_from_user) |
| 10 | Spinlock + Sleep | Sleep while holding spinlock | GFP_ATOMIC under lock; allocate outside lock; udelay not msleep |
| 11 | Wrong IRQ Flags | Missing IRQF_SHARED/ONESHOT | IRQF_SHARED for shared lines; IRQF_ONESHOT for threaded; match level/edge to HW |
| 12 | Missing IRQ Disable | ISR sees half-updated data | spin_lock_irqsave() when sharing data between process ctx and ISR |
| 13 | ISR Reentrancy | Shared ISR on multiple CPUs | spinlock inside ISR for shared state; use per-CPU data where possible |
| 14 | Softirq/Tasklet Misuse | Sleeping in bottom-half | GFP_ATOMIC, spin_lock_bh from process ctx; tasklet_kill before kfree |
| 15 | Resource Leaks | Not releasing IO/IRQ/DMA/GPIO | devm_* preferred; symmetric remove(); test with modprobe/rmmod loop |
| 16 | Wrong Register Access | Wrong offset/size/endianness | Named macros; readl()/writel(); cpu_to_le32(); RMW with mask |
| 17 | Page Table Corruption | Incorrect PTE/PMD manipulation | Use kernel APIs only; flush_tlb_page() after every PTE change |
| 18 | Incorrect mmap | Wrong vm_flags or caching | Validate offset/size; VM_IO|VM_PFNMAP; pgprot_noncached() for MMIO |

## Lock Selection Rule — Memorize This!
/* Lock Selection Decision Table */
/*                                                                   */
/* Context Sharing               ->  Lock API to Use                */
/* ---------------------------------------------------------------  */
/* Process ctx ONLY              ->  mutex (can sleep)              */
/* Process ctx <-> Softirq       ->  spin_lock_bh()                 */
/* Process ctx <-> Hardirq       ->  spin_lock_irqsave(&lock, flags)*/
/* Softirq     <-> Hardirq       ->  spin_lock_irqsave()            */
/* IRQ handler (SMP-safe)        ->  spin_lock()                    */
/* Read-heavy, process ctx       ->  rw_semaphore (down_read/write) */
/* Read-heavy, any ctx           ->  rwlock + _irqsave variants     */
/* Mostly read, rare write       ->  RCU (rcu_read_lock / rcu_assign_pointer)*/
/* Simple counters               ->  atomic_t / atomic64_t          */
/*                                                                   */
/* RULE: If either side is in IRQ context -> use spin_lock_irqsave  */
/* RULE: If only process context  -> use mutex                      */
/* RULE: spin_lock NEVER sleeps   -> never call GFP_KERNEL under it */
## Teardown Order — Memorize This!
/* CORRECT driver shutdown / remove() sequence: */
/*                                               */
/* Step 1: disable_irq() / free_irq()           */
/*         -> Stop new interrupts from firing    */
/*                                               */
/* Step 2: synchronize_irq()                    */
/*         -> Wait for any in-progress ISR to   */
/*            complete before we free its data   */
/*                                               */
/* Step 3: tasklet_kill()                        */
/*         -> Wait for scheduled tasklet to     */
/*            finish; prevents re-scheduling     */
/*                                               */
/* Step 4: cancel_work_sync() / cancel_delayed_work_sync() */
/*         -> Wait for queued work item to       */
/*            complete                           */
/*                                               */
/* Step 5: dmaengine_terminate_sync()            */
/*         -> Stop DMA engine; wait for all      */
/*            in-flight transfers to complete    */
/*                                               */
/* Step 6: Free resources (kfree, iounmap, etc) */
/*         -> NOW safe since all async ops done  */
## Debugging Tools Quick Reference
### Kernel Tools
| Tool | Detects | Kernel Config | How to Use |
| KASAN | UAF, heap/stack OOB, double-free | CONFIG_KASAN=y | Auto-reports with full backtrace |
| KMSAN | Uninitialized memory reads | CONFIG_KMSAN=y | Reports on first uninit read |
| KFENCE | Heap OOB, UAF (low overhead, sampling) | CONFIG_KFENCE=y | Production-safe (<1% overhead) |
| kmemleak | Memory leaks (orphaned allocations) | CONFIG_DEBUG_KMEMLEAK=y | echo scan > /sys/kernel/debug/kmemleak |
| Lockdep | Deadlocks, lock ordering violations | CONFIG_PROVE_LOCKING=y | Auto-reports on first violation |
| KCSAN | Data races, race conditions | CONFIG_KCSAN=y | Sampling-based race detector |
| DEBUG_ATOMIC_SLEEP | Sleeping in atomic/interrupt context | CONFIG_DEBUG_ATOMIC_SLEEP=y | "in_atomic():1" in BUG message |
| SLUB_DEBUG | Slab corruption, red zone violations | CONFIG_SLUB_DEBUG=y | Boot: slub_debug=FZPU |
| VMAP_STACK | Kernel stack overflow (guard pages) | CONFIG_VMAP_STACK=y | Panic on stack guard page hit |
| Sparse | __iomem/__user annotation misuse | N/A (static analysis) | make C=2 drivers/..../file.o |
| ftrace | Function tracing, IRQ latency, scheduling | CONFIG_FTRACE=y | echo function > /sys/kernel/debug/tracing/current_tracer |
| T32 (Lauterbach) | Register dump, page walk, watchpoints, JTAG halt | External HW debugger | mmu.info VA, r.s (registers), frame |

### Userspace Tools
| Tool | Detects | Compile Flag | Usage |
| ASan | Heap/stack OOB, UAF, double-free | -fsanitize=address | ./app (2-3x slowdown) |
| TSan | Data races, lock order violations | -fsanitize=thread | ./app (5-10x slowdown) |
| UBSan | Integer overflow, alignment, shift, null | -fsanitize=undefined | ./app (<2x overhead) |
| MSan | Uninitialized variable reads | -fsanitize=memory | ./app (3x slowdown) |
| Valgrind | All memory errors + thread bugs | None needed | valgrind --tool=memcheck ./app |
| strace | FD leaks, failed syscalls, permissions | None | strace -f ./app 2>&1 | grep EBADF |
| Electric Fence | Heap overflow (immediate SIGSEGV) | -lefence | ./app (high memory overhead) |

## "Which Tool for Which Bug" — Decision Guide
| Symptom / Bug Observed | Primary Tool | Secondary / Confirmatory |
| Random crash / SIGSEGV in kernel | KASAN (CONFIG_KASAN=y) | T32: check FAR_EL1 + ESR_EL1 + backtrace |
| Silent data corruption | KCSAN + KASAN | KFENCE for production reproduction |
| System freeze / hang | Lockdep + SOFTLOCKUP_DETECTOR | T32: task.dtask, frame /caller |
| Memory growing over time | kmemleak scan | Valgrind --leak-check (userspace) |
| Intermittent crash (production) | KFENCE (<1% overhead) | SLUB_DEBUG=FZ for slab issues |
| Crash in interrupt handler | DEBUG_ATOMIC_SLEEP + Lockdep | Check "in_atomic():1" in dmesg |
| Wrong data from hardware | Check endianness + caching (pgprot) | T32 Data.In to verify register values |
| Post-mortem crash dump analysis | T32 / crash tool | decode_stacktrace.sh + vmlinux symbols |
| Userspace memory bug | ASan (-fsanitize=address) | Valgrind for detailed malloc tracking |
| Userspace data race | TSan (-fsanitize=thread) | Helgrind (valgrind --tool=helgrind) |

## Sample Interview Q&A — 3 Model Answers
### Q1: You see a kernel panic with "Translation Fault Level 3 on ARM64". What do you do?
Model Answer:
"Translation fault L3 means the PTE is invalid for that virtual address. My analysis steps are:"
  1. Check FAR_EL1 (Fault Address Register) -> identifies the exact virtual address
  2. Check ESR_EL1 -> confirms DABT (Data Abort) with DFSC=0x07 (L3 translation fault)
  3. Analyze the call backtrace -> identify which function accessed this address
  4. If FAR is near 0x0 -> NULL dereference (missing NULL check after of_match, platform_get)
  5. If FAR is a kernel VA -> possible UAF where the page was freed/unmapped
  6. Enable KASAN (CONFIG_KASAN=y) to catch and identify the root cause reproducibly
  7. Use T32: mmu.info <VA> to walk the page table and verify the PTE state
### Q2: How do you prevent resource leaks in a Qualcomm platform driver?
Model Answer:
"I follow a three-tier approach:"
  Tier 1 (Best): Use devm_* APIs - devm_kzalloc, devm_ioremap_resource, devm_clk_get,
           devm_request_irq, dmam_alloc_coherent. These auto-free on probe failure
           or device removal with no manual cleanup needed.

  Tier 2: For order-sensitive cleanup or when devm_* is inappropriate (runtime PM,
          active DMA), use goto cleanup chains with labels in REVERSE acquisition order.

  Tier 3: Verify with: (a) repeated modprobe/rmmod loop testing,
          (b) /proc/iomem and /proc/interrupts inspection after rmmod,
          (c) kmemleak scan for orphaned allocations
### Q3: What is the difference between spin_lock, spin_lock_bh, and spin_lock_irqsave?
| API | Disables | Use Case | When to Choose |
| spin_lock(&lock) | Preemption only | Both sides in process ctx or same IRQ level | Lock within hardirq handler; or two process-ctx threads |
| spin_lock_bh(&lock) | Preemption + softirqs | Process ctx sharing data with softirq/tasklet | Network driver: ioctl path shares queue with NAPI |
| spin_lock_irqsave(&l, f) | Preemption + all IRQs + saves DAIF | Process ctx sharing data with hardirq handler | UART driver: write() path shares buffer with TX ISR |

Key: "If either side is in hardirq context -> use spin_lock_irqsave(). If only softirq -> spin_lock_bh(). If only process context -> mutex."
## ARM-Specific Key Points Table
| Concept | Value / Register | Description |
| Kernel stack size (ARM32) | 8 KB | Very small! Never use large local arrays. Use kmalloc for >256 bytes. |
| Kernel stack size (ARM64) | 16 KB | Still limited. CONFIG_VMAP_STACK adds guard pages for overflow detection. |
| PAN (Privileged Access Never) | SCTLR_EL1.PAN | Prevents kernel from directly accessing userspace VA. Always use copy_from/to_user(). |
| ESR_EL1 | Exception Syndrome Reg | Fault type: EC=0x25 (DABT), DFSC=0x04-0x07 (translation), 0x0D-0x0F (permission) |
| FAR_EL1 | Fault Address Reg | Contains the virtual address that caused the fault. Check this first in any Oops. |
| DAIF bits | D=Debug, A=SError, I=IRQ, F=FIQ | Interrupt mask bits. local_irq_save sets I-bit (MSR DAIFSet,#2). |
| DSB / ISB / DMB | Barrier instructions | DSB: data sync (all memory ops complete). ISB: instruction sync. DMB: memory ordering. |
| TLB flush | TLBI VAE1IS instruction | REQUIRED after every PTE modification. flush_tlb_page() -> dsb ishst + tlbi + dsb ish + isb |
| MMIO caching rule | Device-nGnRnE | Hardware registers must be non-cacheable. pgprot_noncached() for MMIO, pgprot_writecombine() for DMA. |
| MTE (ARMv8.5+) | Memory Tagging Extension | Hardware-accelerated ASan. Tags memory allocations; mismatch on UAF/overflow = SIGSEGV. |

## Recommended Kernel Debug Config (ARM64 / Qualcomm Development Build)
# === MEMORY DEBUG ===
CONFIG_KASAN=y
CONFIG_KASAN_GENERIC=y        # Generic mode for ARM64
CONFIG_KASAN_INLINE=y         # Better performance than outline
CONFIG_KFENCE=y               # Low-overhead production sampling
CONFIG_KMEMLEAK=y             # Memory leak detector
CONFIG_SLUB_DEBUG=y           # Slab allocator debug (red zones, poison)
CONFIG_DEBUG_PAGEALLOC=y      # Unmaps freed pages
CONFIG_PAGE_POISONING=y       # Fills freed pages with pattern
CONFIG_PAGE_TABLE_CHECK=y     # Validates PT entries on modification

# === LOCKING / CONCURRENCY ===
CONFIG_PROVE_LOCKING=y        # Lockdep: deadlock detection
CONFIG_DEBUG_LOCK_ALLOC=y     # Lock allocation debugging
CONFIG_DEBUG_MUTEXES=y        # Mutex debugging
CONFIG_DEBUG_SPINLOCK=y       # Spinlock debugging
CONFIG_DEBUG_ATOMIC_SLEEP=y   # Catch sleep in atomic context
CONFIG_KCSAN=y                # Data race detection

# === STACK ===
CONFIG_VMAP_STACK=y           # Virtual-mapped stacks with guard pages
CONFIG_FRAME_WARN=1024        # Warn if stack frame > 1KB
CONFIG_STACKPROTECTOR_STRONG=y # Stack canary protection
CONFIG_INIT_STACK_ALL_ZERO=y  # Zero-initialize all stack variables

# === LOCKUP DETECTION ===
CONFIG_SOFTLOCKUP_DETECTOR=y  # CPU stuck in kernel > 20s
CONFIG_HARDLOCKUP_DETECTOR=y  # CPU stuck with IRQs off
CONFIG_DETECT_HUNG_TASK=y     # Tasks in D-state too long
CONFIG_WQ_WATCHDOG=y          # Workqueue stall detection

# === UNDEFINED BEHAVIOR ===
CONFIG_UBSAN=y                # Undefined behavior sanitizer
CONFIG_FORTIFY_SOURCE=y       # memcpy/strcpy bounds checking

# === TRACING ===
CONFIG_FTRACE=y               # Function tracer
CONFIG_STACKTRACE=y           # Stack trace support
CONFIG_DEBUG_INFO=y           # Full debug info for T32/GDB
CONFIG_FRAME_POINTER=y        # Reliable stack unwinding
## Recommended Userspace Compile Flags
| Build Type | GCC Flags | Purpose |
| Development | -fsanitize=address,undefined -fstack-protector-all -D_FORTIFY_SOURCE=2 -g -O1 | Maximum error detection; all sanitizers active |
| CI/Testing | -fsanitize=address,undefined,thread -g -O1 -Werror | Catch all races and memory bugs in automated tests |
| Production | -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -pie -Wl,-z,relro,-z,now -O2 | Hardened binary: ASLR + stack canary + RELRO + NX |
| Sanitizer combo | -fsanitize=address -fsanitize=undefined -fsanitize=bounds | Note: ASan + TSan cannot be combined; use separately |

Document Series Summary
| Document | Pages | Sections Covered | Key Topics |
| Part 1 of 3 | Pages 1-20 | Errors 1-7 | NULL deref, UAF, Double-free, Leaks, Overflow, Slab |
| Part 2 of 3 | Pages 21-40 | Errors 8-14 | Uninit mem, Invalid access, Spinlock sleep, IRQ flags, Softirq |
| Part 3 of 3 | Pages 41-60 | Errors 15-18 + Userspace + Reference | Leaks, Register, Page table, mmap, Userspace 12-cat, Interview QA |

Good luck, Sandeep! You are well-prepared for your Qualcomm interview.
Every ARM64 fault has a story. Read the ESR. Check the FAR. Enable KASAN. Trust the backtrace.

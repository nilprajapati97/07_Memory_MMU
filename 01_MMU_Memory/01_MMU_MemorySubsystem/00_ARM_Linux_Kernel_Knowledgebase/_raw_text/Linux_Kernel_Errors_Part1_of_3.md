Linux Kernel & Userspace
Runtime Errors
Part 1 of 3  |  Pages 1-20

Prepared for: Sandeep Kumar
Interview Preparation  |  Qualcomm Technologies
Focus: ARM64 / Linux Kernel / Embedded Systems
Coverage in this document:
1. NULL Pointer Dereference - ARM64 deep dive with 5 real scenarios
2. Use-After-Free (UAF) - 5 Qualcomm driver scenarios + KASAN
3. Double Free - SLUB corruption, error-path patterns, fix strategies
4. Memory Leaks - goto cleanup, devm_* APIs, kmemleak detection
5. Buffer Overflow / Underflow - Stack and heap, KASAN + FORTIFY_SOURCE
6. Stack Overflow - 8KB/16KB limit, large arrays, deep recursion
7. Slab Corruption - SLUB layout, scenarios, slub_debug detection
# 1. NULL Pointer Dereference
A NULL pointer dereference occurs when the kernel attempts to read from or write to virtual address 0x0 (or a small offset from it) through a pointer that was never initialized or was explicitly set to NULL. On ARM64, this triggers a Data Abort exception, which the kernel translates into an Oops or Panic.
1.1  What It Is
The first page of virtual address space (0x00000000 - 0x00000FFF) is intentionally left unmapped in the kernel page tables. Accessing it causes a hardware exception.
- ARM32 (ARMv7): Triggers a Data Abort → vector at 0xFFFF0010 (high vectors) → calls do_DataAbort() → do_page_fault() → kernel Oops
- ARM64 (ARMv8): Triggers a Synchronous Exception at EL1 → el1_sync handler → do_mem_abort() → do_mem_abort() → kernel Oops or Panic
1.2  ARM64 MMU Perspective
When a NULL dereference occurs on ARM64, here is what happens at the hardware level:

1. CPU executes: LDR X0, [X1]   where X1 = 0x0000000000000000

2. MMU Translation Walk:
   VA: 0x0000000000000000
   L0 Index: 0  =>  L0 Entry: INVALID (descriptor = 0x0)
   => Translation Fault Level 0

3. Exception Registers on ARM64:
   ESR_EL1.EC  = 0x25  (Data Abort from current EL)
   ESR_EL1.ISS = DFSC 0x04  (Translation fault, level 0)
   FAR_EL1     = 0x0000000000000000  (faulting address)

4. Kernel Handler Chain:
   el1_sync -> el1_da -> do_mem_abort() -> do_page_fault()
   -> __do_kernel_fault() -> die("Oops") -> panic (if fatal)

1.3  ESR / FAR Register Reference Table
| DFSC Value | Fault Type | Level | Common Cause |
| 0x04 | Translation fault | L0 | Completely unmapped (wild/NULL ptr) |
| 0x05 | Translation fault | L1 | Large region unmapped |
| 0x06 | Translation fault | L2 | Page directory not present |
| 0x07 | Translation fault | L3 | Specific PTE invalid/NULL |
| 0x0D | Permission fault | L1 | Write to read-only page |
| 0x21 | Alignment fault | - | Unaligned MMIO/Device access |


1.4  Real-Time Scenario A: Platform Driver Probe Failure
This is extremely common in Qualcomm BSP bring-up when device tree nodes are misconfigured. of_match_device() returns NULL if the DT compatible string does not match any entry in the driver's match table.

/* Qualcomm SoC - clock controller driver */
static int qcom_clk_probe(struct platform_device *pdev)
{
    struct qcom_clk_data *clk_data;
    /* BUG: of_match_device() returns NULL if DT compatible mismatch */
    const struct of_device_id *match =
        of_match_device(qcom_clk_match, &pdev->dev);

    /* NULL DEREFERENCE: crash at offset 0x08 from NULL */
    clk_data = (struct qcom_clk_data *)match->data;  /* CRASH */
    ...
}

/* ARM64 Oops output: */
Unable to handle kernel NULL pointer dereference at virtual address
    0000000000000008
ESR = 0x96000004   EC=0x25: DABT (current EL), IL=32 bits
FAR = 0x0000000000000008  <- offset 8 = match->data member
Call trace:
  qcom_clk_probe+0x3c/0x1a0
  platform_drv_probe+0x50/0xa0
  really_probe+0x108/0x348

/* FIX: Always NULL-check after of_match_device */
const struct of_device_id *match =
    of_match_device(qcom_clk_match, &pdev->dev);
if (!match || !match->data) {
    dev_err(&pdev->dev, "No matching device data\n");
    return -ENODEV;
}

1.5  Real-Time Scenario B: I2C/SPI Client Driver - Missing Platform Data
Common in sensor drivers on Qualcomm IoT platforms. On DT-based systems, platform_data is NULL because the driver was written for board-file based systems.

/* Touch controller driver on Qualcomm IoT board */
static int touch_i2c_probe(struct i2c_client *client,
                            const struct i2c_device_id *id)
{
    struct touch_platform_data *pdata;
    /* On DT-based ARM platforms, platform_data is NULL */
    pdata = client->dev.platform_data;

    /* NULL DEREFERENCE: pdata is NULL -> offset access */
    ts->irq_gpio    = pdata->irq_gpio;    /* FAR = 0x14 */
    ts->reset_gpio  = pdata->reset_gpio;  /* FAR = 0x18 */
}

/* T32 / crashdump register dump on ARM64: */
PC:  0xFFFFFFC0005A3210  (touch_i2c_probe+0x48)
ESR: 0x96000004  <- Data Abort, translation fault level 0
FAR: 0x0000000000000014  <- NULL + 0x14 (irq_gpio offset)

/* FIX: Fall back to device tree parsing */
pdata = client->dev.platform_data;
if (!pdata) {
    pdata = touch_parse_dt(&client->dev);
    if (IS_ERR(pdata))
        return PTR_ERR(pdata);
}

1.6  Real-Time Scenario C: Interrupt Handler - DMA Buffer Not Allocated
Common in Qualcomm GENI (QUP) serial engine drivers. If DMA allocation failed during open() but the IRQ is still registered, the handler fires with a NULL buffer pointer. Since this runs in hardirq context, the kernel cannot recover - it results in a full kernel panic.

/* UART DMA completion callback on Qualcomm GENI */
static void qcom_geni_rx_dma_callback(void *data)
{
    struct qcom_geni_serial_port *port = data;
    struct dma_buf *rx_buf = port->rx_dma_buf;
    /* rx_dma_buf might be NULL if DMA allocation failed */
    /* NULL DEREFERENCE in interrupt context => KERNEL PANIC */
    memcpy(port->rx_fifo, rx_buf->vaddr, rx_buf->len); /* CRASH */
}

/* Kernel panic output: */
Kernel panic - not syncing: Fatal exception in interrupt
Hardware name: Qualcomm Technologies, Inc. QCS404 (DT)
Call trace:
  qcom_geni_rx_dma_callback+0x24/0x90
  dma_complete_callback+0x38/0x60

/* FIX: Validate both port and buffer before accessing */
if (!port || !port->rx_dma_buf) {
    pr_err("RX DMA callback: invalid buffer\n");
    return;
}

1.7  Real-Time Scenario D: IS_ERR vs NULL Confusion (Deferred Probe)
Very common during Qualcomm board bring-up. devm_clk_get() returns ERR_PTR(-EPROBE_DEFER), not NULL. Checking for NULL instead of IS_ERR() lets the error pointer through, causing a fault at a very high kernel address.

mclk = devm_clk_get(&pdev->dev, "cam_mclk");

/* BUG: Checking for NULL instead of IS_ERR() */
if (!mclk)  /* WRONG CHECK: ERR_PTR(-EPROBE_DEFER) is non-NULL */
    return -ENODEV;

/* mclk is actually 0xFFFFFFFFFFFFFFE2 (ERR_PTR) */
clk_prepare_enable(mclk);  /* Dereferences error pointer -> CRASH */

/* ARM64 Oops: */
Unable to handle kernel paging request at virtual address
    fffffffffffffffe

/* FIX: Always use IS_ERR() for pointer-returning kernel APIs */
mclk = devm_clk_get(&pdev->dev, "cam_mclk");
if (IS_ERR(mclk)) {
    if (PTR_ERR(mclk) == -EPROBE_DEFER)
        return -EPROBE_DEFER;  /* Try again later */
    dev_err(&pdev->dev, "Failed to get mclk: %ld\n",
            PTR_ERR(mclk));
    return PTR_ERR(mclk);
}

1.8  Real-Time Scenario E: Race Condition During Remove/Unbind
Common in hotplug/runtime PM paths on Qualcomm platforms. If runtime_resume is called during or after remove(), dev_get_drvdata() returns NULL because the remove path cleared it too early.

static int qcom_runtime_resume(struct device *dev)
{
    /* RACE: NULL if runtime_resume fires during/after remove() */
    struct qcom_device *qdev = dev_get_drvdata(dev);
    clk_prepare_enable(qdev->core_clk);  /* NULL deref -> CRASH */
    writel(0x1, qdev->base + REG_OFFSET);
}

/* BUG in qcom_remove: clears drvdata BEFORE disabling PM */
static int qcom_remove(struct platform_device *pdev)
{
    platform_set_drvdata(pdev, NULL);  /* Sets to NULL too early */
    pm_runtime_disable(&pdev->dev);    /* runtime_resume can race */
}

/* FIX: Disable PM BEFORE clearing drvdata */
static int qcom_remove(struct platform_device *pdev)
{
    pm_runtime_disable(&pdev->dev);    /* Stop all PM callbacks */
    pm_runtime_put_noidle(&pdev->dev); /* Balance reference */
    platform_set_drvdata(pdev, NULL);  /* NOW safe to clear */
}

1.9  T32 Debugging Commands for NULL Dereference

; Show all CPU registers - look at PC and FAR for fault address
r.s

; Disassemble around faulting PC
d.l PC-0x10

; Check pointer value in a local variable
v.v %s local_var_name

; Full backtrace with local variables
frame /locals

; Inspect page tables for VA 0x0 - should show INVALID
mmu.info 0x0

; Check ESR and FAR registers directly (ARM64)
Register.Set ELR_EL1    ; Exception Link Register (faulting PC)
Register.Set FAR_EL1    ; Fault Address Register
Register.Set ESR_EL1    ; Exception Syndrome Register

1.10  Summary Table: Common NULL Dereference Patterns
| Pattern | Common Location | Root Cause | Fix |
| Missing NULL check after of_* APIs | DT parsing in probe() | Misconfigured device tree compatible | if (!match) return -ENODEV |
| IS_ERR() vs NULL confusion | Clock/regulator/GPIO get | API returns ERR_PTR, not NULL | Use IS_ERR() + PTR_ERR() |
| Race during remove/unbind | Runtime PM callbacks | Incorrect teardown ordering | pm_runtime_disable() first |
| Uninitialized struct member | Init/probe functions | Forgot to set a pointer field | Use kzalloc() to zero all |
| Failed alloc without check | DMA/buffer allocation | OOM under memory pressure | Always check kmalloc return |
| Deferred probe not handled | Multi-dependency drivers | Dependency not yet probed | Return -EPROBE_DEFER |


# 2. Use-After-Free (UAF)
A Use-After-Free occurs when the kernel accesses memory that has already been freed via kfree(), put_device(), or similar deallocation functions. The freed memory may be reallocated to another object, causing data corruption, privilege escalation, or kernel crashes.
2.1  Why UAF is Dangerous on ARM (Qualcomm SoCs)
- Hard to reproduce - Depends on timing and SLUB allocator behavior
- Exploitable - Attackers can reclaim freed memory with controlled data to gain privilege
- Intermittent - May work 99% of the time, crash occasionally in field devices
- Silent corruption - Without KASAN, freed memory may still contain valid-looking data
2.2  Memory State Before/After kfree()

Before kfree():
  struct qcom_device {
    void __iomem *base = 0xFFFFFF80_10000000   <- valid
    struct clk  *clk   = 0xFFFFFF80_C0A23000   <- valid
    int state          = 3                      <- valid
  }

After kfree() WITH KASAN enabled:
  0xDEAD0100  0xDEAD0100  0xDEAD0100  ...      <- POISONED
  KASAN shadow byte: 0xFF (freed)
  => Any access triggers KASAN BUG report immediately

After kfree() WITHOUT KASAN (DANGEROUS):
  [SLAB freelist ptr] [old data still intact]
  => Code may appear to work until slab is reallocated
  => Intermittent crashes, silent data corruption

2.3  Scenario A: V4L2 Camera Driver - Early Free During Streaming
Extremely common on Qualcomm camera subsystem (CSI/CSID/VFE/IFE). The VFE frame-done ISR accesses a buffer that was freed by the streamoff path running concurrently on another CPU.

/* Qualcomm VFE (Video Front End) - buffer freed during streaming */
static void vfe_buf_cleanup(struct vb2_buffer *vb)
{
    struct vfe_buffer *buf = container_of(vb, struct vfe_buffer,
                                          vb.vb2_buf);
    list_del(&buf->queue);
    kfree(buf);   /* <- Buffer freed here in streamoff path */
}

/* ISR: VFE frame done interrupt - runs on different CPU */
static irqreturn_t vfe_isr(int irq, void *data)
{
    buf = list_first_entry(&vfe->active_bufs, struct vfe_buffer,
                           queue);
    /* USE-AFTER-FREE: buf was freed by vfe_buf_cleanup() */
    dma_unmap_single(vfe->dev, buf->paddr, buf->size,
                     DMA_FROM_DEVICE);  /* CRASH */
    vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
}

/* KASAN Report on SM8250: */
BUG: KASAN: use-after-free in vfe_isr+0x84/0x1f0
Read of size 8 at addr ffff0000c5a23040
Freed by task 1205: vfe_buf_cleanup+0x38/0x60

/* FIX: Stop hardware FIRST, then synchronize IRQ, then free */
static void vfe_stop_streaming(struct vb2_queue *q)
{
    writel(0x0, vfe->base + VFE_IRQ_MASK);   /* 1. Stop HW */
    writel(0x1, vfe->base + VFE_HALT_CMD);
    synchronize_irq(vfe->irq);               /* 2. Wait for ISR */
    list_for_each_entry_safe(buf, tmp, ...) { /* 3. Free buffers */
        vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
    }
}

2.4  Scenario B: Workqueue Accessing Freed Structure (Thermal Driver)
Common in Qualcomm power management and thermal drivers. The remove() path frees the structure before cancelling the delayed work that still holds a pointer to it.

/* BUG in qcom_tsens_remove: frees struct before cancelling work */
static int qcom_tsens_remove(struct platform_device *pdev)
{
    struct qcom_tsens_sensor *sensor = platform_get_drvdata(pdev);
    kfree(sensor);  /* <- Freed HERE */
    /* Work might still be queued and access sensor->base ! */
    cancel_delayed_work_sync(&sensor->polling_work); /* TOO LATE */
}

/* FIX: Cancel work FIRST, then free */
static int qcom_tsens_remove(struct platform_device *pdev)
{
    struct qcom_tsens_sensor *sensor = platform_get_drvdata(pdev);
    cancel_delayed_work_sync(&sensor->polling_work); /* 1st */
    iounmap(sensor->base);
    kfree(sensor);  /* NOW safe */
}

2.5  Scenario C: RCU Use-After-Free in Network Driver (RMNET)
Common in Qualcomm EMAC/RMNET network drivers. Accessing an RCU-protected pointer outside the rcu_read_lock() section means the object can be freed by call_rcu() between the dereference and the use.

/* BUG: Using port pointer OUTSIDE rcu_read_lock() */
static void rmnet_rx_handler(struct sk_buff *skb)
{
    rcu_read_lock();
    port = rcu_dereference(skb->dev->rx_handler_data);
    rmnet_map_deaggregate(skb, port);
    rcu_read_unlock();
    /* port might be freed here via call_rcu()! */
    netdev_dbg(port->dev, "processed\n");  /* UAF CRASH */
}

/* FIX: Do ALL work inside RCU read section */
    rcu_read_lock();
    port = rcu_dereference(skb->dev->rx_handler_data);
    if (!port) { rcu_read_unlock(); kfree_skb(skb); return; }
    dev = port->dev;                    /* Save needed ref */
    rmnet_map_deaggregate(skb, port);
    netdev_dbg(dev, "processed\n");     /* Use saved ref */
    rcu_read_unlock();

2.6  Scenario D: Kobject/Sysfs UAF - Reference Count Bug (Adreno GPU)
Common in Qualcomm SoC drivers exposing sysfs attributes. The adreno_remove() frees the structure directly rather than decrementing the kobject reference count, causing a UAF when a concurrent sysfs read still holds a reference.

/* BUG: sysfs attribute file may be open when driver is removed */
static int adreno_remove(struct platform_device *pdev)
{
    struct adreno_device *adreno = platform_get_drvdata(pdev);
    kobject_del(&adreno->kobj);
    kfree(adreno);  /* FREED while sysfs read may be active! */
}

/* FIX: Use kobject release callback - only free when refcount=0 */
static void adreno_kobj_release(struct kobject *kobj)
{
    struct adreno_device *adreno =
        container_of(kobj, struct adreno_device, kobj);
    kfree(adreno);  /* Only called when ALL references dropped */
}

static int adreno_remove(struct platform_device *pdev)
{
    /* kobject_put calls adreno_kobj_release when refcount hits 0 */
    kobject_put(&adreno->kobj);  /* Do NOT kfree here! */
}

2.7  Scenario E: DMA Completion After Buffer Free (QSPI)
Critical in Qualcomm QSPI/QUP/BAM DMA engines. On a DMA timeout, the buffer is freed while the DMA controller continues writing to the physical address independently of the CPU. If SMMU is enabled, it catches the stale DMA access.

/* BUG: DMA timeout path frees buffer while DMA engine still active */
if (!wait_for_completion_timeout(&qspi->dma_done,
                                  msecs_to_jiffies(1000))) {
    kfree(qspi->rx_buf);   /* FREED - but DMA still writing! */
    return -ETIMEDOUT;
}

/* SMMU fault if IOMMU enabled: */
arm-smmu 15000000.iommu: Unhandled context fault:
  fsr=0x402, iova=0x0000000145A23000, fsynr=0x30001

/* FIX: Stop DMA FIRST, unmap, THEN free */
if (!wait_for_completion_timeout(&qspi->dma_done,
                                  msecs_to_jiffies(1000))) {
    dmaengine_terminate_sync(qspi->rx_chan); /* 1. Stop DMA */
    dma_unmap_single(qspi->dev, qspi->rx_dma,
                     xfer->len, DMA_FROM_DEVICE); /* 2. Unmap */
    kfree(qspi->rx_buf);  /* 3. NOW safe to free */
    return -ETIMEDOUT;
}

2.8  Detecting UAF with KASAN Configuration

# Kernel config for Qualcomm ARM64 targets:
CONFIG_KASAN=y
CONFIG_KASAN_GENERIC=y        # For ARM64
CONFIG_KASAN_INLINE=y          # Better performance than OUTLINE
CONFIG_STACKTRACE=y
CONFIG_SLUB_DEBUG=y
CONFIG_PAGE_OWNER=y

# Boot command line options:
kasan.fault=panic              # Panic on first UAF (for CI)
kasan.fault=report             # Just report (for development)
slub_debug=FZPU                # F=Poison Z=RedZone P=Panic U=Tracking

2.9  T32 Debugging for UAF on Qualcomm Target

; Set data watchpoint on freed address to catch access:
Break.Set 0xFFFF0000C5A23040 /ReadWrite /DATA

; When hit, inspect call stack and local variables:
frame /locals /caller

; Check SLUB state of the suspect object:
task.dtask
slab.info 0xFFFF0000C5A23040

; Calculate KASAN shadow address for this memory:
; Shadow = (addr >> 3) + KASAN_SHADOW_OFFSET
; ARM64: KASAN_SHADOW_OFFSET = 0xDFFF200000000000
; shadow_addr = (0xFFFF0000C5A23040 >> 3) + 0xDFFF200000000000
; If shadow byte = 0xFF => object is freed (KASAN poison value)

2.10  UAF Detection Pattern Comparison Table
| Method | Detection Time | Performance Impact | ARM64 Support | Use Case |
| KASAN | Runtime (immediate) | 2-3x slowdown | Full | Development |
| KFENCE | Runtime (sampling) | <1% overhead | Full | Production |
| SLUB_DEBUG | On free/alloc | Moderate | Full | CI/Testing |
| T32 Watchpoints | Debug session | None | HW-assisted | Post-mortem |


# 3. Double Free
A double free occurs when kfree() is called twice on the same pointer, corrupting the SLUB/SLAB allocator's freelist metadata. This can cause two different kernel objects to share the same memory address, leading to silent data corruption or an exploitable vulnerability.
3.1  Internal SLUB Freelist Corruption

1st kfree(ptr) - Normal:
  freelist -> ptr -> next_free  (ptr inserted once)

2nd kfree(ptr) - CORRUPTION:
  freelist -> ptr -> ptr -> ptr ...  (CIRCULAR freelist!)

  Next TWO kmalloc() calls return the SAME physical address!
  Two unrelated kernel objects now share memory -> data corruption
  OR attacker controls what gets allocated there -> privilege escalation

3.2  Scenario 1: Error Path Double Free

/* Qualcomm SPI driver probe - error path bug */
static int qcom_spi_probe(struct platform_device *pdev)
{
    struct qcom_spi *spi = kzalloc(sizeof(*spi), GFP_KERNEL);

    if (dma_setup(spi))
        goto err_free;
    if (clk_setup(spi))
        goto err_free;    /* BUG: should goto err_dma! */

err_dma:
    dma_cleanup(spi);     /* internally calls kfree(spi->dma_buf) */
err_free:
    kfree(spi->dma_buf);  /* DOUBLE FREE if jumped from clk_setup */
    kfree(spi);
    return -ENOMEM;
}

/* FIX: Use correct goto label ordering */
if (clk_setup(spi))
    goto err_dma;   /* jumps to DMA cleanup first, then err_free */

3.3  Scenario 2: Missing NULL Assignment After Free

/* BUG: Two code paths both free the same pointer */
static void qcom_remove(struct platform_device *pdev)
{
    struct qcom_dev *qdev = platform_get_drvdata(pdev);
    kfree(qdev->fw_buf);   /* Freed here - ptr still holds address */
    /* qdev->fw_buf is NOT set to NULL */
}

static int qcom_runtime_suspend(struct device *dev)
{
    struct qcom_dev *qdev = dev_get_drvdata(dev);
    kfree(qdev->fw_buf);   /* DOUBLE FREE! Same address freed again */
}

/* FIX: Always set pointer to NULL immediately after kfree */
kfree(qdev->fw_buf);
qdev->fw_buf = NULL;  /* kfree(NULL) is a no-op - safe second call */

3.4  Fix Pattern: The Golden Rule
Fix Pattern: After every kfree(), immediately set the pointer to NULL. This makes accidental double frees harmless (kfree(NULL) is safe) and also converts potential UAF into a detectable NULL dereference.

kfree(ptr);    /* Free the memory */
ptr = NULL;    /* ALWAYS null the pointer after freeing */
               /* Second kfree(NULL) = no-op, prevents double free */

3.5  Detection Tools
| Tool | Detection Method | Config |
| KASAN | Immediate report: "BUG: KASAN: double-free in slab" | CONFIG_KASAN=y |
| SLUB_DEBUG | Poisons freed objects, detects reuse of freed slab | slub_debug=FZP |
| ftrace kfree | Trace all kfree calls with stack to find imbalance | trace_event=kfree |


/* KASAN output on ARM64 for double free: */
BUG: KASAN: double-free or invalid-free in qcom_remove+0x48/0x90
Free of addr ffff0000c1234500 by task rmmod/1892
First free:
  kfree+0x90/0x1c0
  qcom_runtime_suspend+0x34/0x80
Second free:
  kfree+0x90/0x1c0
  qcom_remove+0x48/0x90

# 4. Memory Leaks
A memory leak occurs when kernel code allocates memory (kmalloc, vmalloc, devm_*, dma_alloc_*) but never frees it. Over time, the system runs out of memory (OOM), leading to allocation failures, system slowdown, or kernel OOM killer invocations.
4.1  Why Critical on ARM Embedded (Qualcomm SoCs)
Embedded targets (QCS404, SDX55, SA8155P) often have limited RAM (512MB-4GB). A leak of even a few KB per operation can crash the system within hours on devices that never reboot.

Time 0:     [Free Pool: 512 MB]
Time +1hr:  [Free Pool: 480 MB]  <- 32 MB leaked
Time +5hr:  [Free Pool: 320 MB]  <- system slowing
Time +12hr: [Free Pool:  12 MB]  <- OOM killer triggers
Time +13hr: [Free Pool:   0 MB]  <- Kernel panic: Out of memory

4.2  Scenario 1: Missing Free in Error Path

/* Qualcomm DSP firmware loader - leak in error paths */
static int qcom_dsp_load_fw(struct qcom_dsp *dsp)
{
    void *buf = kmalloc(FW_SIZE, GFP_KERNEL);
    if (!buf) return -ENOMEM;

    int ret = request_firmware(&fw, "modem.mdt", dsp->dev);
    if (ret)
        return ret;  /* LEAK: buf never freed! */

    ret = qcom_mdt_load(dsp->dev, fw, buf);
    if (ret)
        return ret;  /* LEAK: both buf and fw not freed! */

    release_firmware(fw);
    kfree(buf);
    return 0;
}

/* FIX: Use goto-based cleanup chain (REVERSE order of acquisition)*/
err_free_fw:    release_firmware(fw);
err_free_buf:   kfree(buf);         return ret;

4.3  Scenario 2: Per-Transaction Leak (Repeated Allocation)
Called thousands of times per second in a QMI message handler. Even a small per-call leak becomes catastrophic very quickly.

/* Qualcomm QMI message handler - called per modem message */
static void qcom_qmi_handle_msg(struct qcom_qmi *qmi,
                                 struct sk_buff *skb)
{
    struct qmi_response *resp = kzalloc(sizeof(*resp), GFP_KERNEL);
    decode_qmi_msg(skb, resp);
    process_response(qmi, resp);
    /* LEAK: resp never freed! */
    /* Called 1000s of times/sec => catastrophic memory loss */
}

/* FIX: Always free after use */
    process_response(qmi, resp);
    kfree(resp);   /* Free on every call path */

4.4  Scenario 3: devm_ vs Manual Allocation Mismatch

/* Qualcomm driver - mixed devm_ and manual allocation */
static int qcom_probe(struct platform_device *pdev)
{
    /* devm_ allocated - auto-freed on driver detach: OK */
    struct qcom_dev *qdev = devm_kzalloc(&pdev->dev,
                                          sizeof(*qdev), GFP_KERNEL);

    /* Manual kmalloc - NOT auto-freed: LEAK */
    qdev->scratch_buf = kmalloc(4096, GFP_KERNEL);
    return 0;
}

static int qcom_remove(struct platform_device *pdev)
{
    /* qdev is auto-freed, but scratch_buf LEAKS! */
    return 0;  /* scratch_buf never freed here! */
}

/* FIX Option A: Use devm_kmalloc for scratch_buf too */
    qdev->scratch_buf = devm_kmalloc(&pdev->dev, 4096, GFP_KERNEL);

/* FIX Option B: Explicitly free in remove() */
static int qcom_remove(...) {
    struct qcom_dev *qdev = platform_get_drvdata(pdev);
    kfree(qdev->scratch_buf);   /* Manual free required */
}

4.5  Fix: goto Cleanup Chains and devm_* APIs
Best Practice: Use devm_* APIs (devm_kzalloc, devm_request_irq, devm_ioremap_resource) in probe functions wherever possible. They auto-free on driver detach or probe failure, eliminating the need for manual cleanup chains.

/* Template: goto cleanup chain (reverse acquisition order) */
err_free_irq:     free_irq(spi->irq, spi);
err_gpio_free:    gpio_free(spi->cs_gpio);
err_clk_put:      clk_put(spi->clk);
err_unmap:        iounmap(spi->base);
err_free_mem:     kfree(spi);        return ret;

/* Or use devm_* for zero-boilerplate cleanup: */
spi = devm_kzalloc(&pdev->dev, sizeof(*spi), GFP_KERNEL);
spi->base = devm_platform_ioremap_resource(pdev, 0);
spi->clk  = devm_clk_get(&pdev->dev, "core_clk");
ret = devm_request_irq(&pdev->dev, spi->irq, spi_isr, 0, "spi", spi);
/* All auto-freed on error or remove - no cleanup code needed! */

4.6  Detection: kmemleak Configuration and Usage

# Kernel config:
CONFIG_DEBUG_KMEMLEAK=y
CONFIG_DEBUG_KMEMLEAK_EARLY_LOG_SIZE=4000

# Trigger a scan (run after stress/soak testing):
echo scan > /sys/kernel/debug/kmemleak

# Read results:
cat /sys/kernel/debug/kmemleak

# Example output:
unreferenced object 0xffff0000c5a12000 (size 4096):
  comm "qcom_qmi", pid 234, jiffies 4295123456
  backtrace:
    kmalloc+0x8c/0x100
    qcom_qmi_handle_msg+0x20/0x80    <- leaked here!
    qmi_recv_worker+0x114/0x1c0

# Additional commands:
cat /proc/slabinfo                # Monitor slab growth over time
cat /proc/meminfo | grep Slab     # Track total slab memory

# 5. Buffer Overflow / Underflow
Buffer overflow/underflow occurs when code accesses memory beyond (overflow) or before (underflow) the allocated boundary, corrupting adjacent objects in SLAB memory or on the stack.
5.1  Stack Buffer Overflow

/* Qualcomm WiFi driver - firmware event parsing */
static void wlan_parse_event(struct wlan_dev *wdev,
                              u8 *data, u32 len)
{
    char buf[64];
    /* OVERFLOW: len from firmware can exceed 64 bytes */
    memcpy(buf, data, len);  /* Stack buffer overflow! */
}

/* FIX: Always validate length before copy */
    if (len > sizeof(buf))
        len = sizeof(buf);
    memcpy(buf, data, len);  /* Now bounded */

5.2  Heap Buffer Overflow

/* Heap overflow: allocated 128 bytes, firmware sends 256 */
char *buf = kmalloc(128, GFP_KERNEL);
memcpy(buf, fw_data, fw_len);  /* OVERFLOW: Overwrites next SLAB obj*/

/* FIX: Validate before allocating OR validate before copying */
if (fw_len > MAX_SAFE_SIZE) {
    dev_err(dev, "Firmware too large: %u > %u\n",
            fw_len, MAX_SAFE_SIZE);
    return -EINVAL;
}
char *buf = kmalloc(fw_len, GFP_KERNEL);  /* Allocate exact size */
memcpy(buf, fw_data, fw_len);              /* Now safe */

5.3  Detection Tools
| Tool | What It Catches | How to Enable |
| KASAN | Heap AND stack out-of-bounds at runtime | CONFIG_KASAN=y |
| FORTIFY_SOURCE | Compile-time memcpy/strcpy size checks | CONFIG_FORTIFY_SOURCE=y |
| Stack Protector | Stack canary - detects stack smashing on return | CONFIG_STACKPROTECTOR_STRONG=y |
| UBSan | Undefined behavior including array OOB | CONFIG_UBSAN=y |

# 6. Stack Overflow
Kernel stack is very limited - 8KB on ARM32 and 16KB on ARM64. Large local variables or deep recursion can exhaust the stack, triggering a guard-page fault and kernel panic.
6.1  Kernel Stack Size on ARM
| Architecture | Kernel Stack Size | Config Constant |
| ARM32 (ARMv7) | 8 KB (8192 bytes) | THREAD_SIZE = 8192 |
| ARM64 (ARMv8) | 16 KB (16384 bytes) | THREAD_SIZE = 16384 |
| IRQ stack (ARM64) | 16 KB (separate) | IRQ_STACK_SIZE = 16384 |

6.2  Scenario 1: Large Local Array

/* Qualcomm IPA filter setup - large local array */
static int qcom_ipa_filter_setup(struct ipa_dev *ipa)
{
    char filter_table[8192];  /* EXCEEDS ENTIRE KERNEL STACK! */
    /* On ARM64: 16KB stack, but other frames also on stack */
    /* This single array likely overflows stack -> panic */
}

/* ARM64 panic output: */
Kernel panic - not syncing: kernel stack overflow
CPU: 1 PID: 432  Task stack: [ffff800010040000..ffff800010050000]
  -> hit the guard page at bottom of stack

/* FIX: Use heap (kmalloc) for large buffers */
static int qcom_ipa_filter_setup(struct ipa_dev *ipa)
{
    char *filter_table = kmalloc(8192, GFP_KERNEL);
    if (!filter_table) return -ENOMEM;
    /* ... use filter_table ... */
    kfree(filter_table);
    return 0;
}

6.3  Scenario 2: Deep Recursion in DT Parsing

/* Deep recursion in device tree node parsing */
static int parse_node(struct device_node *np)
{
    struct device_node *child;
    for_each_child_of_node(np, child)
        parse_node(child);  /* Recursive! Deep DT tree = overflow */
}

/* FIX: Use iterative approach with explicit stack */
static int parse_nodes_iterative(struct device_node *root)
{
    struct device_node *np;
    /* of_find_node_by_path and siblings - iterative traversal */
    for_each_node_of_type(np, "your-type") {
        process_node(np);  /* No recursion */
    }
}

6.4  Detection Tools
| Tool | What It Does | Config |
| VMAP_STACK | Guard pages at end of stack - catches overflow immediately | CONFIG_VMAP_STACK=y |
| FRAME_WARN | Compiler warning if function stack frame exceeds threshold | CONFIG_FRAME_WARN=1024 |
| checkstack.pl | Script to find functions with largest stack usage | scripts/checkstack.pl |

# 7. Slab Corruption
Slab corruption occurs when code writes outside its allocated SLAB object boundaries or modifies SLAB allocator internal metadata (freelist pointers, red zones, poison patterns), breaking the allocator's ability to manage memory correctly.
7.1  SLUB Object Layout on ARM64

SLUB Object Memory Layout:
+------------+----------------------------------+------------+------+
| Red Zone   |  User Data  (your kmalloc object)|  Red Zone  |  FP |
| (guard)    |  <- kmalloc'd / allocated area ->|  (guard)   |(free)|
+------------+----------------------------------+------------+------+
                                                              ^
                                         FP = Freelist Pointer (next)

When you OVERWRITE beyond your object:
  -> You hit the Red Zone (KASAN detects immediately)
  -> Or you overwrite the FP (next object pointer) of adjacent obj
  -> Allocator follows corrupted pointer -> crash or corruption

7.2  Scenario 1: Off-By-One Corrupts Adjacent SLAB Object

/* Qualcomm IPA (Internet Packet Accelerator) driver */
static int ipa_parse_headers(struct ipa_dev *ipa,
                              u8 *pkt, int count)
{
    /* Allocates exactly count headers */
    struct ipa_hdr *hdrs = kmalloc(count * sizeof(*hdrs), GFP_KERNEL);

    for (int i = 0; i <= count; i++)  /* OFF-BY-ONE: should be < */
        hdrs[i].type = pkt[i];         /* Last write hits NEXT obj  */
    /* Overwrites first bytes of next SLAB object! */
}

/* SLUB corruption output: */
==========================================================
BUG kmalloc-128 (Not tainted): Poison overwritten
----------------------------------------------------------
INFO: 0xffff0000c1234568-0xffff0000c1234570 @offset=104.
  First byte 0x41 instead of 0x6b  (0x6b = SLUB free poison)
Allocated in ipa_parse_headers+0x24/0x80 age=12 cpu=2 pid=456

/* FIX: Off-by-one is i < count, NOT i <= count */
    for (int i = 0; i < count; i++)  /* Correct boundary */

7.3  Scenario 2: DMA Writes Past Allocated Buffer

/* Qualcomm BAM DMA engine - hardware descriptor overrun */
struct bam_desc *desc = kmalloc(NUM_DESC * sizeof(*desc), GFP_KERNEL);

/* BUG: DMA hardware writes NUM_DESC+1 descriptors */
/* (hardware misconfiguration or firmware bug) */
/* Overwrites adjacent slab metadata silently */

/* FIX: Always validate DMA transfer count matches allocation */
BUG_ON(num_descs > NUM_DESC);   /* Defensive check */
start_dma_transfer(desc, min(num_descs, NUM_DESC));

7.4  Scenario 3: UAF Corrupts Reallocated Object

/* Use-After-Free corrupts a different object in same slab */
kfree(obj_A);
/* SLUB reuses obj_A memory for obj_B (same size, different type) */
struct new_type *obj_B = kmalloc(same_size, GFP_KERNEL);

/* Old stale pointer still points to obj_A memory (now obj_B!) */
obj_A->field = 0xDEAD;   /* CORRUPTS obj_B's data! */
/* obj_B user reads garbage values - silent data corruption */

/* FIX: Set pointer NULL after free (standard UAF prevention) */
kfree(obj_A); obj_A = NULL;  /* Stale access -> NULL deref (safe) */

7.5  Symptoms and Detection
| Method | Config/Command | What It Detects |
| SLUB Debug | slub_debug=FZPU | F=free poison Z=redzone P=panic U=user tracking |
| KASAN | CONFIG_KASAN=y | Out-of-bounds writes detected immediately at runtime |
| /proc/slabinfo | cat /proc/slabinfo | Check for anomalies in slab statistics |
| Hardened usercopy | CONFIG_HARDENED_USERCOPY=y | Blocks cross-slab copies to/from userspace |


# Enable specific slab debugging (less overhead):
slub_debug=FZPU,kmalloc-128  # Debug only kmalloc-128 slab

# Key rule: Slab corruption is often a SECONDARY symptom of:
# - Buffer overflow (writing past allocated area)
# - Use-after-free (writing to freed/reallocated object)
# - DMA overrun (hardware writes beyond DMA buffer)
# Always find and fix the ROOT CAUSE!

# Part 1 Quick Reference Summary
| Error Type | Root Cause | ARM64 Symptom | Detection |
| NULL Dereference | Missing NULL check after API call, IS_ERR vs NULL confusion | ESR=0x96000004 FAR=0x0...0xNN | KASAN + Lockdep |
| Use-After-Free | Free before synchronizing IRQ/workqueue/DMA | KASAN report with alloc/free backtrace | KASAN + KFENCE |
| Double Free | Pointer not NULLed after free, goto label error | KASAN double-free report | KASAN + SLUB_DEBUG |
| Memory Leak | Missing kfree in error paths, no devm_ used | OOM killer, allocation failures | kmemleak scan |
| Buffer Overflow | No length validation before memcpy/loop | KASAN out-of-bounds, adjacent object corruption | KASAN + FORTIFY |
| Stack Overflow | Large local arrays, deep recursion | Panic: kernel stack overflow (hits guard page) | VMAP_STACK + FRAME_WARN |
| Slab Corruption | Off-by-one, DMA overrun, UAF of reallocated obj | "BUG: Poison overwritten" in slab layer | slub_debug=FZPU |


ARM64 Key Debug Registers
| Register | Full Name | Use in Debugging |
| ESR_EL1 | Exception Syndrome Register | EC field tells fault type (0x25=DABT), DFSC tells translation level |
| FAR_EL1 | Fault Address Register | The virtual address that caused the fault (e.g., 0x0 = NULL deref) |
| ELR_EL1 | Exception Link Register | PC value at time of fault (the instruction that caused the abort) |
| DAIF | Interrupt Mask Bits | D=Debug A=SError I=IRQ F=FIQ - shows interrupt enable state |
| SP_EL1 | Stack Pointer (kernel) | Current kernel stack pointer - check against THREAD_SIZE boundary |


Continue to Part 2 of 3: Part 2 covers: Uninitialized Memory Use, Invalid Memory Access, Spinlock in Sleeping Context, Incorrect IRQ Flags, Missing Interrupt Disabling, Interrupt Handler Reentrancy, and Softirq/Tasklet Misuse - with full ARM64 code examples and debugging guides.

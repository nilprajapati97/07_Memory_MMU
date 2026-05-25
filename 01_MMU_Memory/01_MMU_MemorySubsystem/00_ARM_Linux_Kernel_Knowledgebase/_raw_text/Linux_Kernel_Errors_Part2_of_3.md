Linux Kernel & Userspace Runtime Errors
Part 2 of 3  –  Pages 21–40
Qualcomm Interview Preparation  —  Sandeep Kumar
ARM64 Deep Dive: Scenarios, Code Examples & Fix Patterns
|  |


## Topics Covered in This Part
| Page | Topic | Coverage |
| 8 | Uninitialized Memory Use | Why it matters, kmalloc vs kzalloc, 3 scenarios, detection |
| 9 | Invalid Memory Access | ARM64 VA layout, 4 scenarios, MMU fault table, detection |
| 10 | Spinlock in Sleeping Context | Preemption disabled, 4 scenarios, detection, decision guide |
| 11 | Incorrect IRQ Flags | Flag basics, 4 scenarios, common mistakes table |
| 12 | Missing Interrupt Disabling | Problem visualization, 3 scenarios, ARM64 DAIF register |
| 13 | Interrupt Handler Reentrancy | SMP behavior, 4 scenarios, ARM GIC preemption, summary table |
| 14 | Softirq/Tasklet Misuse | IRQ model, comparison table, 5 scenarios, teardown order, lock guide |


# 8. Uninitialized Memory Use
Uninitialized memory use occurs when code reads from allocated memory (kmalloc, stack variables) before writing meaningful data to it. The memory contains garbage or stale values from previous usage, causing unpredictable behavior or information leaks to userspace.

## 8.1  Why It Matters on ARM (Qualcomm)
- Security: Leaks kernel addresses/data to userspace → defeats KASLR
- Unpredictable behavior: Works on dev board, fails in production (different residual data)
- Intermittent bugs: Hard to reproduce — depends on what previously occupied that memory

## 8.2  Memory State After Allocation
Understanding what kmalloc() and kzalloc() return is essential for avoiding uninitialized memory bugs:

| Allocator | kmalloc() | kzalloc() |
| Memory content | Stale data from previous allocation (0xFFFF0023, 0xDEAD1234...) | All zeros (0x00000000) |
| Safety | Unsafe if any field is read before write | Safe to use immediately |
| Performance | Slightly faster (no zeroing) | Slightly slower (memset) |
| Best for | Performance-critical, full initialization guaranteed | Default choice for structs/objects |


## 8.3  Scenario 1: Info Leak to Userspace via Struct Padding
When a struct with padding bytes is copied to userspace without zeroing, kernel stack data (including addresses) can leak to userspace applications. This is a critical security vulnerability.

💥 Bug: DSP ioctl handler copies stack struct with uninitialized padding to userspace via copy_to_user()

/* Qualcomm DSP ioctl handler - BUGGY version */
struct dsp_info {
    u32 version;    /* offset 0 */
    /* 4 bytes padding */
    u64 status;     /* offset 8 */
    u16 flags;      /* offset 16 */
    /* 6 bytes padding */
};

static long qcom_dsp_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    struct dsp_info info;  /* Stack variable - contains GARBAGE! */

    info.version = dsp->version;
    info.status  = dsp->status;
    info.flags   = dsp->flags;
    /* Padding bytes at offsets 4-7 and 18-23 are UNINITIALIZED */
    /* They may contain kernel stack addresses, keys, or pointers */

    /* LEAKS kernel memory to userspace! */
    copy_to_user((void __user *)arg, &info, sizeof(info));
}

✅ Fix: Zero the entire struct before populating fields to clear all padding bytes
/* CORRECT: Zero entire struct including all padding */
static long qcom_dsp_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    struct dsp_info info;
    memset(&info, 0, sizeof(info));  /* Clears ALL bytes including padding */
    /* OR: struct dsp_info info = { 0 }; */

    info.version = dsp->version;
    info.status  = dsp->status;
    info.flags   = dsp->flags;

    copy_to_user((void __user *)arg, &info, sizeof(info));  /* Safe now */
}

## 8.4  Scenario 2: kmalloc Without Initialization (IPC Packet)
Using kmalloc() for structs that are sent over IPC/communication channels leaves garbage in reserved, flags, and sequence number fields, causing undefined behavior on the receiving end (modem, ADSP, DSP).

/* Qualcomm GLINK/IPC router - BUGGY */
struct ipc_packet {
    u8  type;
    u8  flags;      /* ⚠️ UNINITIALIZED - garbage value */
    u16 seq_num;    /* ⚠️ UNINITIALIZED - remote processor reads garbage */
    u32 len;
    u8  reserved[4]; /* ⚠️ UNINITIALIZED - padding */
    u8  data[];
};

static int qcom_ipc_send(struct ipc_dev *ipc, void *data, int len)
{
    struct ipc_packet *pkt = kmalloc(sizeof(*pkt) + len, GFP_KERNEL);

    pkt->type = IPC_MSG;
    pkt->len  = len;
    memcpy(pkt->data, data, len);
    /* pkt->flags, pkt->seq_num, pkt->reserved are GARBAGE! */

    qcom_glink_send(ipc->channel, pkt);  /* Remote side confused */
}

✅ Fix: Use kzalloc() so ALL fields including flags, seq_num, and reserved are zeroed
    /* CORRECT: kzalloc zeros everything */
    struct ipc_packet *pkt = kzalloc(sizeof(*pkt) + len, GFP_KERNEL);
    if (!pkt)
        return -ENOMEM;
    /* Now pkt->flags = 0, pkt->seq_num = 0, pkt->reserved = {0} */

## 8.5  Scenario 3: Conditional Initialization (Partial Path)
When initialization only happens on some code paths, the variable may reach a read point without being set. This is often missed by code review but can be caught by KMSAN.

/* Qualcomm sensor driver - BUGGY */
static int qcom_sensor_read(struct sensor_dev *dev, int *val)
{
    int raw_data;  /* ⚠️ Uninitialized local variable! */

    if (dev->mode == MODE_POLLING)
        raw_data = readl(dev->base + DATA_REG);  /* Only set in polling mode */
    /* If mode != MODE_POLLING: raw_data is still garbage! */

    *val = raw_data * dev->scale;  /* ⚠️ Reads garbage if not polling mode */
    return 0;
}

✅ Fix: Always initialize to a safe default value, or ensure all code paths assign the variable
/* Option A: Initialize with safe default */
    int raw_data = 0;

/* Option B: Handle all paths explicitly */
    int raw_data;
    if (dev->mode == MODE_POLLING)
        raw_data = readl(dev->base + DATA_REG);
    else if (dev->mode == MODE_INTERRUPT)
        raw_data = dev->cached_value;
    else
        return -EINVAL;  /* Reject unknown modes */

## 8.6  Detection: Tools & Configuration
| Tool | Config / Command | What It Catches |
| KMSAN | CONFIG_KMSAN=y | Runtime detection of uninitialized reads (requires Clang) |
| GCC -Wuninitialized | CFLAGS += -Wuninitialized | Compile-time warning (limited, misses conditional paths) |
| Init stack to zero | CONFIG_INIT_STACK_ALL_ZERO=y | Auto-zeroes all stack variables (defense-in-depth, performance cost) |
| SLUB poison | slub_debug=P | Fills freed objects with 0x6b; stale reads return obvious pattern |
| Sparse | make C=2 | Warns about uses of uninitialized fields in copied structs |


### Sample KMSAN Output on ARM64
BUG: KMSAN: uninit-value in qcom_dsp_ioctl+0x68/0xc0
Local variable info created at:
  qcom_dsp_ioctl+0x20/0xc0
Uninit was stored to memory at:
  copy_to_user+0x44/0x80
CPU: 2 PID: 1432 Comm: test_dsp
Call trace:
  kmsan_check_memory+0x30/0x60
  copy_to_user+0x44/0x80
  qcom_dsp_ioctl+0x68/0xc0

## 8.7  Key Rules Summary
| Rule | Details |
| Use kzalloc() by default | Only use kmalloc() for performance-critical paths with guaranteed full initialization |
| memset() all stack structs | Before copy_to_user(): memset(&info, 0, sizeof(info)) clears padding |
| Initialize all locals | int x = 0; not int x; - especially for conditionally assigned variables |
| CONFIG_INIT_STACK_ALL_ZERO | Enable as defense-in-depth on ARM64 production kernels |
| kzalloc for IPC/network | Any struct sent to remote processors must be fully zeroed first |



# 9. Invalid Memory Access
Invalid memory access occurs when the kernel tries to read or write a virtual address that is either unmapped (no page table entry), protected (wrong permissions), or belongs to an invalid region such as userspace, unmapped holes in the VA space, or freed mappings.

## 9.1  ARM64 Virtual Address Space Layout
0xFFFF_FFFF_FFFF_FFFF  +-----------------------------+
                       |  Kernel Linear Map          |  <- Valid kernel access
                       |  (vmalloc, modules, kimage) |
0xFFFF_0000_0000_0000  +-----------------------------+
                       |                             |
                       |   INVALID HOLE              |  <- Access here = fault
                       |   (non-canonical addresses) |
                       |                             |
0x0000_FFFF_FFFF_FFFF  +-----------------------------+
                       |  Userspace                  |  <- Kernel must NOT access directly
                       |  (heap, stack, mmap, text)  |     (PAN enforces this on ARM64)
0x0000_0000_0000_0000  +-----------------------------+

When the MMU cannot translate a virtual address, it triggers a Synchronous Data Abort at EL1, which the kernel handles via do_mem_abort() leading to an Oops or Panic.

## 9.2  Scenario 1: Accessing MMIO Without ioremap()
Physical addresses are NOT valid virtual addresses. Attempting to access hardware registers using physical addresses directly (without ioremap) will cause an immediate Data Abort fault.

/* Qualcomm GCC Clock Controller - BUGGY */
static int qcom_gcc_init(struct platform_device *pdev)
{
    /* Physical address 0x00100000 is NOT a valid kernel VA! */
    void *base = (void *)0x00100000;

    /* ⚠️ CRASH: Physical address accessed without ioremap() */
    /* MMU has no page table entry for this address            */
    u32 val = readl(base + GCC_OFFSET);  /* Data Abort! */
    return 0;
}

✅ Fix: Always use ioremap() / devm_ioremap_resource() to create a kernel VA mapping for physical MMIO
/* CORRECT: ioremap maps physical -> virtual */
static int qcom_gcc_init(struct platform_device *pdev)
{
    struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res)
        return -ENODEV;

    void __iomem *base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(base))
        return PTR_ERR(base);

    u32 val = readl(base + GCC_OFFSET);  /* Safe: VA is mapped */
    return 0;
}

## 9.3  Scenario 2: Accessing Userspace Pointer Directly (PAN Violation)
ARM64 introduced Privileged Access Never (PAN), which prevents kernel code from directly accessing userspace memory addresses. Any direct dereference of a userspace pointer from kernel mode triggers an immediate fault with ESR indicating a permission fault.

/* Qualcomm DSP ioctl - BUGGY */
static long dsp_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    /* arg is a userspace pointer (PROT_READ|PROT_WRITE from mmap) */
    struct user_cmd *ucmd = (struct user_cmd *)arg;

    /* ⚠️ Direct dereference of userspace pointer! */
    /* ARM64 with PAN enabled -> IMMEDIATE permission fault */
    int type = ucmd->type;   /* SIGSEGV / Kernel Oops */
    return 0;
}

ARM64 PAN fault output:
Unable to handle kernel access to user memory outside uaccess routines
at virtual address 00007f8c12345678
Mem abort info:
  ESR = 0x96000021    <- Permission fault + PAN bit set
  EC = 0x25: DABT (current EL), IL = 32 bits
Call trace:
  dsp_ioctl+0x34/0xa0
  __arm64_sys_ioctl+0xc0/0x120

✅ Fix: Use copy_from_user() or get_user() - these use dedicated uaccess routines that temporarily disable PAN
/* CORRECT: Use uaccess routines */
static long dsp_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    struct user_cmd kcmd;

    /* copy_from_user handles PAN, page faults, and bad addresses */
    if (copy_from_user(&kcmd, (void __user *)arg, sizeof(kcmd)))
        return -EFAULT;

    int type = kcmd.type;  /* Safe: kernel copy */
    return 0;
}

## 9.4  Scenario 3: Accessing Memory After iounmap()
After iounmap() removes the VA-to-PA mapping, any subsequent access to that virtual address triggers a page fault. This commonly occurs when an IRQ handler fires after the driver has already unmapped its MMIO during shutdown.

/* Qualcomm PCIe driver - BUGGY teardown */
static void qcom_pcie_disable(struct qcom_pcie *pcie)
{
    iounmap(pcie->parf);     /* Unmaps the VA - page table entry removed */
    iounmap(pcie->elbi);
    /* Problem: IRQ is still registered and may fire! */
}

static irqreturn_t qcom_pcie_isr(int irq, void *data)
{
    struct qcom_pcie *pcie = data;

    /* ⚠️ pcie->parf VA is now unmapped - Data Abort! */
    u32 status = readl(pcie->parf + PCIE_INT_STATUS);
    return IRQ_HANDLED;
}

✅ Fix: Disable and synchronize IRQ BEFORE unmapping. The synchronize_irq() call ensures no running ISR will access the unmapped region.
/* CORRECT teardown order */
static void qcom_pcie_disable(struct qcom_pcie *pcie)
{
    /* Step 1: Disable IRQ - no new interrupts */
    disable_irq(pcie->irq);

    /* Step 2: Wait for any running ISR to complete */
    synchronize_irq(pcie->irq);

    /* Step 3: NOW safe to unmap */
    iounmap(pcie->parf);
    iounmap(pcie->elbi);
}

## 9.5  Scenario 4: Wild/Corrupted Pointer Dereference
An uninitialized pointer contains whatever value was on the stack previously. Dereferencing it accesses a completely random virtual address, typically causing a translation fault at a non-canonical or unmapped address.

/* Uninitialized pointer on kernel stack */
static void qcom_audio_process(void)
{
    struct qcom_dev *dev;  /* Uninitialized - random stack garbage */

    /* ⚠️ Dereferences garbage address like 0xDEAD_BEEF_1234_5678 */
    dev->state = ACTIVE;   /* Accesses completely random VA */
}

/* ARM64 Oops output: */
/* Unable to handle kernel paging request at virtual address deadbeef12345678 */
/* ESR = 0x96000004  <- Translation fault level 0 (no mapping at all)       */
/* FAR = 0xDEADBEEF12345678                                                  */

✅ Fix: Always initialize pointers: struct qcom_dev *dev = NULL; or dev = get_device(); with proper NULL check

## 9.6  ARM64 MMU Fault Classification (ESR.DFSC Values)
| ESR.DFSC | Fault Type | Common Cause in Drivers |
| 0x04 | Translation fault L0 | Wild/uninitialized pointer (completely unmapped address) |
| 0x05 | Translation fault L1 | Large VA region unmapped (missing ioremap for large MMIO block) |
| 0x06 | Translation fault L2 | Physical page not mapped (access after iounmap) |
| 0x07 | Translation fault L3 | PTE invalid (freed page, use-after-free) |
| 0x0D | Permission fault L1 | Write to read-only mapping |
| 0x0F | Permission fault L3 | PAN violation: kernel accessing userspace directly |
| 0x21 | Alignment fault | Unaligned access to Device memory (MMIO must be aligned) |
| 0x11 | Synchronous External Abort | Bus error (wrong access size to hardware register) |


## 9.7  Detection Tools for Invalid Memory Access
| Tool | Mechanism | What It Catches |
| PAN (Hardware) | ARM64 hardware feature, always on | Kernel accessing userspace directly (ESR 0x21) |
| KASAN | Shadow memory byte checks | Out-of-bounds near valid regions, use-after-free |
| SMMU/IOMMU | ARM SMMU interrupt on DMA fault | DMA to invalid/unmapped physical addresses |
| Sparse | make C=2 | Missing __iomem on MMIO pointers, __user violations |
| MMU fault handler | Kernel Oops with FAR+ESR | Any unmapped/forbidden access (ESR.EC = 0x25) |



# 10. Spinlock in Sleeping Context
A spinlock disables preemption (and possibly interrupts) and busy-waits. If you call a function that sleeps while holding a spinlock, the CPU can never be rescheduled to release the lock. The system deadlocks or hangs because other CPUs spinning on the same lock are permanently blocked.

CPU0 holds spinlock:
+--------------------------------------------------+
| spin_lock(&lock)   -> preemption DISABLED        |
|   |                                              |
| kmalloc(GFP_KERNEL) -> tries to sleep            |
|   |                                              |
| schedule()         -> CANNOT switch out!         |
|                      (preemption disabled)       |
|                                                  |
| Other CPUs spinning on same lock -> ALL STUCK   |
| *** SYSTEM HANG / HARD LOCKUP ***               |
+--------------------------------------------------+

## 10.1  Functions That Sleep (Never Call While Holding Spinlock)
| Category | Sleeping Functions | Non-Sleeping Alternative |
| Memory | kmalloc(GFP_KERNEL) vmalloc() kvmalloc() | kmalloc(GFP_ATOMIC) OR allocate before taking lock |
| Synchronization | mutex_lock() down_read() down_write() | trylock pattern OR switch to mutex for context |
| Delays | msleep() usleep_range() sleep() | udelay() / ndelay() mdelay() for longer waits |
| User Access | copy_from_user() copy_to_user() put_user() | Copy to kernel buffer first, then call outside lock |
| Wait | wait_for_completion() wait_event() schedule() | wait_for_completion_timeout with completion outside lock |
| Firmware | request_firmware() i2c_transfer() | Request firmware before taking lock; use deferred init |


## 10.2  Scenario 1: kmalloc(GFP_KERNEL) Under Spinlock (ADSP Audio Driver)
/* Qualcomm ADSP Audio Driver - BUGGY */
static void qcom_adsp_push_buffer(struct adsp_dev *adsp, void *data, int len)
{
    spin_lock(&adsp->buf_lock);

    /* ⚠️ GFP_KERNEL can sleep: triggers page reclaim, compaction */
    /* CONFIG_DEBUG_ATOMIC_SLEEP will catch this immediately   */
    void *buf = kmalloc(len, GFP_KERNEL);

    memcpy(buf, data, len);
    list_add_tail(&buf->node, &adsp->buf_list);

    spin_unlock(&adsp->buf_lock);  /* Never reached if kmalloc sleeps! */
}

✅ Fix: Allocate outside the spinlock, or use GFP_ATOMIC inside the lock
/* Option A: GFP_ATOMIC inside lock (may fail under pressure) */
    spin_lock(&adsp->buf_lock);
    void *buf = kmalloc(len, GFP_ATOMIC);  /* Non-sleeping atomic alloc */
    if (!buf) { spin_unlock(&adsp->buf_lock); return; }
    spin_unlock(&adsp->buf_lock);

/* Option B (preferred): Allocate before taking lock */
    void *buf = kmalloc(len, GFP_KERNEL);  /* Sleep OK here - no lock held */
    if (!buf) return -ENOMEM;
    memcpy(buf, data, len);

    spin_lock(&adsp->buf_lock);
    list_add_tail(&buf->node, &adsp->buf_list);  /* Only list op under lock */
    spin_unlock(&adsp->buf_lock);

## 10.3  Scenario 2: copy_to_user() Under Spinlock (Diag/QXDM Driver)
/* Qualcomm Diag (QXDM) driver - BUGGY */
static ssize_t diag_read(struct file *f, char __user *ubuf, size_t count, loff_t *off)
{
    struct diag_dev *diag = f->private_data;

    spin_lock_irq(&diag->data_lock);

    /* ⚠️ copy_to_user may page-fault -> sleeps to load page from swap */
    /* Also blocked by PAN: userspace page may need to be faulted in  */
    copy_to_user(ubuf, diag->log_buf, count);

    spin_unlock_irq(&diag->data_lock);
    return count;
}

✅ Fix: Copy to intermediate kernel buffer under lock, then copy to user outside the lock
/* CORRECT: Two-stage copy */
static ssize_t diag_read(struct file *f, char __user *ubuf, size_t count, loff_t *off)
{
    struct diag_dev *diag = f->private_data;
    char kbuf[DIAG_MAX_MSG];

    /* Stage 1: kernel-to-kernel copy under spinlock (no sleep possible) */
    spin_lock_irq(&diag->data_lock);
    memcpy(kbuf, diag->log_buf, count);  /* Cannot sleep */
    spin_unlock_irq(&diag->data_lock);

    /* Stage 2: kernel-to-user copy OUTSIDE spinlock (may page-fault/sleep) */
    if (copy_to_user(ubuf, kbuf, count))
        return -EFAULT;

    return count;
}

## 10.4  Scenario 3: mutex_lock() Inside Spinlock (Remoteproc PIL)
/* Qualcomm Remoteproc PIL driver - BUGGY */
static void qcom_pil_notify(struct pil_dev *pil, int event)
{
    spin_lock(&pil->event_lock);

    /* ⚠️ mutex_lock is a SLEEPING lock - cannot nest inside spinlock! */
    /* DEBUG_ATOMIC_SLEEP will report: in_atomic(): 1            */
    mutex_lock(&pil->state_mutex);
    pil->state = event;
    mutex_unlock(&pil->state_mutex);

    spin_unlock(&pil->event_lock);
}

✅ Fix: Convert the outer spinlock to a mutex if all callers are in process context, or eliminate the inner mutex
/* Option A: Replace spinlock with mutex (all callers process context) */
    mutex_lock(&pil->event_lock);
    pil->state = event;  /* Direct assignment - no inner mutex needed */
    mutex_unlock(&pil->event_lock);

/* Option B: Remove inner mutex entirely (spinlock alone sufficient) */
    spin_lock(&pil->event_lock);
    pil->state = event;  /* Simple atomic assignment under spinlock */
    spin_unlock(&pil->event_lock);

## 10.5  Scenario 4: msleep()/usleep_range() Under Spinlock (PMIC GPIO)
/* Qualcomm PMIC GPIO driver - BUGGY */
static int pmic_gpio_set(struct pmic_gpio *pg, int val)
{
    spin_lock(&pg->lock);

    writel(val, pg->base + GPIO_OUT_REG);

    /* ⚠️ msleep calls schedule() -> context switch -> deadlock! */
    msleep(1);  /* Waiting for GPIO to settle */

    u32 status = readl(pg->base + GPIO_STATUS_REG);
    spin_unlock(&pg->lock);
    return status;
}

✅ Fix: Use udelay() or ndelay() for hardware settle times in atomic context - these busy-wait without sleeping
/* CORRECT: Non-sleeping delay in atomic context */
    spin_lock(&pg->lock);
    writel(val, pg->base + GPIO_OUT_REG);
    udelay(1000);  /* Busy-wait 1ms (acceptable for short hardware delays) */
    u32 status = readl(pg->base + GPIO_STATUS_REG);
    spin_unlock(&pg->lock);
    /* Note: udelay > ~10us is frowned upon; consider restructuring if longer */

## 10.6  Detection
/* Kernel config for detection */
CONFIG_DEBUG_ATOMIC_SLEEP=y    /* Catches sleeping in atomic context */
CONFIG_PROVE_LOCKING=y         /* Lockdep validates lock usage */
CONFIG_PREEMPT_DEBUG=y         /* Warns on schedule() with preemption off */

/* Runtime warning signature on ARM64: */
BUG: sleeping function called from invalid context at mm/slab.h:494
in_atomic(): 1, irqs_disabled(): 0, non_block: 0, pid: 1205
preempt_count: 1                  <- preempt_count != 0 means spinlock held
CPU: 3 PID: 1205 Comm: adsp_worker
Call trace:
  __might_sleep+0x4c/0x80
  kmem_cache_alloc_trace+0x3c/0x2f0
  qcom_adsp_push_buffer+0x44/0xb0

Key indicator: in_atomic(): 1 means we are in atomic context (spinlock held) and attempted to sleep. The preempt_count shows how many non-preemptible contexts are active.

## 10.7  Decision Guide: Need to Sleep While Holding a Spinlock?
| Situation | Solution |
| Can you do work OUTSIDE the lock? | Best option: restructure to sleep before acquiring spinlock |
| Can switch spinlock to mutex? | If ALL callers are in process context, use mutex instead |
| Short hardware delay (< 10 us)? | Use udelay() / ndelay() - busy-wait, no sleep |
| Need memory allocation? | GFP_ATOMIC inside lock OR allocate before taking lock |
| Need to copy to/from userspace? | Copy to kernel buffer under lock; copy_to_user() outside lock |
| None of the above applies? | Redesign the architecture - sleeping and spinlocks are incompatible |



# 11. Incorrect IRQ Flags
Incorrect IRQ flags means passing wrong flags to request_irq() - requesting exclusive access to a shared interrupt line, using wrong trigger type (edge vs level), or missing IRQF_ONESHOT for threaded handlers. This causes IRQ registration failures, missed interrupts, interrupt storms, or system hangs.

## 11.1  ARM Interrupt Flag Reference
| Flag | Purpose | When to Use on ARM SoCs |
| IRQF_SHARED | Multiple devices share this IRQ line | GIC SPI shared by multiple PMIC/peripheral devices |
| IRQF_ONESHOT | Keep IRQ line disabled until threaded handler completes | Level-triggered devices with threaded IRQ handlers |
| IRQF_TRIGGER_RISING | Trigger on rising edge (low to high) | Button press, pulse signals, SPI chip-select |
| IRQF_TRIGGER_FALLING | Trigger on falling edge (high to low) | Edge-triggered GPIO events |
| IRQF_TRIGGER_HIGH | Level high: fires while line is HIGH | Active-high level IRQs |
| IRQF_TRIGGER_LOW | Level low: fires while line is LOW | Active-low: I2C sensors, touch controllers, fuel gauges |
| 0 (no flag) | Exclusive, use DT/firmware trigger type | When DT specifies interrupt-cells with trigger type already |


## 11.2  Scenario 1: Missing IRQF_SHARED on Shared GIC SPI Line (PMIC)
On Qualcomm SoCs, many PMIC peripherals share a single GIC SPI interrupt line routed through the SPMI/I2C bus. All drivers on this shared line must declare IRQF_SHARED, or registration fails with -EBUSY.

/* Qualcomm PMIC driver - BUGGY: missing IRQF_SHARED */
static int qcom_pmic_probe(struct platform_device *pdev)
{
    int irq = platform_get_irq(pdev, 0);

    /* ⚠️ FAILS: This SPI IRQ is shared by USB, charger, GPIO, RTC */
    /* qcom-spmi driver already registered on this line            */
    ret = request_irq(irq, pmic_isr, 0, "qcom-pmic", pdev);
    /* Returns -EBUSY -> driver fails to probe!                    */
}

/* Error in dmesg: */
/* genirq: Flags mismatch irq 78. 00000000 (qcom-pmic) vs. 00000080 (qcom-spmi) */
/* qcom-pmic: request_irq failed: -EBUSY */

✅ Fix: Add IRQF_SHARED and provide a non-NULL, unique dev_id pointer for shared IRQ lines
    /* CORRECT: IRQF_SHARED + unique dev_id */
    ret = request_irq(irq, pmic_isr, IRQF_SHARED, "qcom-pmic", pdev);
    /* Note: dev_id (pdev) MUST be non-NULL for shared IRQs */
    /* Used by free_irq() to identify which handler to remove */

## 11.3  Scenario 2: Wrong Trigger Type - Edge vs Level Mismatch (Touchscreen)
Touch controller ICs (like Synaptics/Goodix on Qualcomm QCS/QCM platforms) hold the IRQ line LOW until all touch data has been read. Using edge trigger causes the interrupt to fire once but never again if the line remains asserted.

/* Touch controller driver - BUGGY */
static int touch_probe(struct i2c_client *client)
{
    int irq = gpio_to_irq(touch->irq_gpio);

    /* ⚠️ BUG: Touch IC holds IRQ LOW until data is drained */
    /* Edge trigger fires once on falling edge, then NEVER AGAIN  */
    /* if line stays low (unread data in FIFO)                    */
    ret = request_irq(irq, touch_isr, IRQF_TRIGGER_FALLING, "touch", touch);
}

/* Sequence of failure:                       */
/* 1. Finger touches screen -> IRQ line LOW   */
/* 2. FALLING EDGE detected -> ISR runs       */
/* 3. ISR reads partial data -> line stays LOW*/
/* 4. No new FALLING EDGE -> no more IRQs!    */
/* 5. Touchscreen completely freezes!         */

✅ Fix: Use IRQF_TRIGGER_LOW with IRQF_ONESHOT. Level trigger fires as long as line is LOW; IRQF_ONESHOT keeps IRQ masked until all data is read.
/* CORRECT: Level-triggered with IRQF_ONESHOT */
    ret = request_irq(irq, touch_isr,
                      IRQF_TRIGGER_LOW | IRQF_ONESHOT,
                      "touch", touch);
    /* IRQF_TRIGGER_LOW: IRQ fires continuously while line is LOW */
    /* IRQF_ONESHOT: Masks IRQ until threaded handler finishes reading */

## 11.4  Scenario 3: Missing IRQF_ONESHOT with Threaded IRQ (Fuel Gauge)
/* Qualcomm fuel gauge (battery monitoring) - BUGGY */
static int fg_probe(struct i2c_client *client)
{
    /* request_threaded_irq with NULL hardirq handler */
    /* This means: no hardirq handler to acknowledge/mask the IRQ */
    ret = request_threaded_irq(client->irq,
                               NULL,          /* No hardirq handler! */
                               fg_thread_fn,  /* Threaded handler only */
                               IRQF_TRIGGER_LOW,  /* Missing IRQF_ONESHOT! */
                               "qcom-fg", fg);
}

/* What happens: */
/* IRQ fires (level LOW) -> no hardirq handler to ACK/mask */
/* IRQ immediately re-fires -> IRQ storm -> system hang     */
/* dmesg: irq 114: nobody cared (try booting with irqpoll) */
/* Kernel disables the IRQ line entirely -> device dead     */

✅ Fix: Always add IRQF_ONESHOT when using a NULL hardirq handler with request_threaded_irq()
/* CORRECT: IRQF_ONESHOT ensures IRQ stays masked until */
/* the threaded handler finishes reading all data         */
    ret = request_threaded_irq(client->irq,
                               NULL,          /* Kernel provides default hardirq */
                               fg_thread_fn,
                               IRQF_TRIGGER_LOW | IRQF_ONESHOT,
                               "qcom-fg", fg);

## 11.5  Scenario 4: NULL dev_id with IRQF_SHARED
/* BUGGY: Passing NULL dev_id with IRQF_SHARED */
request_irq(irq, handler, IRQF_SHARED, "my-dev", NULL);  /* NULL dev_id! */

/* Problems:                                                        */
/* 1. free_irq(irq, NULL) cannot identify which handler to remove  */
/* 2. If multiple devices share, ALL NULL-dev_id handlers removed  */
/* 3. Crash on module unload / driver remove                       */

/* CORRECT: Always pass unique non-NULL pointer as dev_id */
request_irq(irq, handler, IRQF_SHARED, "my-dev", my_dev_struct);
/* On free: */
free_irq(irq, my_dev_struct);  /* Removes ONLY this driver's handler */

## 11.6  Quick Reference: Common IRQ Flag Mistakes
| Mistake | Symptom | Fix |
| Missing IRQF_SHARED | -EBUSY on probe | Add IRQF_SHARED + non-NULL dev_id |
| Edge trigger on level device | Interrupts stop after first | Use IRQF_TRIGGER_LOW/HIGH |
| Missing IRQF_ONESHOT | IRQ storm, "nobody cared" | Add IRQF_ONESHOT for threaded handlers |
| NULL dev_id + IRQF_SHARED | Crash on free_irq() | Pass unique non-NULL dev_id |
| Trigger mismatch with DT | Spurious or no interrupts | Pass 0 for flags, let DT specify trigger |
| Both edge flags set | Double triggering | Pick ONE: RISING or FALLING, not both |



# 12. Missing Interrupt Disabling
When code in process context shares data with an interrupt handler running on the same CPU, you must disable interrupts while modifying that shared data. Otherwise, an IRQ can fire mid-update on the same CPU, and the ISR reads inconsistent or half-modified data.

## 12.1  The Problem: ISR Sees Half-Updated Data
WITHOUT interrupt disabling:

CPU0 (process context):
+--------------------------------------------+
| data->count = 5;         <- Write 1: count |
|   *** IRQ fires HERE ***                   |
|   +---- ISR reads: ----+                   |
|   | count = 5  (NEW)   |  <- updated       |
|   | sum   = 10 (OLD)   |  <- NOT yet!      |
|   | avg = sum/count     |                   |
|   | avg = 10/5 = 2      |  WRONG! (should be 4)
|   +--------------------+                   |
|                                            |
| data->sum = 20;          <- Write 2: sum   |
+--------------------------------------------+

WITH spin_lock_irqsave():

| spin_lock_irqsave(&lock, flags)  <- IRQs DISABLED on this CPU
| data->count = 5;
| data->sum   = 20;
| spin_unlock_irqrestore(&lock, flags)  <- IRQs re-enabled
|  (IRQ fires here, sees BOTH writes complete -> avg = 20/5 = 4)

## 12.2  Scenario 1: UART TX Buffer Shared with TX-Complete ISR (GENI)
/* Qualcomm GENI UART driver - BUGGY */
struct geni_uart {
    spinlock_t lock;
    int tx_pending;
    int tx_total;
    char *tx_buf;
};

/* Process context: write() syscall path */
static void geni_start_tx(struct geni_uart *uart, const char *buf, int len)
{
    /* ⚠️ No interrupt disabling! TX-complete IRQ fires on same CPU mid-update */
    uart->tx_buf     = buf;
    uart->tx_pending = len;      /* <- IRQ fires HERE: ISR sees new tx_pending */
    uart->tx_total  += len;      /*    but OLD tx_total -> WRONG calculation   */
    writel(TX_START, uart->base + SE_GENI_M_CMD0);
}

/* IRQ handler: TX complete interrupt */
static irqreturn_t geni_isr(int irq, void *data)
{
    struct geni_uart *uart = data;
    /* Reads inconsistent state: tx_pending (new), tx_total (old) */
    int remaining = uart->tx_total - uart->tx_pending;  /* WRONG result! */
    return IRQ_HANDLED;
}

✅ Fix: Use spin_lock_irqsave() from process context to atomically disable IRQs and take the lock
/* CORRECT: spin_lock_irqsave prevents IRQ from firing mid-update */
static void geni_start_tx(struct geni_uart *uart, const char *buf, int len)
{
    unsigned long flags;

    spin_lock_irqsave(&uart->lock, flags);   /* Disable IRQs + acquire lock */
    uart->tx_buf     = buf;
    uart->tx_pending = len;
    uart->tx_total  += len;
    writel(TX_START, uart->base + SE_GENI_M_CMD0);
    spin_unlock_irqrestore(&uart->lock, flags); /* Restore IRQ state */
}

static irqreturn_t geni_isr(int irq, void *data)
{
    struct geni_uart *uart = data;
    spin_lock(&uart->lock);    /* IRQs already disabled in ISR context */
    int remaining = uart->tx_total - uart->tx_pending;  /* Consistent */
    spin_unlock(&uart->lock);
    return IRQ_HANDLED;
}

## 12.3  Scenario 2: 64-bit Counter Torn by IRQ on ARM32
On ARM32, a 64-bit write compiles to TWO 32-bit store instructions (STR + STR). An IRQ firing between these two instructions means the ISR reads a torn value: new low 32 bits with old high 32 bits.

/* Qualcomm performance counter on ARM32 - BUGGY */
static void update_cycles(struct perf_counter *pc, u64 new_val)
{
    /* ARM32 assembly for 64-bit assignment:           */
    /*   STR R2, [R0, #0]   <- writes low  32 bits    */
    /*   *** IRQ FIRES HERE ***                        */
    /*   STR R3, [R0, #4]   <- writes high 32 bits    */
    pc->total_cycles = new_val;  /* NOT ATOMIC on ARM32! */
}

/* ISR reads torn value: new_low | old_high -> garbage! */

✅ Fix: Use local_irq_save/restore to disable IRQs on the local CPU, making the 64-bit write atomic with respect to interrupts
/* CORRECT: Disable IRQs to make 64-bit write atomic */
static void update_cycles(struct perf_counter *pc, u64 new_val)
{
    unsigned long flags;
    local_irq_save(flags);        /* Disable IRQs on this CPU */
    pc->total_cycles = new_val;   /* Both STR instructions execute uninterrupted */
    local_irq_restore(flags);     /* Re-enable IRQs */
}

/* On ARM64: 64-bit writes ARE atomic (STR Xn is single instruction) */
/* On ARM32: Always protect 64-bit shared variables with IRQ disabling */

## 12.4  Scenario 3: Linked List Manipulation Interrupted (IPA Queue)
/* Qualcomm IPA packet queue - BUGGY */
static void ipa_enqueue(struct ipa_queue *q, struct ipa_pkt *pkt)
{
    /* ⚠️ list_add updates FOUR pointers (prev->next, next->prev, head, tail) */
    /* If IRQ fires mid-update: ISR traverses corrupted list!              */
    list_add_tail(&pkt->node, &q->pending);
    /* <- IRQ fires HERE: count says N but list has N+1 entries! */
    q->count++;
}

✅ Fix: Wrap the list operation and count update atomically under local_irq_save
/* CORRECT: Atomic list + count update */
static void ipa_enqueue(struct ipa_queue *q, struct ipa_pkt *pkt)
{
    unsigned long flags;
    local_irq_save(flags);
    list_add_tail(&pkt->node, &q->pending);
    q->count++;
    local_irq_restore(flags);
}

## 12.5  Methods to Disable Interrupts on ARM64
| API | Scope | Use Case |
| spin_lock_irqsave(&lock, flags) | This CPU + other CPUs | SMP-safe: shared data with ISR on any CPU (most common) |
| spin_lock_irq(&lock) | This CPU + other CPUs | Same as above but assumes IRQs were enabled (no save/restore) |
| local_irq_save(flags) | This CPU only | Per-CPU data, no cross-CPU sharing needed |
| disable_irq(irq) / enable_irq(irq) | Specific IRQ line globally | Disable one IRQ across ALL CPUs (e.g., during suspend) |


## 12.6  ARM64 local_irq_save Internals (DAIF Register)
/* ARM64 DAIF register bit fields: */
/* D = Debug exception mask        */
/* A = Asynchronous abort mask     */
/* I = IRQ mask (bit we care about)*/
/* F = FIQ mask                    */

/* local_irq_save(flags) compiles to: */
  MRS  X0, DAIF          <- Read current interrupt mask state
  MSR  DAIFSet, #2       <- Set I-bit: mask all IRQs
  /* flags = X0 (saved DAIF value) */

/* local_irq_restore(flags) compiles to: */
  MSR  DAIF, X0          <- Restore original DAIF state
  /* If IRQs were previously enabled, this re-enables them */


# 13. Interrupt Handler Reentrancy
Interrupt handler reentrancy occurs when an ISR is designed assuming it will never be called again while still executing, but under certain conditions gets re-entered. This leads to data corruption, stack overflow, or system crash.

## 13.1  Linux Default Behavior on ARM64
Linux Default IRQ Behavior:
+-------------------------------------------------------+
| Same IRQ:     MASKED while its handler runs           |
|               (same IRQ line won't re-enter on one CPU)|
|                                                       |
| Different IRQ: CAN interrupt current handler          |
|               (higher-priority GIC IRQ nests)         |
|                                                       |
| IRQF_SHARED:  Same handler CAN run on other CPUs!     |
|               (SMP: CPU0 and CPU3 both in same ISR)   |
+-------------------------------------------------------+

## 13.2  Reentrancy Sources
- Shared IRQ on SMP: Same handler called on CPU0 (device A) and CPU3 (device B) simultaneously
- IRQ nesting: Higher-priority IRQ (timer, IPI) preempts current lower-priority handler
- Threaded IRQ preemption: On PREEMPT_RT kernels, threaded handlers can be preempted
- Accidental re-enable: Handler incorrectly calls local_irq_enable() inside hardirq

## 13.3  Scenario 1: Shared IRQ Handler with Global State (PMIC)
/* Qualcomm shared PMIC IRQ - multiple peripherals on same SPI line */
static int total_interrupts = 0;  /* Global counter - non-atomic! */

static irqreturn_t pmic_shared_isr(int irq, void *data)
{
    struct pmic_dev *pmic = data;
    u32 status = readl(pmic->base + INT_STATUS);

    if (!(status & pmic->mask))
        return IRQ_NONE;  /* Not for us */

    /* ⚠️ Non-atomic: CPU0 and CPU3 can both reach this line simultaneously */
    total_interrupts++;  /* LOST UPDATES on SMP! */

    /* CPU0: reads 100, CPU3: reads 100, both write 101 -> should be 102 */
    return IRQ_HANDLED;
}

✅ Fix: Use atomic_t or atomic64_t for all counters and flags accessed from shared ISR contexts on SMP
/* CORRECT: Atomic counter */
static atomic_t total_interrupts = ATOMIC_INIT(0);

static irqreturn_t pmic_shared_isr(int irq, void *data)
{
    struct pmic_dev *pmic = data;
    u32 status = readl(pmic->base + INT_STATUS);

    if (!(status & pmic->mask))
        return IRQ_NONE;

    atomic_inc(&total_interrupts);  /* SMP-safe atomic increment */
    return IRQ_HANDLED;
}

## 13.4  Scenario 2: Read-Modify-Write in Shared Handler (GPI DMA)
/* Qualcomm GPI DMA - shared IRQ across all DMA channels - BUGGY */
static irqreturn_t gpi_isr(int irq, void *data)
{
    struct gpi_dev *gpi = data;
    u32 status;

    /* ⚠️ Read-Modify-Write: CPU0 reads -> CPU3 reads -> both modify */
    /* -> both write back: one CPU's modification lost!          */
    status  = readl(gpi->base + GPI_IRQ_STATUS);   /* READ  */
    status &= ~GPI_CH_DONE;                         /* MODIFY*/
    writel(status, gpi->base + GPI_IRQ_CLEAR);      /* WRITE */
    /* CPU3 may have changed GPI_IRQ_STATUS between our read and write! */
}

✅ Fix: Protect the read-modify-write sequence with a spinlock to serialize access from multiple CPUs
/* CORRECT: Serialize RMW with spinlock */
static irqreturn_t gpi_isr(int irq, void *data)
{
    struct gpi_dev *gpi = data;
    u32 status;

    spin_lock(&gpi->irq_lock);  /* Serializes RMW across CPUs */
    status  = readl(gpi->base + GPI_IRQ_STATUS);
    status &= ~GPI_CH_DONE;
    writel(status, gpi->base + GPI_IRQ_CLEAR);
    spin_unlock(&gpi->irq_lock);
    return IRQ_HANDLED;
}

## 13.5  Scenario 3: Nested IRQ Corrupts Shared Buffer (LPASS Audio)
On ARM GICv3, a higher-priority interrupt (like a timer or IPI) can preempt a lower-priority handler that is currently executing. If both handlers share a buffer without protection, the nested handler corrupts the first handler's state.

/* Qualcomm LPASS Audio + Timer IRQ sharing a debug buffer - BUGGY */
static char debug_buf[256];
static int  buf_pos = 0;

/* Low-priority LPASS PCM IRQ */
static irqreturn_t lpass_pcm_isr(int irq, void *data)
{
    buf_pos += snprintf(debug_buf + buf_pos, 256 - buf_pos, "PCM: ");
    /* <-- HIGH PRIORITY TIMER IRQ PREEMPTS HERE on ARM GICv3 */
    buf_pos += snprintf(debug_buf + buf_pos, 256 - buf_pos, "done
");
    /* buf_pos and debug_buf may be corrupted by timer ISR! */
    return IRQ_HANDLED;
}

/* High-priority Timer IRQ also writes to debug_buf! */
static irqreturn_t timer_isr(int irq, void *data)
{
    /* Both ISRs race on buf_pos and debug_buf */
    buf_pos += snprintf(debug_buf + buf_pos, 256 - buf_pos, "TIMER ");
    return IRQ_HANDLED;
}

✅ Fix: Use DEFINE_PER_CPU buffers so each CPU has its own copy, eliminating cross-ISR sharing entirely
/* CORRECT: Per-CPU buffers - no sharing between ISRs */
static DEFINE_PER_CPU(char[256], debug_buf);
static DEFINE_PER_CPU(int, buf_pos);

static irqreturn_t lpass_pcm_isr(int irq, void *data)
{
    char *buf = this_cpu_ptr(debug_buf);
    int  *pos = this_cpu_ptr(&buf_pos);
    *pos += snprintf(buf + *pos, 256 - *pos, "PCM: done
");
    return IRQ_HANDLED;
    /* Even if timer ISR runs, it uses its OWN per-CPU buffer */
}

## 13.6  Scenario 4: Threaded IRQ Re-entered Due to Preemption (Sensor Hub)
/* Qualcomm sensor hub - threaded IRQ - BUGGY */
static irqreturn_t sensor_thread_fn(int irq, void *data)
{
    struct sensor_dev *sensor = data;
    static int processing = 0;  /* Non-atomic flag - race window! */

    /* On PREEMPT_RT: threaded handlers CAN be preempted and re-entered */
    if (processing)
        return IRQ_HANDLED;  /* Racy check - two threads can both see 0 */

    processing = 1;  /* Non-atomic assignment */

    /* I2C read sleeps - another instance can start during this sleep */
    i2c_smbus_read_block_data(sensor->client, REG_DATA, buf);

    processing = 0;
    return IRQ_HANDLED;
}

✅ Fix: Use mutex_trylock() in threaded handlers since they run in process context and can sleep
/* CORRECT: mutex serializes threaded handler re-entry */
static irqreturn_t sensor_thread_fn(int irq, void *data)
{
    struct sensor_dev *sensor = data;

    /* mutex_trylock is non-blocking: if locked, skip (previous run active) */
    if (!mutex_trylock(&sensor->isr_mutex))
        return IRQ_HANDLED;

    /* Safe: I2C reads allowed here (threaded context, can sleep) */
    i2c_smbus_read_block_data(sensor->client, REG_DATA, buf);

    mutex_unlock(&sensor->isr_mutex);
    return IRQ_HANDLED;
}

## 13.7  ARM GICv3 Priority-Based Preemption
ARM GICv3 Priority Levels (lower number = higher priority):

Priority 0 (highest) --- Watchdog / NMI-like ----+
Priority 1           --- IPI (inter-processor)    | Can preempt anything below
Priority 2           --- DMA completion           |
Priority 3           --- UART / SPI / I2C         |
Priority 4 (lowest)  --- GPIO / touch    <--------+ Gets interrupted by all above

Note: Linux typically DISABLES IRQ nesting (all IRQs same priority level)
      Use local_irq_save() inside ISR only when hardware requires it
      PREEMPT_RT kernels: threaded handlers are preemptible (process context)

## 13.8  Reentrancy Scenarios Summary
| Scenario | How It Happens | Protection |
| Shared IRQ on SMP | Same handler on 2+ CPUs simultaneously | spin_lock() inside ISR; atomic_t for counters |
| Nested IRQ | Higher-priority IRQ interrupts current handler | local_irq_save() or DEFINE_PER_CPU data |
| Threaded IRQ preemption | PREEMPT_RT preempts threaded handler | mutex in threaded handler (can sleep) |
| Self re-enabling | Handler accidentally calls local_irq_enable() | NEVER call local_irq_enable() in hardirq |
| Softirq re-entry | Same softirq runs on multiple CPUs | spin_lock_bh() from process; spin_lock() from softirq |



# 14. Softirq/Tasklet Misuse
Softirqs and tasklets are deferred interrupt processing mechanisms that run in bottom-half context: interrupts are enabled, but the context is still atomic, meaning you CANNOT sleep. Misuse leads to latency spikes, system hangs, race conditions, or use-after-free bugs.

## 14.1  IRQ Processing Model on ARM (Hardirq → Softirq → Workqueue)
Hardware IRQ (ARM GIC triggers):
        |
        v
+-------------------------------+
| HARDIRQ (top half)            |  <- Minimal work, IRQs for THIS line disabled
| * ACK hardware register       |  <- Must be fast (microseconds)
| * Read minimal status         |  <- Cannot sleep
| * Schedule bottom half        |
+-------------------------------+
        |   raise_softirq() OR tasklet_schedule()
        v
+-------------------------------+
| SOFTIRQ / TASKLET (bottom half)|  <- IRQs ENABLED, but CANNOT sleep
| * Process packet/data          |  <- Preemption disabled
| * Update statistics            |  <- Runs on same CPU as hardirq
| * For heavy work: use workqueue|
+-------------------------------+
        |   schedule_work() / queue_work()
        v
+-------------------------------+
| WORKQUEUE (process context)   |  <- CAN sleep, CAN use GFP_KERNEL
| * Long processing             |  <- Runs in kworker thread
| * I2C / SPI transfers         |  <- Fully preemptible
+-------------------------------+

## 14.2  Softirq vs Tasklet vs Workqueue Comparison
| Feature | Softirq | Tasklet | Workqueue |
| Can Sleep? | ❌ No | ❌ No | ✅ Yes |
| Multi-CPU? | ✅ Same type on all CPUs | ❌ Serialized (one at a time) | ✅ Yes |
| Re-entrant? | ✅ Same softirq on diff CPUs | ❌ Never re-enters | ✅ Yes (unless single-threaded) |
| Priority | High (runs immediately) | Medium | Low (kworker scheduling) |
| ARM Use Case | Networking, block I/O, crypto | Simple deferred IRQ work | I2C reads, firmware load, long ops |
| Status | Fixed set, kernel developers only | Deprecated (Linux 5.10+) | Preferred for new drivers |


## 14.3  Scenario 1: Sleeping in Tasklet (WLAN RX Driver)
The most common tasklet misuse: calling mutex_lock() or kmalloc(GFP_KERNEL) from inside a tasklet callback. Both functions may sleep, but tasklets run in atomic (softirq) context where sleeping is forbidden.

/* Qualcomm WLAN RX driver - BUGGY tasklet */
static void wlan_rx_tasklet(unsigned long data)
{
    struct wlan_dev *wdev = (struct wlan_dev *)data;
    struct sk_buff *skb;

    while ((skb = skb_dequeue(&wdev->rx_queue))) {
        /* ⚠️ mutex_lock SLEEPS: tasklet is atomic context! */
        mutex_lock(&wdev->filter_lock);
        apply_rx_filter(wdev, skb);
        mutex_unlock(&wdev->filter_lock);

        /* ⚠️ kmalloc(GFP_KERNEL) can sleep for page reclaim */
        void *meta = kmalloc(64, GFP_KERNEL);
        netif_rx(skb);
    }
}

/* Kernel warning: */
/* BUG: sleeping function called from invalid context */
/* in_atomic(): 1, preempt_count: 256 (SOFTIRQ context indicator) */

✅ Fix: Replace mutex with spinlock, replace GFP_KERNEL with GFP_ATOMIC inside tasklet, OR convert to workqueue if sleeping is genuinely needed
/* Option A: Fix tasklet (no sleeping) */
static void wlan_rx_tasklet(unsigned long data)
{
    struct wlan_dev *wdev = (struct wlan_dev *)data;
    struct sk_buff *skb;

    while ((skb = skb_dequeue(&wdev->rx_queue))) {
        spin_lock(&wdev->filter_lock);   /* Non-sleeping lock */
        apply_rx_filter(wdev, skb);
        spin_unlock(&wdev->filter_lock);

        void *meta = kmalloc(64, GFP_ATOMIC);  /* Non-sleeping alloc */
        netif_rx(skb);
    }
}

/* Option B: Convert to workqueue (if sleeping needed) */
static void wlan_rx_work(struct work_struct *work)
{
    struct wlan_dev *wdev = container_of(work, struct wlan_dev, rx_work);
    mutex_lock(&wdev->filter_lock);  /* Safe in workqueue (process ctx) */
    void *meta = kmalloc(64, GFP_KERNEL);  /* Safe */
}

## 14.4  Scenario 2: Tasklet Not Disabled Before Freeing Data (Crypto Engine)
A classic use-after-free: freeing the data structure that the tasklet is about to access, because tasklet_kill() was called AFTER kfree(). The tasklet can fire between kfree() and tasklet_kill().

/* Qualcomm Crypto Engine driver - BUGGY remove path */
static int qcom_crypto_remove(struct platform_device *pdev)
{
    struct qcom_crypto *crypto = platform_get_drvdata(pdev);

    /* ⚠️ BUG: Free data while tasklet might still be running/scheduled! */
    kfree(crypto->ring_buf);   /* Freed HERE */
    kfree(crypto);             /* Freed HERE */

    /* Tasklet may fire between kfree and tasklet_kill -> UAF! */
    tasklet_kill(&crypto->done_tasklet);  /* TOO LATE - crypto is freed! */
    return 0;
}

✅ Fix: Always call tasklet_kill() BEFORE freeing any data the tasklet accesses. tasklet_kill() blocks until the tasklet finishes.
/* CORRECT teardown order */
static int qcom_crypto_remove(struct platform_device *pdev)
{
    struct qcom_crypto *crypto = platform_get_drvdata(pdev);

    /* Step 1: Kill tasklet first (waits for running instance to finish) */
    tasklet_kill(&crypto->done_tasklet);

    /* Step 2: NOW safe to free - tasklet is guaranteed dead */
    kfree(crypto->ring_buf);
    kfree(crypto);
    return 0;
}

## 14.5  Scenario 3: Softirq Reentrancy on Multiple CPUs (Network RX)
Unlike tasklets, the same softirq type can run simultaneously on multiple CPUs. Any global/shared data accessed from a softirq without protection will have race conditions.

/* Network RX softirq - BUGGY: global non-protected stats */
static struct global_stats {
    int rx_packets;
    int rx_bytes;
} stats;  /* Global data - accessed from NET_RX_SOFTIRQ */

/* This softirq runs simultaneously on ALL CPUs! */
static void net_rx_action(struct softirq_action *h)
{
    /* ⚠️ Race: CPU0, CPU2, CPU5 all increment at same time */
    stats.rx_packets++;   /* Lost updates on SMP! */
    stats.rx_bytes += len;
}

✅ Fix: Use DEFINE_PER_CPU for softirq-private data - each CPU has its own copy, eliminating all races
/* CORRECT: Per-CPU statistics */
static DEFINE_PER_CPU(struct global_stats, stats);

static void net_rx_action(struct softirq_action *h)
{
    this_cpu_inc(stats.rx_packets);   /* Per-CPU: no locking needed */
    this_cpu_add(stats.rx_bytes, len);
}

/* To read aggregate across CPUs: */
int total = 0;
for_each_possible_cpu(cpu)
    total += per_cpu(stats.rx_packets, cpu);

## 14.6  Scenario 4: Process Context Using spin_lock Instead of spin_lock_bh (RMNET)
If a spinlock is shared between process context and a softirq/tasklet, process context MUST use spin_lock_bh() to disable softirqs. Using plain spin_lock() leaves softirqs enabled, causing a deadlock when a softirq fires and tries to acquire the same lock.

/* Qualcomm RMNET driver - BUGGY */
static int rmnet_get_stats(struct rmnet_stats *s)
{
    /* ⚠️ spin_lock() does NOT disable softirqs!           */
    /* Softirq fires on THIS CPU while lock held -> deadlock */
    spin_lock(&s->lock);
    int count = s->tx_count;
    spin_unlock(&s->lock);
    return count;
}

/* Deadlock scenario:                                        */
/* CPU0: spin_lock(&lock)  <- softirqs still ENABLED        */
/*   | softirq fires on same CPU                            */
/*   | rmnet_rx_softirq: spin_lock(&lock) <- DEADLOCK!      */

✅ Fix: Use spin_lock_bh() from process context when the same lock is acquired from softirq/tasklet context
/* CORRECT: spin_lock_bh disables softirqs from process context */
static int rmnet_get_stats(struct rmnet_stats *s)
{
    spin_lock_bh(&s->lock);   /* Disables softirqs on this CPU */
    int count = s->tx_count;
    spin_unlock_bh(&s->lock); /* Re-enables softirqs */
    return count;
}

/* From softirq context: plain spin_lock is correct */
/* (same-type softirqs are serialized per-CPU)      */
static void rmnet_rx_softirq(struct rmnet_stats *s)
{
    spin_lock(&s->lock);
    s->tx_count++;
    spin_unlock(&s->lock);
}

## 14.7  Scenario 5: Tasklet Scheduled After tasklet_kill() (USB DWC3)
/* Qualcomm USB DWC3 driver - BUGGY suspend path */
static void dwc3_ep_complete_isr(int irq, void *data)
{
    struct dwc3 *dwc = data;
    tasklet_schedule(&dwc->ep_tasklet);  /* Scheduled from ISR */
}

static int dwc3_suspend(struct device *dev)
{
    struct dwc3 *dwc = dev_get_drvdata(dev);

    tasklet_kill(&dwc->ep_tasklet);  /* Kill tasklet... */

    /* ⚠️ BUG: IRQ is STILL ENABLED! Can re-schedule the killed tasklet! */
    /* Tasklet fires with USB device in suspended state -> crash!         */
    return 0;
}

✅ Fix: Disable the IRQ first to prevent re-scheduling, then kill the tasklet
/* CORRECT suspend sequence */
static int dwc3_suspend(struct device *dev)
{
    struct dwc3 *dwc = dev_get_drvdata(dev);

    /* Step 1: Disable IRQ - prevents new tasklet scheduling */
    disable_irq(dwc->irq);
    synchronize_irq(dwc->irq);  /* Wait for running ISR to finish */

    /* Step 2: Kill tasklet - no new schedules possible now */
    tasklet_kill(&dwc->ep_tasklet);

    /* Step 3: Now safe to suspend USB hardware */
    return 0;
}

## 14.8  Correct Teardown Order (Critical)
Always follow this sequence when shutting down a driver that uses IRQs and tasklets/workqueues:

/* Correct driver shutdown / remove() order: */

1. disable_irq(irq)          <- Stop new hardirq (and tasklet scheduling)
2. synchronize_irq(irq)      <- Wait for running hardirq to complete
3. tasklet_kill(&tasklet)    <- Wait for running tasklet to finish
4. cancel_work_sync(&work)   <- Wait for running workqueue to finish
5. dmaengine_terminate_sync(chan) <- Stop DMA transfers
6. iounmap() / kfree()       <- NOW safe to free all resources

/* If you free resources BEFORE steps 1-5: Use-After-Free! */

## 14.9  Lock Usage Guide by Execution Context
| Accessing From | Protecting Against | Lock to Use |
| Process context | Softirq/Tasklet | spin_lock_bh() / spin_unlock_bh() |
| Process context | Hardirq | spin_lock_irqsave() / spin_unlock_irqrestore() |
| Process context | Process context only | mutex_lock() / mutex_unlock() |
| Softirq context | Same softirq on other CPU | spin_lock() / spin_unlock() |
| Softirq context | Hardirq | spin_lock_irqsave() / spin_unlock_irqrestore() |
| Hardirq context | Hardirq on other CPU | spin_lock() / spin_unlock() |
| Tasklet context | Same tasklet | Not needed (tasklets are serialized) |
| Tasklet context | Different tasklet/softirq | spin_lock() / spin_unlock() |


⚠️ Important: Tasklets are deprecated since Linux 5.10+. For new Qualcomm BSP drivers, prefer:
- Threaded IRQs (request_threaded_irq) for most deferred IRQ work
- Workqueues (INIT_WORK + queue_work) for processing that may sleep
- NAPI for high-throughput network packet processing

### Modern Replacement for Tasklet:
/* Modern: Threaded IRQ (replaces tasklet for most drivers) */
ret = request_threaded_irq(irq,
                           my_hardirq_handler,  /* Quick: ACK, schedule work */
                           my_thread_handler,   /* Heavy: can sleep, I2C OK   */
                           IRQF_ONESHOT,
                           "qcom-device", dev);


# Quick Reference: Part 2 Summary Table
One-line interview answers for each topic covered in this part:

| Error | Root Cause | Fix in One Line |
| Uninitialized Memory | kmalloc returns stale data; struct padding leaks kernel addrs | kzalloc() by default; memset() all structs before copy_to_user() |
| Invalid Memory Access | No page table entry, PAN violation, freed mapping | ioremap() before MMIO; copy_from_user() for userspace; disable_irq before iounmap() |
| Spinlock + Sleep | Preemption disabled; schedule() cannot run | GFP_ATOMIC inside lock; allocate before lock; udelay() not msleep() |
| Wrong IRQ Flags | Shared line without IRQF_SHARED; level vs edge mismatch | IRQF_SHARED + non-NULL dev_id; IRQF_ONESHOT for threaded IRQ |
| Missing IRQ Disable | ISR fires on same CPU mid-update; torn 64-bit on ARM32 | spin_lock_irqsave() when process ctx shares data with hardirq ISR |
| ISR Reentrancy | Same handler on multiple CPUs; nested IRQ; threaded preemption | spin_lock() in ISR; atomic_t for counters; DEFINE_PER_CPU buffers |
| Softirq/Tasklet Misuse | Sleeping in atomic context; tasklet freed without kill; wrong lock | spin_lock_bh() from process; tasklet_kill() before kfree(); prefer workqueue |


📌 Continues in Part 3: Resource Leaks, Incorrect Register Access, Page Table Corruption, mmap Implementation, Userspace Runtime Errors, Debugging Tools Summary, and Interview Quick-Reference.

Linux Kernel & Userspace Runtime Errors  —  Part 2 of 3  |  Sandeep Kumar  |  Qualcomm Interview Preparation

Qualcomm Staff Engineer
Interview Preparation Guide
Part 2 of 3: Interrupt Handling, Device Drivers, Platform Bring-Up,
TrustZone/QSEE, Yocto, Debugging, Power Management & Qualcomm-Specific Topics

| Field | Details |
| Candidate | Sandeep Kumar |
| Target Role | Staff Engineer — Embedded Linux / Qualcomm SoC |
| Series | Part 2 of 3 |
| Experience | 12 Years (ARM/ARM64, Kernel, SoC Platforms) |
| Document | Interrupt Handling • Device Drivers • Bring-Up • TrustZone • Yocto • Debug • Power • SMEM/QMI • Coding |


| PART 2 — QUICK REFERENCE: KEY SECTIONS |


| Section | Topic |
| Section 1 | C/C++ Fundamentals & Data Structures |
| Section 2 | Interrupt Handling & ARM GICv3 |
| Section 3 | Device Drivers: I2C, SPI, PCIe, USB |
| Section 4 | Qualcomm Platform Bring-Up (UART, TLMM, Boot Sequence) |
| Section 5 | TrustZone / QSEE Architecture |
| Section 6 | Yocto Project & Build System |
| Section 7 | Debugging Tools: JTAG, ftrace, perf, QDSS |
| Section 8 | Power Management (Runtime PM, RPMH) |
| Section 9 | Qualcomm-Specific: SMEM, QMI |
| Section 10 | Coding Questions (Bit Manipulation, Linked List) |
| Section 11 | Interview Process & Priority Topics |



| SECTION 1: C/C++ FUNDAMENTALS & DATA STRUCTURES |


| Q1 What is the difference between volatile, const volatile, and restrict in C? |


volatile
Tells the compiler the variable can change outside program control (hardware register, ISR, shared memory). Prevents optimization — every access goes to memory.
volatile uint32_t *reg = (volatile uint32_t *)0x40001000;
*reg = 0x01;          /* write goes to hardware register */
uint32_t val = *reg;  /* read always fetches from hardware */
const volatile
Read-only from software perspective, but hardware can change it. Used for read-only status registers.
const volatile uint32_t *status_reg = (const volatile uint32_t *)0x40001004;
/* Software cannot write to status_reg, but HW may change it any time */
restrict
C99 keyword — tells the compiler that a pointer is the only way to access that memory in its scope. Enables aggressive optimization (no aliasing). Used in DSP/SIMD code.
void copy(uint8_t * restrict dst, const uint8_t * restrict src, size_t n) {
    /* Compiler may vectorize — guaranteed no overlap between dst and src */
    while (n--) *dst++ = *src++;
}

| ➡ Chain Q: Why is volatile not sufficient for SMP synchronization? |

Because volatile only prevents compiler reordering, not CPU out-of-order execution. On SMP (multi-core ARM), you need memory barriers — DMB, DSB, ISB on ARM; smp_mb(), smp_rmb(), smp_wmb() in Linux kernel — to ensure ordering across cores.

| Q2 Implement a circular buffer (ring buffer) in C for an interrupt-driven UART driver. |


#define BUF_SIZE 256  /* Must be power of 2 */

typedef struct {
    uint8_t  buf[BUF_SIZE];
    uint32_t head;  /* written by producer (ISR) */
    uint32_t tail;  /* read by consumer (process context) */
} ring_buf_t;

/* Called from ISR — producer */
static inline int rb_put(ring_buf_t *rb, uint8_t data)
{
    uint32_t next = (rb->head + 1) & (BUF_SIZE - 1);
    if (next == rb->tail)
        return -1;  /* buffer full */
    rb->buf[rb->head] = data;
    smp_wmb();          /* Ensure data visible before head update */
    rb->head = next;
    return 0;
}

/* Called from process context — consumer */
static inline int rb_get(ring_buf_t *rb, uint8_t *data)
{
    if (rb->head == rb->tail)
        return -1;  /* buffer empty */
    smp_rmb();          /* Ensure head read before data read */
    *data = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) & (BUF_SIZE - 1);
    return 0;
}

Key Points
- Power-of-2 size enables fast modulo via bitwise AND: (head + 1) & (SIZE-1)
- smp_wmb() / smp_rmb() provide lock-free single-producer/single-consumer synchronization
- In real kernel UART driver, prefer kfifo (kernel's optimized ring buffer)
- For multi-producer scenarios, use spinlock around rb_put()

| Q3 What is the difference between a mutex and a spinlock? When do you use each in a kernel driver? |


| Property | Mutex | Spinlock |
| Waiting | Sleeps (blocks) | Busy-waits (spins) |
| Context | Process context only | Any context (IRQ safe) |
| Overhead | Higher (context switch) | Lower (no sleep) |
| Hold Time | Can hold long | Must be very short |
| Preemption | Allowed while waiting | Disabled while held |
| Sleep in CS | Allowed (kmalloc GFP_KERNEL) | NEVER sleep |


Usage Rules
- Spinlock: Use in interrupt handlers, softirqs, or critical sections < few microseconds. Use spin_lock_irqsave() when accessed from both process and interrupt context.
- Mutex: Use in process context when critical section may sleep (e.g., copy_to_user, kmalloc(GFP_KERNEL)).

/* Spinlock example — ISR + process context shared data */
static spinlock_t my_lock;

/* In ISR: */
irq_handler_t my_irq(int irq, void *data) {
    unsigned long flags;
    spin_lock_irqsave(&my_lock, flags);
    /* access shared hardware state */
    spin_unlock_irqrestore(&my_lock, flags);
    return IRQ_HANDLED;
}

| ℹ️ Note: Qualcomm-specific: In Qualcomm camera/display drivers, spinlocks protect HW register access from ISR, while mutexes protect higher-level state machines. |


| SECTION 2: INTERRUPT HANDLING & ARM GICv3 |


| Q4 Describe the ARM GICv3 architecture and how Linux handles interrupts on a multi-core ARM64 SoC. |


GICv3 Components
| Component | Description |
| Distributor (GICD) | Global. Configures SPI (Shared Peripheral Interrupts, ID 32–1019), enables/disables, sets priority and affinity routing. |
| Redistributor (GICR) | Per-CPU. Manages PPI (Private Peripheral Interrupts, ID 16–31) and SGI (Software Generated Interrupts, ID 0–15). |
| CPU Interface (ICC_*) | Per-CPU system registers accessed via MRS/MSR (no MMIO). ICC_IAR1_EL1 (Acknowledge), ICC_EOIR1_EL1 (End of Interrupt), ICC_PMR_EL1 (Priority Mask), ICC_BPR1_EL1 (Binary Point). |


Interrupt Types
| Type | ID Range | Distribution | Examples |
| SGI | 0–15 | Software Generated | IPI (smp_call_function, TLB shootdown, scheduler tick) |
| PPI | 16–31 | Per-CPU Private | ARM generic timer (arch_timer), PMU interrupts |
| SPI | 32–1019 | Shared Peripheral | GPIO, UART, USB, PCIe, Camera CSI |
| LPI | 8192+ | MSI via ITS | PCIe MSI/MSI-X message-signaled interrupts |


Linux Interrupt Flow on ARM64
IRQ Exception
    |
    v
el1_irq / el0_irq  (arch/arm64/kernel/entry.S)
    |   Saves all registers, reads Exception Syndrome
    v
handle_arch_irq()  --> GIC driver: gic_handle_irq()
    |   Reads ICC_IAR1_EL1 --> get irqnr (interrupt ID)
    v
generic_handle_irq(irq) --> irq_desc[irq].handle_irq()
    |   For handle_fasteoi_irq: calls action->handler chain
    v
action->handler(irq, dev_id)   [hard IRQ handler]
    |   Returns IRQ_HANDLED or IRQ_WAKE_THREAD
    v
Write ICC_EOIR1_EL1  (EOI - signal priority drop)

Threaded IRQs (IRQF_THREAD)
static irqreturn_t my_hard_irq(int irq, void *data)
{
    /* Minimal work: read status reg, clear interrupt */
    return IRQ_WAKE_THREAD;  /* Wake the thread */
}

static irqreturn_t my_threaded_irq(int irq, void *data)
{
    /* Can sleep here, do heavy processing */
    process_data();
    return IRQ_HANDLED;
}

/* Registration */
devm_request_threaded_irq(dev, irq,
    my_hard_irq,     /* hard IRQ handler */
    my_threaded_irq, /* threaded handler - runs as irq/N-name */
    IRQF_TRIGGER_RISING, "my-device", dev);

| ℹ️ Note: Threaded handler runs in a kernel thread at SCHED_FIFO priority 50. This reduces interrupt latency and allows sleeping in the handler. |


IPI (Inter-Processor Interrupts) via SGIs
- SGIs (ID 0–15) are used for software-generated cross-CPU signaling
- smp_call_function(), scheduler_ipi(), TLB shootdown all use IPIs
- gic_raise_softirq() writes to ICC_SGI1R_EL1 to target specific CPUs
/* GICv3: send SGI to CPU 1 */
/* Target list: CPUn = bit n in TargetList field */
/* ICC_SGI1R_EL1 = (TargetList << 0) | (SGI_ID << 24) | (Aff << 16) */

| ➡ Chain Q: What is the difference between hardirq, softirq, and tasklet? |


| Type | Hard IRQ | Softirq | Tasklet |
| Context | Interrupt (IRQs disabled) | Interrupt (IRQs enabled) | Interrupt (IRQs enabled) |
| Can sleep | No | No | No |
| Allocation | Static via request_irq | Static (10 types) | Dynamic (tasklet_init) |
| CPU Pinning | Any CPU | Same CPU | Same CPU |
| Runner | CPU handling IRQ | ksoftirqd or IRQ exit | ksoftirqd or IRQ exit |
| Use case | HW IRQ handler | Network RX/TX, timers | Driver deferred work |


| ℹ️ Note: For deferred work that can SLEEP, use workqueues (schedule_work(), queue_work()). Workqueue handlers run in kernel thread context. |


| ➡ Chain Q: Explain TLB Shootdown on ARM64 SMP. What instructions are used? |


When a kernel modifies a page table entry (unmap, permission change), other CPUs may have stale TLB entries. ARM64 TLB shootdown:
- CPU A modifies PTE, calls flush_tlb_range() or flush_tlb_page()
- ARM64 IS (Inner Shareable) TLBI instructions broadcast automatically to all CPUs in the coherency domain
- No explicit IPI needed on ARM64! (Unlike x86, which requires explicit IPIs)
- DSB ISH ensures TLB invalidation completes before proceeding

TLBI VALE1IS, Xt   /* Invalidate by VA, EL1, Inner Shareable (all CPUs) */
TLBI VMALLE1IS     /* Invalidate all EL1 entries, Inner Shareable */
DSB ISH            /* Data Sync Barrier - wait for TLB invalidation */
ISB                /* Instruction Sync Barrier */

| SECTION 3: DEVICE DRIVERS — I2C, SPI, PCIe, USB |


| Q5 Explain the Linux device driver model — platform_driver, probe(), and Device Tree binding. |


Linux Driver Model (LDM) Overview
- Built on kobject / kset / ktype (sysfs backbone)
- Three key abstractions: bus, device, driver
- Bus matches devices to drivers via match() function
- Platform Bus: For SoC peripherals not on discoverable buses (I2C, SPI, UART, GPIO)
- Devices described in Device Tree (DT) or ACPI

Platform Driver Registration
static struct platform_driver my_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = {
        .name           = "my-device",
        .of_match_table = my_of_match,
        .pm             = &my_pm_ops,
    },
};
module_platform_driver(my_driver);  /* Shorthand for init/exit */

Device Tree Binding
my_device@40000000 {
    compatible = "vendor,my-device";
    reg = <0x40000000 0x1000>;
    interrupts = <GIC_SPI 42 IRQ_TYPE_LEVEL_HIGH>;
    clocks = <&clk_controller CLK_MY_DEV>;
    resets = <&reset_controller RESET_MY_DEV>;
    power-domains = <&rpmhpd RPMHPD_CX>;
    status = "okay";
};

probe() Function Responsibilities
- platform_get_resource() → get MMIO base address from DT reg property
- devm_ioremap_resource() → map MMIO to kernel virtual address
- platform_get_irq() → get IRQ number (translated from DT)
- devm_request_irq() → register IRQ handler
- devm_clk_get() + clk_prepare_enable() → enable clocks
- devm_reset_control_get() → manage resets
- Register with subsystem (misc_register(), netdev_register(), etc.)

| ℹ️ Note: devm_* (device-managed resources) are automatically released on driver unbind or probe() failure, preventing resource leaks. |


of_match_table with Multiple Compatibles
static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,my-device-v1", .data = &v1_data },
    { .compatible = "vendor,my-device-v2", .data = &v2_data },
    { } /* sentinel */
};
MODULE_DEVICE_TABLE(of, my_of_match);

/* In probe(): */
const struct of_device_id *match = of_match_device(my_of_match, dev);
const struct my_variant_data *vdata = match->data;

| ➡ Chain Q: What is the difference between ioremap, ioremap_nocache, and ioremap_wc? |


| Function | Memory Attribute (ARM64) | Use Case |
| ioremap() | MT_DEVICE_nGnRnE (non-Gathering, non-Reordering, non-Early Write Ack) | Standard MMIO device registers — strictly ordered |
| ioremap_nocache() | Deprecated alias for ioremap() | Legacy code — same as ioremap() |
| ioremap_wc() | MT_NORMAL_NC or MT_DEVICE_GRE (Write-Combining) | Framebuffers, display memory — write merging allowed |
| ioremap_cache() | MT_NORMAL (cached) | RAM-like regions; rarely used for MMIO |


| ➡ Chain Q: Explain DMA coherency on ARM. What is the difference between coherent and streaming DMA? |


| DMA Type | API | Description |
| Coherent DMA | dma_alloc_coherent() | Always coherent between CPU and device. No explicit cache maintenance needed. Expensive (uncached or HW coherent via CCI/CCN). |
| Streaming DMA | dma_map_single(), dma_map_sg() | Uses existing memory. Explicit cache maintenance: TO_DEVICE=flush; FROM_DEVICE=invalidate; BIDIRECTIONAL=both. |
| IOMMU/SMMU | Transparent via dma_map_*() | Maps device DMA addresses (IOVA) to physical addresses. Provides isolation and protection. |


| Q7 Walk me through writing an I2C client driver for a sensor on a Qualcomm platform. |


/* 1. Define match tables */
static const struct i2c_device_id my_sensor_id[] = {
    { "my-sensor", 0 }, { }
};
MODULE_DEVICE_TABLE(i2c, my_sensor_id);

static const struct of_device_id my_sensor_of_match[] = {
    { .compatible = "vendor,my-sensor" }, { }
};
MODULE_DEVICE_TABLE(of, my_sensor_of_match);

/* 2. probe() */
static int my_sensor_probe(struct i2c_client *client,
                           const struct i2c_device_id *id)
{
    struct my_sensor_data *data;

    if (!i2c_check_functionality(client->adapter,
                                  I2C_FUNC_SMBUS_BYTE_DATA))
        return -ENODEV;

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data) return -ENOMEM;

    data->client = client;
    i2c_set_clientdata(client, data);

    int chip_id = i2c_smbus_read_byte_data(client, REG_CHIP_ID);
    if (chip_id != EXPECTED_CHIP_ID) return -ENODEV;

    return my_sensor_register(data);
}

/* 3. Driver struct */
static struct i2c_driver my_sensor_driver = {
    .driver = { .name = "my-sensor",
                .of_match_table = my_sensor_of_match,
                .pm = &my_sensor_pm_ops },
    .probe  = my_sensor_probe,
    .remove = my_sensor_remove,
    .id_table = my_sensor_id,
};
module_i2c_driver(my_sensor_driver);

Device Tree for I2C Sensor
&i2c3 {
    my_sensor@48 {
        compatible = "vendor,my-sensor";
        reg = <0x48>;              /* 7-bit I2C address */
        interrupt-parent = <&tlmm>;
        interrupts = <25 IRQ_TYPE_EDGE_RISING>;
        vdd-supply = <&pm8150_l10>; /* Regulator reference */
    };
};

| ➡ Chain Q: What is the difference between i2c_smbus_read_byte_data and i2c_transfer? |

- i2c_smbus_*: High-level SMBus protocol wrappers. Simple, limited to SMBus transactions (byte, word, block). Internally calls i2c_transfer.
- i2c_transfer: Low-level, sends raw i2c_msg array. Full control over start/stop/repeated-start conditions. Required for complex transactions (e.g., write register address then read without stop — common for sensors).

/* i2c_transfer: write reg addr + read data (no STOP between) */
struct i2c_msg msgs[2] = {
    { .addr = client->addr, .flags = 0,
      .len = 1, .buf = &reg_addr },       /* write */
    { .addr = client->addr, .flags = I2C_M_RD,
      .len = 2, .buf = rx_buf }            /* read */
};
i2c_transfer(client->adapter, msgs, 2);

| Q8 Explain PCIe enumeration on Linux. What happens when a PCIe device is plugged in? |


PCIe Enumeration Flow
- Root Complex (RC) scans bus 0 → reads Vendor/Device ID from config space offset 0x00
- If device found → assigns bus number, BAR (Base Address Register) resources
- pci_scan_bus() → pci_scan_slot() → pci_scan_device() → pci_add_new_bus() for bridges
- pci_assign_resource() → assigns MMIO/IO/MSI resources from parent bridge windows
- pci_bus_add_devices() → pci_device_add() → triggers driver probe()

Qualcomm PCIe Specifics
| Component | Details |
| RC Driver | pcie-qcom.c — Qualcomm Root Complex controller driver |
| PHY Driver | phy-qcom-qmp.c — QMP PHY for link training and Gen1/2/3/4 configuration |
| Interrupts | MSI/MSI-X only (no legacy INTx on most Qualcomm platforms) |
| DMA Isolation | ARM SMMU (apps_smmu) used for PCIe DMA isolation; iommus = <&apps_smmu ...> |
| Link Speed | Gen1 (2.5GT/s), Gen2 (5GT/s), Gen3 (8GT/s), Gen4 (16GT/s) per platform |
| Config Space | ECAM (Enhanced Config Access Mechanism) — 256MB window for 256 buses |


| ➡ Chain Q: What is a BAR (Base Address Register)? |

BARs define the device's MMIO/IO regions. A device can have up to 6 BARs (0–5). The OS writes all-1s to a BAR, reads back to determine size (bits that remain 0 = size). Then assigns a physical address. The driver uses pci_ioremap_bar() to map to kernel virtual address.

/* PCIe device driver example */
static int my_pcie_probe(struct pci_dev *pdev,
                         const struct pci_device_id *id)
{
    int err;
    err = pci_enable_device(pdev);
    if (err) return err;

    err = pci_request_regions(pdev, "my-pcie");
    if (err) goto err_disable;

    void __iomem *bar0 = pci_ioremap_bar(pdev, 0);
    pci_set_master(pdev);   /* Enable bus mastering for DMA */
    pci_alloc_irq_vectors(pdev, 1, 16, PCI_IRQ_MSI); /* MSI-X */
    return 0;
}

| Q9 Explain USB enumeration. What is the difference between USB 2.0 and USB 3.x from a driver perspective? |


USB Enumeration Sequence
- Device connects → hub detects attach (D+/D- line state change) → resets device
- Host assigns address via SET_ADDRESS control request
- Host reads Device Descriptor (GET_DESCRIPTOR, type=Device)
- Host reads Configuration, Interface, Endpoint, String descriptors
- Host selects configuration (SET_CONFIGURATION)
- usb_match_id() / usb_match_device() matches driver → probe() called

| Aspect | USB 2.0 | USB 3.x (SuperSpeed) |
| Max Speed | 480 Mbps (HighSpeed) | 5/10/20 Gbps (SS/SS+) |
| Protocol | Token-based (SOF, IN/OUT tokens; host polls) | Packet-based (no polling; credit-based flow) |
| Bulk Streams | No streams | SuperSpeed Bulk Streams (up to 65535) |
| Linux HCD | EHCI (USB 2.0 only) | xHCI (handles both 2.0 and 3.x) |
| Power Mgmt | VBUS only | VBUS + USB Power Delivery (PD) |
| Qualcomm IP | Synopsys DWC3 (USB2 port) | Synopsys DWC3 (dwc3-qcom.c) |


| ℹ️ Note: Qualcomm uses DWC3 (DesignWare USB3) IP core. Driver: drivers/usb/dwc3/. Qualcomm wrapper: dwc3-qcom.c. Supports USB role switching (host/device/OTG) via extcon notifiers. |


| SECTION 4: QUALCOMM PLATFORM BRING-UP |


Q: Explain the Linux Boot Sequence on a Qualcomm MSM/SM Platform.

Power On
  |
  v
PBL (Primary Boot Loader) -- ROM, embedded in Qualcomm SoC fuses
  |  Validates XBL using PKHash chain-of-trust, loads from eMMC/UFS
  v
XBL (eXtensible Boot Loader) -- UEFI-based, replaces old SBL
  |  Initializes DDR, PMIC, clocks, loads TZ/QSEE, loads ABL
  v
ABL (Android Boot Loader) -- Little Kernel based
  |  Loads kernel Image/Image.gz, DTB, initramfs from boot partition
  |  Reads board ID from SMEM, selects correct DTB
  |  Passes bootargs (cmdline) to kernel via DTB chosen node
  v
Linux Kernel (Image/Image.gz)
  |  Decompress (if needed), setup MMU, cache, stack
  v
start_kernel()
  |  setup_arch() -- CPU init, MMU, early DT parsing
  |  mm_init()    -- Memory subsystem
  |  sched_init() -- Scheduler init
  |  driver subsystems init (bus, platform, i2c, spi...)
  v
kernel_init() / init process (PID 1) --> User Space

| Stage | Component | Key Responsibilities |
| Stage 0 | PBL (ROM) | Power-on, authentication of XBL using HW fused root of trust |
| Stage 1 | XBL/SBL | DDR init, PMIC init, clock setup, QSEE/TZ load, ABL load |
| Stage 2 | QSEE (TZ) | Secure world init, SMMU secure config, TA loading capability |
| Stage 3 | ABL (LK) | Fastboot, kernel/DTB load, bootargs, board ID selection |
| Stage 4 | Linux Kernel | Full hardware init, driver probe, subsystem bring-up, init/systemd |


| ➡ Chain Q: What is the role of DTB in Qualcomm platform bring-up? |

The Device Tree Blob (DTB) describes hardware topology to the kernel — peripherals, memory maps, interrupt routing, clock trees, pinmux. On Qualcomm, DTBs are appended to the kernel image or stored separately. The ABL selects the correct DTB based on board ID/SKU read from SMEM (Shared Memory). of_platform_populate() in the kernel instantiates platform devices from DT nodes.

| Q10 You are bringing up a new Qualcomm SM8650 platform. The kernel boots but UART console is not working. How do you debug? |


Systematic 6-Step Debug Approach

Step 1 — Verify Hardware Path
- Check schematic: Which UART instance? (GENI UART on Qualcomm = UART_SE0 – SE9)
- Verify TX/RX pins with oscilloscope — look for bit-banging on TX line at boot
- Check voltage levels: 1.8V or 3.3V depending on board design
Step 2 — Check Device Tree
/* Required DT settings for Qualcomm GENI UART */
&uart0 {
    status = "okay";          /* MUST be okay, not disabled */
    pinctrl-names = "default", "sleep";
    pinctrl-0 = <&uart0_active>;
    pinctrl-1 = <&uart0_sleep>;
    /* Verify: reg matches SoC TRM UART instance base address */
    /* Verify: clocks = <&gcc GCC_QUPV3_UART0_CLK> is correct */
};
Step 3 — Check Clock/Power Domains
cat /sys/kernel/debug/clk/gcc_qupv3_uart0_clk/clk_enable_count
cat /sys/kernel/debug/regulator/ldo_cx/enable_count
# Verify QUPV3 AHB clock and UART core clock are enabled
Step 4 — Check Pinmux (TLMM)
cat /sys/kernel/debug/pinctrl/pinctrl-handles
cat /sys/kernel/debug/gpio
# GPIO 4/5 should show "mux: qup0_se0_l0 / qup0_se0_l1" for UART
Step 5 — Use earlycon (Pre-Driver Boot Console)
/* Add to bootargs (ABL cmdline): */
earlycon=msm_serial_dm,0x00C1F000 console=ttyMSM0,115200n8
/* earlycon kicks in before platform driver probe() */
/* Helps identify if issue is HW or driver */
Step 6 — JTAG Debug
- Attach Lauterbach TRACE32 via 20-pin debug header
- Load vmlinux with debug symbols
- Set breakpoint at uart_startup() or msm_serial_startup()
- Inspect register state against SoC TRM register map

| Q11 What is TLMM (Top Level Mode Multiplexer) on Qualcomm? How do you configure GPIO/pinmux? |


TLMM is Qualcomm's pin controller — manages GPIO direction, function mux, drive strength, and pull configuration for all SoC pins. Linux driver: pinctrl-msm.c + SoC-specific file (e.g., pinctrl-sm8650.c).

/* Pin state definition in DT */
uart0_active: uart0-active-state {
    pins = "gpio4", "gpio5";
    function = "qup0_se0_l0";  /* UART function for these pins */
    drive-strength = <2>;       /* 2mA */
    bias-disable;               /* No pull */
};

/* GPIO output example */
&tlmm {
    gpio_reset: gpio-reset-state {
        pins = "gpio10";
        function = "gpio";      /* GPIO function (no mux) */
        drive-strength = <2>;
        output-low;
    };
};

/* Runtime GPIO control in driver: */
struct gpio_desc *gpio = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_LOW);
gpiod_set_value_cansleep(gpio, 1);  /* Assert reset */
msleep(10);
gpiod_set_value_cansleep(gpio, 0);  /* Deassert reset */

| SECTION 5: TRUSTZONE & QUALCOMM QSEE ARCHITECTURE |


| Q12 Explain Qualcomm's TrustZone architecture (QSEE). How does a Linux driver communicate with a Trusted Application? |


Qualcomm TrustZone (QSEE) Stack
Normal World (HLOS)              Secure World (QSEE)
========================        =======================
User App (QSEECom Client)        Trusted Application (TA)
  |                                |
QSEECom library                  QSEE OS (Qualcomm TEE)
  |                                |
SCM driver (drivers/firmware/     TrustZone Kernel (EL3)
             qcom_scm.c)           |
  |                                |
SMC #0 instruction ============> ARM EL3 Secure Monitor
  (x0=SMC function ID,            (validates, switches worlds)
   x1-x3=arguments)

Communication Flow
- Linux driver calls qcom_scm_call() or qseecom_send_cmd()
- SCM driver issues SMC #0 instruction → traps to EL3
- TrustZone monitor at EL3 validates function ID and arguments
- Monitor saves normal world state, switches to secure world
- QSEE dispatches request to the appropriate Trusted Application
- TA processes request, writes response to shared memory
- Monitor switches back to normal world, returns to EL1

Key Qualcomm SCM Calls
/* Read fuse (OTP) value via TrustZone */
ret = qcom_scm_io_readl(fuse_addr, &fuse_val);

/* Assign memory region permissions between VMs */
ret = qcom_scm_assign_mem(mem_addr, mem_size, &src_vm, &dst_vm, 1);

/* Authenticate and load QSEE trusted application */
ret = qcom_scm_qseecom_app_send(app_id, req_buf, req_size,
                                 rsp_buf, rsp_size);

TrustZone Memory Protection
| Region | Access | Notes |
| QSEE Kernel | EL3 only (TZ) | Contains QSEE OS, TAs, secure keys |
| SMEM | Normal + Secure | Shared inter-processor communication |
| Video Secure Memory | Secure World only | DRM/Widevine protected content |
| HLOS DDR | Normal World | Linux kernel + user space memory |


| ➡ Chain Q: What is SMMU and how does it relate to TrustZone security? |

The SMMU (System Memory Management Unit) — Qualcomm's IOMMU — provides DMA isolation. Each peripheral (PCIe, USB, camera) has its own SMMU context bank. TrustZone configures SMMU to prevent non-secure peripherals from accessing secure memory regions. The Linux SMMU driver (arm-smmu.c, arm-smmu-v3.c) manages non-secure context banks. Secure banks are configured exclusively by QSEE via SCM calls.

| SECTION 6: YOCTO PROJECT & BUILD SYSTEM |


| Q13 Explain the Yocto build system. What is a recipe, layer, and bbappend? |


| Concept | Type | Description |
| Recipe (.bb) | Build instructions | Defines source URL, patches, configure flags, compile, install steps for ONE component |
| Layer (meta-*) | Collection | Group of recipes, configs, classes. Provides modularity. Listed in bblayers.conf. |
| .bbappend | Recipe extension | Extends/overrides an existing recipe without modifying it. Add patches, change configs for your platform. |
| MACHINE | Target HW config | e.g., qcom-armv8. Machine-specific configs in conf/machine/. Sets ARCH, BSP, kernel. |
| DISTRO | Distribution policy | e.g., poky. Controls features, init system (systemd/sysvinit), libc (glibc/musl). |
| IMAGE_INSTALL | Package list | Packages included in final rootfs image. |
| bitbake | Build engine | Parses recipes, resolves dependencies (DEPENDS/RDEPENDS), executes tasks (do_fetch, do_compile, do_install) |


Recipe Structure Example
DESCRIPTION = "My custom kernel module for Qualcomm platform"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=..."

SRC_URI = "git://github.com/vendor/my-module.git;branch=main \
           file://0001-fix-build-warning.patch"
SRCREV = "a1b2c3d4e5f6..."

inherit module       # For kernel modules

KERNEL_MODULE_AUTOLOAD += "my_module"
KERNEL_MODULE_PROBECONF += "my_module"

Common bitbake Commands
| Command | Description |
| bitbake core-image-minimal | Build a minimal rootfs image |
| bitbake -c devshell <recipe> | Open interactive shell in recipe build environment |
| bitbake -e <recipe> | grep ^SRC_URI | Inspect resolved variable values for a recipe |
| bitbake-layers show-layers | List all active layers and priorities |
| bitbake -c cleanall <recipe> | Clean recipe build artifacts and sstate cache |
| bitbake -g <recipe> | Generate dependency graph (task-depends.dot) |
| devtool modify <recipe> | Check out source for development/patching |


| ℹ️ Note: Qualcomm provides meta-qcom BSP layer with recipes for MSM/SM platform kernel, proprietary firmware blobs, QSEE, and peripheral firmware. Access requires Qualcomm CodeLinaro membership. |


| SECTION 7: DEBUGGING TOOLS — JTAG, ftrace, perf, QDSS |


| Q14 How do you use JTAG for kernel debugging on a Qualcomm platform? |


JTAG Debug Setup
- Hardware: Lauterbach TRACE32 or ARM DS-5 + 20-pin JTAG/SWD connector on board
- Qualcomm: QDSS (Qualcomm Debug Subsystem) provides unified debug interface
- Ensure kernel built with CONFIG_DEBUG_INFO=y, CONFIG_FRAME_POINTER=y

JTAG Workflow
- Connect JTAG probe to target board debug header (JTAG/SWD)
- Load vmlinux (with DWARF debug symbols) into TRACE32/GDB
- Set hardware breakpoints: b platform_driver_probe
- Inspect CPU registers, kernel stacks, memory regions
- For post-crash analysis: load vmcore + vmlinux into crash utility

QDSS (Qualcomm Debug Subsystem)
| Component | Description |
| ETM | Embedded Trace Macrocell — instruction-level tracing. Captures PC history with timestamps. Enables function call tracing without breakpoints. |
| STM | System Trace Macrocell — software-inserted trace points. Lower overhead than ETM. Used for printk-like tracing in production. |
| Funnel | Aggregates trace streams from multiple cores into single pipeline. |
| TMC/ETB | Trace Memory Controller / Embedded Trace Buffer — stores trace data in on-chip SRAM. |
| TPIU | Trace Port Interface Unit — outputs trace to external analyzer via trace port. |
| Linux Driver | drivers/hwtracing/coresight/ — CoreSight framework (coresight-etm4x.c, coresight-stm.c) |


Oscilloscope Use Cases in Bring-Up
- Verify I2C/SPI signal integrity — rise time, voltage levels, noise
- Measure interrupt latency — GPIO toggle at ISR entry/exit vs. hardware event
- Debug clock issues — verify PLL lock, output frequency accuracy
- Check UART baud rate accuracy — measure bit timing
- I2C/SPI protocol decode — oscilloscope protocol analyzers validate transactions

| Q15 What is ftrace and how do you use it for kernel debugging? |


ftrace — Linux Kernel Tracing Framework
# Enable function tracing
cd /sys/kernel/debug/tracing
echo function > current_tracer
echo 1 > tracing_on
cat trace
echo 0 > tracing_on

# Trace a specific driver function
echo my_driver_irq_handler > set_ftrace_filter
echo function_graph > current_tracer
echo 1 > tracing_on && sleep 1 && cat trace

# Trace events (preferred over function tracing for production)
echo 1 > events/irq/irq_handler_entry/enable
echo 1 > events/sched/sched_switch/enable
cat trace_pipe   # Stream live events

trace-cmd and perf
# trace-cmd (user-space wrapper for ftrace)
trace-cmd record -p function_graph -g my_driver_probe
trace-cmd report | head -100

# perf for performance profiling
perf stat -e cache-misses,cache-references,cycles,instructions ./app
perf record -g -p <pid>    # Record with call graph
perf report                # Analyze hotspots
perf top                   # Live CPU profiling

# Qualcomm: Decode kernel crash backtrace
scripts/decode_stacktrace.sh vmlinux < oops.txt
addr2line -e vmlinux -i 0xffffffc0081234ab

Kernel Debug Config Options
| Config Option | Purpose |
| CONFIG_DEBUG_INFO | Include DWARF debug info in vmlinux (needed for JTAG/crash) |
| CONFIG_KASAN | Kernel Address Sanitizer — detects use-after-free, out-of-bounds |
| CONFIG_UBSAN | Undefined Behavior Sanitizer — integer overflow, misaligned access |
| CONFIG_LOCKDEP | Lock dependency validator — detects deadlocks at runtime |
| CONFIG_PROVE_LOCKING | Enables full lock correctness checking |
| CONFIG_FTRACE | Enables the ftrace kernel tracing infrastructure |
| CONFIG_DYNAMIC_DEBUG | Enables pr_debug() / dev_dbg() at runtime via debugfs |
| CONFIG_KGDB | Kernel GDB stub — debug kernel over serial/network |


| SECTION 8: POWER MANAGEMENT — RUNTIME PM & RPMH |


| Q16 Explain Linux runtime PM (rpm). How does a Qualcomm peripheral driver implement it? |


Runtime PM Concept
Runtime PM (RPM) allows individual devices to be suspended/resumed dynamically based on usage, independent of system suspend (S3). It tracks usage count — when count reaches 0 and autosuspend delay expires, the device is suspended.

Runtime PM API in a Driver
/* In probe() — initialize runtime PM */
pm_runtime_enable(&pdev->dev);
pm_runtime_set_active(&pdev->dev);   /* Device is initially active */
pm_runtime_set_autosuspend_delay(&pdev->dev, 50); /* 50ms delay */
pm_runtime_use_autosuspend(&pdev->dev);

/* Before accessing hardware (increments usage count) */
ret = pm_runtime_get_sync(&pdev->dev);
if (ret < 0) {
    pm_runtime_put_noidle(&pdev->dev);
    return ret;
}
/* ... do hardware access ... */

/* After done (decrements count; autosuspend after delay) */
pm_runtime_put_autosuspend(&pdev->dev);

/* Runtime PM callbacks in dev_pm_ops */
static int my_runtime_suspend(struct device *dev)
{
    clk_disable_unprepare(priv->clk);
    regulator_disable(priv->vdd);
    return 0;
}

static int my_runtime_resume(struct device *dev)
{
    regulator_enable(priv->vdd);
    return clk_prepare_enable(priv->clk);
}

static const struct dev_pm_ops my_pm_ops = {
    SET_RUNTIME_PM_OPS(my_runtime_suspend, my_runtime_resume, NULL)
    SET_SYSTEM_SLEEP_PM_OPS(my_system_suspend, my_system_resume)
};

Qualcomm RPMH (Resource Power Manager Hardened)
Qualcomm platforms use RPMH — a dedicated ARC (Always-on Resource Controller) processor managing shared resources: voltage rails, clocks, buses. Linux communicates asynchronously via TCS (Triggered Command Set) writes.

RPMH Architecture:

  Linux Kernel                   RPMH (ARC Processor)
  ============                   ======================
  rpmhpd_set_performance_state()
    |
    v  (vote via cmd-db)
  RPMH RSC (Resource State Coordinator)  -->  ARC
    |   TCS write: VRM (voltage), BCM (bus)   |
    |                                          v
  SLEEP set TCS  -----> sent on CPU idle    PMIC / Clock / Bus
  WAKE set TCS   -----> sent on CPU wakeup  configuration
  ACTIVE set TCS -----> sent immediately

| RPMH Component | Function | Linux Driver |
| RSC | Resource State Coordinator — arbitrates votes | drivers/soc/qcom/rpmh-rsc.c |
| VRM | Voltage Rail Manager — controls PMICs | drivers/regulator/qcom-rpmh-regulator.c |
| BCM | Bus Clock Manager — manages bus bandwidth | drivers/interconnect/qcom/ |
| CMD-DB | Command Database — resource address lookup | drivers/soc/qcom/cmd-db.c |
| RPMHPD | Power domain driver using RPMH votes | drivers/pmdomain/qcom/rpmhpd.c |


| SECTION 9: QUALCOMM-SPECIFIC TOPICS — SMEM & QMI |


| Q17 What is SMEM (Shared Memory) on Qualcomm platforms? |


SMEM is a fixed shared memory region used for inter-processor communication (IPC) between the Application Processor (APSS/Linux) and other subsystems: MPSS (modem), ADSP (audio DSP), CDSP (compute DSP), WCSS (Wi-Fi/BT).

SMEM Architecture
Physical DDR
+-----------------------+ 0x86200000 (example Qualcomm address)
|   SMEM Region         |   Size: ~2MB, shared across all processors
|  +-----------------+  |
|  | SMEM Header     |  |   Table of items (smem_item_table)
|  +-----------------+  |
|  | Item 0: BoardInfo|  |   Each item: ID + size + data
|  | Item 1: BootInfo |  |
|  | Item 2: ProcComm |  |
|  | Item N: ...      |  |
|  +-----------------+  |
+-----------------------+

SMEM Usage in Linux
/* Linux driver: drivers/soc/qcom/smem.c */

/* Device Tree: */
memory@86000000 {
    compatible = "qcom,smem";
    reg = <0x86000000 0x200000>;  /* Physical addr, size */
    no-map;                       /* Not mapped in kernel */
    hwlocks = <&tcsr_mutex 3>;    /* TCSR mutex for access protection */
};

/* Reading SMEM item from driver: */
#include <linux/soc/qcom/smem.h>

size_t size;
void *ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY,
                          SMEM_BOARD_INFO, &size);
if (!IS_ERR(ptr)) {
    struct smem_board_info *board = ptr;
    dev_info(dev, "Board ID: %d, HW version: %d",
             board->board_id, board->hw_ver);
}

Common SMEM Item IDs
| SMEM Item ID | Content | Used By |
| SMEM_BOARD_INFO | Board ID, HW revision, SKU | ABL (DTB selection), kernel drivers |
| SMEM_BOOT_INFO | Boot status, warm boot flag | Bootloader, kernel init |
| SMEM_VERSION_INFO | Software versions per subsystem | Diagnostic/bug reports |
| SMEM_MODEM_REASON | Modem crash reason/dump info | SSR (Subsystem Restart) driver |
| SMEM_POWER_ON_STATUS | Power-on reason (cold/warm) | PMIC driver, diagnostics |


| Q18 What is QMI (Qualcomm MSM Interface) and how does it work? |


QMI is Qualcomm's IPC (Inter-Process Communication) protocol used for structured message-based communication between HLOS (Linux) and remote subsystems (modem/MPSS, ADSP, WCSS).

QMI Architecture
Linux Application Processor (HLOS)      Remote Subsystem (MPSS/ADSP)
==============================          ===========================
QMI Client (e.g., netdev driver)        QMI Service (e.g., modem)
     |                                       |
QMI Helper (lib/qmi_encdec.c)              QMI Service Handler
     | (encode/decode TLV messages)          |
QRTR Socket (AF_QIPCRTR)  <============>  IPC Router (QRTR)
     |   (Qualcomm Router - socket IPC)       |
Shared Memory Transport                  Shared Memory Transport
(via SMEM or SMP2P)                      (via SMEM or SMP2P)

| Component | Description |
| QMI IDL | Interface Definition Language — defines message structures (similar to Protobuf). Auto-generates encode/decode code. |
| QRTR | Qualcomm Router — socket-based IPC (AF_QIPCRTR). Multiplexes multiple QMI services over shared transport. drivers/net/qrtr/ |
| SMP2P | Shared Memory Point-to-Point — lightweight signaling mechanism. Used for subsystem notifications. |
| TLV | Type-Length-Value encoding format used by QMI messages for extensibility and backward compatibility. |


QMI Flow Example
/* Linux QMI client (e.g., Wi-Fi firmware control) */
#include <linux/soc/qcom/qmi.h>

struct qmi_handle qmi;

/* Initialize QMI handle */
qmi_handle_init(&qmi, MAX_MSG_SIZE, &ops, NULL);
qmi_add_lookup(&qmi, service_id, version, instance);

/* Send synchronous QMI request */
ret = qmi_send_request(&qmi, sq, &txn,
                       msg_id, msg_size,
                       req_ei, &req_struct);
/* Wait for response */
ret = qmi_txn_wait(&txn, msecs_to_jiffies(QMI_TIMEOUT_MS));

| ℹ️ Note: QMI is used by: Wi-Fi (WCNSS/WCN6xxx), Modem data (rmnet), Sensors (ADSP), Thermal (CDSP), Subsystem Restart (SSR). Understanding QMI is essential for Qualcomm platform debugging. |


| Q6 What is a kernel oops vs. kernel panic? How do you debug them? |


| Type | Kernel Oops | Kernel Panic |
| Severity | Non-fatal (by default) | Fatal — system halts |
| Cause | NULL ptr deref, bad memory access, BUG() | panic(), double fault, oops in critical path |
| System State | Continues (but often unstable) | Halts, optionally reboots |
| Config | panic_on_oops=1 converts to panic | panic_timeout=N for auto-reboot |


Debugging Steps
- Decode backtrace: scripts/decode_stacktrace.sh vmlinux < oops.txt
- addr2line: addr2line -e vmlinux -i 0xffffffc0081234ab
- objdump: objdump -d vmlinux | grep -A20 <fault_addr>
- KASAN: Kernel Address Sanitizer catches use-after-free and OOB
- KGDB/JTAG: Attach GDB via JTAG for live debugging
- Qualcomm ramdump: Captures full DDR on panic; analyze with crash utility

| ➡ Chain Q: What does the oops output tell you? |

- PC (Program Counter): Exact instruction that faulted
- LR (Link Register) / x30: Caller function
- SP: Stack pointer at fault time
- ESR_EL1: Exception syndrome — fault type (translation, permission, alignment)
- FAR_EL1: The virtual address that caused the fault
- Call Stack: Full backtrace from fault to entry point

| SECTION 10: CODING QUESTIONS |


| Q19 Reverse the bits of a 32-bit integer. (Common Qualcomm coding question) |


Approach 1: Simple Loop — O(32)
uint32_t reverse_bits(uint32_t n)
{
    uint32_t result = 0;
    int i;
    for (i = 0; i < 32; i++) {
        result = (result << 1) | (n & 1);  /* LSB of n -> MSB of result */
        n >>= 1;
    }
    return result;
}

Approach 2: Optimized Bit-Swap — O(log 32) = O(5)
uint32_t reverse_bits_fast(uint32_t n)
{
    /* Swap 16-bit halves */
    n = ((n & 0xFFFF0000) >> 16) | ((n & 0x0000FFFF) << 16);
    /* Swap 8-bit quarters */
    n = ((n & 0xFF00FF00) >>  8) | ((n & 0x00FF00FF) <<  8);
    /* Swap 4-bit nibbles */
    n = ((n & 0xF0F0F0F0) >>  4) | ((n & 0x0F0F0F0F) <<  4);
    /* Swap 2-bit pairs */
    n = ((n & 0xCCCCCCCC) >>  2) | ((n & 0x33333333) <<  2);
    /* Swap individual bits */
    n = ((n & 0xAAAAAAAA) >>  1) | ((n & 0x55555555) <<  1);
    return n;
}

Example: reverse_bits(0x80000000)
Input:  0x80000000 = 1000 0000 0000 0000 0000 0000 0000 0000
Output: 0x00000001 = 0000 0000 0000 0000 0000 0000 0000 0001

| Q20 Detect a cycle in a linked list. (Classic DS question asked at Qualcomm) |


Floyd's Cycle Detection — Tortoise and Hare
struct ListNode {
    int val;
    struct ListNode *next;
};

/* Detect cycle — O(n) time, O(1) space */
bool has_cycle(struct ListNode *head)
{
    struct ListNode *slow = head, *fast = head;
    while (fast && fast->next) {
        slow = slow->next;           /* moves 1 step */
        fast = fast->next->next;    /* moves 2 steps */
        if (slow == fast)            /* they meet? cycle! */
            return true;
    }
    return false;
}

/* Find cycle start node */
struct ListNode *find_cycle_start(struct ListNode *head)
{
    struct ListNode *slow = head, *fast = head;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) {
            /* Reset slow to head, keep fast at meeting point */
            slow = head;
            while (slow != fast) {
                slow = slow->next;
                fast = fast->next;
            }
            return slow;  /* cycle start */
        }
    }
    return NULL;
}

| Q21 Count the number of set bits (1s) in a 32-bit integer. |


/* Brian Kernighan's Algorithm — O(number of set bits) */
int count_bits(uint32_t n)
{
    int count = 0;
    while (n) {
        n &= (n - 1);  /* Clears the lowest set bit */
        count++;
    }
    return count;
}

/* GCC built-in (preferred in practice): */
int count = __builtin_popcount(n);

/* Key insight: n & (n-1) always clears the rightmost set bit */
/* Example: n=0b1100 -> n-1=0b1011 -> n&(n-1)=0b1000 */

| SECTION 11: QUALCOMM INTERVIEW PROCESS & PRIORITY TOPICS |


Interview Process at Qualcomm — Staff Engineer Level

| Round | Format | Duration | Focus Areas |
| HR Screen | Phone | 30 min | Background, motivation, compensation expectations |
| Tech Screen | Phone/Video | 60 min | C/C++ fundamentals, OS basics, one coding problem |
| Onsite R1 | In-person | 60 min | Deep dive on most complex project you have delivered |
| Onsite R2 | In-person | 60 min | Linux kernel internals + device driver development |
| Onsite R3 | In-person | 60 min | ARM architecture + Qualcomm platform bring-up |
| Onsite R4 | In-person | 60 min | Coding: DSA — linked list, trees, bit manipulation |
| Onsite R5 | In-person | 60 min | System design: e.g., design power management for new SoC |
| HM Round | In-person | 45 min | Leadership, cross-team collaboration, roadmap vision |


| ℹ️ Note: Qualcomm interviews are known for pivoting mid-question. Example: starting with "explain SMMU" and pivoting to "show me how you would debug an SMMU fault in a PCIe driver." Prepare to go 3 levels deep on any topic. |


Priority Topics to Brush Up (Ordered by Impact)

| # | Topic | Why Critical | Key Concepts |
| 1 | Qualcomm GENI | Unique to Qualcomm — asked in 90% of interviews | Unified SE (Serial Engine) for I2C/SPI/UART, GSI DMA, FIFO vs DMA mode |
| 2 | RPMH / BCM | Platform-specific PM — separates Qualcomm experts | TCS (SLEEP/WAKE/ACTIVE sets), VRM, BCM, cmd-db, rpmhpd |
| 3 | Camera Bring-up | Major Qualcomm revenue — deep V4L2 knowledge needed | V4L2 subdev framework, CSI-2 lanes, CAMSS pipeline, ISP |
| 4 | QSEECOM / SCM | TrustZone is core to all Qualcomm products | qcom_scm.c, SMC IDs, QSEECOM ioctls, TA lifecycle |
| 5 | Subsystem Restart | Modem/ADSP crash recovery is everyday Qualcomm work | remoteproc framework, SSR notifiers, crash dump, PIL |
| 6 | IOMMU / SMMU | DMA security is increasingly important | Context banks, stream IDs, iommu_domain, SMMU faults |
| 7 | Concurrency Primitives | Always asked at Staff level for correctness | RCU, seqlock, per-CPU variables, lockless data structures |


System Call Path on ARM64 — Quick Reference

1. User space: syscall number in x8, args in x0-x5, then: SVC #0
2. CPU: synchronous exception to EL1 -> VBAR_EL1 + 0x400
3. el0_sync (entry.S): save pt_regs, read ESR_EL1 (EC=0x15=SVC)
4. el0_svc() -> do_el0_svc(): index into sys_call_table[x8]
5. Execute syscall handler (e.g., sys_read, sys_write, sys_open)
6. Return: restore pt_regs, ERET to EL0

Qualcomm SCM (Secure Channel Manager):
  EL1 (Linux) -> SMC #0 -> EL3 (TrustZone) -> Secure World
  x0 = function ID (owner | service | function_num)
  x1-x5 = arguments

Linux Boot Sequence on Qualcomm — Kernel Init
start_kernel() {
    setup_arch();         /* CPU, MMU, early DT, BSS clear */
    setup_log_buf();
    mm_init();            /* Buddy allocator, slab, vmalloc */
    sched_init();
    rcu_init();
    early_irq_init();
    init_IRQ();           /* GIC init */
    time_init();          /* ARM generic timer */
    softirq_init();
    driver_init();        /* Core bus types: platform, i2c, spi */
    of_core_init();       /* Device Tree node population */
    rest_init();          /* Create kthreadd, call kernel_init() */
}

| QUICK REFERENCE: COMMON QUALCOMM INTERVIEW TIPS |


| DO | AVOID |
| Always mention Qualcomm-specific components (GENI, RPMH, TLMM, QSEE) when relevant | Generic answers that ignore Qualcomm platform specifics |
| Draw ASCII diagrams or block diagrams to explain architectures | Vague hand-waving without concrete register/API references |
| Reference actual Linux kernel source files/functions | Saying "the driver handles it" without explaining how |
| Volunteer debugging approach: hardware first, then DT, then driver | Jumping straight to code without verifying hardware setup |
| Show depth: mention KPTI/Spectre mitigations, RCU, lockless design | Staying at surface level for senior/staff questions |
| Admit when uncertain, explain your reasoning process | Bluffing or guessing specific register values without confidence |


—  End of Part 2  —  Sandeep Kumar • Qualcomm Staff Engineer Interview Guide


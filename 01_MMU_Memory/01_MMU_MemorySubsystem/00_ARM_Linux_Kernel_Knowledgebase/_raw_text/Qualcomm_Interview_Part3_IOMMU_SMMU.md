Qualcomm Staff Engineer
Interview Preparation Guide
Part 3 of 3 — IOMMU / SMMU Deep Dive & ARM Processor Modes

██████  SANDEEP KUMAR  ██████

Target Role: Staff Software Engineer — Embedded Linux / Qualcomm MSM Platforms
May 2026
| TOPICS COVERED IN THIS DOCUMENT |
| Section 1: IOMMU vs SMMU — Generic Concept & RelationshipSection 2: Why IOMMU Exists — Use Cases & BenefitsSection 3: ARM SMMU Architecture — SMMUv2 Deep DiveSection 4: SMMUv3 Architecture — Command Queues, Stream Tables, New FeaturesSection 5: Linux IOMMU Subsystem — Core, Data Structures, DT BindingsSection 6: DMA With & Without SMMU — Code WalkthroughsSection 7: SMMU Fault Handling — Context Faults, Debugging, STALL ModelSection 8: Qualcomm-Specific SMMU — MMU-500, Secure CBs, VirtualizationSection 9: SMMU vs CPU MMU Comparison + Complete Interview Chain QuestionsSection 10: SYS Mode vs SVC Mode (ARMv7-A) — Deep Dive + Chain Questions |


⚠️  This is Part 3 of a 3-part series. Parts 1 and 2 cover ARM Architecture Fundamentals, Linux Memory Management, Scheduling, Interrupt Handling, Device Drivers, Platform Bring-Up, TrustZone, Yocto, Debugging, and Power Management.
# Section 1: IOMMU vs SMMU — The Relationship
| 📌 Key Insight: SMMU IS an IOMMU — specifically ARM's implementation of the IOMMU concept. The terms are NOT interchangeable across architectures. |


An IOMMU (Input/Output Memory Management Unit) is an architecture-agnostic concept. Different processor vendors implement their own IOMMU. The table below maps the relationship:

| Term | Vendor / Architecture | Description |
| IOMMU | Generic (all architectures) | Input/Output Memory Management Unit — concept of translating device DMA addresses to physical addresses |
| SMMU | ARM | System Memory Management Unit — ARM's IOMMU specification (SMMUv1, v2, v3) |
| Intel VT-d | Intel x86/x86_64 | Virtualization Technology for Directed I/O — Intel's IOMMU |
| AMD-Vi | AMD x86/x86_64 | AMD I/O Virtualization (IOMMU) technology |
| Qualcomm SMMU | Qualcomm SoC (ARM) | ARM MMU-500 (SMMUv2) or SMMUv3 IP instantiated in Qualcomm MSM/SM SoCs (apps_smmu, gpu_smmu, kgsl_smmu) |


## 1.1 ASCII Architecture Overview
The fundamental difference is WHERE translation occurs:
WITHOUT IOMMU:
  DMA Device ---------> Physical RAM
  (device sees PA directly -- NO protection)

WITH IOMMU (SMMU):
  DMA Device --[IOVA]--> SMMU --[PA]--> Physical RAM
  (device sees I/O Virtual Address, SMMU translates to PA)

IOVA = I/O Virtual Address  (device-visible)
IPA  = Intermediate Physical Address  (with virtualization Stage 2)
PA   = Physical Address (DRAM address)

SMMU Versions Summary:
| Version | Year / Platform | Key Features |
| SMMUv1 | Early ARM platforms | Basic 2-stage translation, limited Stream IDs, 32-bit address |
| SMMUv2 | Qualcomm SM8150-SM8550 | ARMv7/ARMv8 compatible, up to 2^16 Stream IDs, Stage 1+Stage 2 translation, LPAE format, per-context-bank IRQs |
| SMMUv3 | ARMv8.3+ SoCs | Complete redesign: command/event queues (CMDQ/EVTQ), Stream Table in memory, PASID/SubstreamID (SVA), STALL model, PCIe ATS/PRI, Hardware Table Walk (HTTU), 64-bit PA support |

# Section 2: Why Does IOMMU Exist? — Use Cases & Benefits
## 2.1 Primary Use Cases
| Use Case | Explanation |
| 1. DMA Protection | Prevent rogue or buggy drivers from DMAing to arbitrary physical memory. Each device has its own IOMMU domain with only explicitly mapped pages. A hacked PCIe device cannot read kernel/secure memory. |
| 2. Address Translation | Devices with 32-bit DMA address buses can only directly access the first 4 GB. IOMMU allows mapping DRAM above 4 GB (high memory) into device-visible IOVA space within 32-bit range. |
| 3. Scatter-Gather | Physical memory is fragmented (non-contiguous pages). IOMMU presents these pages as a contiguous IOVA range to the device, eliminating bounce buffers and enabling true scatter-gather DMA. |
| 4. Virtualization | In a VM environment, the guest OS sees Guest Physical Addresses (GPAs). IOMMU Stage 2 translation maps GPA -> HPA, allowing device passthrough (direct DMA from VM) safely. Used with VFIO. |
| 5. SVA (Shared Virtual Addressing) | SMMUv3 feature: the DMA device uses the same virtual address space as the CPU userspace process (same page tables). Eliminates IOVA management overhead. Critical for AI/ML accelerators and GPUs. |


## 2.2 Security Model
Without IOMMU, a compromised device driver or malicious PCIe device can issue DMA transactions to any physical address in the system, including:
- Kernel code and data sections
- TrustZone secure memory
- Other processes' memory
- Hardware security registers

With IOMMU, each device is confined to its own IOMMU domain (set of page table mappings). Any DMA outside the mapped region triggers a Translation Fault or Permission Fault, raising an interrupt to the CPU.
# Section 3: ARM SMMUv2 Architecture — Deep Dive
SMMUv2 is the dominant SMMU version on Qualcomm MSM/SM platforms (e.g., SM8150, SM8250, SM8350, SM8450, SM8550). It uses a register-based configuration model with Context Banks as the central abstraction.
## 3.1 SMMUv2 Block Diagram
+------------------+   Stream ID    +------------------------------------------+
|   DMA Device     |  ----------->  |              SMMUv2                      |
|  (USB, PCIe,     |                |                                          |
|   Camera, GPU,   |   IOVA ---->   |  +-------------------------------+       |
|   Modem DMA)     |                |  |   Stream Matching Registers   |       |
+------------------+                |  |   SMR[0..N]: {SID, Mask}      |       |
                                    |  |   SMR matches -> CB index     |       |
                                    |  +-------------+-----------------+       |
                                    |                |                          |
                                    |  +-------------v-----------------+       |
                                    |  |      Context Bank (CB)         |       |
                                    |  |   TTBR0: Stage-1 page table   |       |
                                    |  |   VTTBR: Stage-2 page table   |       |
                                    |  |   CBA2R: 64-bit enable         |       |
                                    |  |   CBAR:  CB type (S1,S2,S1S2) |       |
                                    |  +-------------+-----------------+       |
                                    |                |                          |
                                    |  +-------------v-----------------+       |
                                    |  |   Page Table Walker (HW)       |       |
                                    |  |   LPAE format (same as CPU)    |       |
                                    |  |   Generates PA from IOVA       |       |
                                    |  +-------------------------------+       |
                                    |                                          |
                                    |  TLB  +------> Physical RAM             |
                                    +------------------------------------------+
## 3.2 Key SMMUv2 Components
### 3.2.1 Stream ID (SID)
Stream ID uniquely identifies the originating DMA master for every transaction entering the SMMU. This is the fundamental mechanism for per-device isolation.
| Bus Type | SID Assignment | Notes |
| PCIe | Requester ID (Bus:Dev:Fn) | Each PCIe function gets its own SID |
| Platform Device | Assigned in Device Tree | iommus = <&apps_smmu SID mask> |
| USB (DWC3) | Single SID per controller | e.g., 0x740 on SM8550 |
| GPU (Adreno) | Multiple SIDs | kgsl_smmu has dedicated GPU SIDs |

### 3.2.2 Stream Matching Register (SMR)
The SMR maps a Stream ID (or range of IDs via mask) to a Context Bank:
SMR[n] = { StreamID: 16 bits, Mask: 16 bits }
S2CR[n] = { Type: TRANSLATE/BYPASS/FAULT, CBNDX: context bank index }

If SMR[n].StreamID & ~Mask == incoming_SID & ~Mask, then S2CR[n] applies. This allows multiple PCIe functions to share one Context Bank.
### 3.2.3 Context Bank (CB)
Context Banks are the per-device (or per-domain) address spaces. Each CB has:
- Its own TTBR0 (Stage 1: IOVA → PA) and VTTBR (Stage 2: IPA → PA)
- TCR (Translation Control Register): configures page size, address range, cache attributes
- MAIR (Memory Attribute Indirection Register): memory type encoding
- FAR (Fault Address Register): IOVA that caused a fault
- FSR (Fault Status Register): fault type (translation fault, permission fault, etc.)
- CONTEXTIDR: ASID equivalent for SMMU TLB tagging
- Dedicated interrupt line per CB for fault reporting

Context Bank Types (configured via CBAR register):
| CBAR Type | Stages Active | Use Case |
| S1_TRANS_S2_BYPASS | Stage 1 only | Standard HLOS device: IOVA -> PA (no hypervisor) |
| S1_TRANS_S2_FAULT | Stage 1 only | Secure allocation: fault if no S2 configured |
| S2_TRANS | Stage 2 only | Hypervisor IPA->PA passthrough for guest VMs |
| S1_TRANS_S2_TRANS | Stage 1 + Stage 2 | Full nested: guest IOVA->IPA->PA (device passthrough to VM) |

### 3.2.4 Page Table Format
SMMUv2 uses the same LPAE (Large Physical Address Extension) page table format as the ARM CPU MMU:
- 4 KB small pages, 64 KB large pages, 2 MB sections, 1 GB super-sections
- Up to 3 levels of page table (equivalent to CPU L1/L2/L3)
- Same PTE attribute bits: AP (Access Permissions), SH (Shareability), MA (Memory Attributes)
- Implemented in kernel as:
- io-pgtable-arm.c with format ARM_64_LPAE_S1
## 3.3 SMMUv2 Register Map (Selected)
| Register | Offset | Function |
| SCR0/SCR1 | 0x000 | Global configuration: SMMU enable, bypass, fault behavior |
| SMR[n] | 0x800+n*4 | Stream matching: {StreamID, Mask} |
| S2CR[n] | 0xC00+n*4 | Stream-to-context routing: {Type, CBNDX} |
| CBAR[n] | CB base+0x0 | Context Bank type and VM ID |
| CB_TTBR0 | CB_n+0x020 | Stage-1 page table base address |
| CB_TCR | CB_n+0x030 | Translation control: T0SZ, granule, cache policy |
| CB_FSR | CB_n+0x058 | Fault status: fault type bits |
| CB_FAR | CB_n+0x060 | Fault address: IOVA that triggered the fault |
| CB_FSYNR0 | CB_n+0x068 | Fault syndrome: WNR (write/read), SS (streamside), MULTI |

# Section 4: ARM SMMUv3 Architecture — Major Redesign
SMMUv3 (introduced in ARM SMMU Architecture Specification v3.x) is a complete redesign from SMMUv2. It uses memory-resident data structures and queue-based command/event interfaces instead of MMIO registers.
## 4.1 SMMUv3 Block Diagram
  CPU (Linux SMMU Driver)
       |
       |  CMDQ (Command Queue in DRAM -- circular ring)
       |  Commands: TLBI_NH_ALL, CFGI_STE, CFGI_CD, SYNC
       v
+--------------------------------------------------+
|                   SMMUv3                         |
|                                                  |
|  Stream Table (in DRAM)                          |
|  +---------------------------------+             |
|  |  STE[SID]  (Stream Table Entry) |  <-- indexed by SID
|  |   Config: BYPASS/ABORT/TRANS    |
|  |   S1ContextPtr -> CD Table      |
|  |   VMID for Stage-2              |
|  +--------------+------------------+
|                 |
|  CD Table (per stream, in DRAM)
|  +---------------------------------+
|  |  CD[SSID] (Context Descriptor)  |  <-- indexed by SubstreamID/PASID
|  |   TTBR0: Stage-1 page tables    |
|  |   ASID, T0SZ, IR/ORcache attr   |
|  +--------------+------------------+
|                 |
|  Page Table Walker (HW) --> PA
|                                                  |
|  Event Queue (EVTQ) --> Fault events to CPU     |
|  PRI Queue (PRIQ)   --> Page Request Interface  |
+--------------------------------------------------+
       |
       v  Physical RAM
## 4.2 SMMUv3 Key Innovations
### 4.2.1 Stream Table
Instead of limited SMR registers (SMMUv2 has at most 128), SMMUv3 uses a Stream Table in DRAM. Each entry (STE) is 64 bytes and contains the full configuration for one Stream ID:
- Linear Stream Table: array indexed by SID (small SID space)
- 2-level Stream Table: L1 table of pointers to L2 spans (large/sparse SID space, e.g., PCIe)
- STE.Config field: 0=ABORT, 1=BYPASS, 4/5=S1/S2 translation enabled
### 4.2.2 Context Descriptor (CD) + PASID/SubstreamID
Each STE points to a CD table. The SubstreamID (SSID) (equivalent to PCIe PASID — Process Address Space ID) indexes into the CD table. This enables:
- One device (SID) to have multiple independent address spaces (one per PASID)
- SVA (Shared Virtual Addressing): CD can point directly to a userspace process's page tables
- Critical for AI/ML workloads: GPU/NPU can DMA using the same virtual addresses as the CPU process
### 4.2.3 Command Queue (CMDQ)
Commands are written to a circular ring buffer in DRAM (CMDQ). The SMMU hardware consumes them. Key commands:
| Command | Effect |
| CFGI_STE | Invalidate Stream Table Entry cache for given SID |
| CFGI_CD | Invalidate Context Descriptor cache for given SID+SSID |
| TLBI_NH_VA | Invalidate TLB by VA, ASID, VMID (non-hypervisor) |
| TLBI_S12_VMALL | Invalidate all stage-1+2 TLB entries for a VMID |
| SYNC | Barrier: wait for all previous commands to complete (MSI completion signal) |

### 4.2.4 STALL Model
SMMUv3 introduces the STALL model for graceful fault handling (critical for SVA):
- Device issues a DMA to an unmapped IOVA
- SMMU stalls the transaction (holds the device) instead of immediately faulting
- SMMU writes a STALL event to the Event Queue (EVTQ)
- Linux SMMU driver processes the event: can map the page (like a CPU page fault) or terminate
- SMMU resumes or terminates the stalled transaction

This enables on-demand device memory mapping, similar to CPU demand paging — the device can trigger page faults that are resolved by the OS.
### 4.2.5 PCIe ATS / PRI Support
- [object Object]: PCIe device can request pre-translated PA from SMMU and cache it in its own internal TLB
- [object Object]: Device sends a page request when its internal TLB misses (works with STALL model)
- Reduces SMMU TLB pressure for high-bandwidth devices (NVMe, GPU)
## 4.3 SMMUv3 vs SMMUv2 Comparison
| Feature | SMMUv2 | SMMUv3 |
| Configuration Interface | Register-based (MMIO SMR, S2CR, CBAR) | Queue-based (CMDQ/EVTQ in DRAM) |
| Stream Mapping | SMR registers (max ~128 entries) | Stream Table in DRAM (linear or 2-level, scalable) |
| Per-stream config | Context Bank (per-CB TTBR) | Context Descriptor table (per SID/SSID) |
| Substream/PASID | Not supported | Yes -- enables SVA, multiple addr spaces per device |
| Stall Model | No -- immediate fault/terminate | Yes -- stall DMA, OS resolves, resume |
| PCIe ATS/PRI | Not supported | Full ATS and PRI support |
| Page table format | LPAE (ARMv7 LPAE) | ARMv8 (4K/16K/64K granule, 4 levels) |
| TLB invalidation | Register writes (TLBIALL, TLBIVMID...) | CMDQ commands (TLBI_NH_VA, TLBI_S12_VMALL) |
| Fault reporting | Per-CB interrupt line | Event Queue (EVTQ) + single SMMU interrupt |
| Secure streams | Secure CB allocated by TZ | SecureWorld SMMU (separate instance or partition) |

# Section 5: Linux IOMMU Subsystem
## 5.1 Kernel Architecture
                     User Space
  +------------------------------------------------------+
  |  VFIO (device passthrough)  |  dma-buf  |  userspace |
  +------------------------------------------------------+
                          |
  +------------------------------------------------------+
  |            IOMMU Core (drivers/iommu/iommu.c)        |
  |   - iommu_group, iommu_domain                        |
  |   - DMA-IOMMU API (dma-iommu.c)                     |
  |   - iova allocator (iova.c)                          |
  |   - sysfs /sys/class/iommu/                          |
  +--------------------+-----------------+---------------+
                       |                 |
  +--------------------+  +--------------+  +------------+
  | arm-smmu.c         |  | arm-smmu-v3.c|  |intel-iommu |
  | (SMMUv1/v2 backend)|  | (SMMUv3)     |  | (VT-d)     |
  +--------------------+  +--------------+  +------------+
                       |                 |
  +--------------------+-----------------+-----------+
  | io-pgtable-arm.c  (ARM_64_LPAE_S1 / S2)         |
  | io-pgtable-arm-v7s.c (ARM_V7S for 32-bit)       |
  +--------------------------------------------------+
## 5.2 Key Data Structures
/* iommu_domain: one isolated address space (one per device/group) */
struct iommu_domain {
    unsigned int type;           /* IOMMU_DOMAIN_UNMANAGED,     */
                                 /* IOMMU_DOMAIN_DMA,           */
                                 /* IOMMU_DOMAIN_IDENTITY,      */
                                 /* IOMMU_DOMAIN_BLOCKED        */
    const struct iommu_ops *ops; /* backend driver ops          */
    unsigned long pgsize_bitmap; /* supported page sizes        */
    iommu_fault_handler_t handler;
    void *handler_token;
    struct iommu_domain_geometry geometry; /* IOVA aperture */
};

/* iommu_group: set of devices sharing an IOMMU context */
/* (PCIe functions in same ACS group must share a domain) */
struct iommu_group {
    struct list_head devices;     /* struct group_device list  */
    struct iommu_domain *domain;  /* current domain            */
    char *name;                   /* e.g., "usb-controller"    */
};

/* iommu_ops: implemented by arm-smmu.c / arm-smmu-v3.c */
struct iommu_ops {
    /* Attach device to domain (program SMMU context bank) */
    int (*attach_dev)(struct iommu_domain *domain,
                      struct device *dev);
    /* Map IOVA -> PA (add PTE to SMMU page table) */
    int (*map)(struct iommu_domain *domain, unsigned long iova,
               phys_addr_t paddr, size_t size, int prot,
               gfp_t gfp);
    /* Unmap IOVA (remove PTE) */
    size_t (*unmap)(struct iommu_domain *domain, unsigned long iova,
                    size_t size, struct iommu_iotlb_gather *gather);
    /* Translate IOVA -> PA (debug/verify) */
    phys_addr_t (*iova_to_phys)(struct iommu_domain *domain,
                                 dma_addr_t iova);
    /* Probe bus for SMMU-attached devices */
    struct iommu_group *(*device_group)(struct device *dev);
};
## 5.3 IO Page Table Formats
The kernel's io-pgtable-arm.c implements ARM page table allocation/management for the SMMU, using the same format as the CPU MMU:
| Format Enum | Usage | Description |
| ARM_32_LPAE_S1 | SMMUv2 32-bit | Stage 1, 32-bit LPAE (ARMv7 LPAE) |
| ARM_32_LPAE_S2 | SMMUv2 32-bit virt | Stage 2, 32-bit LPAE |
| ARM_64_LPAE_S1 | SMMUv2/v3 (most common) | Stage 1, 64-bit (AArch64 format, 4K/16K/64K granule) |
| ARM_64_LPAE_S2 | SMMUv3 hypervisor | Stage 2, 64-bit -- IPA to PA translation |
| ARM_MALI_LPAE | ARM Mali GPU | Mali-specific LPAE variant |

## 5.4 Device Tree Binding — Qualcomm SMMU
/* SMMU instance declaration */
apps_smmu: iommu@15000000 {
    compatible = "qcom,sm8550-smmu-500", "arm,mmu-500";
    reg = <0x15000000 0x100000>;   /* MMIO base + size */
    #iommu-cells = <2>;             /* cells: <SID mask> */
    #global-interrupts = <2>;       /* global fault IRQs */
    interrupts =
        <GIC_SPI 65 IRQ_TYPE_LEVEL_HIGH>,  /* global fault 0 */
        <GIC_SPI 97 IRQ_TYPE_LEVEL_HIGH>,  /* global fault 1 */
        /* ... one IRQ per context bank ... */
        <GIC_SPI 100 IRQ_TYPE_LEVEL_HIGH>; /* CB0 fault     */
    clocks = <&gcc GCC_SMMU_500_AXI_CLK>,
             <&gcc GCC_AGGRE_USB_NOC_AXI_CLK>;
};

/* USB Controller using SMMU */
usb_controller@a600000 {
    compatible = "qcom,dwc3";
    reg = <0xa600000 0x10000>;
    iommus = <&apps_smmu 0x740 0x0>;  /* SID=0x740, mask=0x0 */
    dma-coherent;   /* CCI-backed: no explicit cache maintenance */
};

/* PCIe with range of Stream IDs (multiple PCIe functions) */
pcie@1c00000 {
    compatible = "qcom,pcie-sm8550";
    iommus = <&apps_smmu 0x1c00 0x1f>; /* SID base=0x1c00, mask=0x1f */
    /* mask=0x1f allows SID 0x1c00..0x1c1f (32 PCIe functions) */
};
# Section 6: DMA With and Without SMMU
## 6.1 DMA Without SMMU (Bypass / Identity Mode)
When SMMU is bypassed or configured in identity mode, device DMA addresses map 1:1 to physical addresses:

/* Standard DMA allocation -- no SMMU */
dma_addr_t dma_handle;
void *buf = dma_alloc_coherent(dev, 4096, &dma_handle, GFP_KERNEL);
/* dma_handle == PHYSICAL ADDRESS directly */
/* Device programs dma_handle into its DMA_ADDR register */
writel(dma_handle, device_base + DMA_ADDR_REG);
writel(4096,       device_base + DMA_LEN_REG);
writel(1,          device_base + DMA_START_REG);
/* RISK: Device can DMA to ANY physical address if given wrong addr! */

⚠️  Identity/bypass mode is used when: (1) No SMMU IP is present in the SoC, (2) SMMU is present but the device is trusted and bypassed by DT iommus property being absent, (3) SMMU is in passthrough mode via smmu.passthrough kernel param.
## 6.2 DMA With SMMU (Translation Active)
When an SMMU context bank is active, dma_alloc_coherent() goes through the IOMMU DMA layer:

/* DMA allocation WITH SMMU (arm-smmu + dma-iommu integration) */
dma_addr_t dma_handle;   /* This will be an IOVA */
void *buf = dma_alloc_coherent(dev, 4096, &dma_handle, GFP_KERNEL);

/* Under the hood (dma-iommu.c): */
/* 1. iommu_dma_alloc():                                            */
/*    a. alloc_pages() --> physical page (PA)                       */
/*    b. iova_alloc() --> find free IOVA in domain's IOVA space     */
/*    c. iommu_map(domain, iova, pa, size, prot)                    */
/*       --> arm_smmu_map() --> io_pgtable_ops->map()               */
/*       --> writes PTE: iova -> pa in SMMU page table              */
/*    d. Returns IOVA to caller as dma_handle                       */

/* Device DMA flow: */
writel(dma_handle, device_base + DMA_ADDR_REG);  /* IOVA */
/* Device issues DMA to IOVA                                        */
/* SMMU intercepts, looks up page table: IOVA -> PA                */
/* Device DMA reaches correct physical memory                       */
/* If device tries to access IOVA not in table: FAULT!             */
## 6.3 Streaming DMA (Map/Unmap)
For existing buffers (not specially allocated), use streaming DMA API:

/* Map existing buffer for DMA (SMMU maps IOVA -> existing PA) */
dma_addr_t dma_addr = dma_map_single(dev, cpu_addr, size,
                                       DMA_TO_DEVICE);
if (dma_mapping_error(dev, dma_addr))
    /* handle error */;

/* program device DMA registers with dma_addr (IOVA) */
/* ... device performs DMA ... */

/* Unmap after DMA completes (removes IOVA -> PA PTE) */
dma_unmap_single(dev, dma_addr, size, DMA_TO_DEVICE);

| Direction | Operation at map() | Operation at unmap() |
| DMA_TO_DEVICE | Cache clean (flush) CPU -> memory | Nothing (device only wrote; CPU cache may be stale but device sees final data) |
| DMA_FROM_DEVICE | Cache invalidate (ensure device writes visible) | Cache invalidate (remove stale CPU cache lines so CPU reads device-written data) |
| DMA_BIDIRECTIONAL | Cache clean + invalidate | Cache clean + invalidate |


⚠️  On dma-coherent platforms (hardware-managed cache coherency via CCI/DSU/CCN interconnect), cache maintenance is skipped by dma_map/unmap. The dma-coherent DT property on a device node signals this to the DMA framework.
## 6.4 Scatter-Gather DMA with SMMU
SMMU's key advantage: presenting physically non-contiguous pages as a contiguous IOVA range to the device:
/* sg_table has multiple physically-scattered pages */
struct sg_table sgt;
int nents = dma_map_sg(dev, sgt.sgl, sgt.nents, DMA_FROM_DEVICE);
/* SMMU maps: IOVA[0..4095] -> PA_page0    */
/*            IOVA[4096..8191] -> PA_page1  (may be non-adjacent) */
/*            IOVA[8192..12287] -> PA_page2 (may be anywhere)    */
/* Device DMAs to a contiguous IOVA range: transparent!         */
# Section 7: SMMU Fault Handling
## 7.1 SMMUv2 Fault Flow
When a device DMA hits an unmapped IOVA or a permission violation:
  Device issues DMA to IOVA (e.g., 0xdeadbeef000)
         |
         v
  SMMU page table walk: IOVA not found (translation fault)
  OR IOVA found but permissions wrong (permission fault)
  OR device sent wrong stream ID (stream match fault)
         |
         v
  SMMU writes fault info to Context Bank registers:
    CB_FSR (Fault Status Register): fault type bits
    CB_FAR (Fault Address Register): the faulting IOVA
    CB_FSYNR0: WNR (write=1/read=0), SS (streamside), MULTI
         |
         v
  SMMU asserts Context Bank interrupt line to GIC
         |
         v
  Linux: arm_smmu_context_fault() (ISR)
    --> reads CB_FSR, CB_FAR, CB_FSYNR0
    --> logs: "Unhandled context fault: iova=0xdeadbeef000, fsynr=0x..."
    --> calls iommu_report_device_fault() -> report_iommu_fault()
    --> if no handler registered: DMA transaction is TERMINATED
## 7.2 Fault Types (CB_FSR Register)
| Fault Type | FSR Bit | Cause |
| Translation Fault | TF (bit 1) | IOVA has no page table mapping (most common -- buggy driver not mapping buffer) |
| Access Flag Fault | AFF (bit 2) | PTE Access Flag (AF) is 0 -- page table walk completed but AF not set |
| Permission Fault | PF (bit 3) | IOVA is mapped but device tried write on read-only mapping (AP bits violation) |
| External Fault | EF (bit 4) | Page table walk returned an abort from memory system (bus error) |
| Transaction Size Fault | SS (bit 30) | Fault occurred on Streamside (device DMA), not tablewalk side |
| Multi Fault | MULTI (bit 31) | Multiple faults occurred before FSR was cleared |

## 7.3 SMMUv3 STALL Fault Handling
SMMUv3 introduces a more sophisticated fault model via the Event Queue:
  Device DMA to unmapped IOVA (SVA use case: page not yet faulted in)
         |
         v
  SMMUv3: STALL event written to EVTQ (circular ring in DRAM)
  Device transaction is STALLED (not terminated yet)
         |
         v
  Linux SMMU v3 driver: arm_smmu_evtq_thread() (threaded IRQ)
    --> reads EVTQ event: STALL_EVENT record
    --> calls iommu_report_device_fault()
    --> iommu_page_response() -> SMMU CMD: RESUME (RETRY or ABORT)
         |
  If SVA domain: OS can handle the fault like a CPU page fault
    --> fault handler maps the missing page
    --> sends RESUME(RETRY) command via CMDQ
    --> DMA completes successfully
## 7.4 Debugging SMMU Faults on Qualcomm
# 1. Check dmesg for context fault messages
dmesg | grep -iE "smmu|iommu|context fault|arm-smmu"

# Typical fault message:
# arm-smmu-v2 15000000.iommu: Unhandled context fault:
#   iova=0x0000001234000, fsr=0x402, fsynr=0x1, cbndx=3, sid=0x740
# --> cbndx=3: context bank 3 (USB controller)
# --> sid=0x740: USB DWC3 stream ID
# --> fsr=0x402: TF (translation fault) + SS
# --> iova=0x1234000: the address the device tried to DMA to

# 2. IOMMU debug filesystem
ls /sys/kernel/debug/iommu/
cat /sys/kernel/debug/iommu/devices   # all SMMU-attached devices

# 3. Per-device domain info
cat /sys/bus/platform/devices/a600000.usb/iommu_group/*/type

# 4. Test translation (debugfs)
echo "a600000.usb 0x1234000" > /sys/kernel/debug/iommu/iova_to_phys

# 5. Enable IOMMU fault printout (kernel boot args)
# Add: iommu.strict=0 arm-smmu.disable_bypass=1 to cmdline
# Section 8: Qualcomm-Specific SMMU Details
## 8.1 ARM MMU-500 in Qualcomm SoCs
Qualcomm integrates ARM's MMU-500 IP core as their SMMUv2 implementation. Key Qualcomm-specific details:
| Detail | Description |
| Multiple SMMU Instances | apps_smmu: HLOS devices (USB, PCIe, camera, display) | gpu_smmu/kgsl_smmu: Adreno GPU | ipa_smmu: IPA (Internet Packet Accelerator) | qseecom_smmu: QSEE/TZ secure access | camss_smmu: camera IOMMU domain |
| Context Banks per Instance | Typically 64 to 128 Context Banks per apps_smmu instance. GPU SMMU may have fewer dedicated CBs. |
| Secure vs Non-Secure CBs | TrustZone (QSEE) reserves certain Context Banks for secure peripherals. Non-secure Linux driver cannot access secure CBs. Camera secure buffers (for DRM protected content) use TZ-configured CBs. |
| SMMU Proxy (SCM calls) | On some Qualcomm platforms, programming secure SMMU context banks requires an SMC call to TZ via qcom_scm_assign_mem(). This prevents normal world from re-mapping secure camera or DRM buffers. |
| SID Namespacing | Each Qualcomm SoC TRM defines the SID assignment for every IP block. E.g., SM8550: USB=0x740, PCIe0=0x1C00-0x1C1F, GPU=0x1800, CAMSS=0x800-0x87F |
| apps_smmu Upstream Driver | drivers/iommu/arm/arm-smmu-v3/ (SMMUv3 platforms) or drivers/iommu/arm/arm-smmu/ (SMMUv2/MMU-500). Qualcomm-specific quirks handled via qcom,sm8550-smmu-500 compatible string in arm-smmu.c |

## 8.2 SMMU + TrustZone Interaction
TrustZone and SMMU work together to enforce memory isolation between secure and non-secure worlds:
Normal World (HLOS)                Secure World (QSEE)
-------------------                ------------------
Linux SMMU driver                  TZ SMMU driver
  |  programs non-secure CBs         |  programs secure CBs
  |  (apps_smmu non-secure range)     |  (apps_smmu secure range)
  |                                   |
  v                                   v
+--------------------------------------------------+
|  apps_smmu (ARM MMU-500)                         |
|  Non-secure CBs: Linux-managed                   |
|  Secure CBs:     TZ-managed (NS bit=0)           |
|                                                   |
|  SCR.NS controls which CBs are accessible from   |
|  normal world                                     |
+--------------------------------------------------+
                    |
          Physical RAM
    [Normal memory] [Secure memory: QSEE-protected,
                     Linux cannot map via SMMU]
## 8.3 SMMU + Virtualization (Two-Stage Translation)
Qualcomm's hypervisor (QHEE — Qualcomm Hypervisor Execution Environment) leverages SMMU Stage 2 translation to provide secure device passthrough to VMs:
Guest Linux (VM at EL1)              QHEE Hypervisor (EL2)
------------------------             -------------------
  Guest SMMU driver                 Configures Stage 2
  sets up Stage 1:                  page tables for each VM:
  IOVA -> GPA (IPA)                 GPA -> HPA
         |                                  |
         v                                  |
  Device DMA (IOVA)                         |
         |                                  |
         v Stage 1 translate               |
  IPA (Intermediate Physical Addr)          |
         |                                  |
         v Stage 2 translate (EL2 SMMU)    |
  HPA (Host Physical Address) <------------+
         |
  Physical RAM (DRAM)

This allows a VM to have a PCIe NIC or GPU device directly passed through while the hypervisor retains full control via Stage 2 translation. The VM cannot access HPA addresses not mapped by QHEE.
## 8.4 SMMU in Qualcomm Camera Pipeline
Qualcomm's camera subsystem (CAMSS) uses SMMU extensively:
- CSI (Camera Serial Interface) DMA: raw sensor data DMA'd to IOVA-mapped buffers
- ISP (Image Signal Processor): processes raw frames, outputs to IOVA-mapped output buffers
- Secure camera mode: TZ programs a separate secure CB; encrypted video frames go to secure heap
- V4L2/videobuf2 framework: calls dma_buf_map() which invokes SMMU map() for camera buffers
- Driver: drivers/media/platform/qcom/camss/ + iommu domain creation per pipeline
# Section 9: SMMU vs CPU MMU — Comparison & Interview Chain Questions
## 9.1 SMMU vs CPU MMU — Complete Comparison
| Aspect | CPU MMU | SMMU (IOMMU) |
| Translates for | CPU instruction fetches and data accesses | Device-initiated DMA transactions |
| Address input | CPU Virtual Address (VA) | IOVA (I/O Virtual Address) |
| Address output | Physical Address (PA) | Physical Address (PA) or IPA in Stage 2 |
| Config register | TTBR0_EL1, TTBR1_EL1 (per-process) | CB TTBR0 (SMMUv2) or CD.TTBR0 (SMMUv3) |
| Context switch | On process switch (change TTBR0/ASID) | On device attach/detach to domain |
| Page table format | ARM LPAE / AArch64 (same format as SMMU) | Same ARM LPAE / AArch64 format -- reuses io-pgtable-arm.c |
| TLB | Per-core ITLB/DTLB + L2 TLB | SMMU-internal TLB (separate from CPU TLBs) |
| Fault mechanism | Synchronous exception to EL1 (do_page_fault) | Asynchronous IRQ to CPU (arm_smmu_context_fault) |
| ASID equivalent | ASID (process identifier for TLB tagging) | Stream ID (device identifier) + ASID in CD |
| Shareability | IS (Inner Shareable) TLB invalidation | SMMU TLB invalidation via SMMU registers / CMDQ |
| Virtualization | Stage 2 at EL2 (VTTBR_EL2) | Stage 2 in SMMU Context Bank or STE VMID |

## 9.2 Complete Interview Chain Questions
➡️  Q: What is the difference between IOMMU and SMMU?
IOMMU is the generic architectural concept of an MMU for I/O devices. SMMU is ARM's specific implementation. Intel uses VT-d, AMD uses AMD-Vi. On Qualcomm SoCs, the SMMU is ARM MMU-500 (SMMUv2) or SMMUv3 IP. In Linux, all are managed through the common IOMMU subsystem (drivers/iommu/iommu.c) with architecture-specific backends.
➡️  Q: What happens if a device DMAs to an unmapped IOVA?
If the SMMU is in translation mode: The SMMU page table walk fails. SMMU raises a Translation Fault. SMMUv2: raises a per-context-bank interrupt; Linux logs "Unhandled context fault" and terminates the DMA. SMMUv3 with STALL: may stall the transaction and raise a STALL event for OS resolution. If SMMU is in bypass mode: The DMA goes directly to the physical address — no protection.
➡️  Q: How does VFIO use SMMU for device passthrough to userspace/VMs?
- VFIO (Virtual Function I/O) enables userspace or VMs to directly control a device:
- Bind device to vfio-pci driver: removes it from host kernel driver
- VFIO creates a new IOMMU domain (isolated address space) for the device
- Userspace calls VFIO_IOMMU_MAP_DMA ioctl to map GPA -> HPA in SMMU
- SMMU ensures device can only DMA to explicitly mapped regions (secure passthrough)
- For VM passthrough (QHEE): SMMU Stage 2 maps GPA -> HPA; Stage 1 managed by guest
➡️  Q: What is the dma-coherent DT property and how does it relate to SMMU?
The dma-coherent DT property on a device node signals that the device's DMA is hardware-coherent with CPU caches (via interconnect fabric like ARM CCI-550 or DSU). Without it, the DMA framework performs explicit cache maintenance (DC CIVAC, DC CVAC) on dma_map/unmap. SMMU translation is orthogonal — you can have SMMU with or without hardware cache coherency.
➡️  Q: How does the kernel manage SMMU TLB invalidation?
SMMUv2: Register writes. E.g., TLBIALL (invalidate all), TLBIVMID (by VMID), TLBIASID (by ASID). Must be followed by TLBSYNC + TLBSTATUS poll.
SMMUv3: CMDQ commands: TLBI_NH_VA (by IOVA+ASID), TLBI_S12_VMALL (all Stage-1+2 for a VMID), followed by SYNC command with MSI completion.
Linux handles this in arm_smmu_iotlb_sync() called from iommu_unmap() after removing page table mappings.
➡️  Q: What is SVA (Shared Virtual Addressing) and why is it important?
SVA allows a DMA device to use the same virtual address space as a CPU userspace process. Instead of allocating separate IOVA ranges, the device's SMMU context descriptor points directly to the process's CPU page tables (same TTBR0). Importance:
- Eliminates IOVA management overhead (no separate IOMMU page table maintained)
- Device can DMA directly to/from userspace virtual addresses
- Critical for AI/ML accelerators (NPUs, GPUs, DSPs) that process userspace tensors
- Requires SMMUv3 with PASID/SubstreamID support + CPU SMMU driver SVA API
- Linux API: iommu_sva_bind_device(), iommu_sva_get_pasid()
➡️  Q: How does the Linux SMMU driver handle probe() and device initialization?
/* arm-smmu.c probe() (simplified) */
static int arm_smmu_device_probe(struct platform_device *pdev) {
    /* 1. Map SMMU MMIO registers */
    smmu->base = devm_ioremap_resource(&pdev->dev, res);

    /* 2. Read SMMU capabilities (IDR0, IDR1, IDR2 registers) */
    /*    Determine: num context banks, num stream IDs,     */
    /*    supported page sizes, ASID width, etc.           */
    arm_smmu_device_cfg_probe(smmu);

    /* 3. Initialize global SMMU state (SCR0, bypass config) */
    arm_smmu_device_reset(smmu);

    /* 4. Register with IOMMU core */
    iommu_device_register(&smmu->iommu, &arm_smmu_ops, NULL);

    /* 5. IOMMU core will call arm_smmu_probe_device() */
    /*    for each device with iommus = <&this_smmu ...>  */
}

/* Per-device: when USB controller driver calls dma_map_*()  */
/* -> iommu_attach_device() -> arm_smmu_attach_dev()        */
/*    - looks up SID from device's iommu-fwspec             */
/*    - allocates a Context Bank                            */
/*    - programs SMR/S2CR to route device SID to CB         */
/*    - programs CB_TTBR0 with domain's page table base     */
➡️  Q: What is an IOMMU group and why is it important for security?
An IOMMU group is the smallest set of devices that must share the same IOMMU domain (address space). For PCIe, this is determined by ACS (Access Control Services) capability — functions that can peer-to-peer DMA without going through the IOMMU must be in the same group. Importance:
- For VFIO passthrough: the entire group must be passed to the VM (not just one function)
- Prevents a device in the same group from attacking the passed-through device
- Platform devices (non-PCIe): each device is its own group (no peer-to-peer)
- Visible in sysfs: /sys/kernel/iommu_groups/
# Section 10: SYS Mode vs SVC Mode (ARMv7-A) — Deep Dive
| 🎯 Interview Tip: This is a classic ARMv7-A interview question at Qualcomm. Both SYS and SVC are privileged modes -- the key difference is in register banking and how they are entered. |

## 10.1 ARMv7-A Processor Mode Overview
ARMv7-A has 9 processor modes, each identified by the CPSR[4:0] mode bits. Eight modes are privileged; only USR (User) is unprivileged.
CPSR [4:0]  Mode   Description
-----------+------+------------------------------------------
  10000    | USR  | User mode -- unprivileged
  10001    | FIQ  | Fast Interrupt -- banked R8-R14, SPSR_fiq
  10010    | IRQ  | Interrupt -- banked R13, R14, SPSR_irq
  10011    | SVC  | Supervisor -- banked R13, R14, SPSR_svc  <-- kernel mode
  10110    | MON  | Monitor (TrustZone) -- banked R13, R14, SPSR_mon
  10111    | ABT  | Abort -- banked R13, R14, SPSR_abt
  11010    | HYP  | Hypervisor (Virtualization Ext.) -- banked R13, ELR_hyp
  11011    | UND  | Undefined -- banked R13, R14, SPSR_und
  11111    | SYS  | System -- NO banking, shares USR registers  <-- special!
## 10.2 SVC Mode — Supervisor Mode
### Entry and Purpose
| Property | Detail |
| CPSR mode bits | 10011 (0x13) |
| Entered by | SVC instruction (formerly SWI -- Software Interrupt) or processor Reset |
| Banked SP | YES -- SP_svc: separate stack pointer for kernel/SVC code |
| Banked LR | YES -- LR_svc (R14_svc): holds return address back to caller (user mode PC) |
| SPSR | YES -- SPSR_svc: saves CPSR at time of SVC (user mode state) |
| Privilege | Full privilege -- can execute MCR/MRC to CP15, MSR to CPSR |
| Typical use | Primary Linux kernel execution mode. System calls from user space land here. Also used for some exception paths. |

### SVC Exception Entry Flow
When a user-space application executes SVC #0 (the Linux syscall mechanism on ARMv7):
User mode (USR) executing SVC #0:

  1. CPU saves:  PC+4 --> LR_svc  (return address after SVC instruction)
  2. CPU saves:  CPSR --> SPSR_svc  (entire user mode state saved)
  3. CPU sets:   CPSR[4:0] = 10011  (enter SVC mode)
  4. CPU sets:   CPSR[7] = 1  (disable IRQ)
  5. CPU jumps:  to SVC vector (0x00000008 or 0xFFFF0008 if HIVECS)

  Linux vector_swi handler (arch/arm/kernel/entry-common.S):
  6. stmdb sp!, {r0-r12, lr}  -- push user registers + LR_svc to SVC stack
  7. Read SVC number from r7 (Linux ARM EABI convention)
  8. Call sys_call_table[r7]()
  9. Return: ldmia sp!, {r0-r12, pc}^  -- restore + SPSR_svc -> CPSR
## 10.3 SYS Mode — System Mode
### Entry and Purpose
| Property | Detail |
| CPSR mode bits | 11111 (0x1F) |
| Entered by | Software ONLY -- by writing mode bits: MSR CPSR_c, #0x1F |
| Banked SP | NO -- uses the SAME SP as USR mode (SP_usr) |
| Banked LR | NO -- uses the SAME LR as USR mode (LR_usr / R14_usr) |
| SPSR | NO SPSR -- cannot be entered by exception, so no need to save CPSR |
| Privilege | Full privilege -- same as SVC, can execute all privileged instructions |
| Typical use | Privileged code that needs direct access to user-mode SP and LR (e.g., context switch, signal frame setup) |

### Why SYS Mode Exists — The Key Insight
In SVC mode, if the kernel needs to access the user-space SP or LR, it must use banked register access instructions or mode-switching tricks:
@ In SVC mode -- to read user SP:
MRS r0, SP_usr      @ ARMv7 banked register read (requires ARMv7 Virtualization Ext.)
@ OR switch to SYS mode temporarily

@ In SYS mode -- SP IS the user SP, direct access:
@ No special instruction needed -- SP register directly holds user SP
MOV r1, sp          @ r1 = user stack pointer (direct!)
MOV r2, lr          @ r2 = user link register (direct!)

This is why Linux ARM uses SYS mode in parts of the exception return path and signal frame setup — when it needs to push the signal frame onto the user-mode stack, switching to SYS mode gives direct access to the user's SP.
## 10.4 Side-by-Side Comparison
| Feature | SVC Mode (Supervisor) | SYS Mode (System) |
| CPSR[4:0] mode bits | 10011 (0x13) | 11111 (0x1F) |
| Entered by | SVC instruction OR Reset (hardware exception) | Software only: MSR CPSR_c, #0x1F |
| Banked SP | YES -- SP_svc (kernel stack) | NO -- shares SP_usr (user stack) |
| Banked LR | YES -- LR_svc (return to user PC) | NO -- shares LR_usr (user link register) |
| SPSR | YES -- SPSR_svc (saves user CPSR) | NO SPSR (not exception entry mode) |
| Privilege level | Privileged | Privileged (same as SVC) |
| Register set | Own SP, LR; shared R0-R12 | Same as USR: R0-R14 all unbanked |
| Can be entered by exception | YES (SVC instruction / Reset) | NO -- only by software mode change |
| Typical Linux use | Syscall entry, primary kernel execution | Context switch user register save, signal frame setup |

## 10.5 Banked Register Architecture Diagram
ARMv7-A Register Banking:

         USR/SYS   FIQ       IRQ       SVC       ABT       UND       MON
  R0     R0                                                              (shared)
  R1     R1                                                              (shared)
  R2     R2                                                              (shared)
  ...    ...                                                             (shared)
  R7     R7                                                              (shared)
  R8     R8        R8_fiq
  R9     R9        R9_fiq
  R10    R10       R10_fiq
  R11    R11       R11_fiq
  R12    R12       R12_fiq
  SP     SP_usr    SP_fiq    SP_irq    SP_svc    SP_abt    SP_und    SP_mon
  LR     LR_usr    LR_fiq    LR_irq    LR_svc    LR_abt    LR_und    LR_mon
  PC     PC                                                              (shared)
  CPSR   CPSR                                                           (shared)
  SPSR   NONE      SPSR_fiq  SPSR_irq  SPSR_svc  SPSR_abt  SPSR_und  SPSR_mon

KEY: SYS mode (11111) uses USR register set -- NO separate SP_sys or LR_sys!
     SVC mode (10011) has its own SP_svc and LR_svc.
## 10.6 Interview Chain Questions — SYS vs SVC
➡️  Q: What happens if you execute an SVC instruction while already in SVC mode?
This is a classic trap question. When SVC executes in SVC mode:
- LR_svc gets OVERWRITTEN with the return address of the new SVC
- SPSR_svc gets OVERWRITTEN with the current CPSR (which is already SVC mode)
- The previous LR_svc (which was the return address to user space) is LOST
- Result: CANNOT return to user space! Stack corruption / hang

Fix: Before making any nested call in SVC mode, push LR_svc to the SVC stack:
@ Correct nested SVC handling:
vector_swi:
    stmdb sp!, {r0-r12, lr}  @ CRITICAL: save LR_svc FIRST
    bl    sys_call_handler    @ can now safely use SVC
    ldmia sp!, {r0-r12, pc}^  @ restore + return
➡️  Q: Can SYS mode be entered from an exception?
NO. SYS mode cannot be the target of any exception vector. The ARM exception model does not have a SYS vector. SYS mode can only be entered by explicitly modifying CPSR mode bits in software (MSR CPSR_c, #0x1F). This is a key architectural constraint — if you could enter SYS mode via exception, you would corrupt the user-mode SP and LR that SYS mode is supposed to transparently access.
➡️  Q: What is the only difference between SYS mode and USR mode?
Both SYS and USR modes use the identical register set (R0-R14, SP, LR — all unbanked). The ONLY difference is the privilege level:
- USR mode (10000): UNPRIVILEGED -- cannot execute MCR/MRC to CP15, cannot MSR to CPSR[4:0], restricted MMIO access
- SYS mode (11111): PRIVILEGED -- can execute all ARM instructions, access CP15 system registers, change mode bits

Memory protection (via domains / PTE AP bits) also restricts USR mode but not SYS mode. SYS mode can access kernel-only memory; USR mode would trigger a permission fault.
➡️  Q: In ARMv8-A, what replaces SVC mode and SYS mode?
ARMv8-A eliminates the mode-based model entirely in favor of Exception Levels (EL0-EL3):
| ARMv7-A Mode | ARMv8-A Equivalent | Notes |
| USR | EL0 (AArch64/AArch32) | Unprivileged applications |
| SVC | EL1 (OS kernel) | SVC instruction -> EL1 via ESR_EL1 EC=0x15 |
| SYS | Not needed in AArch64 | At EL1, read SP_EL0 directly (system register) or use SPSel=0 |
| HYP | EL2 (Hypervisor) | HVC instruction -> EL2 |
| MON | EL3 (Secure Monitor) | SMC instruction -> EL3 (TrustZone) |
| FIQ/IRQ/ABT/UND | EL1 (unified exception model) | All exceptions go to EL1; ESR_EL1 distinguishes type |


SYS mode concept in AArch64: At EL1, to access the user SP, you can either:
- Use MSR SPSel, #0 to switch the stack pointer to SP_EL0 (user SP) — functionally equivalent to switching to SYS mode
- Read SP_EL0 directly as a system register: MRS x0, SP_EL0
- AArch64's per-EL SP architecture makes SYS mode's special "shared register" trick unnecessary
➡️  Q: What is SPSR and how does it differ between modes?
SPSR (Saved Program Status Register) saves the CPSR at the point of exception entry for a given mode. On exception entry to mode X:
SPSR_x = CPSR (entire CPSR: N,Z,C,V flags + mode bits + I,F bits + T bit)
LR_x   = PC of interrupted instruction (+ offset depending on exception type)

On exception return (MOVS PC, LR or LDMIA sp!, {pc}^):
CPSR = SPSR_x  (restore pre-exception state)
PC   = LR_x    (return to interrupted code)

⚠️  SYS mode has NO SPSR because it can never be the target of an exception. SYS mode is always entered via explicit software modification, not via the exception mechanism.
➡️  Q: Explain the full system call path on Linux ARM (ARMv7) from user space to kernel
User space:
    MOV r7, #__NR_write     @ syscall number in r7
    MOV r0, #1              @ fd = stdout
    LDR r1, =buf_addr       @ buffer ptr
    MOV r2, #len            @ length
    SVC #0                  @ software interrupt -> SVC mode

CPU hardware (automatic):
    LR_svc = PC+4 of SVC instruction  (return address)
    SPSR_svc = CPSR  (saves USR mode state)
    CPSR[4:0] = 0x13  (SVC mode)
    CPSR[7]   = 1     (disable IRQ)
    PC = 0xFFFF0008   (SVC vector, HIVECS)

Linux vector_swi (arch/arm/kernel/entry-common.S):
    stmdb sp!, {r0-r12, lr}  @ save user regs + LR_svc on SVC stack
    ldr   r8, [r7, sys_call_table]  @ lookup syscall handler
    blx   r8                         @ call sys_write()
    str   r0, [sp, #S_R0]            @ store return value
    ldmia sp!, {r0-r12, pc}^         @ restore + ERET
    @ ^ means: restore CPSR from SPSR_svc (back to USR mode)
# Summary — Interview One-Liners
| Topic | One-Liner for Interviews |
| IOMMU vs SMMU | "IOMMU is the concept; SMMU is ARM's implementation. On Qualcomm, it's ARM MMU-500 (SMMUv2) or SMMUv3, providing per-device address space isolation via Stream IDs and Context Banks." |
| SMMUv2 vs SMMUv3 | "SMMUv2 uses registers + context banks. SMMUv3 uses DRAM-based stream tables + command queues, adds PASID/SVA support, STALL model, and ATS/PRI for PCIe." |
| DMA with SMMU | "dma_alloc_coherent returns an IOVA, not a PA. The SMMU maps IOVA->PA. Device is confined to only the mapped IOVA space -- any stray DMA causes a context fault." |
| SMMU Fault | "Translation fault = IOVA not mapped. Permission fault = wrong R/W. SMMUv2 raises per-CB IRQ; Linux logs iova + SID + fsr in arm_smmu_context_fault()." |
| SVC mode | "SVC is entered by the SVC instruction. It has banked SP_svc, LR_svc, and SPSR_svc. The kernel runs here. LR_svc must be pushed before nested SVC calls." |
| SYS mode | "SYS mode is privileged but shares USR register set (no banking). Only entered via software. Used when kernel needs direct access to user-mode SP/LR for signal frame setup or context switch." |
| SYS vs SVC | "Both are privileged. SVC has banked SP+LR+SPSR; SYS has none. SVC entered by SVC instruction; SYS only by MSR. SYS sees user-mode SP/LR directly -- that is its only advantage." |


⚠️  Continue to review: Linux arm-smmu.c and arm-smmu-v3.c source code in Linux kernel tree. Run "grep -r SMMU drivers/iommu/arm/" to trace the full call chain from dma_alloc_coherent() to SMMU page table setup.

— End of Part 3 —
Part 1: ARM Architecture & Memory Management  |  Part 2: Drivers, Bring-Up & Tools  |  Part 3: IOMMU/SMMU & ARM Modes

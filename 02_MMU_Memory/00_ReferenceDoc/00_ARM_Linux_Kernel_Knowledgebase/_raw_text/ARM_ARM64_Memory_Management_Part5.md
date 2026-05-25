| ARM/ARM64 Linux KernelMemory ManagementPart 5 — Final Reference GuideComplete Technical Reference for ARM/ARM64 Kernel Developers |


| Topics Covered in Part 5 |
| ARM64 MMU Translation• 4-level page tables (L0-L3)• Complete PTE walk with binary index extraction• PTE format: all bit fields (AP, XN, AF, AttrIndx)• MAIR_EL1 memory attributes• Huge pages, TLB with ASID• TLB maintenance instructions• Fault types with ESR_EL1 DFSC codes | ARM32 MMU + Boot + Interview• ARM32: 2-level tables, section/page descriptors• AP+APX, TEX+C+B, DACR domains• head.S walkthrough: __create_page_tables• __enable_mmu, identity map, paging_init• ARM64 vs ARM32 comparison table• Interview summary & one-liner recall table• Top 10 Q&A, key numbers, Qualcomm tips |


# 1. ARM64 MMU Translation — 4-Level Page Tables
ARM64 (AArch64) uses a 4-level page table hierarchy with 4 KB granule and 48-bit virtual addresses. The MMU translates virtual addresses to physical addresses using a hardware page table walker, accelerated by the TLB.
## 1.1 Virtual Address Breakdown (4 KB Granule, 48-bit VA)
With 4 KB pages and 48-bit virtual addresses, each 9-bit index selects one of 512 entries in a 4 KB table:
Virtual Address (48 bits):
+----------+---------+---------+---------+------------+
| [47:39]  | [38:30] | [29:21] | [20:12] |  [11:0]    |
| L0 index | L1 index| L2 index| L3 index|  offset    |
|  9 bits  |  9 bits |  9 bits |  9 bits |  12 bits   |
|  (512)   |  (512)  |  (512)  |  (512)  |  4096 B    |
+----------+---------+---------+---------+------------+

bits[63:48] = all 0 -> TTBR0_EL1 (user space)
bits[63:48] = all 1 -> TTBR1_EL1 (kernel space)
any other  -> Translation fault (non-canonical)
## 1.2 Page Table Levels
| Level | ARM64 | Linux | Covers | Entry Maps |
| 0 (L0) | L0 | PGD | 256 TB per entry | Points to L1 table |
| 1 (L1) | L1 | PUD | 512 GB per entry | Points to L2 or 1 GB block |
| 2 (L2) | L2 | PMD | 1 GB per entry | Points to L3 or 2 MB block |
| 3 (L3) | L3 | PTE | 2 MB per entry | Points to 4 KB page |

## 1.3 Complete PTE Walk Example with Binary Index Extraction
Example: Translating kernel virtual address 0xFFFF800012345678
VA = 0xFFFF800012345678

Step 0: bits[63:48] = 0xFFFF -> all 1s -> use TTBR1_EL1 (kernel)
        TTBR1_EL1 = 0x80000000 (physical base of PGD)

Binary decomposition:
  VA bits[47:39] = L0 index = 0b100000000 = 256
  VA bits[38:30] = L1 index = 0b000000000 = 0
  VA bits[29:21] = L2 index = 0b000010010 = 18
  VA bits[20:12] = L3 index = 0b001101000 = 104
  VA bits[11:0]  = offset   = 0x678

Step 1: L0 (PGD) lookup
  PGD base = TTBR1_EL1 = 0x80000000
  Entry addr = 0x80000000 + (256 x 8) = 0x80000800
  Read entry = 0x0000_0000_8010_0003
  bits[1:0] = 0b11 -> Table descriptor
  bits[47:12] = 0x80100 -> L1 table @ 0x80100000

Step 2: L1 (PUD) lookup
  PUD base = 0x80100000
  Entry addr = 0x80100000 + (0 x 8) = 0x80100000
  Read entry = 0x0000_0000_8020_0003
  bits[1:0] = 0b11 -> Table descriptor -> L2 table @ 0x80200000

Step 3: L2 (PMD) lookup
  PMD base = 0x80200000
  Entry addr = 0x80200000 + (18 x 8) = 0x80200090
  Read entry = 0x0000_0000_8030_0003
  bits[1:0] = 0b11 -> Table descriptor -> L3 table @ 0x80300000
  (If bits[1:0]=01 -> 2 MB block, walk stops here)

Step 4: L3 (PTE) lookup
  PTE base = 0x80300000
  Entry addr = 0x80300000 + (104 x 8) = 0x80300340
  Read entry = 0x0060_0000_9000_0713
  bits[1:0] = 0b11 -> Page descriptor (valid)
  bits[47:12] = 0x90000 -> Physical page @ 0x90000000
  bit[10] AF = 1 -> accessed
  bits[9:8] SH = 0b11 -> Inner Shareable
  bits[7:6] AP = 0b01 -> EL1 RW, EL0 RO
  bits[4:2] AttrIndx = 0b100 -> MAIR_EL1[4] = MT_NORMAL

Step 5: Form Physical Address
  Physical page base = 0x90000000
  Page offset        = VA[11:0] = 0x678
  Physical Address   = 0x90000000 + 0x678 = 0x90000678  [DONE]
## 1.4 ARM64 PTE Format — All Bit Fields
Bits 63   54  53  52  51          12  11  10   9  8  7  6  5  4  2  1  0
+------+ +--++--++--++------------+ +--++--++--++--++--++--++--++----++--++--+
|Upper | |XN||PX||DB||Output Addr | |nG||AF||SH||AP||NS||  ||  ||Atr||  ||  |
|attrs | |  ||N ||M ||  [47:12]   | |  ||  ||  ||  ||  ||  ||  ||Idx||  ||  |
+------+ +--++--++--++------------+ +--++--++--++--++--++--++--++----++--++--+
                                                                          11=Page
                                                                          01=Block
                                                                          00=Invalid
| Bit(s) | Name | Description |
| [1:0] | Type | 0b11=Page, 0b01=Block descriptor, 0b00=Invalid |
| [4:2] | AttrIndx | Index into MAIR_EL1 (0-7). Selects memory type: Normal, Device, NC, WT |
| [6] | AP[1] | Access Permission. AP[2:1]: 00=EL1 RW only, 01=EL1 RW + EL0 RO, 11=EL0+EL1 RO |
| [7] | AP[2] | Access Permission extended. Combined with AP[1] for full permission matrix |
| [8] | NS | Non-Secure. 1=Non-Secure PA space, 0=Secure PA space |
| [9:8] | SH | Shareability: 0b00=Non-Shareable, 0b10=Outer, 0b11=Inner Shareable |
| [10] | AF | Access Flag. 0=first access triggers AF fault; set to 1 by HW or fault handler |
| [11] | nG | not Global. 1=TLB entry is ASID-tagged (user pages), 0=Global (kernel pages) |
| [51] | DBM | Dirty Bit Modifier. HW sets AP dirty bit on write (FEAT_HAFDBS) |
| [53] | PXN | Privileged Execute Never. 1=EL1 cannot execute this page |
| [54] | UXN/XN | Unprivileged Execute Never. 1=EL0 cannot execute this page |

## 1.5 AP/XN Bits — Access Permission Matrix
| AP[2] | AP[1] | EL0 (User) | EL1 (Kernel) | Use Case |
| 0 | 0 | No access | Read/Write | Kernel-only data |
| 0 | 1 | Read-only | Read/Write | Shared read-only user/kernel |
| 1 | 0 | No access | Read-only | Kernel read-only (COW source) |
| 1 | 1 | Read-only | Read-only | Shared read-only |

XN/PXN/UXN combinations:
Kernel data pages:  UXN=1, PXN=1  -> neither EL0 nor EL1 can execute
Kernel code pages:  UXN=1, PXN=0  -> EL1 can execute, EL0 cannot
User code pages:    UXN=0, PXN=1  -> EL0 can execute, EL1 cannot (W^X)
User data pages:    UXN=1, PXN=1  -> neither can execute
## 1.6 MAIR_EL1 — Memory Attribute Indirection Register
MAIR_EL1 holds 8 memory attribute slots (1 byte each). The PTE AttrIndx[2:0] field selects which slot. Linux ARM64 configuration:
MAIR_EL1 Linux ARM64 layout:
  Index 0: MT_DEVICE_nGnRnE (0x00) -> Strongly ordered MMIO (ioremap)
  Index 1: MT_DEVICE_nGnRE  (0x04) -> Device memory (PCIe MMIO)
  Index 2: MT_DEVICE_GRE    (0x0C) -> Write-combining (framebuffers)
  Index 3: MT_NORMAL_NC     (0x44) -> Non-cacheable normal memory
  Index 4: MT_NORMAL        (0xFF) -> WB/WA cached <- most RAM uses this
  Index 5: MT_NORMAL_WT     (0xBB) -> Write-through

Attribute byte encoding:
  0xFF = 0b11111111:
         bits[7:4] = outer = 0b1111 = WB WA cacheable
         bits[3:0] = inner = 0b1111 = WB WA cacheable
  0x00 = 0b00000000:
         Device nGnRnE = no gathering, no reorder, no early write ack
## 1.7 Huge Pages — Block Descriptors
ARM64 supports large mapping shortcuts by using block descriptors at L1 or L2, avoiding lower-level tables:
Normal 4 KB walk:   L0 -> L1 -> L2 -> L3 -> PA  (4 levels, 3 memory reads + PTE)
2 MB huge page:     L0 -> L1 -> L2[block] -> PA  (3 levels, walk stops at L2)
1 GB huge page:     L0 -> L1[block] -> PA  (2 levels, walk stops at L1)

Block descriptor bits[1:0] = 0b01 (vs 0b11 for table descriptor)

L2 block entry covers: 2 MB (2^21 bytes)
L1 block entry covers: 1 GB (2^30 bytes)

Benefits:
  - 1 TLB entry covers 512x more VA than a 4 KB TLB entry (2 MB case)
  - Fewer page table levels to traverse
  - Linux uses 2 MB blocks for kernel linear map
  - Transparent Huge Pages (THP) use 2 MB blocks for user processes
## 1.8 TLB (Translation Lookaside Buffer) with ASID
The TLB caches recent VA->PA translations. Without it, every memory access would require 4 physical memory reads (one per page table level).
TLB Entry structure:
+--------+--------------------------+------------------------+
|  ASID  |  Virtual Page Number     |  Physical Page Number  |
| (16-bit|  (VA bits[63:12])        |  (PA bits[47:12])      |
|  ARM64)|                          |  + attributes (AP,SH..)|
+--------+--------------------------+------------------------+
  0x0042   0xFFFF800012345           0x90000 + RW, cached
  0x0042   0x0000000000401           0xA1000 + RO, cached
  0x0017   0x0000000000401           0xB2000 + RW, cached <- different process!
ASID (Address Space Identifier) allows multiple processes to share the TLB simultaneously without flushing on context switch:
| Property | Detail |
| ARM64 ASID size | 16-bit (65536 values) with FEAT_ASID16 |
| ARM32 ASID size | 8-bit (256 values) via CONTEXTIDR register |
| Kernel pages | nG=0 (global) -> no ASID tag, visible to all ASIDs |
| User pages | nG=1 (not global) -> ASID-tagged, process-specific |
| Context switch | Only update TTBR0_EL1 + ASID; no TLB flush needed (old entries ignored) |
| ASID rollover | After 65536 processes: full TLB flush + reassign ASIDs |

// Linux ARM64 context switch (simplified arch/arm64/mm/context.c):
void cpu_switch_mm(pgd_t *pgd, struct mm_struct *mm) {
    unsigned long asid = ASID(mm);          // get or assign 16-bit ASID
    unsigned long ttbr0 = phys_to_ttbr(virt_to_phys(pgd));
    write_sysreg(ttbr0 | (asid << 48), ttbr0_el1);  // atomic write
    isb();                                  // sync pipeline
}
## 1.9 TLB Maintenance Instructions
The kernel must manually invalidate TLB entries when page table mappings change:
// When to flush TLB:
//  1. PTE changed (permissions, PA changed)    4. ASID rollover
//  2. Page unmapped (PTE cleared)              5. ioremap/iounmap
//  3. Process exit (all user mappings removed) 6. mprotect() syscall

// ARM64 TLB invalidation instructions:
TLBI VAAE1IS, Xn    // By VA, all ASIDs, Inner Shareable (Xn = VA >> 12)
TLBI VAE1IS, Xn     // By VA, specific ASID (Xn = ASID<<48 | VA>>12)
TLBI VMALLE1IS      // All EL1 entries, Inner Shareable (full flush)
TLBI ASIDE1IS, Xn   // All entries for specific ASID

// Always follow TLBI with DSB+ISB:
DSB ISH             // ensure TLBI completes before next access
ISB                 // flush pipeline

// Linux kernel API (arch/arm64/include/asm/tlbflush.h):
flush_tlb_page(vma, addr);              // single VA
flush_tlb_range(vma, start, end);       // VA range
flush_tlb_mm(mm);                       // entire user address space
flush_tlb_kernel_range(start, end);     // kernel mapping range

// TLB performance numbers:
L1 cache hit:     ~4 cycles
TLB hit:          ~1 cycle (parallel with L1 lookup)
TLB miss:         ~10-100 cycles (page table walk in cache)
TLB miss + fault: ~1000+ cycles (kernel handler)
Major fault:      ~millions of cycles (disk I/O)
## 1.10 Fault Types with ESR_EL1 DFSC Codes
When a page table walk fails, the CPU raises a synchronous exception. ESR_EL1 encodes the exact fault type:
ESR_EL1 (Exception Syndrome Register):

EC field [31:26] - Exception Class:
  0b100100 = Data Abort from EL0 (user memory access fault)
  0b100101 = Data Abort from EL1 (kernel memory access fault)
  0b100000 = Instruction Abort from EL0
  0b100001 = Instruction Abort from EL1

DFSC field [5:0] - Data Fault Status Code:
  0b000000 = Address size fault, L0
  0b000001 = Address size fault, L1
  0b000100 = Translation fault, L0  <- page not mapped
  0b000101 = Translation fault, L1
  0b000110 = Translation fault, L2
  0b000111 = Translation fault, L3  <- MOST COMMON (PTE not present)
  0b001001 = Access flag fault, L1
  0b001011 = Access flag fault, L3  <- AF=0, first access to page
  0b001101 = Permission fault, L1
  0b001111 = Permission fault, L3  <- write to read-only (CoW trigger)
  0b100001 = Alignment fault

FAR_EL1  = Faulting virtual address
ELR_EL1  = Faulting instruction address (exception return address)
| DFSC Code | Type | Kernel Action |
| 0b000111 | Translation fault L3 | do_page_fault() -> demand paging, swap in, CoW allocation |
| 0b001111 | Permission fault L3 | CoW: copy page, update PTE; or SIGSEGV if no write permission |
| 0b001011 | Access flag fault | Set AF=1 in PTE (first access), update LRU age, retry |
| 0b100001 | Alignment fault | SIGBUS (or handle with fix_exception on some paths) |
| N/A | No VMA found | do_bad_area() -> send SIGSEGV to process |

Linux ARM64 fault dispatch chain:
// arch/arm64/mm/fault.c
do_mem_abort(far, esr, regs)
  -> esr_to_fault_info(esr)     // decode EC + DFSC
  -> inf->fn(far, esr, regs)    // dispatch to handler
       do_translation_fault()   // demand paging, CoW, stack growth
       do_page_fault()          // calls handle_mm_fault()
         -> find_vma(mm, addr)  // look up VMA
         -> handle_pte_fault()  // allocate/swap/CoW
         -> update PTE          // set valid bit, physical page
         -> flush_tlb_page()    // invalidate stale TLB
         -> return to user      // re-execute faulting instruction
       do_bad_area()            // SIGSEGV (invalid access)
## 1.11 Key ARM64 Kernel Data Structures
// Process memory descriptor (mm_struct)
struct mm_struct {
    pgd_t *pgd;              // Physical addr of L0 page table (PGD)
    unsigned long mmap_base; // Start of mmap region
    struct vm_area_struct *mmap; // Linked list of VMAs
    atomic64_t context.id;   // ASID + generation counter
};

// Virtual Memory Area - describes a contiguous virtual region
struct vm_area_struct {
    unsigned long vm_start;  // Start virtual address
    unsigned long vm_end;    // End virtual address
    pgprot_t vm_page_prot;   // Page protection flags -> PTE bits
    unsigned long vm_flags;  // VM_READ|VM_WRITE|VM_EXEC|VM_SHARED
    struct file *vm_file;    // Backing file (NULL=anonymous/heap/stack)
};

// Page Table Entry types (ARM64, all 64-bit)
typedef u64 pgd_t;   // L0 entry
typedef u64 pud_t;   // L1 entry
typedef u64 pmd_t;   // L2 entry
typedef u64 pte_t;   // L3 entry

// PTE manipulation macros (arch/arm64/include/asm/pgtable.h)
pte_present(pte)     // is page mapped? checks valid bit
pte_write(pte)       // is page writable? checks AP bits
pte_dirty(pte)       // has page been written? (DBM/SW bit)
pte_young(pte)       // has page been accessed? checks AF bit
pte_mkwrite(pte)     // set writable (clear AP[2])
pte_mkdirty(pte)     // set dirty
pte_mkyoung(pte)     // set AF bit
pte_pfn(pte)         // extract page frame number
# 2. ARM32 MMU Translation — 2-Level Page Tables
ARM32 (ARMv7) uses a 2-level page table with a fundamentally different design from ARM64. The PGD covers the entire 4 GB virtual address space with 1 MB section mappings available as a fast-path optimization.
## 2.1 Virtual Address Breakdown (ARM32, 4 KB pages)
Virtual Address (32 bits):
+----------------+----------+------------+
|    [31:20]     |  [19:12] |   [11:0]   |
| L1 (PGD) index| L2 index |   offset   |
|   12 bits      |  8 bits  |  12 bits   |
| (4096 entries) |(256 ent.)|  4096 B    |
+----------------+----------+------------+

PGD size:  4096 entries x 4 bytes = 16 KB (must be 16 KB aligned!)
L2 size:   256 entries x 4 bytes = 1 KB

vs ARM64:  4096 x 9-bit levels x 8 bytes = 4 KB PGD
| Level | ARM32 Name | Linux | Entries | Entry Size | Table Size |
| 1 (L1) | First-level / PGD | PGD | 4096 | 4 bytes | 16 KB |
| 2 (L2) | Second-level / PTE | PTE | 256 | 4 bytes | 1 KB |

## 2.2 Section Descriptor vs Page Table Descriptor
ARM32 L1 entries can be either a 1 MB Section (huge page) or a pointer to an L2 page table:
L1 Section Descriptor (1 MB mapping):
  bits[1:0]   = 0b10    -> Section descriptor type
  bits[31:20] = Physical base address (1 MB aligned)
  Walk stops at L1 -> maps 1 MB directly
  VA bits[19:0] = offset within 1 MB section

L1 Coarse Page Table Descriptor (4 KB mapping):
  bits[1:0]   = 0b01    -> Coarse page table type
  bits[31:10] = L2 table base address (1 KB aligned)
  Walk continues to L2 table

L1 Invalid:
  bits[1:0]   = 0b00    -> Fault on access
## 2.3 Section Descriptor Bit Fields (ARM32)
| Bits | Name | Description |
| [31:20] | PA base | Physical base address (1 MB aligned) |
| [19] | NS | Non-Secure bit |
| [17] | nG | Not Global: 1=ASID-tagged (user), 0=global (kernel) |
| [16] | S | Shareable: 1=shared memory (coherent across cores) |
| [15] | APX | Access Permission Extension (see AP+APX table) |
| [14:12] | TEX | Type Extension: combined with C+B for memory type |
| [11:10] | AP | Access Permissions (2 bits, extended by APX) |
| [8:5] | Domain | Domain ID (0-15), checked against DACR |
| [4] | XN | Execute Never: 1=cannot execute from this section |
| [3] | C | Cacheable bit (combined with TEX+B) |
| [2] | B | Bufferable bit (combined with TEX+C) |
| [1:0] | Type | 0b10=Section, 0b01=Page table, 0b00=Invalid |

## 2.4 AP+APX — Access Permission Matrix (ARM32)
| APX | AP | Privileged (EL1/SVC) | Unprivileged (EL0/USR) | Use Case |
| 0 | 00 | No access | No access | Reserved / fault |
| 0 | 01 | Read/Write | No access | Kernel-only data |
| 0 | 10 | Read/Write | Read-only | Shared read-only |
| 0 | 11 | Read/Write | Read/Write | Normal user+kernel RW |
| 1 | 01 | Read-only | No access | Kernel read-only (const data) |
| 1 | 10 | Read-only | Read-only | Shared read-only all ELs |
| 1 | 11 | Read-only | Read-only | All read-only |

## 2.5 TEX+C+B — Memory Type Encoding (ARM32)
| TEX | C | B | Memory Type | Use Case |
| 000 | 0 | 0 | Strongly Ordered | MMIO registers (ioremap) |
| 000 | 0 | 1 | Shared Device | Peripheral I/O |
| 000 | 1 | 0 | Write-Through, no WA | Framebuffers (rare) |
| 000 | 1 | 1 | Write-Back, no WA | Normal RAM (old) |
| 001 | 0 | 0 | Non-cacheable Normal | Coherent DMA buffers |
| 001 | 1 | 1 | Write-Back, Write-Allocate | Normal RAM (standard) |
| 010 | 0 | 0 | Non-Shared Device | Non-shared peripheral |

## 2.6 DACR — Domain Access Control Register
ARM32 has a unique domain system (16 domains) absent in ARM64. Each domain gets 2-bit permission in DACR:
DACR bits per domain:
  00 = No access  -> fault on any access
  01 = Client     -> check page table AP bits normally
  10 = Reserved
  11 = Manager    -> BYPASS all permission checks!

Linux ARM32 domain assignment:
  Domain 0 = DOMAIN_KERNEL -> Manager (0b11)
    -> kernel bypasses AP permission checks entirely!
    -> this is why Linux ARM32 kernel can access any physical memory
  Domain 1 = DOMAIN_USER   -> Client (0b01)
    -> AP bits are enforced for user space accesses
  Domain 2 = DOMAIN_IO     -> Client (0b01)

// DACR is written at boot and context switch:
mcr p15, 0, r5, c3, c0, 0   // write DACR

Note: ARM64 removed domains entirely. All access controlled by AP bits.
## 2.7 ARM32 Walk Example — VA to PA
Translate VA = 0xC0012345 (kernel address, ARM32)

Step 0: TTBR0 = 0x80004000 (PGD physical base, 16 KB)

Step 1: Extract index bits
  0xC0012345 = 1100 0000 0000 0001 0010 0011 0100 0101
  bits[31:20] = 1100 0000 0000 = 0xC00 = 3072  <- L1 index
  bits[19:12] = 0001 0010     = 0x12  = 18     <- L2 index
  bits[11:0]  = 0011 0100 0101 = 0x345          <- page offset

Step 2: L1 (PGD) lookup
  Entry addr = 0x80004000 + (3072 x 4) = 0x80007000
  Read 4 bytes at 0x80007000 = 0x80100002
  bits[1:0] = 0b10 -> SECTION descriptor!
  bits[31:20] = 0x801 -> Physical base = 0x80100000
  -> 1 MB Section mapping, walk stops here!
  PA = 0x80100000 | bits[19:0] of VA
  PA = 0x80100000 | 0x12345 = 0x80112345  [DONE - 1 level!]

OR if L1 = Page Table descriptor (bits[1:0]=0b01):

Step 3: L2 (PTE) lookup
  L2 table base = L1 entry bits[31:10] = 0x80200000
  Entry addr = 0x80200000 + (18 x 4) = 0x80200048
  Read 4 bytes = 0x90000FF2
  bits[1:0] = 0b10 -> Small page (4 KB)
  bits[31:12] = 0x90000 -> Physical page @ 0x90000000
  PA = 0x90000000 | bits[11:0] = 0x90000000 | 0x345 = 0x90000345
# 3. ARM32 Boot Page Table Setup — head.S Walkthrough
At power-on or U-Boot handoff, the ARM32 CPU starts with MMU disabled, executing at physical addresses in SVC mode. The head.S assembly establishes the minimal page tables needed to enable the MMU safely.
## 3.1 Boot State at Kernel Entry (head.S)
// CPU state at stext (kernel entry point):
//   Mode: SVC (Supervisor) - equivalent to EL1 on ARM64
//   MMU:  OFF - all addresses are physical
//   Caches: OFF
//   PC:  physical address of _stext
//   r0:  0 (boot protocol)
//   r1:  machine type ID
//   r2:  physical address of ATAGs or Device Tree Blob

ENTRY(stext)
    safe_svcmode_maskall r9      // ensure SVC mode, IRQs/FIQs disabled
    mrc p15, 0, r9, c0, c0      // read MIDR (CPU ID) into r9

    bl  __lookup_processor_type  // identify CPU type
    movs r10, r5
    beq __error_p                // unknown CPU -> halt

    bl  __lookup_machine_type    // identify board/machine
    movs r8, r5
    beq __error_a                // unknown machine -> halt

    ldr r13, =__mmap_switched    // r13 = virtual address of C entry

    bl  __create_page_tables     // ** CREATE BOOT PAGE TABLES **
    b   __enable_mmu             // ** ENABLE MMU **
## 3.2 __create_page_tables — Building the Boot PGD
This function creates the minimum page tables needed to enable the MMU. It uses 1 MB section mappings for speed. The PGD is placed just below the kernel in physical memory.
__create_page_tables:
    // r4 = physical address of PGD (PHYS_OFFSET - 0x4000)
    // PGD placed just below kernel image in physical memory
    pgtbl   r4, r8              // compute PGD physical address

    // Step 1: Zero out entire 16 KB PGD
    mov r0, r4
    mov r3, #0
    add r6, r0, #PG_DIR_SIZE    // PG_DIR_SIZE = 0x4000 (16 KB)
1:  str r3, [r0], #4            // write 4 zeros at a time
    str r3, [r0], #4
    str r3, [r0], #4
    str r3, [r0], #4
    teq r0, r6
    bne 1b

    // Step 2: Load MMU flags for section mapping
    ldr r7, [r10, #PROCINFO_MM_MMUFLAGS]
    // r7 = PMD_TYPE_SECT | PMD_SECT_AP_WRITE | PMD_SECT_AP_READ
    //    | PMD_FLAGS_CACHED  (TEX=001, C=1, B=1 = WB/WA normal RAM)

    // Step 3: Create IDENTITY MAP (PA=VA for kernel image area)
    //   Purpose: keeps CPU alive immediately after MMU enable
    //   Because: PC has physical address at that moment
    mov r6, pc                  // r6 = current PC (physical)
    mov r6, r6, lsr #20         // r6 = bits[31:20] = L1 index
    orr r3, r7, r6, lsl #20     // r3 = section descriptor (PA | flags)
    str r3, [r4, r6, lsl #2]    // PGD[r6] = r3 (identity map entry)

    // Step 4: Create KERNEL VIRTUAL MAP (PA -> 0xC0000000+)
    //   This is the permanent mapping used by the kernel
    add r0, r4, #(KERNEL_START & 0xFF000000) >> 18
    // KERNEL_START = 0xC0000000 -> L1 index = 3072
    // r0 = address of PGD[3072]

    ldr r6, =KERNEL_START       // r6 = 0xC0000000 (first kernel VA)
    ldr r5, =KERNEL_END         // r5 = end of kernel image
    ldr r3, =PHYS_OFFSET        // r3 = 0x80000000 (physical RAM start)
    orr r3, r3, r7              // r3 = section descriptor (PA | flags)

2:  str r3, [r0], #4            // PGD[3072++] = section descriptor
    add r3, r3, #1 << 20        // advance to next physical MB
    cmp r6, r5                  // reached KERNEL_END?
    add r6, r6, #1 << 20        // advance to next virtual MB
    blo 2b                      // loop until kernel fully mapped

    // Step 5: Map page tables so kernel can modify them
    add r0, r4, #PAGE_OFFSET >> 18
    orr r6, r4, r7              // section descriptor for PGD itself
    str r6, [r0]

    mov pc, lr                  // return
## 3.3 PGD State After __create_page_tables
PGD (physical @ PHYS_OFFSET - 0x4000, e.g. 0x7FFFC000):

Index  VA Range              Entry Type    Physical Target
--------------------------------------------------------------
0      0x00000000-0x000FFFFF  0 (invalid)
...    ...
2048   0x80000000-0x800FFFFF  Section     0x80000000  <- IDENTITY MAP
2049   0x80100000-0x801FFFFF  Section     0x80100000  <- (PA=VA)
...    (covers entire kernel image at physical addresses)
....
3072   0xC0000000-0xC00FFFFF  Section     0x80000000  <- KERNEL VIRT MAP
3073   0xC0100000-0xC01FFFFF  Section     0x80100000  <- (VA=PA+0x40000000)
3074   0xC0200000-0xC02FFFFF  Section     0x80200000
...    (maps all physical RAM to 0xC0000000+)
4095   0xFFF00000-0xFFFFFFFF  0 (invalid)

NOTE: Two mappings for same physical memory:
  0x80000000 (identity)  -> PA 0x80000000  (temporary, for MMU enable)
  0xC0000000 (virtual)   -> PA 0x80000000  (permanent kernel mapping)
## 3.4 __enable_mmu — Turning On the MMU
__enable_mmu:
    // Step 1: Set Domain Access Control Register
    mov r5, #(domain_val(DOMAIN_USER,   DOMAIN_MANAGER) |
              domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) |
              domain_val(DOMAIN_IO,     DOMAIN_CLIENT))
    mcr p15, 0, r5, c3, c0, 0   // write DACR
    // Domain 0 (kernel) = Manager -> bypasses all AP checks
    // Domain 1 (user)   = Client  -> AP bits enforced

    // Step 2: Write PGD to TTBR0
    mcr p15, 0, r4, c2, c0, 0   // TTBR0 = PGD physical address

    // Step 3: Flush entire TLB (must be clean before enabling MMU)
    mcr p15, 0, r0, c8, c7, 0   // TLBIALL (invalidate all TLB)

    // Step 4: Enable MMU, I-cache, D-cache
    mrc p15, 0, r0, c1, c0, 0   // read SCTLR
    orr r0, r0, #CR_M            // set M bit = MMU ENABLE
    orr r0, r0, #CR_C            // set C bit = D-cache enable
    orr r0, r0, #CR_I            // set I bit = I-cache enable
    mcr p15, 0, r0, c1, c0, 0   // write SCTLR -> MMU IS NOW ON!

    //  *** CRITICAL MOMENT ***
    //  Next instruction executes with MMU ON
    //  PC still has PHYSICAL address (e.g., 0x80008004)
    //  Identity map: VA 0x80008004 -> PA 0x80008004  [CPU stays alive!]

    mov pc, r13                  // jump to __mmap_switched (VIRTUAL addr)
    //  Now PC = 0xC0008xxx  (virtual)
    //  MMU: VA 0xC0008xxx -> PA 0x80008xxx  [kernel map works!]
    //  Identity map no longer needed after this jump
## 3.5 Identity Map — The Critical Moment Explained
Timeline of events around MMU enable:

Time T0: PC = 0x80008000 (physical)
         CPU executing: mcr p15,0,r0,c1,c0,0  (enable MMU)

Time T1: MCR completes, MMU is NOW ON
         NEXT instruction: PC = 0x80008004 (still looks physical)
         MMU translation: VA 0x80008004
           -> Identity map: PGD[2048] = Section @ 0x80000000
           -> PA = 0x80000000 + 0x8004 = 0x80008004
           -> CPU fetches instruction at 0x80008004  [SUCCESS!]

Time T2: Execute: mov pc, r13  (r13 = 0xC0008xxx)
         PC jumps to VIRTUAL address 0xC0008xxx
         MMU translation: VA 0xC0008xxx
           -> Kernel map: PGD[3072] = Section @ 0x80000000
           -> PA = 0x80000000 + 0x8xxx = 0x80008xxx
           -> CPU fetches at 0x80008xxx  [SUCCESS!]

Time T3: Identity map removed in paging_init()
         PGD[2048] = 0 (invalid)
         TLB flushed
         Only virtual map at 0xC0000000+ remains
## 3.6 __mmap_switched — Post-MMU C Entry Point
__mmap_switched:
    // NOW running at virtual addresses (0xC0xxxxxx)!

    adr r3, __mmap_switched_data
    ldmia r3!, {r4, r5, r6, r7}

    // Clear BSS section (.bss is zeroed before C code runs)
    mov fp, #0
1:  cmp r6, r7                   // r6=__bss_start, r7=__bss_stop
    strcc fp, [r6], #4           // zero 4 bytes at a time
    bcc 1b

    // Set up initial kernel stack
    ldmia r3, {r4, r5, r6, sp}

    // Stash boot info for use by C code
    str r9, [r4]                 // processor_id = CPU ID (MIDR)
    str r1, [r5]                 // __machine_arch_type = machine type
    str r2, [r6]                 // __atags_pointer = ATAGs/DTB address

    b   start_kernel             // jump to C entry point!
## 3.7 paging_init() — Full Page Table Replacement
After start_kernel() is called, paging_init() replaces the coarse boot section maps with proper 4 KB PTE mappings:
// arch/arm/mm/mmu.c
void __init paging_init(const struct machine_desc *mdesc)
{
    build_mem_type_table();   // set up TEX/C/B memory type descriptors
    prepare_page_table();     // clear identity map + old boot mappings
    map_lowmem();             // map physical RAM to 0xC0000000+ with PTEs
    devicemaps_init(mdesc);   // map I/O regions (UART, interrupt controller)
    kmap_init();              // set up PKMAP area for highmem access
    bootmem_init();           // initialize bootmem allocator
}

// Call chain from power-on to paging_init:
stext (head.S)
  -> __create_page_tables   // 1 MB section maps
  -> __enable_mmu           // MMU ON
  -> __mmap_switched        // clear BSS, setup stack
  -> start_kernel()         // C entry
     -> setup_arch()
        -> setup_processor() // CPU specific init
        -> setup_machine_fdt() // parse Device Tree
        -> arm_memblock_init() // reserve memory regions
        -> paging_init()      // REPLACE section maps with PTEs
           -> map_lowmem()    // proper PTE mappings for all lowmem
     -> mm_init()
        -> mem_init()         // hand pages to buddy allocator
# 4. ARM64 vs ARM32 — Complete Comparison Table
A comprehensive side-by-side comparison of memory management architecture between ARM64 (AArch64/ARMv8+) and ARM32 (ARMv7):
| Aspect | ARM64 (AArch64 / ARMv8+) | ARM32 (ARMv7) |
| Virtual address size | 48-bit (256 TB per side) | 32-bit (4 GB total) |
| User address space | 0x0000_0000_0000_0000 to 0x0000_FFFF_FFFF_FFFF (128 TB) | 0x00000000 to 0xBFFFFFFF (3 GB, classic 3G/1G) |
| Kernel address space | 0xFFFF_0000_0000_0000 to 0xFFFF_FFFF_FFFF_FFFF (128 TB) | 0xC0000000 to 0xFFFFFFFF (1 GB) |
| Page table levels | 4 levels (L0/PGD, L1/PUD, L2/PMD, L3/PTE) | 2 levels (L1/PGD, L2/PTE) |
| PGD size | 4 KB (512 entries x 8 bytes) | 16 KB (4096 entries x 4 bytes) |
| PTE entry size | 8 bytes (64-bit) | 4 bytes (32-bit) |
| Index bits per level | 9 bits -> 512 entries per level | L1: 12 bits, L2: 8 bits |
| Huge page sizes | 2 MB (L2 block), 1 GB (L1 block) | 1 MB sections, 64 KB large pages |
| TTBR registers | TTBR0_EL1 (user) + TTBR1_EL1 (kernel) - always split | TTBR0 + TTBR1 (split only when TTBCR.N > 0) |
| ASID size | 16-bit (65,536 values, FEAT_ASID16) | 8-bit (256 values, via CONTEXTIDR) |
| Domains (DACR) | REMOVED - no domains in ARM64 | Yes: 16 domains, 2 bits each in DACR |
| Kernel domain | N/A - AP bits always enforced | Domain Manager (0b11) - bypasses AP checks |
| Memory type encoding | AttrIndx[2:0] -> MAIR_EL1 (8 slots) | TEX[2:0] + C + B bits directly in PTE |
| Access permissions | AP[2:1] bits in PTE (2 bits) | APX + AP[1:0] (3 bits total) |
| Execute Never | UXN (EL0) + PXN (EL1) per PTE | XN bit per section/page |
| Access Flag (AF) | AF bit in PTE, FEAT_HAFDBS for HW update | No explicit AF, MMU sets C bit on access (platform-specific) |
| Dirty tracking | DBM bit (FEAT_HAFDBS) | SW-managed (no HW dirty bit) |
| Fault registers | ESR_EL1 (syndrome), FAR_EL1 (addr), ELR_EL1 (return) | DFSR/DFAR (data), IFSR/IFAR (instruction) |
| TLB instructions | TLBI system instructions (e.g., TLBI VAAE1IS) | CP15 coprocessor (MCR p15,0,r0,c8,c7,0) |
| Cache maintenance | DC/IC system instructions (DC CIVAC, etc.) | CP15 coprocessor (MCR p15,0,r0,c7,...) |
| Boot page tables | 2 MB block mappings (head.S creates initial maps) | 1 MB section mappings (head.S __create_page_tables) |
| High memory | DOES NOT EXIST - all RAM directly mapped | ZONE_HIGHMEM above ~896 MB - needs kmap() |
| Linear map base | PAGE_OFFSET = 0xFFFF800000000000 | PAGE_OFFSET = 0xC0000000 |
| Key source files | arch/arm64/mm/mmu.c, fault.c, tlbflush.h | arch/arm/kernel/head.S, arch/arm/mm/mmu.c |

# 5. Interview Summary
## 5.1 One-Liner Recall Table
Quick reference for interview preparation — one or two sentence answers for each topic:
| Topic | Interview Answer (1-2 lines) |
| Kernel in every process | Mapped for fast syscalls/interrupts - no full address space switch needed, only privilege level changes. Protected by MMU AP/PXN bits. |
| Page Fault | MMU exception when VA not mapped or permission violated. Enables demand paging, CoW, lazy allocation - transparent to user; faulting instruction re-executes. |
| Low Memory | Permanently and directly mapped physical RAM (<~896 MB on 32-bit). virtual = physical + PAGE_OFFSET. Fast: __pa()/__va() are trivial. |
| High Memory | Physical RAM >896 MB on 32-bit that has no permanent VA. Needs kmap() (sleepable) or kmap_atomic() (interrupt-safe). ARM64: does not exist. |
| Memory Zones | ZONE_DMA (legacy DMA <16MB), ZONE_NORMAL (main workhorse), ZONE_HIGHMEM (32-bit only). GFP flags control which zone allocators use. |
| Buddy Allocator | Page-level, power-of-2 blocks. Splits on alloc, coalesces buddy on free. buddy_pfn = pfn XOR (1 << order). Prevents external fragmentation. |
| SLUB Allocator | Sub-page objects. Per-CPU freelist = zero-lock fast path. Freelist pointer stored inside free objects. Slow path: node partial list -> buddy. |
| vmalloc | Virtually contiguous, physically scattered. For large non-DMA buffers. Slower (per-page TLB entries). No DMA - use dma_alloc_coherent instead. |
| CMA | Reserved at boot, reused for movable pages. Evicted on demand for large DMA buffers. DT: reusable. Used for camera ISP, GPU, video codec on SoCs. |
| DMA Coherency | HW coherent (CCI/CMN snoops cache) vs non-coherent (SW must flush/invalidate). dma_alloc_coherent = always safe. Streaming DMA uses explicit direction. |
| SMMU/IOMMU | Translates device IOVA to PA. Provides per-device isolation. ARM SMMUv2/v3. Configured via DT iommus property. |
| MMU ARM64 | 4-level page table (L0-L3), 9 bits per level, 4 KB granule. TTBR0 (user) + TTBR1 (kernel). TLB tagged with ASID for no-flush context switch. |
| MMU ARM32 | 2-level: 16 KB PGD (4096 x 4B) + 1 KB L2. 1 MB section = fast single-level lookup. Domains (DACR): kernel = Manager bypasses AP. |
| ARM32 Boot | head.S: __create_page_tables (identity + virtual section maps) -> __enable_mmu (DACR + TTBR0 + TLBI + SCTLR.M=1) -> identity map saves PC -> paging_init(). |
| TLB | Caches VA->PA translations. ASID tags entries so context switch needs no flush (16-bit ARM64, 8-bit ARM32). TLBI VAE1IS = invalidate by VA. |

## 5.2 Top 10 Interview Questions & Structured Answers
Q1: Explain virtual-to-physical address translation on ARM64
Answer structure:
1. Check VA bits[63:48]: all 0 -> TTBR0_EL1 (user), all 1 -> TTBR1_EL1 (kernel)
2. TLB lookup first: hit = physical address in ~1 cycle
3. TLB miss -> hardware page table walk: 4 levels (L0->L1->L2->L3)
   Each level: 9-bit index into 512-entry table, 8 bytes per entry
4. PTE contains: physical page base [47:12], AP, AF, UXN, PXN, AttrIndx
5. Final PA = PTE output address + VA[11:0] offset
6. Block descriptors at L1 (1 GB) or L2 (2 MB) = huge pages, fewer TLB entries
7. AF=0 triggers access fault on first access; kernel sets AF=1 and retries
Q2: What happens when a process accesses unmapped memory?
1. MMU detects PTE valid bit = 0 -> raises synchronous Data Abort exception
2. CPU saves state: ESR_EL1 (DFSC=0b000111 = translation fault L3),
   FAR_EL1 (faulting VA), ELR_EL1 (faulting instruction)
3. Kernel handler do_page_fault() -> find_vma(mm, addr)
4. Valid VMA found:
   -> Anonymous (heap/stack): allocate zeroed page, update PTE
   -> File-backed (mmap): read from file, update PTE (major fault)
   -> CoW: AP=RO but write attempted: allocate new page, copy, update PTE
5. No VMA -> do_bad_area() -> SIGSEGV sent to process
6. After resolution: flush TLB entry, return to user -> faulting instruction re-executes
Q3: Explain the difference between kmalloc and vmalloc
| Property | kmalloc | vmalloc |
| Physical memory | Contiguous | Scattered (not contiguous) |
| DMA safe | YES | NO - do not use for DMA |
| Speed | Fast - no TLB setup | Slower - per-page TLB entries |
| Max size | ~4 MB (physically contiguous limit) | Limited by virtual address space |
| Use case | DMA buffers, small/medium objects | Large buffers, kernel modules, firmware |

Q4: How does the buddy allocator prevent fragmentation?
1. Maintains free lists per order (2^0 to 2^11 pages = 4 KB to 8 MB)
2. On alloc: finds smallest order >= request, splits larger blocks
   split order-2 [P0P1P2P3] -> two order-1 [P0P1]+[P2P3]
   split [P0P1] -> two order-0 [P0]+[P1], allocate [P0]
3. On free: checks if buddy is free (buddy_pfn = pfn XOR (1 << order))
   P0 freed, buddy P1 free -> merge -> [P0P1]
   [P0P1] freed, buddy [P2P3] free -> merge -> [P0P1P2P3]
4. This aggressive coalescing reconstructs large blocks automatically
5. Migration types (MOVABLE/UNMOVABLE) in separate freelists prevent
   unmovable pages from blocking coalescence of movable regions
Q5: How does DMA work on ARM64? Explain cache coherency.
Problem: CPU uses cache, DMA accesses DRAM directly -> stale data

Hardware Coherent SoC (Qualcomm with CCI/CMN):
  -> Interconnect snoops CPU caches automatically
  -> DT property: dma-coherent
  -> dma_alloc_coherent() needs no SW cache maintenance

Non-coherent SoC (must use DMA API):
  DMA_TO_DEVICE:   DC CIVAC (clean+invalidate) before DMA reads
  DMA_FROM_DEVICE: DC IVAC (invalidate) before CPU reads

APIs:
  dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL)
    -> always safe, may be non-cached mapping (MT_NORMAL_NC)
  dma_map_single(dev, ptr, size, DMA_TO_DEVICE)
    -> streaming, cached, explicit flush
  of_reserved_mem_device_init() + dma_alloc_coherent()
    -> uses CMA region for large DMA buffers on SoCs

SMMU translates device IOVA->PA, provides per-device isolation
Q6: What is CMA and why is it needed?
Problem: Camera ISP needs 33 MB contiguous DMA buffer at runtime.
  Compaction cannot guarantee this, especially under memory pressure.

CMA solution:
  1. Reserve region at boot time (DT: compatible="shared-dma-pool", reusable)
  2. During normal operation: kernel uses it for MOVABLE user pages (no waste)
  3. When driver needs it: kernel evicts movable pages to other locations
  4. Returns contiguous region to driver via dma_alloc_coherent()

Driver pattern:
  of_reserved_mem_device_init(&pdev->dev);   // bind to DT memory-region
  buf = dma_alloc_coherent(&pdev->dev, 33<<20, &dma_handle, GFP_KERNEL);
  // program dma_handle into ISP hardware registers

vs Carveout (no-map): permanently reserved, always available, wastes memory
vs Compaction: slow, not guaranteed, no reservation
Q7: Explain SLUB allocator fast path and why it needs no lock
Each CPU has a private active slab page + freelist:
  freelist = singly-linked list of free objects in the active page
  Next-free pointer stored INSIDE the free object at "offset" bytes

Fast path allocation (pseudo-code):
  object = cpu_slab->freelist;         // grab head
  cpu_slab->freelist = object->next;   // advance list
  return object;                       // DONE - zero locking!

Fast path free:
  object->next = cpu_slab->freelist;   // push to head
  cpu_slab->freelist = object;         // DONE - zero locking!

No lock needed because:
  - Per-CPU data: only current CPU accesses its own cpu_slab
  - Preemption is disabled during the critical section

Slow path (freelist empty): grab partial page from node list (spinlock)
Slowest path: alloc_pages() from buddy allocator (node lock)
Q8: How does ARM32 boot set up page tables?
1. head.S entry: SVC mode, MMU OFF, physical addresses
2. __create_page_tables:
   a. Zero 16 KB PGD (physical @ PHYS_OFFSET - 0x4000)
   b. Create IDENTITY MAP: PGD[physical_index] = Section @ same PA
      Purpose: next instruction after MMU enable still works (PC=physical)
   c. Create KERNEL VIRTUAL MAP: PGD[3072..] = Sections @ PHYS_OFFSET+n
      Covers entire kernel image at 0xC0000000+
   d. Uses 1 MB section descriptors (fast, 1 entry per MB)
3. __enable_mmu:
   a. Write DACR (kernel=Manager, user=Client)
   b. Write TTBR0 = PGD physical address
   c. TLBIALL (flush TLB)
   d. Set SCTLR.M=1 -> MMU IS ON
4. Identity map: VA 0x80008004 -> PA 0x80008004 -> CPU stays alive!
5. Jump to __mmap_switched (virtual address 0xC0xxxxxx)
6. paging_init() replaces sections with proper 4 KB PTEs + removes identity map
Q9: ARM32 vs ARM64 - top 5 most important differences
1. Page table levels: ARM32 = 2 levels (16 KB PGD), ARM64 = 4 levels (4 KB PGD)
2. ASID size: ARM32 = 8 bits (256), ARM64 = 16 bits (65536)
   -> ARM32 context switch more often triggers full TLB flush
3. Domains: ARM32 has DACR (kernel=Manager bypasses AP), ARM64 removed completely
4. Highmem: ARM32 has ZONE_HIGHMEM + kmap(), ARM64 has no highmem
5. Memory types: ARM32 uses TEX+C+B bits directly, ARM64 uses MAIR_EL1 indirection
Q10: When to use GFP_KERNEL vs GFP_ATOMIC? What goes wrong if you mix them?
GFP_KERNEL:
  - Process context ONLY (can sleep)
  - Allows memory reclaim, waits for pages to be freed
  - Use in: probe(), ioctl(), workqueue, thread context

GFP_ATOMIC:
  - Cannot sleep - interrupt handler, spinlock held, tasklet
  - Uses emergency memory reserves
  - May return NULL if reserves are low

CRITICAL BUG if you use GFP_KERNEL while holding a spinlock:
  1. kmalloc(GFP_KERNEL) tries to reclaim memory
  2. Memory reclaim tries to take the same spinlock
  3. DEADLOCK -> system hangs

Rule: if in doubt, check might_sleep() warning in debug kernel.
The kernel will warn on GFP_KERNEL in atomic context.
## 5.3 Key Numbers to Remember
| Item | Value |
| ARM64 page size (default) | 4 KB (4096 bytes) |
| ARM64 virtual address bits | 48-bit (128 TB per side) |
| ARM64 page table levels | 4 (L0/PGD, L1/PUD, L2/PMD, L3/PTE) |
| ARM64 entries per table | 512 (9 bits = 2^9) |
| ARM64 PTE entry size | 8 bytes (64-bit) |
| ARM64 ASID size | 16 bits (65,536 values) |
| ARM64 huge page: L2 block | 2 MB (2^21 bytes) |
| ARM64 huge page: L1 block | 1 GB (2^30 bytes) |
| ARM64 cache line size | 64 bytes (typical) |
| ARM64 kernel stack size | 16 KB per thread |
| ARM32 PGD size | 16 KB (4096 entries x 4 bytes) |
| ARM32 section size | 1 MB (2^20 bytes) |
| ARM32 L2 table size | 1 KB (256 entries x 4 bytes) |
| ARM32 ASID size | 8 bits (256 values, via CONTEXTIDR) |
| Buddy MAX_ORDER | 11 (2^11 = 2048 pages = 8 MB max allocation) |
| kmalloc practical max | ~4 MB (physically contiguous limit) |
| SLUB kmalloc sizes | 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 bytes |
| SLUB poison pattern (free) | 0x6b (use-after-free: you see 0x6b6b6b6b) |
| SLUB poison pattern (alloc) | 0x5a (before user write, with SLAB_POISON) |
| SLUB red zone pattern | 0xbb (overflow detection around object) |
| PAGE_OFFSET ARM64 | 0xFFFF800000000000 |
| PAGE_OFFSET ARM32 | 0xC0000000 |
| ZONE_NORMAL limit (ARM32) | ~896 MB physical (above = ZONE_HIGHMEM) |

## 5.4 Key Registers Reference
ARM64 (AArch64) critical registers:
| Register | Purpose and Key Details |
| TTBR0_EL1 | User space page table base register. bits[63:48] = ASID, bits[47:1] = PA of PGD |
| TTBR1_EL1 | Kernel space page table base register. Contains PA of kernel PGD. Never changes per-process. |
| TCR_EL1 | Translation Control Register. T0SZ/T1SZ (VA size), TG0/TG1 (page size), SH/IRGN/ORGN (cache attrs for walks) |
| MAIR_EL1 | Memory Attribute Indirection Register. 8 slots x 1 byte = memory types. PTE AttrIndx[2:0] indexes into this. |
| SCTLR_EL1 | System Control Register. M bit (bit 0) = MMU enable. C bit = D-cache. I bit = I-cache. |
| ESR_EL1 | Exception Syndrome Register. EC[31:26] = exception class. DFSC[5:0] = fault status code. |
| FAR_EL1 | Fault Address Register. Contains the virtual address that caused the fault. |
| ELR_EL1 | Exception Link Register. Contains the VA of the faulting instruction (return address). |

ARM32 (ARMv7) critical registers:
| Register | Purpose and Key Details |
| TTBR0 | User/combined page table base. Covers all VA when TTBCR.N=0 (Linux default) |
| TTBR1 | Kernel page table base (used when TTBCR.N > 0 to split VA space) |
| TTBCR | Translation Table Base Control. N[2:0] controls split between TTBR0/TTBR1 |
| DACR | Domain Access Control Register. 2 bits per domain: 00=no access, 01=client, 11=manager |
| SCTLR | System Control Register. M bit = MMU enable, C = D-cache, I = I-cache |
| CONTEXTIDR | Context ID Register. bits[7:0] = ASID (8-bit). Used for TLB tagging. |
| DFSR | Data Fault Status Register. bits[3:0] = fault type (translation, permission, etc) |
| DFAR | Data Fault Address Register. VA that caused the data abort. |
| IFSR/IFAR | Instruction Fault Status/Address. For prefetch aborts (code fetch faults) |

## 5.5 Qualcomm Interview Tips
| Focus Area | What to Say / How to Stand Out |
| SMMU/IOMMU | Always mention SMMU when DMA comes up. Qualcomm SoCs (Snapdragon) use SMMUv2/v3 extensively. Show DT binding knowledge: iommus = <&smmu 0x200>. |
| CMA for multimedia | Camera ISP, GPU, video codec all use CMA. Reference: of_reserved_mem_device_init() + dma_alloc_coherent(). Mention frame buffer sizes (4K RGBA = 33 MB). |
| Device Tree expertise | Show DT binding knowledge: reserved-memory, reusable vs no-map, iommus, dma-coherent. Qualcomm interviews always ask about DT. |
| Debugging skills | Mention KASAN, SLUB_DEBUG, /proc/buddyinfo, dmesg SMMU faults, CONFIG_DMA_API_DEBUG. Shows production experience. |
| Layered understanding | Always explain the full stack: hardware -> buddy -> SLUB -> kmalloc -> driver API -> DT binding. Qualcomm values system-level thinking. |
| Real use case examples | "In a camera ISP driver I would use dma_alloc_coherent with a CMA region from DT, program the dma_handle into ISP registers, and use dma_sync for ping-pong buffers." |
| ARM64 vs ARM32 | Qualcomm makes both (legacy Cortex-A5 ARM32 modems + Kryo ARM64 application cores). Know the differences cold - especially DACR, highmem, TLB flush frequency. |
| Performance mindset | Mention: SLAB_HWCACHE_ALIGN prevents false sharing, huge pages reduce TLB misses, per-CPU allocator eliminates spinlocks, ASID avoids TLB flushes on context switch. |

## 5.6 Key Kernel Source Files
| File | Purpose |
| arch/arm64/mm/mmu.c | ARM64 page table setup, map_kernel(), create_pgd_mapping() |
| arch/arm64/mm/fault.c | ARM64 fault handler: do_mem_abort(), do_page_fault() |
| arch/arm64/include/asm/pgtable.h | PTE manipulation macros: pte_present, pte_write, pte_mkwrite, pte_pfn |
| arch/arm64/include/asm/pgtable-hwdef.h | Hardware PTE bit definitions: PTE_AF, PTE_SH_*, PTE_AP_*, PTE_UXN |
| arch/arm64/include/asm/tlbflush.h | TLB maintenance: flush_tlb_page, flush_tlb_mm, __flush_tlb_range |
| arch/arm64/mm/context.c | ASID management: cpu_switch_mm(), check_and_switch_context() |
| arch/arm/kernel/head.S | ARM32 boot: stext, __create_page_tables, __enable_mmu, __mmap_switched |
| arch/arm/mm/mmu.c | ARM32: paging_init(), map_lowmem(), devicemaps_init() |
| mm/page_alloc.c | Buddy allocator: __alloc_pages(), expand(), __free_one_page() |
| mm/slub.c | SLUB allocator: kmem_cache_alloc(), kfree(), kmem_cache_create() |
| mm/cma.c | CMA: cma_alloc(), cma_release(), cma_init_reserved_mem() |
| drivers/iommu/arm/ | ARM SMMU driver: arm-smmu.c, arm-smmu-v3.c |
| kernel/dma/ | DMA API: dma_alloc_coherent(), dma_map_single(), dma_sync_* |
| arch/arm64/mm/dma-mapping.c | ARM64-specific DMA mapping: cache maintenance, SMMU integration |


| End of Part 5 — ARM/ARM64 Linux Kernel Memory Management Reference GuideParts 1-5 cover: Address Space Layout • Page Faults • Low/High Memory • Zones • Buddy • SLUB • CMA • DMA Coherency • SMMU • ARM64 MMU • ARM32 MMU • Boot Setup • Interview Q&A |


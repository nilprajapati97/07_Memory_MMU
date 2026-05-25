


ARM/ARM64 Linux Kernel
Memory Management Reference Guide
Part 1: Virtual Address Space, Page Faults,
Low/High Memory, and Memory Zones
|  |



ARM_ARM64_Memory_Management_Part1
Targeting: ARM32 (AArch32) and ARM64 (AArch64)
Linux Kernel Memory Management — Comprehensive Developer Reference


CONTENTS COVERED IN THIS PART:
1.  Kernel Virtual Address Space Layout (ARM32 — 3G/1G Split)
2.  Kernel Virtual Address Space Layout (ARM64 — 48-bit and 52-bit)
3.  Why Kernel Space Is Mapped Into Every Process
4.  Page Faults — Complete Mechanism with ARM64 Registers
5.  Demand Paging and Copy-on-Write (CoW) — Flow Diagrams + Code
6.  Low Memory and High Memory — Complete Developer Guide
7.  Memory Zones — Full Diagrams and GFP Flags Reference




# Table of Contents

# 1. Kernel Virtual Address Space Layout
Understanding how the kernel organizes virtual address space is the foundation of Linux memory management. ARM32 and ARM64 have fundamentally different layouts driven by their address space widths.
## 1.1 ARM32 — Classic 3G/1G Split
On a 32-bit ARM system, the CPU can address only 4 GB of virtual address space (2³² = 4,294,967,296 bytes). The kernel configures a split between user space and kernel space. The default —13G/1G” split allocates 3 GB to user processes and 1 GB to the kernel.
ARM32 Virtual Address Space — 3G/1G Split
Virtual Address Space (32-bit ARM, 3G/1G split)

┌─────────────────────────────────────────┐ 0xFFFFFFFF
│  FIXMAP   (~4 MB)                          │  kmap_atomic() slots
├─────────────────────────────────────────┤ 0xFFC00000
│  PKMAP    (~8 MB)                          │  kmap() persistent slots
├─────────────────────────────────────────┤ 0xFF800000
│                                           │
│  vmalloc area  (~112 MB)                  │  vmalloc(), ioremap()
│                                           │
├─────────────────────────────────────────┤ ~0xF8000000 (VMALLOC_START)
│                                           │
│  Kernel Direct Map / Lowmem  (~896 MB)    │  ZONE_DMA + ZONE_NORMAL
│  virtual = physical + PAGE_OFFSET         │  Always mapped
│  (0xC0000000 ⇒ physical 0x0)            │
│                                           │
└─────────────────────────────────────────┘ 0xC0000000  ← PAGE_OFFSET
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                  KERNEL / USER BOUNDARY
- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
┌─────────────────────────────────────────┐ 0xBFFFFFFF
│  Stack (grows downward)                   │
│  ...                                      │
│  mmap / shared libraries                  │  USER SPACE (3 GB)
│  Heap (grows upward)                      │
│  BSS / Data / Text                        │
└─────────────────────────────────────────┘ 0x00000000

ARM32 Kernel Space Regions
| Region | Virtual Range | Size | Purpose |
| Direct Map (Lowmem) | 0xC0000000 – 0xF7FFFFFF | ~896 MB | Kernel code, data, slab, page tables |
| vmalloc area | ~0xF8000000 – 0xFF7FFFFF | ~112 MB | vmalloc(), ioremap(), module space |
| PKMAP | 0xFF800000 – 0xFFBFFFFF | ~8 MB | kmap() — persistent highmem mappings |
| FIXMAP | 0xFFC00000 – 0xFFFFFFFF | ~4 MB | kmap_atomic(), compile-time fixed maps |


## 1.2 ARM64 (AArch64) — 48-bit and 52-bit Virtual Address Space
ARM64 dramatically expands the virtual address space. With 48-bit addressing (the default in most Linux configurations), each side of the address space gets 256 TB. With 52-bit VA (when supported), each side gets 4 PB. The kernel and user space are separated by a large ”canonical gap” — addresses in this gap are invalid and trigger a fault.
ARM64 uses two separate page table base registers:
- TTBR0_EL1 — holds the page table base for user space (low addresses starting at 0x0)
- TTBR1_EL1 — holds the page table base for kernel space (high addresses starting at 0xFFFF...)
ARM64 Virtual Address Space — 48-bit (256 TB each side)
ARM64 Virtual Address Space (48-bit VA, 4KB pages)

┌────────────────────────────────────────┐ 0xFFFFFFFFFFFFFFFF
│  Kernel virtual space (128 TB)            │
├────────────────────────────────────────┤ 0xFFFFFF8000000000  (MODULES_VADDR)
│  Kernel modules / BPF JIT                 │  128 MB
├────────────────────────────────────────┤ 0xFFFF000000000000  (PAGE_OFFSET)
│                                           │
│  Linear Map (Direct Physical Map)         │  All physical RAM directly mapped
│  Physical 0x0 ⇒ 0xFFFF000000000000      │  No highmem zone! All low memory.
│  Physical 0x1000 ⇒ 0xFFFF000000001000    │
│  ...                                      │
│  vmalloc area                             │  vmalloc(), ioremap()
│  FIXMAP area                              │  Fixed virtual addresses
└────────────────────────────────────────┘ 0xFFFF000000000000

  ~~~~~~~~~~~~~~~~~~ CANONICAL GAP ~~~~~~~~~~~~~~~~~~
  (invalid addresses — access causes SIGSEGV / SError)

┌────────────────────────────────────────┐ 0x0000FFFFFFFFFFFF
│  User space (128 TB)                      │
│                                           │  TTBR0_EL1 manages this range
│  Stack (grows downward)                   │
│  mmap / shared libraries                  │
│  Heap                                     │
│  BSS / Data / Text                        │
└────────────────────────────────────────┘ 0x0000000000000000

ARM64 Key Kernel Virtual Address Constants
| Constant / Region | Address (48-bit VA) | Description |
| PAGE_OFFSET | 0xFFFF000000000000 | Start of kernel linear map |
| KIMAGE_VADDR | ~0xFFFF800010000000 | Kernel text/data (KASLR randomized) |
| MODULES_VADDR | 0xFFFFFF8000000000 | Kernel modules and BPF JIT area (128 MB) |
| VMALLOC_START | ~0xFFFF000800000000 | Start of vmalloc/ioremap region |
| TTBR0_EL1 | (set per process) | User space page table root (EL0 accesses) |
| TTBR1_EL1 | (set once at boot, ASID-switched) | Kernel space page table root (EL1 accesses) |


Key Register: TCR_EL1 (Translation Control Register)
  T0SZ: size of user space address region (controls 0x0... range)
  T1SZ: size of kernel space address region (controls 0xFFFF... range)
  For 48-bit VA: T0SZ = T1SZ = 16 (64 - 48 = 16)
  For 52-bit VA: T0SZ = T1SZ = 12 (64 - 52 = 12)
  IPS: Intermediate Physical Address Size
  TG0/TG1: Translation Granule (4KB, 16KB, or 64KB pages)


# 2. Why Kernel Space Is Mapped Into Every Process
A common question when learning about Linux memory management is: if kernel space is supposed to be private and protected, why is it mapped into every process’s virtual address space at all? The answer lies in performance and the architecture of exception handling.
The short answer: it is not fully shared — it is mapped into the same virtual address space, but with strict hardware-enforced access controls.
## 2.1 Reason 1: Efficient System Calls (Syscall Performance)
When a user process makes a system call, the CPU switches from EL0 (user mode) to EL1 (kernel mode) on ARM64. If the kernel lived in a completely separate address space, the CPU would need to perform a full address space switch on every syscall:
- 1. Save current TTBR0_EL1 (user page tables)
- 2. Load kernel page tables into TTBR1_EL1
- 3. Full TLB flush (expensive!)
- 4. Execute syscall
- 5. Restore user page tables
- 6. Another full TLB flush
By keeping the kernel permanently mapped in the upper portion of every process’s virtual address space (via TTBR1_EL1 on ARM64), the transition only requires a privilege level change — no page table swap, no TLB flush for kernel pages.
## 2.2 Reason 2: Interrupt and Exception Handling
Hardware interrupts can occur at any instant during user process execution. The CPU must immediately vector to a kernel exception handler. The ARM64 exception vector table (VBAR_EL1) must be reachable without any address space reconfiguration. Since the kernel is already mapped at fixed high addresses via TTBR1_EL1, the CPU can jump directly to the handler.
## 2.3 Reason 3: Memory-Mapped I/O and Driver Access
Kernel device drivers map hardware registers into kernel virtual space using ioremap(). These mappings persist independently of which user process is currently executing. Since all processes share the same kernel mappings, drivers can always access their registers without per-process setup.
## 2.4 Why User Space Cannot Access Kernel Pages
Even though kernel pages are mapped in the process’s address space, the MMU enforces strict access control via page table entry attributes:
- AP (Access Permission) bits: control read/write access for EL0 vs EL1
- PXN (Privileged Execute-Never): prevents kernel pages from being executed at EL0
- UXN (Unprivileged Execute-Never): prevents user execution of pages with this bit set
- Any EL0 access to a kernel page triggers a Permission Fault → SIGSEGV to the process
## 2.5 KPTI: Kernel Page Table Isolation (Post-Meltdown)
After the Meltdown vulnerability discovered in 2018, Linux introduced KPTI to prevent speculative execution side-channel attacks. With KPTI enabled:
- In user mode (EL0): only a minimal stub of the kernel is mapped — just enough to handle syscall/interrupt entry and TTBR switch
- The full kernel mapping is present only in kernel mode (EL1)
- This trades some syscall performance for security
- On ARM64, implemented via the TTBR1_EL1 switch mechanism using ASID (Address Space Identifier) tagging
Summary Table
| Aspect | Reason |
| Kernel mapped in user process space | Fast syscall/interrupt handling without full address space switch |
| User cannot access kernel pages | MMU enforces privilege-level page protections (AP/PXN bits on ARM) |
| KPTI further restricts mapping | Mitigates Meltdown speculative execution side-channel attacks |
| TTBR0_EL1 vs TTBR1_EL1 | Separate page table bases for user and kernel enforced at hardware level |



# 3. Page Faults — Complete Mechanism
A page fault is an exception raised by the CPU’s Memory Management Unit (MMU) when a running process accesses a virtual address that either is not currently mapped in the page table, or is mapped but the access violates permissions (e.g., writing to a read-only page).
Page faults are not just error conditions — they are a deliberate and essential mechanism that enables many key OS features.
## 3.1 Why Page Faults Are Needed
- Demand Paging — Pages are only loaded into physical RAM when actually accessed, not all at once at process startup. This dramatically reduces startup time and memory usage.
- Lazy Allocation — malloc() reserves virtual address space but does not allocate physical pages until the program actually writes to them.
- Memory-Mapped Files — Files are mapped into virtual address space; actual disk reads happen only when the specific page is accessed.
- Swapping / Paging — Pages evicted to disk are marked "not present"; accessing them triggers a page fault that brings them back from swap.
- Copy-on-Write (CoW) — After fork(), parent and child share the same physical pages (marked read-only). A write triggers a fault, the kernel copies the page, and both get their own writable copy.
- Guard Pages / Stack Growth — Stack grows dynamically; a fault below the current stack pointer triggers the kernel to extend the stack mapping.
## 3.2 Types of Page Faults
| Type | Cause | Kernel Action |
| Minor (soft) fault | Page not in TLB but is in RAM | Update TLB/page table, no disk I/O |
| Major (hard) fault | Page not in RAM (on disk/swap) | Load page from disk into RAM |
| Invalid fault | Access to unmapped or protected address | Send SIGSEGV to process |


## 3.3 ARM64 Registers Involved in Page Fault Handling
On ARM64, page faults are handled as part of the synchronous exception mechanism. The following registers carry essential information:
| Register | Full Name | Content During Page Fault |
| ESR_EL1 | Exception Syndrome Register | EC bits: fault class; DFSC: Data Fault Status Code (translation, permission, alignment) |
| FAR_EL1 | Fault Address Register | Virtual address that caused the fault |
| ELR_EL1 | Exception Link Register | PC of the faulting instruction (return address after handling) |
| SPSR_EL1 | Saved Program Status Register | Saved CPU state (PSTATE) at time of exception |
| TTBR0_EL1 | Translation Table Base Reg 0 | Points to the page table of the faulting process (user space) |


### ESR_EL1 Exception Class (EC) Values for Page Faults
The EC field (bits [31:26] of ESR_EL1) identifies the fault type:
| EC Value | Fault Type | Description |
| 0b100100 (0x24) | Data Abort from EL0 | User space data access fault (read/write from EL0) |
| 0b100101 (0x25) | Data Abort from EL1 | Kernel space data access fault (read/write from EL1) |
| 0b100000 (0x20) | Instruction Abort from EL0 | User space instruction fetch fault |
| 0b100001 (0x21) | Instruction Abort from EL1 | Kernel space instruction fetch fault |


## 3.4 Page Fault Flow Diagram
When the MMU detects a faulting access, the hardware automatically vectors to the exception handler. The following diagram shows the complete flow from fault detection through resolution:
User Process
    |
    |  accesses virtual address (e.g., 0x4000) — not mapped yet
    v
  MMU checks Page Table Entry (PTE)
    |
    |  Present bit = 0  --------> CPU raises Page Fault Exception
    v                                        |
                               CPU saves registers + PC to kernel stack
                               CPU switches to EL1 (kernel mode on ARM64)
                                        |
                                        v
             Kernel Page Fault Handler: do_mem_abort() -> do_page_fault()
                  (arch/arm64/mm/fault.c)
                                        |
             +------- Is the address in a valid VMA? -------+
             |                                               |
            NO                                             YES
             |                                               |
      Send SIGSEGV                      +--- Is it a permission violation (write to RO)?
      process killed                    |                   |
                                      YES                   NO
                                        |                   |
                              Is CoW set?          Is page in swap?
                              /        \              /         \
                            YES        NO           YES          NO
                             |          |            |            |
                      Alloc new    Send SIGSEGV  Read from   Alloc zeroed
                      page, copy              swap device    page (anon)
                      content
                             |                   |            |
                             +-------------------+------------+
                                        |
                           Update PTE: set Present=1
                           Flush TLB entry for VA
                                        |
                                        v
                           Return from exception (ERET)
                           Re-execute faulting instruction
                                        |
                                        v
                           Access succeeds — process continues
                           (process was never aware of the fault)

## 3.5 Code Example — Demand Paging
This example shows how demand paging works transparently. When malloc() is called, only virtual address space is reserved. The physical page is only allocated when the first write occurs, triggering a page fault:
#include <stdlib.h>
#include <string.h>

int main() {
    /* (1) Virtual address range reserved via brk()/mmap()
     *     NO physical page allocated yet.
     *     PTE: Present=0, Type=anonymous, permissions set */
    char *buf = malloc(4096);

    /* (2) PAGE FAULT triggered here!
     *
     * What happens:
     *   a) CPU tries to write to buf
     *   b) MMU checks PTE -> Present bit = 0
     *   c) CPU raises Data Abort (EC=0x24, DFSC=0x04 = Level 0 translation fault)
     *   d) do_mem_abort() -> do_page_fault() -> handle_mm_fault()
     *   e) Kernel checks: is buf in a valid VMA? -> YES (heap)
     *   f) Kernel allocates a zeroed physical page (4 KB)
     *   g) Kernel updates PTE: maps buf -> new physical page, Present=1, RW=1
     *   h) Kernel flushes TLB entry
     *   i) ERET -> re-execute the write instruction
     *   j) Write succeeds! buf[0] = 'A' */
    buf[0] = 'A';

    /* All subsequent accesses to buf succeed without faulting */
    memset(buf, 0xAB, 4096);

    free(buf);
    return 0;
}

NOTE — Transparent to User Space
The user process never knew a page fault occurred. The faulting instruction is re-executed
after the kernel resolves the fault. Minor faults take microseconds; major faults (disk I/O)
can take milliseconds. Use /proc/<pid>/stat to monitor minor_faults and major_faults.

## 3.6 Code Example — Copy-on-Write (CoW)
After fork(), the parent and child share the same physical pages, all marked read-only. When either process writes to a shared page, a page fault occurs and the kernel creates a private copy for the writing process:
#include <unistd.h>
#include <stdio.h>

int x = 10;   /* Lives in parent data segment */

int main() {
    pid_t pid = fork();

    /* After fork():
     * - Parent and child share the SAME physical page for x
     * - The shared page PTE is marked READ-ONLY in BOTH processes
     * - vm_area_struct.vm_flags has VM_SHARED cleared, VM_COW set */

    if (pid == 0) {
        /* CHILD PROCESS:
         * Writing to x triggers PAGE FAULT:
         *   a) Write to x -> MMU detects write to RO page
         *   b) Data Abort: EC=0x24, DFSC=0x0F (permission fault)
         *   c) do_page_fault() -> handle_pte_fault() -> do_wp_page()
         *   d) Kernel checks vm_page_prot -> CoW page
         *   e) get_user_pages() to pin old page
         *   f) alloc_page(GFP_KERNEL) -> new physical page
         *   g) copy_user_highpage(new_page, old_page)
         *   h) Child's PTE updated -> new page (RW=1)
         *   i) Parent's PTE remains pointing to original page
         *   j) ERET -> re-execute write -> x = 20 in child */
        x = 20;
        printf("Child: x = %d\n", x);  /* prints 20 */
    } else {
        /* PARENT PROCESS:
         * x still points to original page, unchanged */
        printf("Parent: x = %d\n", x);  /* still prints 10 */
    }
    return 0;
}

## 3.7 ARM64 Kernel Call Chain for Page Faults
The following shows the actual Linux kernel call chain when a page fault occurs on ARM64 (arch/arm64/mm/fault.c):
/* Exception vector entry */
el0_sync / el1_sync
    |
    v
el0_da / el1_abort          /* Data Abort handler */
    |
    v
do_mem_abort()              /* arch/arm64/mm/fault.c */
    |  reads ESR_EL1 for EC and DFSC
    |  reads FAR_EL1 for fault address
    v
do_page_fault()             /* maps to handle_mm_fault() */
    |
    v
handle_mm_fault()           /* mm/memory.c */
    |
    v
__handle_mm_fault()         /* walks VMA, calls handle_pte_fault() */
    |
    +-- handle_pte_fault()  /* actual PTE-level fault handling */
        |
        +-- do_anonymous_page()   /* new anonymous page (heap/stack) */
        +-- do_fault()            /* file-backed page (mmap, exec) */
        +-- do_swap_page()        /* page in swap */
        +-- do_wp_page()          /* Copy-on-Write write fault */

| Aspect | Detail |
| Trigger | MMU raises exception on invalid/missing virtual address access |
| ARM64 registers | ESR_EL1 (fault reason), FAR_EL1 (fault address), ELR_EL1 (return PC) |
| Key Linux function | do_page_fault() -> handle_mm_fault() -> handle_pte_fault() |
| Transparent to user | Yes — faulting instruction is re-executed (ERET) after resolution |
| Performance cost | Minor fault: ~1-5 microseconds; Major fault (disk I/O): ~1-10 milliseconds |
| Monitoring | /proc/<pid>/stat fields: minflt (minor), majflt (major) |


# 4. Low Memory and High Memory — Complete Developer Guide
Low memory and high memory is one of the most important — and often confusing — topics in Linux kernel memory management. This section provides the complete picture: what these regions are, why they exist, and how to correctly work with each one.
## 4.1 Background: Why This Problem Exists
Platform Scope
This problem is specific to 32-bit systems (ARM32, x86). On 64-bit ARM64, the virtual address
space is so large (48-bit or 52-bit) that all physical RAM can be directly mapped, and this
problem essentially disappears. However, understanding it is critical because:
  - Many embedded/IoT SoCs still run 32-bit kernels
  - The concepts explain kernel memory architecture fundamentals
  - Legacy code and drivers still reference these concepts
  - Kernel source still contains highmem handling code paths

On a 32-bit system, the CPU can address only 4 GB of virtual address space (2³²). The kernel splits this between user and kernel using PAGE_OFFSET (typically 0xC0000000 on ARM, giving the classic 3G/1G split).
The kernel therefore has only 1 GB of virtual address space to work with for all of its own needs. Now suppose the machine has 2 GB or more of physical RAM. The kernel cannot directly map all physical RAM into its 1 GB virtual window. This fundamental constraint creates the split between low memory and high memory.
## 4.2 Low Memory (LOWMEM)
Definition: Physical memory that is permanently and directly mapped into the kernel’s virtual address space.
- On ARM with PAGE_OFFSET = 0xC0000000, the kernel maps physical RAM starting at 0xC0000000 virtual
- The mapping formula is: virtual_address = physical_address + PAGE_OFFSET
- This region is called lowmem or the direct-mapped region
- Typically covers the first ~896 MB of physical RAM (on x86; ARM varies by configuration)
- The kernel can access lowmem addresses directly — no dynamic mapping setup needed
- __pa(vaddr) and __va(paddr) macros perform the conversion instantly (just arithmetic)
### Lowmem Direct-Map Diagram
Physical RAM                    Kernel Virtual Space

┌──────────────┐             ┌─────────────────────┐ 0xFFFFFFFF
│              │             │  FIXMAP  (~4 MB)     │
│  HIGH MEM    │             ├─────────────────────┤ 0xFFC00000
│  (>896 MB)   │             │  PKMAP  (~8 MB)      │
│              │             ├─────────────────────┤ 0xFF800000
│              │             │                     │
│              │             │  vmalloc area       │
│              │             │  (~112 MB)          │
│              │             ├─────────────────────┤ ~0xF8000000
│              │             │                     │
├──────────────┤ <--------> │  Direct Map (Lowmem)│
│              │             │  (~896 MB)          │
│   LOW MEM    │             │  virtual =          │
│  (0–896 MB)  │             │  physical +        │
│              │             │  PAGE_OFFSET        │
└──────────────┘             └─────────────────────┘ 0xC0000000

### Key Properties of Low Memory
- Always mapped — kernel can dereference a lowmem pointer at any time without setup
- Used for: kernel code, kernel data, page tables, slab/slub allocator, most kernel allocations
- kmalloc() allocates from lowmem (physically contiguous, directly mapped)
- alloc_pages() for low memory returns directly usable pages
- DMA operations work reliably because DMA controllers can reach low physical addresses
## 4.3 High Memory (HIGHMEM)
Definition: Physical memory that cannot be permanently mapped into the kernel’s virtual address space due to address space exhaustion.
- Physical RAM above ~896 MB (the exact boundary is the ZONE_HIGHMEM start)
- The kernel knows this memory exists but has no permanent virtual address for it
- To use a highmem page, the kernel must temporarily map it into a small reserved virtual window, use it, then unmap it
- page_address() returns NULL for highmem pages — direct dereference is impossible
## 4.4 Three Mechanisms to Access High Memory
### Mechanism 1: kmap() / kunmap() — Sleepable Persistent Mapping
Uses the PKMAP (Persistent Kernel Map) area: a small window (~2–4 MB) of virtual addresses at the top of kernel space. Limited slots are available; can sleep/block if all slots are in use. Safe to use only in process context.
struct page *page = alloc_page(GFP_HIGHUSER);
if (!page)
    return -ENOMEM;

/* Map the highmem page into kernel virtual space */
void *vaddr = kmap(page);       /* Maps page into PKMAP area (may sleep) */

/* Now vaddr is a valid kernel virtual address */
memcpy(vaddr, src, PAGE_SIZE);

/* CRITICAL: Always unmap when done to release PKMAP slot */
kunmap(page);                   /* Unmap the page */

/* After kunmap(), vaddr is invalid. Do NOT use it! */

### Mechanism 2: kmap_atomic() / kunmap_atomic() — Atomic Non-Sleeping Mapping
Uses FIXMAP area: fixed virtual addresses reserved at compile time. Very fast with no locking. Cannot sleep between kmap_atomic and kunmap_atomic. Used in interrupt handlers, softirqs, and other atomic contexts.
/* Safe to use in interrupt context, softirqs, atomic sections */
irqreturn_t my_irq_handler(int irq, void *dev_id) {
    struct page *page = get_pending_page();

    /* CRITICAL: Cannot sleep, preemption disabled between these calls */
    void *vaddr = kmap_atomic(page);   /* Uses FIXMAP slots, no locking */

    /* Process data in atomic context */
    process_data(vaddr);

    /* Must unmap before any sleep/schedule point */
    kunmap_atomic(vaddr);

    return IRQ_HANDLED;
}

/* NOTE: On ARM64 and modern 64-bit kernels, kmap_atomic() is essentially
 * a no-op because all physical RAM is directly mapped (no HIGHMEM zone).
 * The call is preserved for source compatibility. */

### Mechanism 3: Bounce Buffers — For DMA with Highmem
DMA controllers often cannot access highmem (limited address bus). The kernel transparently allocates a lowmem "bounce buffer", copies data to/from the highmem page, and the DMA operation targets the bounce buffer. This is entirely transparent to the driver.
/* The kernel handles this transparently via the DMA subsystem */
/* Driver code looks normal: */
dma_addr_t dma_handle;
void *cpu_addr = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
/* dma_alloc_coherent always returns DMA-accessible (lowmem) memory */
/* No need to worry about highmem for DMA — the subsystem handles it */

## 4.5 Critical Developer Rules for Highmem
### Rule 1: Never dereference a struct page * directly
/* WRONG — page is a metadata struct describing physical memory,
 * NOT a pointer to the data itself */
void *ptr = (void *)page;   /* WRONG! This is kernel metadata! */

/* RIGHT — for lowmem pages (returns NULL for highmem) */
void *ptr = page_address(page);

/* RIGHT — safe for all pages including highmem */
void *ptr = kmap(page);
/* ... use ptr ... */
kunmap(page);

### Rule 2: page_address() Returns NULL for Highmem Pages
struct page *p = alloc_page(GFP_HIGHUSER);
void *va = page_address(p);  /* Returns NULL if page is in ZONE_HIGHMEM! */

/* Always check before using */
if (!va) {
    va = kmap(p);
    /* use va */
    kunmap(p);
} else {
    /* use va directly (lowmem page) */
}

### Rule 3: DMA Allocations Must Come from Lowmem
/* Correct DMA allocation — always lowmem, always physically contiguous */
dma_addr_t dma_handle;
void *cpu_addr = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
if (!cpu_addr)
    return -ENOMEM;

/* cpu_addr: kernel virtual address of the buffer
 * dma_handle: physical/bus address for DMA controller
 * Cache coherency is handled by the DMA subsystem */

/* At end of use */
dma_free_coherent(dev, size, cpu_addr, dma_handle);

## 4.6 Key Kernel Macros and Functions
| Macro / Function | Purpose |
| __pa(vaddr) | Convert virtual address to physical address (lowmem only) |
| __va(paddr) | Convert physical address to virtual address (lowmem only) |
| page_address(page) | Get kernel virtual address of a page (returns NULL for highmem) |
| kmap(page) | Map highmem page into PKMAP area (sleepable, process context only) |
| kunmap(page) | Release PKMAP slot for a previously kmap()'d page |
| kmap_atomic(page) | Map highmem page atomically using FIXMAP (non-sleeping, interrupt safe) |
| kunmap_atomic(vaddr) | Release atomic FIXMAP mapping |
| PageHighMem(page) | Test if a page is in ZONE_HIGHMEM (returns true for highmem) |
| virt_to_page(vaddr) | Get struct page * from kernel virtual address |
| pfn_to_page(pfn) | Get struct page * from page frame number |


## 4.7 Low Memory vs High Memory — Summary Table
| Aspect | Low Memory | High Memory |
| Physical range | 0 – ~896 MB | > ~896 MB |
| Kernel virtual mapping | Permanent, direct (always mapped) | Temporary, on demand via kmap() |
| Access method | Direct pointer dereference | kmap() or kmap_atomic() required |
| page_address() | Returns valid kernel virtual addr | Returns NULL |
| DMA capable | Yes (always) | Usually No (limited bus address) |
| ARM64 | All memory is lowmem on ARM64 | Does not exist on ARM64 |
| Zone | ZONE_NORMAL / ZONE_DMA | ZONE_HIGHMEM |


## 4.8 ARM64: High Memory Is Gone
On ARM64 (AArch64), the virtual address space is 48-bit (or 52-bit), providing 128 TB (or 4 PB) to the kernel. This is vastly more than any physically installed RAM, so all physical RAM can be directly mapped in the kernel’s linear map region.
- No ZONE_HIGHMEM on ARM64 — CONFIG_HIGHMEM is not set
- kmap() and kunmap() are compiled as no-ops (just return the linear map address)
- page_address() always returns a valid pointer for any page
- All physical RAM appears in the kernel linear map starting at PAGE_OFFSET (0xFFFF000000000000)
- ARM64 drivers are therefore simpler — you never need kmap()
/* ARM64 linear map relationship */
Physical 0x00000000  -->  Virtual 0xFFFF000000000000  (PAGE_OFFSET)
Physical 0x00001000  -->  Virtual 0xFFFF000000001000
Physical 0x40000000  -->  Virtual 0xFFFF000040000000
/* ... all physical RAM directly mapped ... */

/* kmap() on ARM64: literally a no-op */
static inline void *kmap(struct page *page)
{
    might_sleep();
    return page_address(page);  /* just returns linear map address */
}


# 5. Memory Zones — Complete Reference with Diagrams
The Linux kernel divides physical memory into zones to manage different hardware constraints. Each zone has specific properties relating to DMA accessibility, kernel mappability, and address range. Understanding zones is essential for correct use of GFP (Get Free Pages) flags.
## 5.1 Physical Memory Zones on 32-bit ARM
PHYSICAL RAM (e.g., 2 GB total)

┌─────────────────────────────────────────┐ 2048 MB (2 GB)
│                                         │
│           ZONE_HIGHMEM                  │  ~1152 MB
│   Not permanently mapped in kernel      │
│   Accessed via kmap() / kmap_atomic()   │
│   Used for: user pages, file cache      │
│                                         │
├─────────────────────────────────────────┤ 896 MB   <--- HIGHMEM boundary
│                                         │
│           ZONE_NORMAL                   │  ~880 MB
│   Directly mapped into kernel space     │
│   kmalloc(), slab/slub, page tables     │
│   DMA32-capable on most ARM SoCs        │
│                                         │
├─────────────────────────────────────────┤ 16 MB    <--- ZONE_DMA boundary
│           ZONE_DMA                      │  16 MB
│   Legacy DMA, ISA devices               │
│   Physical address < 16 MB              │
│   x86 legacy; many ARM SoCs skip this   │
└─────────────────────────────────────────┘ 0 MB

## 5.2 Kernel Virtual Address Space — Zone Mapping (32-bit ARM)
KERNEL VIRTUAL ADDRESS SPACE (32-bit ARM, 1 GB window)

┌─────────────────────────────────────────┐ 0xFFFFFFFF
│   FIXMAP  (~4 MB)                       │  kmap_atomic() FIXMAP slots
├─────────────────────────────────────────┤ 0xFFC00000
│   PKMAP   (~8 MB)                       │  kmap() persistent slot area
├─────────────────────────────────────────┤ 0xFF800000
│                                         │
│   vmalloc area  (~112 MB)               │  vmalloc(), ioremap()
│                                         │  Pages can come from any zone
├─────────────────────────────────────────┤ ~0xF8000000 (VMALLOC_START)
│                                         │
│   Direct Map / ZONE_NORMAL  (~880 MB)   │  ZONE_DMA + ZONE_NORMAL
│   virtual = physical + PAGE_OFFSET      │  Always mapped, fast access
│   0xC0000000 = physical 0x0             │
│                                         │
└─────────────────────────────────────────┘ 0xC0000000 (PAGE_OFFSET)

## 5.3 ARM64 — Zone Layout (No Highmem)
ARM64 NOTE: ZONE_HIGHMEM does not exist on ARM64
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

PHYSICAL RAM layout on ARM64:

┌─────────────────────────────────────────┐ (all installed RAM)
│                                         │
│           ZONE_NORMAL                   │  All RAM except DMA
│   All physical RAM directly mapped      │
│   via linear map (no highmem ever)      │
│   Physical 0x0 -> 0xFFFF000000000000   │
│                                         │
├─────────────────────────────────────────┤ platform-specific
│           ZONE_DMA / ZONE_DMA32         │
│   Low physical addresses (e.g., <4 GB) │
│   For 32-bit DMA-capable devices        │
└─────────────────────────────────────────┘ 0

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
On ARM64: kmap() / kunmap() are no-ops.
page_address() always returns valid pointer.
CONFIG_HIGHMEM is never set on ARM64.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

## 5.4 Zone Definitions Table
| Zone | Physical Range (ARM32) | Kernel Mapping | DMA | ARM64 |
| ZONE_DMA | 0 – 16 MB | Direct (permanent) | Yes (ISA DMA) | Optional |
| ZONE_DMA32 | 0 – 4 GB | Direct (permanent) | Yes (32-bit DMA) | Present |
| ZONE_NORMAL | 16 MB – 896 MB | Direct (permanent) | Usually Yes | All RAM |
| ZONE_HIGHMEM | > 896 MB | Temporary (kmap) | Usually No | N/A |
| ZONE_MOVABLE | Configurable | Any | No | Present |


## 5.5 Allocator Behavior with Zones
| Allocator Call | Zone Used | Notes |
| kmalloc(size, GFP_KERNEL) | ZONE_NORMAL | Physically contiguous, directly mapped, can sleep |
| kmalloc(size, GFP_ATOMIC) | ZONE_NORMAL | Cannot sleep — for interrupt/atomic context |
| kmalloc(size, GFP_DMA) | ZONE_DMA | For DMA-capable buffers below 16 MB |
| vmalloc(size) | Any zone | Virtually contiguous, not physically; TLB overhead |
| alloc_page(GFP_HIGHUSER) | ZONE_HIGHMEM preferred | For user pages, needs kmap() to access in kernel |
| get_free_page(GFP_KERNEL) | ZONE_NORMAL | Single 4 KB page, directly usable in kernel |
| dma_alloc_coherent() | ZONE_DMA or DMA32 | DMA-safe, cache-coherent; always lowmem |


## 5.6 GFP Flags — Complete Reference
GFP stands for Get Free Pages. These flags control which memory zone is used, whether the allocator can sleep or use swap, and other allocation behavior. Choosing the correct GFP flag is critical for kernel correctness.
### Context Rule Summary
The Golden Rule of GFP Flags
  - Process context (can sleep):        use GFP_KERNEL
  - Interrupt / atomic context:         use GFP_ATOMIC
  - DMA buffer needing low memory:      use GFP_DMA or dma_alloc_coherent()
  - User page allocation:               use GFP_HIGHUSER or GFP_USER
  - Large allocation, non-DMA:          use GFP_KERNEL (consider vmalloc if very large)

  NEVER use GFP_KERNEL in interrupt handlers or spin lock critical sections —
  it can sleep and will cause BUG() in atomic context.

| GFP Flag | Zone / Behavior | When to Use |
| GFP_KERNEL | ZONE_NORMAL; can sleep, can swap, can reclaim | Normal kernel allocation in process context (most common) |
| GFP_ATOMIC | ZONE_NORMAL; cannot sleep, uses emergency reserves | Interrupt handlers, softirqs, tasklets, spinlock sections |
| GFP_DMA | ZONE_DMA (<16 MB physical) | ISA legacy DMA devices needing <16 MB physical address |
| GFP_DMA32 | ZONE_DMA32 (<4 GB physical) | 32-bit DMA devices that cannot address above 4 GB |
| GFP_USER | ZONE_NORMAL; can sleep | User space allocations via kernel (rarer) |
| GFP_HIGHUSER | ZONE_HIGHMEM preferred; can sleep | User pages that can live in highmem (page cache, mmap) |
| GFP_NOWAIT | Any zone; no waiting at all | When allocation failure is acceptable and no stalling allowed |
| GFP_NOIO | ZONE_NORMAL; can sleep, no I/O | Contexts where block I/O must not be triggered (e.g., I/O path) |
| GFP_NOFS | ZONE_NORMAL; no filesystem ops | Filesystem code to prevent re-entrant filesystem calls |
| __GFP_ZERO | Modifier: zero-fill the page(s) | When zeroed memory is needed (or use kzalloc() instead) |
| __GFP_HIGHMEM | Modifier: allow ZONE_HIGHMEM | Allow allocation from highmem zone (add to base flag) |
| __GFP_NOFAIL | Modifier: retry until success | Critical allocations that absolutely must not fail (use sparingly) |
| __GFP_RECLAIM | Allow memory reclaim (swap out pages) | Default for GFP_KERNEL; explicitly set when needed |


## 5.7 vmalloc vs kmalloc — The Zone and Contiguity Connection
kmalloc():
  - Physically contiguous pages
  - From ZONE_NORMAL (or ZONE_DMA with GFP_DMA)
  - Directly mapped in kernel (fast — no extra TLB entries)
  - Limited by contiguous free lowmem (fragmentation limits size)
  - Max practical size: ~4 MB (order 10 = 1024 pages)
  - Use for: DMA, hardware registers, small/medium kernel structures

vmalloc():
  - Virtually contiguous, physically scattered (each page can be anywhere)
  - Pages can come from ANY zone (including highmem)
  - Uses vmalloc virtual address area (separate TLB entries per page)
  - Slower (page table setup + TLB pressure)
  - Can allocate very large buffers (limited only by virtual address space)
  - Use for: large non-DMA buffers, kernel modules, firmware loading

  Physical Memory:                  Virtual Memory (kernel):

  +-------+ Page A (any location)   +-------+ vmalloc area
  |       |  <---------------------- |  VA1  |  (virtually contiguous)
  +-------+                          +-------+
  +-------+ Page B (different loc.)  +-------+
  |       |  <---------------------- |  VA2  |
  +-------+                          +-------+
  +-------+ Page C (yet another)     +-------+
  |       |  <---------------------- |  VA3  |
  +-------+                          +-------+

  vs kmalloc (physically contiguous):

  +-------+-------+-------+          +-------+-------+-------+
  |PageA  |PageB  |PageC  |  <=====> | kmalloc region (contig)|
  +-------+-------+-------+          +-------+-------+-------+
  Contiguous in physical AND virtual

Next in Part 2
Part 2 of this reference guide covers:
  - Memory Allocation Mechanisms: Buddy Allocator, Slab/Slub/Slob Allocators
  - kmalloc, kzalloc, vmalloc, and dma_alloc_coherent in depth
  - Per-CPU allocators and memory pools
  - Memory reclaim and the OOM killer
  - DMA coherency and cache management on ARM64



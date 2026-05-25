
Qualcomm Staff Engineer
Interview Preparation Guide

PART 1 OF 3

ARM Architecture Fundamentals • Memory Management • Scheduling

| Prepared for: | Sandeep Kumar |
| Target Role: | Staff Engineer — Embedded Linux, Qualcomm |
| Experience: | 12 Years — ARM/ARM64, Kernel Development, SoC Platforms |
| Part: | 1 of 3 — ARMv7-A & ARMv8-A, Memory Mgmt, Scheduling |


Section 1: Fundamentals of ARM Architecture — ARMv7-A (32-bit)
ARMv7-A is the 32-bit ARM application profile architecture, widely used in embedded systems, mobile SoCs, and networking devices. Understanding ARMv7-A is foundational for any ARM/ARM64 engineer.
1.1 Core Characteristics
- Word size: 32-bit (registers R0–R15)
- Instruction sets: ARM (32-bit), Thumb (16-bit), Thumb-2 (mixed 16/32-bit), Jazelle (Java bytecode)
- Registers: 16 general-purpose registers (R0–R15):
- R13 = SP (Stack Pointer)
- R14 = LR (Link Register)
- R15 = PC (Program Counter)
- CPSR: Current Program Status Register — holds condition flags (N, Z, C, V), mode bits, interrupt masks (I, F), Thumb state (T)
- SPSR: Saved Program Status Register — one per exception mode, saves CPSR on exception entry
- Banked registers: Each exception mode (FIQ, IRQ, SVC, ABT, UND, SYS) has its own banked SP, LR, and SPSR
1.2 Processor Modes (ARMv7-A)
ARMv7-A defines nine processor modes, each with different privilege levels and banked registers:
| Mode | Abbrev | CPSR[4:0] | Description |
| User | USR | 10000 | Unprivileged user mode — applications run here |
| System | SYS | 11111 | Privileged, shares all USR registers (no banked SP/LR) |
| Supervisor | SVC | 10011 | Kernel entry via SVC/SWI instruction; own SP_svc, LR_svc |
| IRQ | IRQ | 10010 | Normal interrupt handling; own SP_irq, LR_irq |
| FIQ | FIQ | 10001 | Fast interrupt; banked R8–R14, SPSR_fiq — minimal latency |
| Abort | ABT | 10111 | Prefetch/Data abort (page fault handler entry) |
| Undefined | UND | 11011 | Undefined instruction trap |
| Monitor | MON | 10110 | TrustZone secure monitor; transition between worlds via SMC |
| Hypervisor | HYP | 11010 | Hypervisor mode (ARMv7-A Virtualization Extensions, EL2 equiv) |


1.3 Memory Model (ARMv7-A)
- Virtual address space: 4 GB (32-bit), split between user (TTBR0) and kernel (TTBR1)
- MMU: Two-level page tables (L1 + L2):
- L1 Section mapping: 1 MB (direct from L1 descriptor)
- L2 Large page: 64 KB
- L2 Small page: 4 KB (most common)
- TTBR0: User space translation table base; configurable split via TTBCR
- TTBR1: Kernel space translation table base (fixed upper address range)
- TTBCR: Translation Table Base Control Register — controls N-bit split (VA[31:32-N] selects TTBR0 or TTBR1)
- Domains: 16 memory domains with access control (Manager, Client, No-access) — deprecated in later designs
- Cache: VIPT/PIPT L1, Harvard L1 (separate I-cache/D-cache), unified L2 cache
- TLB: Separate ITLB/DTLB, unified L2 TLB; 8-bit ASID for context tagging
1.4 Exception Handling (ARMv7-A)
- Exception Vector Table: Fixed at 0x00000000 (normal) or 0xFFFF0000 (HIVECS — High Vectors)
Vector table layout (each entry is 4 bytes — a branch instruction):
| Offset | Vector | Handler / Mode Entered |
| 0x00 | Reset | Reset handler — boot |
| 0x04 | Undefined Instruction | UND mode entry |
| 0x08 | SVC (SWI) | SVC mode — syscall entry |
| 0x0C | Prefetch Abort | ABT mode — instruction fetch fault |
| 0x10 | Data Abort | ABT mode — data access fault |
| 0x14 | Reserved | — |
| 0x18 | IRQ | IRQ mode — normal interrupt |
| 0x1C | FIQ | FIQ mode — fast interrupt |


Exception entry sequence: (1) CPSR → SPSR_mode, (2) PC → LR_mode (adjusted), (3) Mode bits changed in CPSR, (4) Interrupts masked (I bit), (5) PC loaded from vector table.
1.5 Security Extensions (TrustZone)
- Two worlds: Secure World and Non-Secure (Normal) World — hardware-enforced separation
- SCR (Secure Configuration Register): Controls NS (Non-Secure) bit; only accessible in Secure state
- Monitor mode: Entry point for world switching; SMC (Secure Monitor Call) instruction triggers Monitor mode entry
- NSACR: Non-Secure Access Control Register — controls which coprocessor/NEON resources are accessible from Normal world
Section 2: ARMv8-A Architecture (64-bit)
ARMv8-A introduced the AArch64 execution state — a clean 64-bit ISA — while retaining AArch32 backward compatibility. This is the architecture powering all modern Qualcomm SoCs (Snapdragon SM series).
2.1 Core Characteristics
- Execution states: AArch64 (64-bit operations) and AArch32 (32-bit backward compatible)
- Instruction sets: A64 (AArch64 — fixed 32-bit encoding), A32 (ARMv7 compatible), T32 (Thumb-2 compatible)
- Registers (AArch64):
- 31 general-purpose 64-bit registers: X0–X30 (32-bit views: W0–W30)
- X30 = LR (Link Register) — holds return address
- SP (Stack Pointer) — NOT a general-purpose register; separate SP per EL
- PC (Program Counter) — NOT directly readable/writable in A64 instructions
- XZR/WZR — Zero register (reads always return 0, writes discarded)
- V0–V31: 128-bit SIMD/FP registers (AArch64 Advanced SIMD is mandatory)
- PSTATE: Replaces CPSR. A collection of PE state bits: N (Negative), Z (Zero), C (Carry), V (oVerflow), DAIF (interrupt masks), SP selector, EL (current exception level), nRW (AArch64/AArch32)
2.2 Exception Levels (ARMv8-A) — Replaces Modes
ARMv8-A replaces the flat mode model with hierarchical Exception Levels (ELs), providing cleaner privilege isolation:
| Level | Role | Used By | Key Capabilities |
| EL0 | Unprivileged | User applications (processes) | Lowest privilege. Cannot access system registers. |
| EL1 | OS Kernel | Linux kernel, device drivers | Can configure MMU (TTBR0/TTBR1_EL1), handle exceptions. |
| EL2 | Hypervisor | KVM, Xen, Qualcomm Hypervisor | Controls Stage 2 page tables, traps to VMs. |
| EL3 | Secure Monitor | TrustZone (QSEE/ATF) | Highest privilege. Controls Secure/Non-Secure state via SCR_EL3. |


- Stack Pointers per EL: SP_EL0, SP_EL1, SP_EL2, SP_EL3 — each EL has a dedicated SP
- SPSel: PSTATE bit selects whether current EL uses SP_EL0 (SPSel=0) or SP_ELn (SPSel=1)
2.3 Memory Model (ARMv8-A)
- Virtual address space: Up to 48-bit (256 TB) per TTBR; extendable to 52-bit with ARMv8.2-LPA (Large Physical Address extension)
- MMU: Up to 4-level page tables (L0–L3), granule-dependent:
- 4 KB granule: L0 → L1 → L2 → L3 (4 levels, 9-bit index each)
- 16 KB granule: L0 → L1 → L2 → L3 (4 levels, 11-bit/11-bit/11-bit/11-bit)
- 64 KB granule: L1 → L2 → L3 (3 levels, 13-bit index each)
- TTBR0_EL1: User-space page table base (VA bit 63 = 0)
- TTBR1_EL1: Kernel-space page table base (VA bit 63 = 1)
- TCR_EL1: Translation Control Register — configures T0SZ/T1SZ (address range), TG0/TG1 (granule), IRGN/ORGN/SH (cache/shareability), TBI (Top Byte Ignore)
- ASID: 8-bit or 16-bit (configured via TCR_EL1.AS) — 16-bit enables more processes without TLB flushes
- Stage 2 translation: EL2 adds a second stage converting IPA (Intermediate Physical Address) to PA — used for VM isolation
- MAIR_EL1: Memory Attribute Indirection Register — 8 configurable attribute slots (Normal WB, Normal NC, Device, etc.)
2.4 Exception Handling (ARMv8-A)
- VBAR_ELn: Vector Base Address Register — one per EL, fully programmable (no fixed address like ARMv7)
The ARMv8-A vector table has 4 groups × 4 exception types = 16 entries (each entry is 128 bytes of instructions):
| Group | Offset Range | Condition |
| SP_EL0 | 0x000–0x180 | Exception taken from current EL using SP_EL0 |
| SP_ELn | 0x200–0x380 | Exception taken from current EL using SP_ELn |
| Lower EL (AArch64) | 0x400–0x580 | Exception from lower EL running AArch64 |
| Lower EL (AArch32) | 0x600–0x780 | Exception from lower EL running AArch32 |


Each group has 4 entries for: Synchronous, IRQ, FIQ, SError (System Error / async abort).
Exception entry: (1) PC → ELR_ELn, (2) PSTATE → SPSR_ELn, (3) SP switched to SP_ELn, (4) PSTATE.DAIF masked, (5) jumps to VBAR_ELn + offset. Return via ERET.
2.5 System Registers (AArch64)
AArch64 accesses system registers via MRS/MSR instructions (replacing ARMv7's MCR/MRC to CP15 coprocessor):
| Register | Purpose |
| SCTLR_EL1 | System Control: MMU enable (bit 0), D-cache enable (bit 2), I-cache enable (bit 12), alignment check |
| TCR_EL1 | Translation Control: T0SZ, T1SZ, granule, shareability, ASID width |
| TTBR0_EL1 | User space page table base + ASID |
| TTBR1_EL1 | Kernel space page table base |
| MAIR_EL1 | Memory attribute indirection: 8 slots defining Normal/Device memory types |
| ESR_EL1 | Exception Syndrome Register: EC field (exception class), ISS (instruction-specific syndrome) |
| FAR_EL1 | Fault Address Register: virtual address that caused the fault |
| ELR_EL1 | Exception Link Register: PC of interrupted instruction (for ERET) |
| SPSR_EL1 | Saved PSTATE at time of exception (restored on ERET) |
| VBAR_EL1 | Vector Base Address Register: base of exception vector table at EL1 |
| ICC_IAR1_EL1 | GICv3 CPU IF: Interrupt Acknowledge Register (read to claim interrupt) |
| ICC_EOIR1_EL1 | GICv3 CPU IF: End of Interrupt signaling |


Access syntax: MRS X0, ESR_EL1 (read ESR_EL1 into X0), MSR TTBR0_EL1, X1 (write X1 to TTBR0_EL1)
Section 3: Key Differences — ARMv7-A vs ARMv8-A
This comprehensive comparison table is a frequently tested topic in Qualcomm and other ARM-focused interviews. Memorize the key distinctions.
| Feature | ARMv7-A | ARMv8-A |
| Architecture width | 32-bit only | 64-bit (AArch64) + 32-bit (AArch32) |
| General-purpose registers | 16 (R0–R15); R13=SP, R14=LR, R15=PC | 31 (X0–X30) + separate SP, PC not addressable; XZR zero reg |
| Privilege model | Modes: USR, SVC, IRQ, FIQ, ABT, UND, SYS, HYP, MON | Exception Levels: EL0 (user), EL1 (kernel), EL2 (hypervisor), EL3 (secure monitor) |
| Status register | CPSR / SPSR (banked per mode) | PSTATE (distributed state); SPSR_ELn, ELR_ELn per EL |
| Virtual address | 32-bit (4 GB total) | 48-bit (256 TB per TTBR); extendable to 52-bit (ARMv8.2-LPA) |
| Page table levels | 2-level (L1 + L2) | Up to 4-level (L0–L3) depending on granule |
| Page granules | 4 KB small pages, 64 KB large pages, 1 MB sections | 4 KB, 16 KB, 64 KB selectable granules per TTBR |
| ASID width | 8-bit (256 entries) | 8-bit or 16-bit (64K entries) — configured via TCR_EL1.AS |
| Exception vector table | Fixed: 0x00000000 or 0xFFFF0000 (HIVECS) | VBAR_ELn fully programmable per EL |
| System reg access | MCR/MRC to CP15 coprocessor (e.g., MCR p15,0,r0,c1,c0,0) | MRS/MSR direct (e.g., MRS X0, SCTLR_EL1) |
| Instruction sets | ARM (A32), Thumb (T16), Thumb-2 (T32), Jazelle (Java) | A64 (AArch64), A32 (AArch32), T32 (Thumb-2) — no Jazelle |
| Virtualization | Optional Virtualization Extensions (adds HYP mode) | Native — EL2 always present; VMID in VTTBR_EL2 |
| TrustZone | MON mode + NS bit in SCR | EL3 (Secure Monitor); SCR_EL3 controls NS; ATF at EL3 |
| SIMD / FP | NEON optional (VFPv3/v4 + Advanced SIMD) | Advanced SIMD + NEON mandatory; FP/SIMD regs V0–V31 (128-bit) |
| Exclusive access (atomic) | LDREX/STREX (exclusive monitor) | LDXR/STXR + ARMv8.1 LSE atomics (LDADD, SWP, CAS, etc.) |
| Pointer Authentication (PAC) | Not available | ARMv8.3-A PAuth — signs/authenticates pointers against key |
| Memory Tagging (MTE) | Not available | ARMv8.5-A MTE — hardware-enforced heap/stack safety tags |
| Branch Target Identification | Not available | ARMv8.5-A BTI — marks valid branch targets, defeats code-reuse attacks |
| EL2 Stage-2 translation | Optional (w/ Virt Extensions) | Native — VTTBR_EL2, VTCR_EL2; IPA→PA via VSMMU |
| Debug architecture | ARMv7 Debug (DBGDSCR, hardware BPs/WPs) | ARMv8 Debug (MDSCR_EL1, OS lock, Self-hosted debug) |


| ✔ Interview Tip: When asked about ARMv7 vs ARMv8 differences, lead with: (1) 64-bit AArch64 state + 31 general registers, (2) Exception Levels replacing mode model, (3) Larger VA space (48-bit), (4) Mandatory SIMD/NEON, (5) Native virtualization at EL2, (6) MRS/MSR direct system register access. |

Section 4: SYS Mode vs SVC Mode — ARMv7-A Deep Dive
The difference between SYS mode and SVC mode is a classic ARM interview question. Both are privileged modes, but they serve fundamentally different purposes. This is frequently asked at Qualcomm for candidates with ARM bring-up experience.
4.1 SVC Mode (Supervisor Mode)
- CPSR mode bits: 0b10011 (0x13)
- Entered via: SVC instruction (formerly SWI — Software Interrupt), or Reset
- Purpose: Primary kernel/OS execution mode. All Linux kernel code runs in SVC mode on ARMv7.
- Banked registers: Own SP_svc (stack pointer) and LR_svc (link register), plus SPSR_svc
- Has SPSR: Yes — saves the pre-exception CPSR so it can be restored on exception return (MOVS PC, LR)
When user-space executes SVC #0 (Linux syscall), the processor performs:
- CPSR → SPSR_svc (save current processor state)
- PC+offset → LR_svc (save return address)
- CPSR mode bits → SVC (0x13); IRQ disabled
- PC → SVC vector (0x00000008 or 0xFFFF0008)
The clean SP_svc ensures the kernel entry path has a dedicated stack, completely separate from the user-space stack.
4.2 SYS Mode (System Mode)
- CPSR mode bits: 0b11111 (0x1F)
- Entered via: Software only — by writing mode bits in CPSR using MSR CPSR_c, #0x1F
- Purpose: Privileged mode that shares all registers with USR mode — same R0–R14, same SP, same LR
- Banked registers: NONE — uses the same SP and LR as USR mode (no banking)
- Has SPSR: No — SYS mode cannot be entered via an exception, so no need to save CPSR
- Use case: OS code that needs to run privileged but wants direct access to user-space stack/registers for context save/restore, signal frame setup
4.3 Side-by-Side Comparison
| Feature | SVC Mode | SYS Mode |
| Mode bits (CPSR[4:0]) | 10011 (0x13) | 11111 (0x1F) |
| Entered by | SVC instruction / Reset | Software only (MSR CPSR_c, #0x1F) |
| Banked SP | Yes — SP_svc (own kernel stack) | No — shares USR SP directly |
| Banked LR | Yes — LR_svc | No — shares USR LR directly |
| SPSR | Yes — SPSR_svc | No SPSR (not entered by exception) |
| Privilege level | Privileged | Privileged |
| Register set | Own SP_svc, LR_svc; R0–R12 shared | Same as USR (R0–R14, SP, LR all shared) |
| Can be exception target | Yes (SVC vector at 0x08) | No (software-only entry) |
| Typical Linux use | Syscall entry, primary kernel execution | Context switch, signal delivery, user reg access |
| Nested exception risk | Corrupts LR_svc if nested SVC without saving | N/A (not exception-entered) |


4.4 Why Does SYS Mode Exist?
The key motivation: in SVC mode, to access the user-space SP or LR (e.g., to save full user context for a context switch), the kernel must use special banked register access instructions. In SYS mode, since SP and LR are the SAME as USR mode's, direct access is possible.
Assembly example — accessing user-space SP from kernel:
| @ ---- In SVC mode: to get user SP, use banked register access ----MRS r0, SP_usr @ ARMv7 banked register access (special instruction) @ ---- In SYS mode: SP is ALREADY the user SP, direct access ----@ Simply use SP register directly -- no special instructions neededSTR sp, [r2] @ Stores user-space SP value directly @ ---- Linux ARM context switch (simplified) ----@ Kernel switches to SYS mode to manipulate user stack directly:MSR cpsr_c, #0x1F @ Enter SYS mode (privilege remains)STMIA r0, {r0-r14}^ @ Save user registers (including USR SP/LR)MSR cpsr_c, #0x13 @ Back to SVC mode |


4.5 Chain Questions & Answers
→ Q: What happens if you execute SVC while already in SVC mode?
LR_svc gets OVERWRITTEN with the return address of the new SVC call. This is a classic trap — nested SVC calls corrupt LR_svc unless you explicitly save it to the stack first. The kernel MUST push LR_svc onto the SVC stack (PUSH {r0-r12, lr} at entry) before making any nested kernel calls or enabling interrupts.
→ Q: Can SYS mode be entered from an exception?
No. SYS mode can ONLY be entered by explicitly modifying CPSR mode bits in software (MSR instruction). It is never the target of any exception vector. This is why SYS mode has no SPSR — there is no "pre-exception state" to save.
→ Q: What is the difference between SYS mode and USR mode?
Both SYS and USR share the SAME register set (R0–R14, SP, LR). The ONLY difference is privilege level: USR mode CANNOT execute privileged instructions (MSR CPSR, MCR to CP15, cache maintenance, etc.) and CANNOT access protected memory regions. SYS mode can do all of these. This makes SYS mode valuable — full privilege with direct access to user registers.
→ Q: In ARMv8-A, what replaces SVC mode and SYS mode?
SVC mode → EL1 (the Linux kernel runs at EL1 in AArch64). The SVC instruction triggers a synchronous exception to EL1, saving PC → ELR_EL1 and PSTATE → SPSR_EL1.
SYS mode concept → not needed in AArch64. At EL1 you can directly access SP_EL0 (user-space SP) via the system register: MRS X0, SP_EL0 — no mode-switching required.
| ✔ Interview One-Liner: SVC has its own banked SP and LR (clean kernel stack), entered by hardware exception. SYS has NO banked registers (shares USR SP/LR), entered by software only. SVC = kernel execution mode. SYS = user-register-access mode. |

Section 5: Linux Kernel Memory Management — Interview Q&A
This section covers memory management interview questions with detailed answers and chain questions, specifically targeting 12 years of kernel experience at Qualcomm Staff Engineer level.
Q1: Explain the Linux kernel virtual memory layout for ARM64. How does TTBR0/TTBR1 split work?
Answer: On ARM64 (AArch64) with a 48-bit virtual address space, the kernel uses a canonical split based on the top bits of the virtual address:
- TTBR0_EL1 → User space: 0x0000_0000_0000_0000 to 0x0000_FFFF_FFFF_FFFF (VA bit 63 = 0, lower 48-bit canonical)
- TTBR1_EL1 → Kernel space: 0xFFFF_0000_0000_0000 to 0xFFFF_FFFF_FFFF_FFFF (VA bit 63 = 1, upper 48-bit canonical)
The MMU uses bit 63 of the virtual address to select the translation table: bit 63=0 → TTBR0 (user), bit 63=1 → TTBR1 (kernel). This is enforced automatically by the hardware.
TCR_EL1 key fields:
- T0SZ: Size of TTBR0 address space. For 48-bit: T0SZ=16 (64-48=16 bits)
- T1SZ: Size of TTBR1 address space (same value for symmetrical split)
- TG0/TG1: Granule size (00=4K, 01=64K, 10=16K for TG1)
- IRGN/ORGN/SH: Inner/Outer cache attributes and Shareability for page table walks
Linux ARM64 kernel virtual memory layout (4K pages, 48-bit VA):
| Virtual Address Range Region----------------------------------------------------------------------0xFFFF_0000_0000_0000 start of kernel address space0xFFFF_8000_0000_0000 ---- 0xFFFF_FEFF_BFFF_FFFF vmalloc / ioremap area0xFFFF_FF00_0000_0000 ---- 0xFFFF_FF7F_FFFF_FFFF PCI I/O space (4 MB)0xFFFF_FF80_0000_0000 ---- 0xFFFF_FFFF_FFFF_FFFF linear map (physmem)0xFFFF_FFFF_8000_0000 ---- 0xFFFF_FFFF_FFFF_FFFF kernel image (.text/.data/.bss) 0x0000_0000_0000_0000 ---- 0x0000_FFFF_FFFF_FFFF user space (TTBR0) |


- KPTI (Kernel Page Table Isolation): Post-Meltdown, the kernel maintains two separate page table sets: one minimal set for user mode (only syscall entry stubs mapped) and one full set for kernel mode. On syscall entry, TTBR1 is switched to the full kernel page table. On return to user, TTBR1 is switched to the minimal set.
→ Q1a: What is KASLR and how does it interact with ARM64 page tables?
KASLR (Kernel Address Space Layout Randomization) randomizes the kernel image base address at boot. On ARM64: the physical load address is randomized within a range (based on kaslr-seed from DTB or UEFI RNG). Page tables are built at runtime using the randomized base. kimage_vaddr and kimage_voffset track the offset. The __pa() / __va() macros use kimage_voffset for address conversion. KASLR is orthogonal to KPTI and both can be active simultaneously.
→ Q1b: Explain vmalloc, kmalloc, and kzalloc. When would you use each?
| Allocator | Physical Contiguous? | Can Sleep? | Max Size | Use Case |
| kmalloc(size, flags) | Yes | Depends on flags | ~128KB (power-of-2) | Small kernel structs, DMA buffers, fast path |
| kzalloc(size, flags) | Yes (zero-init) | Depends on flags | ~128KB | Like kmalloc but zeroed; prevents info leaks |
| vmalloc(size) | No (virtually contiguous) | Yes (GFP_KERNEL internally) | Limited by VA space | Large allocations (firmware, big lookup tables) |
| get_free_pages / alloc_pages | Yes | Yes | 2^MAX_ORDER pages | Page-aligned, physically contiguous; buddy allocator direct |
| dma_alloc_coherent | Yes (DMA zone) | Yes | Device-limited | DMA buffers; CPU/device coherent; IOMMU-mapped on SMMU systems |


→ Q1c: What is the difference between GFP_KERNEL and GFP_ATOMIC?
| Flag | Can Sleep? | Context | Priority | Use Case |
| GFP_KERNEL | Yes | Process context only | Normal | Default kernel allocation — can reclaim, swap, wait for pages |
| GFP_ATOMIC | No | Any (IRQ, spinlock held) | High (uses emergency reserves) | Interrupt handlers, atomic sections, must not sleep |
| GFP_NOWAIT | No | Any | Normal (no emergency reserves) | Like GFP_ATOMIC but no emergency memory — may fail more |
| GFP_DMA | Depends on base flags | Any | Normal | Allocates from DMA zone (<16 MB on x86); device-constrained DMA |
| GFP_ZERO | Depends on base | Any | Normal | Zero-initializes page (combined with base flag: GFP_KERNEL | GFP_ZERO) |
| GFP_HIGHUSER | Yes | Process context | Normal | User-space page allocation from high memory |


Q2: How does the Linux page fault handler work on ARM64?
Answer: On ARM64, a page fault triggers a synchronous exception at EL1 (kernel fault) or EL0 (user fault). The complete call chain:
| Hardware: MMU raises fault exception |-- exception taken to EL1 --> VBAR_EL1 + offset (el1_sync or el0_sync) |el1_sync / el0_sync (arch/arm64/kernel/entry.S) |-- Save all general-purpose registers to pt_regs on kernel stack |-- Read ESR_EL1 (Exception Syndrome Register -- what kind of fault) |-- Read FAR_EL1 (Fault Address Register -- which address faulted) |ESR_EL1.EC (Exception Class): |-- 0x20 = Instruction Abort from lower EL |-- 0x21 = Instruction Abort from current EL |-- 0x24 = Data Abort from lower EL |-- 0x25 = Data Abort from current EL |do_mem_abort(addr, esr, regs) --> dispatches to do_page_fault() |do_page_fault(): |-- find_vma(mm, addr) Find VMA covering the faulting address |-- No VMA found? --> SIGSEGV (user) / kernel oops |-- VMA found, call handle_mm_fault(vma, addr, flags) | __handle_mm_fault() -- walks 4-level page tables (pgd>pud>pmd>pte) | +-- PTE not present (anonymous page): do_anonymous_page() | Alloc new zero page, install PTE, return +-- PTE not present (file-backed): do_fault() | Read page from file/swap, install PTE, return +-- PTE present, write to read-only: do_wp_fault() [Copy-on-Write] Alloc new page, copy content, set PTE writable, return |


Key registers used in fault handling:
- ESR_EL1: Exception Syndrome Register — EC field identifies fault type, ISS field has fault status code (translation fault level 0/1/2/3, permission fault, alignment, etc.)
- FAR_EL1: Fault Address Register — the exact virtual address that caused the fault
- ELR_EL1: Exception Link Register — the PC of the faulting instruction (for restart or ERET)
→ Q2a: What is the difference between a minor and major page fault?
Minor fault: Page is in memory but not mapped in the page table (e.g., first access to anonymous page, or page shared between processes not yet faulted by this process). No disk I/O. Fast. Increments the minflt counter in /proc/<pid>/stat.
Major fault: Page is not in memory — must be read from disk (swap or file-backed). Involves blocking I/O, much slower (ms vs µs). Increments majflt counter. Triggers pgmajfault vmstat entry. Excessive major faults indicate memory pressure.
→ Q2b: Explain Transparent Huge Pages (THP) on ARM64. How does the kernel manage 2 MB pages?
ARM64 with 4K granule supports 2 MB block mappings at L2 (PMD level) and 1 GB blocks at L1 (PUD level). THP (Transparent Huge Pages) allows the kernel to automatically promote a 2 MB-aligned, contiguous anonymous region to a single PMD entry when fully populated. The khugepaged daemon scans for promotion opportunities. On fault, do_huge_pmd_anonymous_page() allocates a 2 MB compound page. ARM64 also supports the Contiguous PTE hint: 16 × 4K pages = 64K mapped with a SINGLE TLB entry via the Contiguous bit in the page table descriptor — extremely useful for reducing TLB pressure on Qualcomm SoCs.
→ Q2c: What is mmap_lock and what are the implications of holding it?
mmap_lock (formerly mmap_sem) is a per-mm_struct read-write semaphore protecting the VMA tree. Read lock: page fault handling, /proc/pid/maps reads. Write lock: mmap(), munmap(), mprotect(), brk(), execve(). Holding write lock blocks ALL page faults for that process — a major scalability bottleneck in multi-threaded applications. This is being addressed with per-VMA locking (Linux 6.3+) using maple tree (replaces rbtree for VMAs, Linux 6.1+).

Section 6: Process Scheduling — Interview Q&A
Q3: Explain the CFS (Completely Fair Scheduler). How does it work on a multi-core ARM SoC?
Answer: CFS (Completely Fair Scheduler), introduced in Linux 2.6.23, aims to give each runnable task a fair share of CPU time using a virtual runtime model:
Core Concept — vruntime
- vruntime: Each task has a virtual runtime = actual CPU time normalized by task weight (priority/niceness)
vruntime formula:
| vruntime += delta_exec × (NICE_0_LOAD / task_weight) delta_exec = actual nanoseconds task ran NICE_0_LOAD = 1024 (weight of nice=0 task) task_weight = weight corresponding to task nice value Lower nice = higher weight = smaller vruntime increment = runs more often |


- Red-black tree: Tasks stored in per-CPU rb-tree (cfs_rq->tasks_timeline) keyed by vruntime. Leftmost node = task with smallest vruntime = next to run (O(log N) selection)
- sched_latency_ns: Target scheduling period (default 6 ms). Each task gets sched_latency_ns / nr_running time slice
- sched_min_granularity_ns: Minimum time slice to prevent excessive context switches when many tasks compete
Multi-Core Operation on ARM SoC
- Per-CPU runqueues: Each CPU core has its own runqueue with its own cfs_rq. This is key for scalability.
- Load balancing: load_balance() called periodically (scheduler tick) and on CPU idle. Flow: find_busiest_group() → find_busiest_queue() → move_tasks()
- Scheduling domains: Hierarchical topology (SMT → MC → NUMA). Migration cost increases at higher domain levels. ARM big.LITTLE creates distinct capacity domains.
- EAS (Energy Aware Scheduling): Qualcomm big.LITTLE / DynamIQ extension. find_energy_efficient_cpu() places tasks on the most energy-efficient CPU that still meets performance requirements.
PELT — Per-Entity Load Tracking
- util_avg: CPU utilization average, computed with exponential decay (half-life ~32 ms)
- load_avg: CPU load average accounting for task weight
PELT feeds into: (1) EAS for task placement, (2) schedutil cpufreq governor for frequency scaling, (3) task migration decisions.
→ Q3a: What is the difference between SCHED_FIFO, SCHED_RR, and SCHED_DEADLINE?
| Policy | Priority | Preemption | Time Slice | Use Case |
| SCHED_NORMAL (CFS) | nice (-20 to +19) | Yes | sched_latency/N | Default — regular applications |
| SCHED_FIFO | RT 1-99 | Yes (by higher RT) | None — runs until block/yield | Hard real-time: audio, modem DSP, sensor ISR threads |
| SCHED_RR | RT 1-99 | Yes (by higher RT) | Quantum (100ms default) | RT round-robin — same priority gets equal slices |
| SCHED_DEADLINE | Per-task runtime/deadline/period | Yes (if missed deadline) | Specified by application | Sporadic tasks with hard deadline; EDF — highest priority class |
| SCHED_IDLE | Lowest possible | Always preemptible | Idle quantum | Background/cleanup tasks; runs only when nothing else runnable |


SCHED_DEADLINE detail: Each task specifies three parameters: runtime (max CPU time), deadline (relative deadline from arrival), period (task period). Kernel guarantees runtime within each period. Uses CBS (Constant Bandwidth Server) for isolation between tasks. Kernel performs admission control: rejects tasks that would make the system unschedulable.
→ Q3b: How does CPU affinity work? What kernel mechanisms enforce it?
sched_setaffinity() syscall sets task_struct->cpus_mask bitmask. select_task_rq_fair() respects this mask during task placement and migration. The cpuset cgroup subsystem provides hierarchical CPU affinity for groups of tasks. IRQ affinity: /proc/irq/N/smp_affinity controls which CPUs handle a given IRQ. On ARM SoC with GICv3: GICR_IROUTER register (per redistributor) routes interrupts to specific PEs.
| ✔ Interview One-Liner: CFS picks the task with the smallest vruntime from a red-black tree. vruntime = normalized CPU time (lower priority tasks accumulate faster). On ARM big.LITTLE, EAS extends CFS to pick the most energy-efficient CPU, feeding off PELT load tracking and the CPU energy model. |


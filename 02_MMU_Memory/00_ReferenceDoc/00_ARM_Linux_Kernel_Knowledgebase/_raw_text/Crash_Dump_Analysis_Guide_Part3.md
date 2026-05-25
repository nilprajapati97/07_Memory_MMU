
Crash Dump Analysis Guide
Part 3 of 3: Stack Dump Analysis & Interview Summary

| ARM64 & ARM32 Stack Dump Deep Dive | ESR Decode | Register ReferenceCalling Conventions | Stack Frames | Interview Cheat Sheet |


Qualcomm Staff Engineer Technical Reference | Sandeep

| SECTION A: Stack Dump Examples & Register Explanations |


1. ARM64 (AArch64) Kernel Oops/Panic Stack Dump
This section presents a complete ARM64 kernel panic stack dump as it appears in the kernel log (dmesg / last_kmsg), followed by a detailed line-by-line breakdown of every field.

Complete Example Output
[  142.857392] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000028
[  142.857401] Mem abort info:
[  142.857404]   ESR = 0x96000005
[  142.857408]   EC = 0x25: DABT (current EL), IL = 32 bits
[  142.857412]   SET = 0, FnV = 0
[  142.857415]   EA = 0, S1PTW = 0
[  142.857418]   FSC = 0x05: level 1 translation fault
[  142.857421] Data abort info:
[  142.857424]   ISV = 0, ISS = 0x00000005
[  142.857427]   CM = 0, WnR = 0
[  142.857430] user pance: 0000000000000028 [#1] PREEMPT SMP
[  142.857439] Modules linked in: wlan(O) machine_dlkm(O) snd_soc(O)
[  142.857456] CPU: 4 PID: 1523 Comm: kworker/4:1 Tainted: G        W  O  5.15.78 #1
[  142.857462] Hardware name: Qualcomm Technologies, Inc. SM8550 (DT)
[  142.857466] Workqueue: events my_buggy_work_handler [my_module]
[  142.857475] pstate: 60400005 (nZCv daif +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[  142.857483] pc : my_buggy_function+0x48/0x120 [my_module]
[  142.857491] lr : my_caller_function+0x84/0xf0 [my_module]
[  142.857498] sp : ffffffc01a3cbc80
[  142.857501] x29: ffffffc01a3cbc80  x28: ffffff8012345000  x27: 0000000000000001
[  142.857510] x26: ffffff8056789abc  x25: ffffffc0104e3000  x24: 0000000000000000
[  142.857518] x23: ffffff8034567890  x22: 0000000000000001  x21: ffffff8012340078
[  142.857526] x20: ffffff801234a000  x19: 0000000000000000  x18: ffffffc01a480000
[  142.857534] x17: 0000000000000000  x16: ffffffc010123450  x15: 0000000000000040
[  142.857542] x14: 0000000000000001  x13: 0000000000000000  x12: 0000000000000001
[  142.857550] x11: ffffffc010987650  x10: ffffffc01a3cbb10  x9 : ffffffc010234560
[  142.857558] x8 : 0000000000000000  x7 : 0000000000000000  x6 : 000000000000003f
[  142.857566] x5 : 0000000000000000  x4 : 0000000000000000  x3 : ffffffc010abcde0
[  142.857574] x2 : 0000000000000028  x1 : 0000000000000000  x0 : 0000000000000000
[  142.857583] Call trace:
[  142.857586]  my_buggy_function+0x48/0x120 [my_module]
[  142.857594]  my_caller_function+0x84/0xf0 [my_module]
[  142.857601]  process_one_work+0x1e8/0x390
[  142.857608]  worker_thread+0x50/0x410
[  142.857614]  kthread+0x108/0x110
[  142.857620]  ret_from_fork+0x10/0x20
[  142.857628] Code: f9400693 b4000073 f9401663 f9400a84 (f9401660)
[  142.857636] ---[ end trace 8e4c23b5a1d3f678 ]---
[  142.857641] Kernel panic - not syncing: Oops: Fatal exception

2. Line-by-Line Breakdown
A. Fault Description
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000028

| Field | Meaning |
| Unable to handle | Kernel could not resolve the memory access - a fatal fault |
| kernel NULL pointer dereference | Access near address 0x0 (NULL + struct member offset) |
| virtual address 0000000000000028 | Actual VA accessed: NULL pointer + offset 0x28 into a struct (struct member access on a NULL base pointer) |


Interpretation: Code executed
ptr->member  // where ptr = NULL, member is at offset 0x28 within the structure
B. Memory Abort Info (ESR Decode)
ESR = 0x96000005
EC = 0x25: DABT (current EL), IL = 32 bits
SET = 0, FnV = 0   EA = 0, S1PTW = 0
FSC = 0x05: level 1 translation fault

ESR_EL1 Bit Field Layout
+--------+--------+----+------------------------+
| 63..32 | 31..26 | 25 |       24..0            |
|  RES0  |   EC   | IL |         ISS             |
+--------+--------+----+------------------------+

| Field | Bits | Value | Meaning |
| EC | [31:26] | 0x25 (100101b) | Data Abort from current EL (EL1 kernel mode) |
| IL | [25] | 1 | Instruction Length: 32-bit A64 instruction |
| ISS | [24:0] | 0x000005 | Instruction Specific Syndrome |
| SET | [12:11] | 0 | Synchronous Error Type: Recoverable |
| FnV | [10] | 0 | FAR (Fault Address Register) is Valid |
| EA | [9] | 0 | Not an External Abort |
| S1PTW | [7] | 0 | Not a Stage-1 page table walk fault |
| FSC | [5:0] | 0x05 | Fault Status Code: Level 1 Translation Fault |


FSC (Fault Status Code) Decode Table
| FSC[5:0] | Meaning |
| 0x04 | Level 0 translation fault |
| 0x05 | Level 1 translation fault (our case - NULL pointer, no PTE at level 1) |
| 0x06 | Level 2 translation fault |
| 0x07 | Level 3 translation fault |
| 0x09 | Level 1 access flag fault |
| 0x0A | Level 2 access flag fault |
| 0x0B | Level 3 access flag fault |
| 0x0D | Level 1 permission fault |
| 0x0E | Level 2 permission fault |
| 0x0F | Level 3 permission fault |
| 0x10 | Synchronous external abort (bus error / NoC error) |
| 0x21 | Alignment fault |


C. Data Abort Info (ISV, CM, WnR)
ISV = 0, ISS = 0x00000005
CM = 0, WnR = 0

| Field | Meaning |
| ISV = 0 | Instruction Syndrome NOT valid - cannot fully decode the faulting instruction from ESR alone |
| CM = 0 | Not a cache maintenance operation - this is a regular data access |
| WnR = 0 | READ access caused the fault (WnR=1 means Write, WnR=0 means Read). We were reading from the NULL pointer. |


D. Process & CPU Context
CPU: 4 PID: 1523 Comm: kworker/4:1 Tainted: G        W  O  5.15.78 #1
Hardware name: Qualcomm Technologies, Inc. SM8550 (DT)
Workqueue: events my_buggy_work_handler [my_module]

| Field | Value | Meaning |
| CPU | 4 | Crash occurred on CPU core 4 (out of 8 cores on SM8550) |
| PID | 1523 | Process ID of the crashing task |
| Comm | kworker/4:1 | Kernel worker thread on CPU 4, worker #1 - NOT a user process, but kernel context |
| Tainted | G W O | Kernel taint flags: G=Proprietary module, W=WARN_ON triggered, O=Out-of-tree module |
| 5.15.78 #1 | Kernel ver | Linux kernel version 5.15.78, build #1 |
| Workqueue | events | The workqueue name - "events" is the generic kernel workqueue |


Taint Flags Reference
| Flag | Meaning |
| G | Proprietary module loaded (non-GPL) |
| W | A WARN_ON() was previously triggered during this boot |
| O | Out-of-tree (external) module was loaded |
| P | Proprietary module taints the kernel |
| F | Module was force-loaded despite version mismatch |
| E | Unsigned module was loaded |


E. PSTATE Register Decode
pstate: 60400005 (nZCv daif +PAN -UAO -TCO -DIT -SSBS BTYPE=--)

PSTATE Bit Layout (ARM64)
+---+---+---+---+-----+---+---+---+---+-----+-----+-----+-----+------+-------+------+
| N | Z | C | V |     | D | A | I | F | PAN | UAO | TCO | DIT | SSBS | BTYPE |  EL  |
| 31| 30| 29| 28|     | 9 | 8 | 7 | 6 |  22 |  23 |  25 |  24 |  12  | 11:10 | 1:0  |
+---+---+---+---+-----+---+---+---+---+-----+-----+-----+-----+------+-------+------+

| Flag | Bit | Value | Meaning |
| N | 31 | 0 (n) | Negative: result was NOT negative |
| Z | 30 | 1 (Z) | Zero: last arithmetic result WAS zero |
| C | 29 | 1 (C) | Carry: carry/borrow occurred in last operation |
| V | 28 | 0 (v) | Overflow: no signed overflow |
| D | 9 | 1 | Debug exceptions masked |
| A | 8 | 1 | SError (async abort) masked |
| I | 7 | 1 | IRQ masked (interrupts disabled) |
| F | 6 | 1 | FIQ masked (fast interrupts disabled) |
| +PAN | 22 | 1 | Privileged Access Never: kernel CANNOT directly access user memory without uaccess APIs |
| -UAO | 23 | 0 | User Access Override: disabled - no override to user-mode semantics |
| -TCO | 25 | 0 | Tag Check Override: disabled (MTE - Memory Tagging Extension) |
| -DIT | 24 | 0 | Data Independent Timing: disabled |
| -SSBS | 12 | 0 | Speculative Store Bypass Safe: disabled |
| BTYPE=-- | 11:10 | 00 | Branch Type: no BTI restriction (Branch Target Identification) |
| EL | 1:0 | 01 (EL1) | Exception Level 1 = Kernel mode (EL0 = user, EL2 = hypervisor, EL3 = secure monitor) |


Key Note: The "daif" field shows ALL exceptions masked (D=1, A=1, I=1, F=1) - typical during panic handler execution to prevent nested faults.

F. Key Registers (PC, LR, SP) with Roles
pc : my_buggy_function+0x48/0x120 [my_module]
lr : my_caller_function+0x84/0xf0 [my_module]
sp : ffffffc01a3cbc80

| Reg | Full Name | Value | Role & Interpretation |
| PC | Program Counter | my_buggy_function+0x48 | FAULTING INSTRUCTION ADDRESS. "+0x48" = 72 bytes into function. "/0x120" = function is 288 bytes total. This is WHERE the crash occurred. |
| LR / X30 | Link Register | my_caller_function+0x84 | RETURN ADDRESS - where execution returns after the current function. This tells you WHO called the crashing function. |
| SP / X31 | Stack Pointer | ffffffc01a3cbc80 | Current top-of-stack. Must be 16-byte aligned in AArch64. Used to validate stack integrity and read local variables. |


G. General Purpose Registers (X0-X29) with AAPCS64 Calling Convention
x29: ffffffc01a3cbc80  x28: ffffff8012345000  x27: 0000000000000001
x26: ffffff8056789abc  x25: ffffffc0104e3000  x24: 0000000000000000
x23: ffffff8034567890  x22: 0000000000000001  x21: ffffff8012340078
x20: ffffff801234a000  x19: 0000000000000000  <-- NULL POINTER!
x18: ffffffc01a480000  x17: 0000000000000000  x16: ffffffc010123450
...
x2 : 0000000000000028  x1 : 0000000000000000  x0 : 0000000000000000

| Register | ABI Role | Caller/Callee | Value (Example) | Description & Debug Tips |
| X0-X7 | Args / Return | Caller-saved | X0=0x0 | Function args (X0-X7); X0 holds return value. May be overwritten by any function call - less reliable for tracing |
| X8 | Indirect result | Caller-saved | 0x0 | Used for struct return addresses (large return values). Scratch register. |
| X9-X15 | Temporaries | Caller-saved | Various | Intra-procedure scratch - can be overwritten freely between calls |
| X16 (IP0) | Intra-procedure | Caller-saved | 0xffffffc0... | Used by linker veneers and PLT stubs for long-range branches |
| X17 (IP1) | Intra-procedure | Caller-saved | 0x0 | Same as X16 - linker veneer second scratch register |
| X18 | Platform reg | Special | 0xffffffc01a480000 | Per-CPU variable base in Linux kernel (kernel uses this for per-CPU data access). TLS in userspace. |
| X19-X28 | Callee-saved | CALLEE-SAVED | X19=0x0 (NULL!) | MOST IMPORTANT FOR DEBUG: Preserved across function calls. These hold local variables and struct pointers at crash time. X19=NULL is the bad pointer in our example! |
| X29 (FP) | Frame Pointer | Callee-saved | 0xffffffc01a3cbc80 | Points to current stack frame. Enables stack unwinding (FP chain walking) |
| X30 (LR) | Link Register | Special | my_caller+0x84 | Return address - where to jump when current function returns (via RET instruction) |
| SP | Stack Pointer | Special | 0xffffffc01a3cbc80 | Points to current top of stack. Must remain 16-byte aligned at all times in AArch64 |


H. Address Space Identification (ARM64 Kernel Address Ranges)
ARM64 Kernel Address Ranges:
+--------------------------------+----------------------------------+
| Range                          | What It Is                       |
+--------------------------------+----------------------------------+
| 0x0000000000000000-0x0000FFFF  | NULL dereference zone (our fault)|
| 0x0000xxxxxxxxxxxx             | User-space virtual addresses      |
| 0xFFFF000000000000+            | Kernel space starts here          |
| 0xFFFF800000000000+ (ffffff80) | Linear map (PAGE_OFFSET)         |
| 0xFFFFC00000000000+ (ffffffc0) | vmalloc / kernel image region    |
| 0xFFFFE00000000000+ (ffff e0)  | Modules region                   |
+--------------------------------+----------------------------------+

From our dump:
  x28: ffffff8012345000  -> Linear map addr (physical memory struct/buffer)
  x25: ffffffc0104e3000  -> Kernel vmalloc/image region
  x18: ffffffc01a480000  -> Per-CPU base (kernel region)
  sp:  ffffffc01a3cbc80  -> Kernel stack (vmalloc region)
  x19: 0000000000000000  -> NULL! (bug: should be a kernel pointer)

I. Call Trace Reading Format
Call trace:
 my_buggy_function+0x48/0x120 [my_module]    <- CRASHED HERE
 my_caller_function+0x84/0xf0 [my_module]    <- Called by this
 process_one_work+0x1e8/0x390                <- Workqueue framework
 worker_thread+0x50/0x410                    <- Worker thread main loop
 kthread+0x108/0x110                         <- Kernel thread entry
 ret_from_fork+0x10/0x20                     <- Thread creation return

Format: function_name + offset / total_size [module_name]
              |              |         |           |
              |              |         |           +-- Which .ko module (blank = vmlinux)
              |              |         +-- Total function size in bytes
              |              +-- Offset from function start
              +-- Symbol/function name

Call flow (bottom to top = execution order):
  ret_from_fork()  -> kthread()  -> worker_thread()  -> process_one_work()
  -> my_caller_function()  -> my_buggy_function()  -> CRASH at +0x48

J. Code Dump Decode (Instruction-by-Instruction)
Code: f9400693 b4000073 f9401663 f9400a84 (f9401660)
                                           ^^^^^^^^^^
                                           Faulting instruction (in parentheses!)

| Addr Offset | Hex | Assembly | Meaning |
| PC-0x10 | f9400693 | LDR X19, [X20, #0x8] | Load X19 from *(X20+8) - loading the suspect pointer |
| PC-0x0C | b4000073 | CBZ X19, <skip> | If X19==NULL branch ahead - NULL check PRESENT but missed this instance! |
| PC-0x08 | f9401663 | LDR X3, [X19, #0x28] | Earlier access to X19+0x28 - happened before our fault point |
| PC-0x04 | f9400a84 | LDR X4, [X20, #0x10] | Load from X20+16 (different struct, fine) |
| PC (FAULT) | (f9401660) | LDR X0, [X19, #0x28] | FAULTING: Load X0 from X19+0x28. X19=0x0 (NULL), so address=0x28 -> PAGE FAULT! |


f9401660 -> LDR X0, [X19, #0x28]
  X19 = 0x0000000000000000 (NULL)
  Offset = 0x28 (40 bytes)
  Effective address = 0x0 + 0x28 = 0x0000000000000028  <- matches fault address!

Root cause: X19 holds a pointer to a struct that is NULL.
The code attempts to read struct->member_at_0x28.

3. ARM32 (AArch32) Stack Dump - Complete Example
[   87.123456] Unable to handle kernel NULL pointer dereference at virtual address 0000001c
[   87.123460] pgd = (ptrval)
[   87.123463] [0000001c] *pgd=00000000
[   87.123470] Internal error: Oops: 5 [#1] PREEMPT SMP ARM
[   87.123478] Modules linked in: wlan(O) my_module(O)
[   87.123485] CPU: 2 PID: 987 Comm: my_process Tainted: G        W  O  4.19.157 #1
[   87.123489] Hardware name: Qualcomm Technologies, Inc. QCS405 (DT)
[   87.123495] PC is at my_arm32_function+0x2c/0x80 [my_module]
[   87.123500] LR is at my_arm32_caller+0x44/0x6c [my_module]
[   87.123504] pc : [<bf012a2c>]    lr : [<bf012b44>]    psr: 60000013
[   87.123508] sp : c1a45e80  ip : 00000000  fp : c1a45ea4
[   87.123512] r10: c0e89000  r9 : c1a44000  r8 : c0456780
[   87.123516] r7 : 00000001  r6 : d5678900  r5 : d1234000  r4 : 00000000
[   87.123520] r3 : 00000000  r2 : 0000001c  r1 : 00000000  r0 : 00000000
[   87.123524] Flags: nZCv  IRQs on  FIQs on  Mode SVC_32  ISA ARM  Segment none
[   87.123530] Control: 10c5387d  Table: 6520406a  DAC: 00000051
[   87.123534] Process my_process (pid: 987, stack limit = 0x(ptrval))
[   87.123540] Stack: (0xc1a45e80 to 0xc1a46000)
[   87.123544] 5e80: d1234000 d5678900 00000001 c0456780 c1a44000 c0e89000 c1a45ecc
[   87.123550] 5ea0: c1a45ea8 bf012b44 bf012a00 60000013 ffffffff c1a45ed4 d1234000
[   87.123580] Backtrace:
[   87.123584]  [<bf012a00>] (my_arm32_function) from [<bf012b44>] (my_arm32_caller+0x44/0x6c)
[   87.123598]  [<bf012b00>] (my_arm32_caller) from [<c0234568>] (process_one_work+0x268/0x380)
[   87.123610]  [<c0234300>] (process_one_work) from [<c0234abc>] (worker_thread+0x4c/0x380)
[   87.123618]  [<c0234a70>] (worker_thread) from [<c023a120>] (kthread+0x130/0x148)
[   87.123626]  [<c0239ff0>] (kthread) from [<c02010b4>] (ret_from_fork+0x14/0x20)
[   87.123632] Code: e5954010 e5940008 e3500000 0a000005 (e590001c)

4. ARM32 Register Breakdown
A. Full ARM32 Register Role Table (R0-R15 / AAPCS32)
pc : [<bf012a2c>]  lr : [<bf012b44>]  psr: 60000013
sp : c1a45e80  ip : 00000000  fp : c1a45ea4
r10: c0e89000  r9 : c1a44000  r8 : c0456780
r7 : 00000001  r6 : d5678900  r5 : d1234000  r4 : 00000000
r3 : 00000000  r2 : 0000001c  r1 : 00000000  r0 : 00000000

| Reg | Alias | ABI Role | Caller/Callee | Value | Meaning |
| R0 | a1 | Arg1 / Return | Caller-saved | 0x00000000 | First arg or return value |
| R1 | a2 | Arg 2 | Caller-saved | 0x00000000 | Second argument |
| R2 | a3 | Arg 3 | Caller-saved | 0x0000001c | Third arg - matches fault offset 0x1c! |
| R3 | a4 | Arg 4 | Caller-saved | 0x00000000 | Fourth argument |
| R4 | v1 | Variable 1 | CALLEE-SAVED | 0x00000000 (NULL!) | NULL POINTER - the bad struct pointer in our example |
| R5 | v2 | Variable 2 | CALLEE-SAVED | 0xd1234000 | Valid kernel pointer |
| R6 | v3 | Variable 3 | CALLEE-SAVED | 0xd5678900 | Valid kernel pointer |
| R7 | v4 | Variable 4 | CALLEE-SAVED | 0x00000001 | Counter or boolean flag |
| R8 | v5 | Variable 5 | CALLEE-SAVED | 0xc0456780 | Kernel data pointer |
| R9 | v6/SB | Var6/Static Base | CALLEE-SAVED | 0xc1a44000 | Thread_info base (Linux ARM32 uses this) |
| R10 | v7/SL | Var7/Stack Limit | CALLEE-SAVED | 0xc0e89000 | Stack limit / kernel data |
| R11 | FP | Frame Pointer | CALLEE-SAVED | 0xc1a45ea4 | Points to current stack frame for unwinding |
| R12 | IP | Intra-proc scratch | Caller-saved | 0x00000000 | Scratch register for linker veneers |
| R13 | SP | Stack Pointer | Special | 0xc1a45e80 | Current top of kernel stack |
| R14 | LR | Link Register | Special | 0xbf012b44 | Return address (caller) |
| R15 | PC | Program Counter | Special | 0xbf012a2c | CRASH LOCATION - current executing instruction |


B. ARM32 PSR (CPSR) Decode
psr: 60000013
Flags: nZCv  IRQs on  FIQs on  Mode SVC_32  ISA ARM  Segment none

CPSR Bit Layout:
+---+---+---+---+---+---+---+---+---+---+---+---+-------+---------+
| N | Z | C | V | Q |   | J |   | GE| E | A | I |   F   |  Mode   |
|31 |30 |29 |28 |27 |   |24 |   |19 | 9 | 8 | 7 |   6   |  4:0    |
+---+---+---+---+---+---+---+---+---+---+---+---+-------+---------+

0x60000013 = 0110 0000 0000 0000 0000 0000 0001 0011

| Bit(s) | Field | Value | Meaning |
| 31 | N | 0 | Negative: NOT set (n) - last result was not negative |
| 30 | Z | 1 (Z) | Zero: SET - last comparison result was equal |
| 29 | C | 1 (C) | Carry: SET - carry/borrow occurred |
| 28 | V | 0 (v) | Overflow: NOT set - no signed overflow |
| 9 | E | 0 | Endianness: Little-endian mode |
| 8 | A | 0 | Async abort NOT masked - SError interrupts are enabled |
| 7 | I | 0 | IRQ NOT masked - interrupts are enabled (normal kernel execution) |
| 6 | F | 0 | FIQ NOT masked - fast interrupts enabled |
| 5 | T | 0 | ISA: ARM state (T=1 would be Thumb mode) |
| 4:0 | Mode | 10011 (0x13) | SVC mode (Supervisor = Kernel). See mode table below. |


ARM32 Processor Mode Table
| Mode Bits | Mode (Hex) | Meaning |
| 10000 | USR (0x10) | User mode - unprivileged, cannot access hardware directly |
| 10001 | FIQ (0x11) | Fast Interrupt mode - dedicated banked registers for low-latency ISR |
| 10010 | IRQ (0x12) | Normal Interrupt mode - standard interrupt service routine |
| 10011 | SVC (0x13) | Supervisor/Kernel mode - our crash occurred here! |
| 10111 | ABT (0x17) | Abort mode - entered on data/prefetch abort faults |
| 11011 | UND (0x1B) | Undefined Instruction mode - illegal instruction exception |
| 11111 | SYS (0x1F) | System mode - privileged with user-mode register bank |
| 11010 | HYP (0x1A) | Hypervisor mode - ARMv7-VE virtualization extension |


C. ARM32 Control Register / Page Table / DAC Decode
Control: 10c5387d  Table: 6520406a  DAC: 00000051

| Field | Value | Meaning |
| Control (SCTLR) | 0x10c5387d | System Control Register: MMU enabled (bit0=1), caches enabled (C=1, I=1), alignment check active, vector table in high memory |
| Table (TTBR0) | 0x6520406a | Translation Table Base Register 0 - physical address of the current process page table. Low bits (0x6a) are flags: inner/outer cacheable + shareable attributes. |
| DAC (DACR) | 0x00000051 | Domain Access Control Register. 0x51 = 0b01010001 -> Domain0:Client(01), Domain1:Client(01), Domain3:NoAccess. Client=use page table permissions. |


D. ARM32 Code Dump Decode
Code: e5954010 e5940008 e3500000 0a000005 (e590001c)

| Instruction | Encoding | Assembly | Meaning |
| PC-0x10 | e5954010 | LDR R4, [R5, #0x10] | Load R4 from *(R5+16) - loading the NULL pointer into R4 |
| PC-0x0C | e5940008 | LDR R0, [R4, #0x8] | Load R0 from *(R4+8) - R4=NULL, so R0=garbage/0 (may succeed in some cases) |
| PC-0x08 | e3500000 | CMP R0, #0 | Compare R0 with 0 (NULL check) |
| PC-0x04 | 0a000005 | BEQ <skip> | Branch-if-equal (NULL check) - NOT taken, meaning R0 was non-zero OR conditional check failed |
| PC (FAULT) | (e590001c) | LDR R0, [R0, #0x1C] | FAULT: R0 holds NULL, accesses NULL+0x1C = 0x0000001c - matches fault address! |


5. Stack Memory Analysis
A. ARM32 Stack Frame Layout
Stack: (0xc1a45e80 to 0xc1a46000)
5e80: d1234000 d5678900 00000001 c0456780 c1a44000 c0e89000 c1a45ecc
5ea0: c1a45ea8 bf012b44 bf012a00 60000013 ffffffff c1a45ed4 d1234000

Higher addresses (stack bottom: 0xc1a46000 = thread_info + THREAD_SIZE)
+----------------------------------------------------+
| ... previous frames ...                            |
+----------------------------------------------------+ <- Previous FP (R11) points here
| Saved LR (return addr of prev function)            | [FP + 4]
| Saved FP (previous frame pointer)                  | [FP + 0]
+----------------------------------------------------+ <- Current FP (R11 = 0xc1a45ea4)
| Saved LR                                           |
| Saved FP                                           |
| Saved callee-saved regs (R4-R10 as needed)         |
| Local variables and temporaries                    |
| (grows downward)                                   |
+----------------------------------------------------+ <- Current SP (R13 = 0xc1a45e80)
| (next push goes here - stack grows toward low addr)|
Lower addresses

B. ARM64 Stack Frame Layout
Higher addresses (stack grows DOWN from thread top)
+----------------------------------------------------+
| (Previous frame data)                              |
|                                                    |
+----------------------------------------------------+ <- X29 (FP) of previous frame
| Saved X30 (LR) - return addr of previous function  | [X29 + 8]
| Saved X29 (FP) - frame pointer of caller           | [X29 + 0]
+----------------------------------------------------+ <- Current X29 (FP = 0xffffffc01a3cbc80)
| Saved X30 (LR) - our return address (to caller)    | [X29 + 8]
| Saved X29 (FP) - previous frame pointer (chain)    | [X29 + 0]
| Saved callee regs: (X19,X20), (X21,X22), ...       | [X29 - 16] etc
| Local variables / temporaries                      |
| (grows downward, 16-byte aligned)                  |
+----------------------------------------------------+ <- Current SP (0xffffffc01a3cbc80)
| (must be 16-byte aligned at all times in AArch64)  |
Lower addresses

6. Special Kernel Pointers Cheat Sheet
ARM64 Address Ranges & Poison Patterns
| Address / Pattern | What It Is |
| 0x0000000000000000 | NULL pointer - most common kernel bug |
| 0x0000000000000000 - 0x0000FFFF | NULL dereference zone - any access here is a NULL + small offset bug |
| 0x0000xxxxxxxxxxxx | User-space virtual addresses (48-bit address space) |
| 0xFFFF800000000000+ | Kernel linear map start (PAGE_OFFSET) - direct mapped physical memory |
| 0xFFFFC00000000000+ | vmalloc/ioremap region + kernel image - non-contiguous virtual allocations |
| 0xFFFFE00000000000+ | Modules region - where .ko modules are loaded |
| DEAD000000000100 | LIST_POISON1: next pointer in a freed list_head - use-after-free if seen as pointer |
| DEAD000000000122 | LIST_POISON2: prev pointer in a freed list_head - guaranteed invalid address |
| 6B6B6B6B6B6B6B6B | SLAB_FREED (SLUB debug poison) - memory was freed, now accessed = use-after-free |
| 5A5A5A5A5A5A5A5A | SLAB_RED_ZONE: slab red zone marker, overflow if seen in adjacent region |
| A5A5A5A5A5A5A5A5 | POISON_INUSE: allocated but uninitialized slab object content |


ARM32 Address Ranges
| Range | What It Is |
| 0x00000000 - 0x0000FFFF | NULL dereference zone |
| 0x00000000 - 0xBFFFFFFF | User-space (3G/1G split: 3GB user, 1GB kernel) |
| 0xBF000000 - 0xBFFFFFFF | Modules region |
| 0xC0000000 - 0xFFFFFFFF | Kernel space (PAGE_OFFSET = 0xC0000000) |
| 0xF0000000 - 0xFEFFFFFF | vmalloc/ioremap region |
| 0xFFFF0000 - 0xFFFF0FFF | Exception vectors (high vectors) |
| 0x6B6B6B6B | SLAB_FREED poison (use-after-free pattern) |
| 0xDEAD0100 | LIST_POISON1 (ARM32 version) |
| 0xDEAD0122 | LIST_POISON2 (ARM32 version) |


7. Analysis Flowchart (6-Step Process)
+---------------------------------------------------------------+
| STEP 1: Read the fault description                            |
|   -> What address? What type? (NULL deref, page fault, etc.)  |
|   -> Is it a user or kernel address?                          |
|   -> Does the offset match a common struct member?            |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
| STEP 2: Decode ESR (ARM64) or FSR/Oops code (ARM32)           |
|   -> Read or Write fault? (WnR field)                         |
|   -> Translation or Permission fault? (FSC field)             |
|   -> Which page table level failed?                           |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
| STEP 3: Identify the faulting instruction (Code: line)        |
|   -> Instruction in parentheses = the one that faulted        |
|   -> Decode: which register was base? what offset?            |
|   -> Cross-reference with register dump                       |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
| STEP 4: Find the NULL / bad pointer in registers              |
|   -> Look at callee-saved regs first (X19-X28 / R4-R11)       |
|   -> Which one is NULL, 0x6B6B... (freed), or DEAD00...?      |
|   -> These are most reliable - preserved across function calls |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
| STEP 5: Walk the call trace (bottom to top = execution order) |
|   -> Understand the code path that led to the crash           |
|   -> Where was the bad pointer supposed to be initialized?    |
|   -> Look for race conditions, error paths, use-after-free    |
+---------------------------------------------------------------+
                              |
                              v
+---------------------------------------------------------------+
| STEP 6: Map to source code                                    |
|   -> addr2line -e vmlinux <PC_address>                        |
|   -> pahole -C struct_name vmlinux   (find member at offset)  |
|   -> objdump -d vmlinux | grep -A20 <function_name>           |
+---------------------------------------------------------------+

8. Using pahole to Find Struct Offset
When you have a fault address like 0x28 (or 0x1c for ARM32), use pahole to identify which struct member was accessed:
# Find which struct member is at offset 0x28
$ pahole -C device vmlinux

struct device {
    struct kobject              kobj;           /*   0  0x40 */
    struct device             *parent;          /*0x40   0x8 */
    struct device_private     *p;               /*0x48   0x8 */
    const char                *init_name;       /*0x28   0x8 */  <- OFFSET 0x28!
    const struct device_type  *type;            /*0x30   0x8 */
    ...
};

# Conclusion: The NULL pointer points to struct device.
# Code tried to access device->init_name (at offset 0x28).
# Fix: Check and add NULL guard before dereferencing the device pointer.

# Other useful commands:
$ addr2line -e vmlinux -f <PC_address>    # Source file + line + function name
$ objdump -d vmlinux | grep -A 30 "<my_buggy_function>"  # Disassembly
$ nm vmlinux | grep my_buggy_function     # Symbol address lookup


| SECTION B: Interview Summary - Complete Cheat Sheet |


Crash Dump Analysis - Interview Cheat Sheet
Qualcomm Staff Engineer Level | Sandeep

B1. Types of Crash Data
Kernel-Level Crashes
| Data Type | What It Is | Key Tool |
| Full Ramdump | Entire DDR contents captured via warm reset path (GBs of data) | T32, CrashScope, Ramparse |
| Minidump | Only pre-registered memory regions (lightweight, 50-200MB) | Ramparse, CrashScope |
| kdump/vmcore | Memory captured by kexec secondary kernel after panic | crash utility, GDB |
| pstore/ramoops | Last dmesg + ftrace preserved in persistent RAM across reboot | /sys/fs/pstore/ |
| Kernel Oops/Panic log | Registers, backtrace, faulting instruction in kernel log | dmesg / last_kmsg |


User-Level Crashes
| Data Type | What It Is | Key Tool |
| Core dump | Process memory + registers at crash time (ELF format) | GDB, eu-readelf |
| Tombstone (Android) | Registers, backtrace, memory around regs, logcat snippets | debuggerd, ndk-stack |
| /proc/PID/ info | maps, smaps, status, stack, wchan - process memory & state | Manual / scripts |


B2. Qualcomm Ramdump vs. Minidump
| Aspect | Full Ramdump | Minidump |
| Size | Entire DDR (2GB-12GB+) | 50-200MB typical |
| Collection Time | Minutes (USB limited) | Seconds to ~1 min |
| Content | ALL physical memory (kernel + user) | Only pre-registered regions |
| Production Use | NO - too large/slow | YES - enabled in production |
| Infrastructure | Sahara protocol, QPST/QFIL | Sahara + minidump table |
| Analysis Tools | T32, crash, CrashScope, Ramparse | Ramparse, CrashScope |
| Kernel Config | CONFIG_QCOM_DLOAD_MODE=y | CONFIG_QCOM_MINIDUMP=y |


Interview Answer - Ramdump vs Minidump
| "Full ramdump captures the entire DDR (2-12GB+) via Sahara protocol after a warm reset. The PMIC keeps DDR in self-refresh so contents survive the reset. It takes minutes and is used for complex crashes in development builds. Minidump captures only pre-registered regions (50-200MB) via msm_minidump_add_region() API. Regions include: kernel log, CPU contexts, task stacks, run queues. The bootloader reads the minidump table from SMEM/IMEM and transfers via Sahara. Fast (seconds), small, production-safe - the preferred choice for field deployments." |


B3. Qualcomm Crash Flow
One-Liner
Panic -> Flush caches -> Set IMEM cookie -> Warm reset -> Bootloader detects cookie
      -> Enters Sahara mode -> Dumps memory to host

Detailed 5-Stage Flow
| # | Stage | Details |
| 1 | Crash Event | Kernel panic() / die() / watchdog bite. Panic notifier chain executes. |
| 2 | Panic Handler | Flushes caches (DDR coherent), sets IMEM download mode magic cookie (0x12345678), writes CPU context to minidump region, disables MMU+caches, triggers warm reset via PSHOLD. |
| 3 | Hardware Reset | PMIC PON detects PSHOLD change. Performs WARM reset (DDR stays powered in self-refresh mode - memory contents PRESERVED). |
| 4 | Bootloader (XBL) | Reads IMEM cookie: detects download mode magic. Checks dload_type (full vs. mini). DDR still contains crash-time memory! Full: maps all DDR and transfers. Mini: reads minidump table from SMEM, iterates registered entries, transfers only those regions. |
| 5 | Host Collection | QPST/QFIL/custom tool receives Sahara packets. Writes .BIN files to disk. Generates DDRCS_info.txt with address mapping metadata. |


B4. Stack Dump Key Registers Table (ARM64)
| Register | Role | Interview Tip |
| PC | Program Counter - exact crash instruction | "First thing I look at - tells me exactly WHERE the crash occurred" |
| LR (X30) | Link Register - return address / caller | "Tells me WHO called the crashing function - reveals the call path" |
| SP | Stack Pointer - current stack top | "Used to validate stack integrity and read local variables from raw memory" |
| X29 (FP) | Frame Pointer - stack frame chain | "Used for stack frame unwinding when iterating the call chain manually" |
| X19-X28 | Callee-saved - preserved across calls | "MOST RELIABLE for finding the bad pointer - these hold struct pointers and are not clobbered by callees" |
| X0-X7 | Arguments / return value - caller-saved | "May be overwritten by any function call - less reliable but useful at exact crash point" |
| PSTATE | Condition flags + exception mask + EL | "Tells me IRQ state (daif), Exception Level (EL1=kernel), PAN/TCO flags" |


B5. ESR_EL1 Key Fields
| Field | Bits | What to Tell the Interviewer |
| EC | [31:26] | Exception Class. 0x25 = Data Abort from current EL (EL1 = kernel). Key for identifying fault type. |
| IL | [25] | Instruction Length. 1 = 32-bit A64 instruction (always 1 in AArch64 kernel) |
| WnR | [6] in ISS | Write not Read. WnR=0 means READ fault, WnR=1 means WRITE fault. Helps narrow down read vs write corruption. |
| FSC | [5:0] | Fault Status Code. 0x04-0x07 = translation faults (NULL/unmapped). 0x0D-0x0F = permission faults. 0x10 = bus error. 0x21 = alignment. |
| S1PTW | [7] | Stage 1 page table walk fault. Important for hypervisor/SMMU debugging. |


B6. "How I Analyze a Stack Dump" - 6-Step Workflow
When I see a kernel crash, I follow these 6 steps (answer this confidently in your interview):

| # | Step | What I Do & What I Say |
| 1 | Read the fault address | Is it NULL+offset? Poison pattern (0x6B6B...)? User or kernel address? Low address (0x0-0xFFF) = NULL deref. DEAD0100 = freed list. This tells me what TYPE of bug I am dealing with. |
| 2 | Decode ESR / error code | Read or Write fault? (WnR). Translation or Permission? (FSC). Which PTE level failed? This tells me HOW the fault happened (no PTE, or PTE denied). |
| 3 | Decode faulting instruction | Look at "Code:" line. Instruction in (parentheses) = the one that faulted. Decode: which register was base? what offset? Cross-reference with register dump to find the bad value. |
| 4 | Find the bad pointer | Look at callee-saved regs (X19-X28 or R4-R11) FIRST - they are preserved across calls and hold the actual bad pointer. Which one is NULL or has poison? That is my prime suspect. |
| 5 | Walk the call trace | Read bottom-to-top (execution order). Understand HOW we got here. Where was the bad pointer supposed to be initialized? Is there a race condition or error path that skips initialization? |
| 6 | Map to source code | addr2line -e vmlinux <PC>. pahole -C struct_name vmlinux (find member at offset). Form hypothesis, code review the allocation/initialization path, then propose fix. |


B7. Reading Code Dump Line
Code: f9400693 b4000073 f9401663 f9400a84 (f9401660)
                                           ^^^^^^^^^^
                                           Faulting instruction (parentheses)

f9401660 -> Decode with objdump/capstone: LDR X0, [X19, #0x28]
  X19 = 0x0000000000000000 (NULL)
  Offset = 0x28
  Effective address = 0x0 + 0x28 = 0x0000000000000028  <- matches fault addr!

| Interview Quote: "I decode the faulting instruction to identify which register held the badpointer, then cross-reference with the register dump to find the NULL or corrupted value.The offset (0x28) from the faulting instruction must match the fault address reported at thetop of the oops. Once confirmed, I use pahole to identify the exact struct member." |


B8. Key Crash Patterns
| Pattern | Signature / Indicators | My Analysis Approach |
| NULL deref | Fault at 0x0 to 0xFFF. FSC=0x05 (level 1 translation). NULL+small offset. | Find callee-saved reg = 0. Use pahole to identify struct member at fault offset. Trace where pointer should have been initialized. |
| Use-after-free | 0x6B6B6B6B pattern in registers or fault address. SLUB_DEBUG: alloc/free traces. | Identify slab cache. Check KASAN report for exact alloc/free stacks. Look for missing mutex/refcount protecting the object lifetime. |
| Stack overflow | Corruption at thread_info boundary. "stack-protector: Kernel stack is corrupted". Very deep backtrace. | Check stack frame sizes (bt -F in crash). Look for deep recursion, large stack locals. Check VMAP_STACK guard page hit. |
| Deadlock/Lockup | "BUG: soft lockup - CPU#X stuck for XXs!". All CPUs blocked. Watchdog bite. | bt -a (all CPUs). Check spinlock holders. Look for circular wait: CPU0 holds lock A, waits for B; CPU1 holds B, waits for A. IRQ-disabled paths. |
| List corruption | Fault at 0xDEAD0100 or 0xDEAD0122. LIST_POISON1/2 patterns. | A freed list_head was accessed. Find which list and which object. Look for double-free or missing list_del before kfree. |


B9. Tools I Use
| Tool | Purpose & Key Commands |
| T32 (Lauterbach) | Load ramdump+vmlinux, browse memory, source-level debug, MMU page table walk, register view, frame unwinding |
| CrashScope | Qualcomm GUI tool: auto-parses crash reason, per-CPU backtraces, task list, workqueues, IRQs, scheduler info from ramdump |
| Ramparse | Python-based parser: generates dmesg.txt, tasks.txt, backtrace.txt, irq.txt, workqueues.txt, rtb.txt, dcc.txt from ramdump/minidump |
| crash utility | Interactive CLI for vmcore: "bt" (backtrace), "ps" (process list), "kmem -s" (slab), "log" (dmesg), "rd/wr" (memory read/write), "dis" (disassemble) |
| addr2line | addr2line -e vmlinux -f <PC_address> -> converts crash address to source file:line:function |
| pahole | pahole -C struct_name vmlinux -> dumps struct layout with offsets to identify which member was at the fault offset |
| objdump -d | Full disassembly for instruction flow analysis. "objdump -d vmlinux | grep -A30 <func_name>" for local context |


B10. DCC (Data Capture and Compare)
| Interview Quote on DCC:"DCC is a hardware debug block on Qualcomm SoCs that autonomously captures pre-programmed register values at crash time without requiring software intervention after configuration. It is invaluable for debugging NOC errors, clock/power state, and peripheral register state at the exact moment of a crash - even when the CPU itself is hung and cannot run code. DCC results are stored in DCC_SRAM and automatically included in minidumps. DCC is configured at boot with a list of registers to capture (via Device Tree or sysfs). Typical registers: GCC clock gates, NOC error status, BIMC/LLCC state, TLMM GPIO, RPM status." |


B11. Subsystem Crashes (Modem/ADSP/CDSP)
Qualcomm SoCs have multiple independent processors that can each crash:

| Processor | Dump Mechanism | Analysis Tool | Storage Location |
| Apps (HLOS) | Full ramdump / Minidump | CrashScope, T32, crash | USB via Sahara |
| Modem (MPSS) | SSR dump via PIL | T32 + Modem symbols | /data/vendor/ssrdump/ |
| ADSP | SSR dump via PIL | T32 + Hexagon tools | /data/vendor/ssrdump/ |
| CDSP | SSR dump via PIL | T32 + Hexagon tools | /data/vendor/ssrdump/ |
| TZ (Secure) | Limited TZBSP logs | Special TZ access only | Restricted |
| RPM/AOP | RPM dump region | T32 + RPM symbols | In ramdump / minidump |


When a subsystem crashes: SSR (Subsystem Restart) collects the dump via PIL (Peripheral Image Loader) before restarting just that subsystem. Analyzed with T32 + subsystem-specific symbols (Hexagon tools for DSP).

B12. Quick One-Liners for Interview
Practice these until they are automatic:

| Question | Your Answer |
| "What is a ramdump?" | "A binary snapshot of the entire DDR captured after a warm reset. The PMIC keeps DDR powered in self-refresh, so crash-time memory contents are preserved for post-mortem analysis." |
| "Full dump vs. minidump?" | "Full captures everything (GBs, minutes, dev builds only). Minidump captures only registered critical regions (MBs, seconds, production-safe). Registered via msm_minidump_add_region() API." |
| "How does warm reset preserve memory?" | "The PMIC holds DDR in self-refresh during the warm reset cycle. Since the DDR power rails stay up, the DRAM retains its contents. The bootloader then reads this memory before handing control to the OS again." |
| "What is the first thing you look at?" | "The fault address and PC. Fault address tells me what memory was accessed (NULL? poison? valid kernel addr?). PC tells me exactly where in the code we crashed. Then ESR to decode read vs write and fault type." |
| "How do you find the struct member?" | "I use pahole -C struct_name vmlinux and look for the member at the fault offset. For example if fault is at 0x28, I search for which struct member starts at offset 0x28 to identify what was NULL-dereferenced." |
| "What if you cannot reproduce it?" | "I rely entirely on the dump. Decode instructions, walk the stack manually (pahole + addr2line + objdump), check for race conditions in code review. Enable KASAN/lockdep for next occurrence to get automatic stack traces." |


| Key Differentiators for Staff Engineer Interview1. Show DEPTH - Decode ESR bits, know FSC values, explain warm reset DDR retention mechanism2. Show WORKFLOW - Structured 6-step approach, not ad-hoc or "I just look at logs"3. Show QUALCOMM-SPECIFIC knowledge - IMEM cookies, Sahara protocol, DCC, minidump table, SSR4. Show TOOL MASTERY - T32 CMM scripts, crash utility commands, Ramparse, pahole, addr2line5. Show PATTERN RECOGNITION - NULL deref, UAF (0x6B), list poison (DEAD00), stack overflow |


End of Part 3 of 3 | Crash Dump Analysis Guide
Part 1: Crash Data Types & Analysis Workflow | Part 2: Qualcomm Ramdump & Minidump Deep Dive

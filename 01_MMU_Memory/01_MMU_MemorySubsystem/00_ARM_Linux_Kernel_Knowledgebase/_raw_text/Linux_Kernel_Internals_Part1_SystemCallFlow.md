


Linux Kernel Internals
Part 1: System Call Flow
ARM32 and ARM64 — Complete Deep Dive


Prepared for: Sandeep Kumar
Role: Senior Embedded Linux Engineer
Target: Qualcomm Interview Preparation

Coverage: System Call Flow (ARM32 & ARM64) • VFS to Driver Routing • read/write/ioctl • Platform Driver


| CONFIDENTIAL — INTERVIEW PREPARATION DOCUMENT |

1. System Call Flow — ARM64 (AArch64)
This section covers the complete, low-level walkthrough of the Linux system call flow on ARM64, tracing every step and function from user space through hardware exception handling and back.
1.1  User Space: Invoking the System Call
Step 1a: Application Calls the glibc Wrapper
When your application calls a function like open(), the call goes first to a glibc wrapper function, not directly to the kernel.
// User code
int fd = open("/dev/mydevice", O_RDONLY);
The glibc wrapper (e.g., in sysdeps/unix/sysv/linux/open.c) does the following:
- Places the syscall number into the designated register (x8 on ARM64)
- Places the arguments into argument registers (x0–x5 on ARM64)
- Executes the SVC #0 supervisor call instruction
Step 1b: The SVC #0 Instruction
; ARM64 (AArch64)
MOV     x8, #56         ; __NR_openat = 56
MOV     x0, ...         ; arg1: dfd (AT_FDCWD)
MOV     x1, ...         ; arg2: filename pointer
MOV     x2, ...         ; arg3: flags
SVC     #0              ; Supervisor Call (triggers exception)
What SVC #0 Does at the Hardware Level (ARM64)
When the CPU executes SVC #0, the following happen atomically:
- CPU switches from EL0 (User) → EL1 (Kernel)
- Saves the return address in ELR_EL1 (Exception Link Register)
- Saves the processor state (PSTATE) in SPSR_EL1
- Disables interrupts (DAIF masked)
- Jumps to the exception vector table entry for synchronous exceptions at VBAR_EL1 + 0x400
1.2  Exception Vector Table Entry
Step 2a: ARM64 Vector Table Layout
The vector table base is set in VBAR_EL1. The ARM64 vector table has 16 entries of 128 bytes each. For a syscall from user space (EL0 → EL1), the CPU jumps to offset 0x400.
Offset    Exception Type                  Source
------    --------------                  ------
0x000     Synchronous                     Current EL, SP_EL0
0x080     IRQ                             Current EL, SP_EL0
0x100     FIQ                             Current EL, SP_EL0
0x180     SError                          Current EL, SP_EL0
0x200     Synchronous                     Current EL, SP_ELx
0x280     IRQ                             Current EL, SP_ELx
...
0x400     Synchronous (from EL0)   <-- THIS ONE for syscalls
0x480     IRQ (from EL0)
0x500     FIQ (from EL0)
0x580     SError (from EL0)
Step 2b: el0_sync Handler (ARM64)
File: arch/arm64/kernel/entry.S
el0_sync:
    kernel_entry 0              ; Save all user registers to pt_regs on kernel stack
    mrs     x25, esr_el1        ; Read Exception Syndrome Register (ESR)
    lsr     x24, x25, #26       ; Extract Exception Class (EC) field (bits [31:26])
    cmp     x24, #0x15          ; EC=0x15 means SVC from AArch64
    b.eq    el0_svc             ; Branch to syscall handler
    ...                         ; Other exception types (data abort, prefetch abort, etc.)
Interview Tip: The ESR_EL1 Exception Class (EC) field distinguishes why the synchronous exception occurred. EC=0x15 = SVC from AArch64. EC=0x25 = data abort from lower EL.
1.3  Saving User Context: kernel_entry Macro
Step 3: The kernel_entry Macro (arch/arm64/kernel/entry.S)
This macro saves the complete user-space CPU state onto the kernel stack as a struct pt_regs:
.macro kernel_entry, el
    sub     sp, sp, #S_FRAME_SIZE       ; Allocate pt_regs on kernel stack
    stp     x0, x1, [sp, #16 * 0]      ; Save x0, x1
    stp     x2, x3, [sp, #16 * 1]      ; Save x2, x3
    stp     x4, x5, [sp, #16 * 2]      ; Save x4, x5
    stp     x6, x7, [sp, #16 * 3]      ; Save x6, x7
    stp     x8, x9, [sp, #16 * 4]      ; Save x8 (syscall#), x9
    stp     x10, x11, [sp, #16 * 5]    ; Save x10, x11
    stp     x12, x13, [sp, #16 * 6]    ; Save x12, x13
    stp     x14, x15, [sp, #16 * 7]    ; Save x14, x15
    stp     x16, x17, [sp, #16 * 8]    ; Save x16, x17
    stp     x18, x19, [sp, #16 * 9]    ; Save x18, x19
    stp     x20, x21, [sp, #16 * 10]   ; Save x20, x21
    stp     x22, x23, [sp, #16 * 11]   ; Save x22, x23
    stp     x24, x25, [sp, #16 * 12]   ; Save x24, x25
    stp     x26, x27, [sp, #16 * 13]   ; Save x26, x27
    stp     x28, x29, [sp, #16 * 14]   ; Save x28, x29 (FP)

    mrs     x22, elr_el1                ; Save return address (PC)
    mrs     x23, spsr_el1               ; Save saved program status (PSTATE)
    stp     lr, x21, [sp, #S_LR]       ; Save LR
    stp     x22, x23, [sp, #S_PC]      ; Save PC and PSTATE

    ; Save user SP
    mrs     x21, sp_el0
    str     x21, [sp, #S_SP]
.endm
struct pt_regs (arch/arm64/include/asm/ptrace.h)
struct pt_regs {
    union {
        struct user_pt_regs user_regs;
        struct {
            u64 regs[31];   // x0-x30
            u64 sp;         // stack pointer (from SP_EL0)
            u64 pc;         // program counter (from ELR_EL1)
            u64 pstate;     // processor state (from SPSR_EL1)
        };
    };
    u64 orig_x0;            // original x0 (for syscall restart)
    s32 syscallno;          // syscall number (from x8)
    u64 sdei_ttbr1;         // SDEI TTBR1
};
Key: orig_x0 saves the original first argument. If a signal interrupts the syscall and it needs to be restarted, orig_x0 is used to restore x0 to its original value.
1.4  System Call Dispatch
Step 4a: el0_svc (Assembly Entry)
File: arch/arm64/kernel/entry.S
el0_svc:
    mov     x0, sp                  ; x0 = pointer to pt_regs
    bl      el0_svc_handler         ; Call C handler
    b       ret_to_user             ; Return to user space
Step 4b: el0_svc_handler() (C Function)
File: arch/arm64/kernel/syscall.c
void el0_svc_handler(struct pt_regs *regs)
{
    el0_svc_common(regs, regs->regs[8],   // x8 = syscall number
                   __NR_syscalls,           // max syscall number
                   sys_call_table);         // syscall table pointer
}
Step 4c: el0_svc_common() — The Core Dispatch Logic
static void el0_svc_common(struct pt_regs *regs, int scno,
                           int sc_nr, const syscall_fn_t *syscall_table)
{
    unsigned long flags = current_thread_info()->flags;

    regs->orig_x0 = regs->regs[0];     // Save original arg0 for restart
    regs->syscallno = scno;              // Store syscall number

    /* ---- Syscall Entry Tracing ---- */
    if (flags & _TIF_SYSCALL_WORK)       // ptrace, seccomp, audit?
        scno = syscall_trace_enter(regs); // May modify syscall number

    /* ---- Invoke the actual syscall ---- */
    if (scno < sc_nr) {
        invoke_syscall(regs, scno, sc_nr, syscall_table);
    } else {
        regs->regs[0] = -ENOSYS;        // Invalid syscall number
    }

    /* ---- Syscall Exit Work ---- */
    syscall_trace_exit(regs);
}
Step 4d: invoke_syscall() — Table Lookup & Call
static void invoke_syscall(struct pt_regs *regs, unsigned int scno,
                           unsigned int sc_nr,
                           const syscall_fn_t *syscall_table)
{
    syscall_fn_t syscall_fn;

    // array_index_nospec: Spectre v1 mitigation -- bounds check
    // with speculation barrier to prevent out-of-bounds speculative access
    syscall_fn = syscall_table[array_index_nospec(scno, sc_nr)];

    regs->regs[0] = syscall_fn(regs);
    // Call the actual handler; return value goes into x0
}
Interview Critical: array_index_nospec() inserts a speculation barrier after bounds check, preventing Spectre v1 gadget from speculatively reading out-of-bounds syscall table entries.
1.5  The System Call Table
Step 5: sys_call_table[] (arch/arm64/kernel/sys.c)
// Generated from the syscall table definition
const syscall_fn_t sys_call_table[__NR_syscalls] = {
    [0]   = __arm64_sys_io_setup,
    [1]   = __arm64_sys_io_destroy,
    ...
    [56]  = __arm64_sys_openat,       // <-- our open() ends up here
    [57]  = __arm64_sys_close,
    [63]  = __arm64_sys_read,
    [64]  = __arm64_sys_write,
    ...
};
Each entry is defined using the SYSCALL_DEFINE macro (SYSCALL_DEFINEn where n = number of arguments):
// fs/open.c
SYSCALL_DEFINE3(openat, int, dfd, const char __user *, filename,
                int, flags)
{
    // ... actual implementation
}

// Macro expansion (simplified):
asmlinkage long __arm64_sys_openat(const struct pt_regs *regs)
{
    return __se_sys_openat(regs->regs[0],   // dfd
                           regs->regs[1],   // filename
                           regs->regs[2]);  // flags
}

static long __se_sys_openat(int dfd, const char __user *filename, int flags)
{
    return __do_sys_openat(dfd, filename, flags);
}
1.6  Return Path: From Kernel Back to User Space
Step 7a: ret_to_user — Exit Processing
File: arch/arm64/kernel/entry.S
ret_to_user:
    disable_daif                        ; Disable interrupts for atomic check
    ldr     x1, [tsk, #TSK_TI_FLAGS]   ; Load thread_info->flags
    and     x2, x1, #_TIF_WORK_MASK    ; Check for pending work
    cbnz    x2, work_pending            ; If work pending, handle it
    kernel_exit 0                       ; Restore user context & return
Step 7b: Pending Work Flags
Before returning to user space, the kernel checks thread_info->flags:
| Flag | Meaning | Action |
| TIF_NEED_RESCHED | Scheduler wants to preempt | Call schedule() |
| TIF_SIGPENDING | Signals pending for this task | Call do_signal() |
| TIF_NOTIFY_RESUME | Notification callbacks | Call tracehook_notify_resume() |
| TIF_SYSCALL_TRACE | ptrace tracing active | Call syscall_trace_exit() |
| TIF_UPROBE | Uprobe pending | Handle uprobe |


Step 7c: kernel_exit Macro — Restoring User Context
.macro kernel_exit, el
    ldp     x21, x22, [sp, #S_PC]      ; Restore PC and PSTATE
    msr     elr_el1, x21                ; Set return address
    msr     spsr_el1, x22               ; Set saved program status

    ldp     x0, x1, [sp, #16 * 0]      ; Restore x0 (return value), x1
    ldp     x2, x3, [sp, #16 * 1]      ; Restore x2, x3
    ldp     x4, x5, [sp, #16 * 2]      ; Restore x4, x5
    ...                                 ; (all remaining registers)
    ldp     x28, x29, [sp, #16 * 14]   ; Restore x28, x29 (FP)
    ldr     lr, [sp, #S_LR]            ; Restore LR
    add     sp, sp, #S_FRAME_SIZE      ; Deallocate pt_regs
    eret                                ; Exception Return
    ; CPU: EL1 -> EL0, restores PC from ELR_EL1, PSTATE from SPSR_EL1
.endm
Step 7d: The ERET Instruction
The ERET instruction performs atomically:
- Restores PC from ELR_EL1 (the instruction after SVC #0)
- Restores PSTATE from SPSR_EL1 (user mode PSTATE)
- Switches from EL1 (Kernel) back to EL0 (User)
- Execution resumes in user space right after the SVC #0 instruction
Step 8: glibc Post-Processing
// glibc wrapper (simplified)
long open(const char *pathname, int flags, ...)
{
    long ret = INLINE_SYSCALL(openat, 3, AT_FDCWD, pathname, flags);

    if (ret < 0) {
        errno = -ret;       // Set errno from negative return value
        return -1;
    }
    return ret;             // Return fd on success
}
1.7  Complete ARM64 Flow Diagram
USER SPACE (EL0)                          KERNEL SPACE (EL1)
----------------                          -----------------

app: open("/dev/x")
  |
  v
glibc: open() wrapper
  | MOV x8, #56 (__NR_openat)
  | MOV x0-x5 (args)
  | SVC #0
  |                          +-----------------------------------+
  | === HW EXCEPTION ======> | Vector Table @ VBAR_EL1 + 0x400  |
  |                          |         el0_sync:                 |
  |                          |           kernel_entry 0          |
  |                          |           (save all regs->pt_regs)|
  |                          |           mrs x25, esr_el1        |
  |                          |           EC==0x15? -> el0_svc    |
  |                          |                                   |
  |                          |  el0_svc_handler(pt_regs)         |
  |                          |    -> el0_svc_common()            |
  |                          |      -> syscall_trace_enter()     |
  |                          |      -> invoke_syscall()          |
  |                          |        -> sys_call_table[56]      |
  |                          |          -> __arm64_sys_openat()  |
  |                          |            -> do_sys_openat2()    |
  |                          |              -> VFS -> driver     |
  |                          |      -> syscall_trace_exit()      |
  |                          |                                   |
  |                          |  ret_to_user:                     |
  |                          |    check TIF_NEED_RESCHED -> sched |
  |                          |    check TIF_SIGPENDING -> signals |
  |                          |    kernel_exit 0                  |
  |                          |      (restore regs from pt_regs)  |
  | <======= ERET ========== |      ERET                         |
  |                          +-----------------------------------+
  v
glibc: check return value
  | if (ret < 0) errno = -ret
  v
app: fd = open(...)
1.8  Key Data Structures Summary (ARM64)
| Structure | Purpose | Location |
| pt_regs | Saved user CPU state on kernel stack | arch/arm64/include/asm/ptrace.h |
| thread_info | Per-thread flags (TIF_*) | arch/arm64/include/asm/thread_info.h |
| task_struct | Full process descriptor | include/linux/sched.h |
| sys_call_table[] | Array of syscall function pointers | arch/arm64/kernel/sys.c |
| ESR_EL1 | Exception Syndrome Register | ARM hardware register |
| ELR_EL1 | Exception Link Register (return PC) | ARM hardware register |
| SPSR_EL1 | Saved Program Status Register | ARM hardware register |


2. ARM32 (SWI) System Call Flow
This section covers the complete ARM32 (ARMv7) variant of the system call flow, covering every step from user space through the SWI exception into the kernel and back. This maps directly to i.MX6, i.MX8X, and legacy Qualcomm SoCs.
2.1  User Space: The SWI/SVC Instruction
Step 1a: ARM32 EABI Syscall Convention
On ARM32, the instruction is SWI 0 (Software Interrupt) — renamed to SVC 0 in the Unified Assembly Language (UAL), but the opcode is identical.
; ARM32 EABI syscall convention
MOV     r7, #5              ; __NR_open = 5 (syscall number in r7)
MOV     r0, r_filename      ; arg1: filename pointer
MOV     r1, r_flags         ; arg2: flags
MOV     r2, r_mode          ; arg3: mode
SVC     #0                  ; Software interrupt -> Supervisor mode
; (Old syntax: SWI #0)

; Return value is in r0
; Negative value = -errno
ARM32 EABI Syscall Register Convention
| Register | Purpose |
| r7 | Syscall number |
| r0 | Argument 1 / Return value |
| r1 | Argument 2 |
| r2 | Argument 3 |
| r3 | Argument 4 |
| r4 | Argument 5 |
| r5 | Argument 6 |


Note: The older OABI (Old ABI) convention embedded the syscall number in the SWI instruction itself (e.g., SWI #0x900005 for __NR_open). The kernel had to decode the instruction to extract it. EABI uses r7 instead, which is much faster as it avoids a memory access.
Step 1c: What SVC #0 Does at Hardware Level (ARM32)
+------------------------------------------------------------+
|  1. CPU switches mode: User (USR) -> Supervisor (SVC)     |
|  2. CPSR -> SPSR_svc  (saves current program status)      |
|  3. PC+4 -> LR_svc    (saves return address)              |
|  4. IRQs disabled     (I-bit set in CPSR)                 |
|  5. PC <- Vector Base + 0x08  (SWI vector offset)         |
|  6. ARM state forced  (T-bit cleared if in Thumb mode)    |
+------------------------------------------------------------+
ARM32 Processor Modes
| Mode | Encoding | Banked Registers | Usage |
| USR (User) | 10000 | — | Normal user code |
| SVC (Supervisor) | 10011 | R13_svc (SP), R14_svc (LR), SPSR_svc | Kernel / syscalls |
| IRQ | 10010 | R13_irq, R14_irq, SPSR_irq | Interrupts |
| ABT (Abort) | 10111 | R13_abt, R14_abt, SPSR_abt | Data/prefetch aborts |
| UND (Undefined) | 11011 | R13_und, R14_und, SPSR_und | Undefined instructions |
| FIQ | 10001 | R8-R12_fiq, R13_fiq, R14_fiq, SPSR_fiq | Fast interrupts |


2.2  ARM32 Exception Vector Table
Step 2a: ARM32 Vector Table Layout
The vector table base is set via VBAR in CP15 c12. ARM32 has 8 vector entries at fixed offsets, each 4 bytes:
Offset    Exception                    Handler
------    ---------                    -------
0x00      Reset                        reset_handler
0x04      Undefined Instruction        undef_handler
0x08      Software Interrupt (SWI) <-- vector_swi   *** SYSCALLS ***
0x0C      Prefetch Abort               pabt_handler
0x10      Data Abort                   dabt_handler
0x14      Reserved (Hyp trap)          ---
0x18      IRQ                          irq_handler
0x1C      FIQ                          fiq_handler
; arch/arm/kernel/entry-armv.S
__vectors_start:
    W(b)    vector_rst          ; 0x00: Reset
    W(b)    vector_und          ; 0x04: Undefined
    W(ldr)  pc, __vectors_start + 0x1000  ; 0x08: SWI -> vector_swi
    W(b)    vector_pabt         ; 0x0C: Prefetch Abort
    W(b)    vector_dabt         ; 0x10: Data Abort
    W(b)    vector_addrexcptn   ; 0x14: Reserved
    W(b)    vector_irq          ; 0x18: IRQ
    W(b)    vector_fiq          ; 0x1C: FIQ
Step 2b: vector_swi — The SWI Vector Entry
File: arch/arm/kernel/entry-common.S
ENTRY(vector_swi)
    sub     sp, sp, #PT_REGS_SIZE       ; Allocate pt_regs on SVC stack
    stmia   sp, {r0 - r12}             ; Save r0-r12 to pt_regs
    ARM(    add     r8, sp, #S_PC       )
    ARM(    stmdb   r8, {sp, lr}^       ) ; Save user SP & LR (^ = user bank regs)
    mrs     r8, spsr                    ; Get saved CPSR (user's CPSR from SPSR_svc)
    str     lr, [sp, #S_PC]             ; LR_svc -> pt_regs.pc (return address)
    str     r8, [sp, #S_PSR]            ; Save user CPSR -> pt_regs.psr
    str     r0, [sp, #S_OLD_R0]         ; Save original r0 for syscall restart
2.3  ARM32 pt_regs and Stack Layout
Step 3: ARM32 struct pt_regs (arch/arm/include/asm/ptrace.h)
struct pt_regs {
    unsigned long uregs[18];
};

// Register mapping:
// uregs[0]  = r0  (arg1 / return value)
// uregs[1]  = r1  (arg2)
// uregs[2]  = r2  (arg3)
// uregs[3]  = r3  (arg4)
// uregs[4]  = r4  (arg5)
// uregs[5]  = r5  (arg6)
// uregs[6]  = r6
// uregs[7]  = r7  (syscall number)
// uregs[8]  = r8
// uregs[9]  = r9
// uregs[10] = r10 (sl)
// uregs[11] = r11 (fp)
// uregs[12] = r12 (ip)
// uregs[13] = sp  (user SP, saved with ^ suffix)
// uregs[14] = lr  (user LR, saved with ^ suffix)
// uregs[15] = pc  (return address = LR_svc)
// uregs[16] = cpsr (from SPSR_svc = user CPSR)
// uregs[17] = orig_r0 (for syscall restart)

#define ARM_r0      uregs[0]
#define ARM_r7      uregs[7]
#define ARM_sp      uregs[13]
#define ARM_lr      uregs[14]
#define ARM_pc      uregs[15]
#define ARM_cpsr    uregs[16]
#define ARM_ORIG_r0 uregs[17]
Kernel Stack Layout After Context Save
         SVC Stack (Kernel)
         +-----------------+  <- sp (after sub)
         |  r0  (arg1)     |  [sp, #S_R0]
         |  r1  (arg2)     |
         |  r2  (arg3)     |
         |  r3  (arg4)     |
         |  r4  (arg5)     |
         |  r5  (arg6)     |
         |  r6             |
         |  r7  (scno)     |  <-- syscall number
         |  r8             |
         |  r9             |
         |  r10            |
         |  r11 (fp)       |
         |  r12 (ip)       |
         |  sp  (user)     |  [sp, #S_SP]   (saved with ^ suffix)
         |  lr  (user)     |  [sp, #S_LR]   (saved with ^ suffix)
         |  pc  (ret addr) |  [sp, #S_PC]   (from LR_svc)
         |  cpsr (user)    |  [sp, #S_PSR]  (from SPSR_svc)
         |  OLD_R0         |  [sp, #S_OLD_R0]
         +-----------------+
Key ARM32 Detail: The ^ suffix on STMDB/LDMIA accesses the user-mode banked SP and LR registers while in SVC mode. This is a unique ARM32 feature -- ARM64 uses the SP_EL0 system register instead (no banked GPRs).
2.4  ARM32 Syscall Number Extraction
Step 4a: EABI Path (Modern — Linux 2.6.16+)
    ; Continuing in vector_swi...

    ; ---- EABI: syscall number is in r7 ----
#ifdef CONFIG_AEABI
    mov     scno, r7                    ; scno = r7 (syscall number)
#else
    ; ---- OABI: extract from SWI instruction ----
    ; The SWI instruction encoding: 0xEF000000 | imm24
    ; For OABI: SWI #(0x900000 + __NR_xxx)

    ldr     r10, [lr, #-4]             ; Load the SWI instruction itself from memory
    bic     scno, r10, #0xFF000000     ; Extract bottom 24 bits
    ; scno now = 0x900000 + syscall_number (OABI)

    eor     scno, scno, #__NR_OABI_SYSCALL_BASE  ; Remove 0x900000 offset
#endif
OABI vs EABI Comparison
| Feature | OABI (Old ABI) | EABI (Embedded ABI) |
| Syscall number | Encoded in SWI instruction: SWI #0x900005 | In register r7, SVC #0 |
| Extraction | Load & decode instruction from memory | Read r7 directly |
| Performance | Slower (memory access + decode) | Faster (register read only) |
| 64-bit args | Packed in any register pair | Aligned to even register pair |
| Kernel config | CONFIG_OABI_COMPAT | CONFIG_AEABI |


2.5  ARM32 Syscall Dispatch
Step 6a: Fast Path — Single-Instruction Dispatch
    ; ---- Syscall invocation (fast path, no tracing) ----
    cmp     scno, #NR_syscalls          ; Bounds check
    badr    lr, ret_fast_syscall        ; Set return address for when handler returns
    ldrcc   pc, [tbl, scno, lsl #2]    ; if (scno < NR_syscalls):
                                        ;   pc = sys_call_table[scno]
                                        ;   (each entry is 4 bytes -> lsl #2)

    ; If scno >= NR_syscalls, fall through:
    b       sys_ni_syscall              ; returns -ENOSYS
Breakdown of ldrcc pc, [tbl, scno, lsl #2]:
- ldr = Load Register
- cc = Condition code Carry Clear (unsigned less-than): only executes if CMP above found scno < NR_syscalls
- pc = Destination is the Program Counter -- effectively an indirect branch
- [tbl, scno, lsl #2] = Address: tbl + (scno << 2) = sys_call_table[scno]
Interview Tip: This single ldrcc instruction does bounds check (condition code from CMP), table lookup (indexed addressing), AND branch (loading into PC). ARM64 dropped conditional execution for most instructions, so it uses C-level dispatch with explicit Spectre mitigations.
Step 6b: Slow Path — With Tracing (ptrace/seccomp/audit)
__sys_trace:
    mov     r1, scno                    ; Pass syscall number to C function
    add     r0, sp, #S_OFF              ; Pass pt_regs pointer
    bl      syscall_trace_enter         ; C function: may modify scno

    mov     scno, r0                    ; Updated syscall number (from ptrace)
    ldmia   sp, {r0 - r6}              ; Reload args from pt_regs
    adr     lr, __sys_trace_return      ; Set return to trace exit path
    cmp     scno, #NR_syscalls
    ldrcc   pc, [tbl, scno, lsl #2]    ; Call handler
    b       sys_ni_syscall              ; Invalid -> -ENOSYS

__sys_trace_return:
    str     r0, [sp, #S_R0 + S_OFF]    ; Store return value in pt_regs
    mov     r0, sp
    bl      syscall_trace_exit          ; Notify tracer of exit
    b       ret_slow_syscall            ; Return to user
2.6  ARM32 Return Path
Step 9a: ret_fast_syscall — Fast Return
ret_fast_syscall:
    disable_irq_notrace                 ; Disable IRQs for atomic check
    ldr     r1, [tsk, #TI_FLAGS]        ; Load thread_info->flags
    tst     r1, #_TIF_WORK_MASK         ; Any pending work?
    bne     fast_work_pending           ; Yes -> handle it

    ; ---- No pending work: fast return ----
    restore_user_regs fast = 1, offset = S_OFF
Step 9c: Syscall Restart After Signal Interruption
// In do_signal() -> handle_signal()
static void handle_signal(struct ksignal *ksig, struct pt_regs *regs, int syscall)
{
    if (syscall) {
        switch (regs->ARM_r0) {     // Check return value
        case -ERESTARTSYS:
            if (!(ksig->ka.sa.sa_flags & SA_RESTART)) {
                regs->ARM_r0 = -EINTR;  // Convert to EINTR
                break;
            }
            /* fall through */
        case -ERESTARTNOINTR:
            // Restart: restore original r0 and back up PC
            regs->ARM_r0 = regs->ARM_ORIG_r0;
            regs->ARM_pc -= 4;      // Point PC back to SVC instruction
            break;
        }
    }
    setup_return(regs, ksig);   // Set up signal handler frame
}
Step 10: restore_user_regs and MOVS PC, LR
.macro  restore_user_regs, fast = 0, offset = 0
    ldr     r1, [sp, #\offset + S_PSR]  ; Load saved CPSR

    ; Restore user SP and LR using ^ (user bank access)
    mov     r0, sp
    add     r0, r0, #\offset + S_SP
    ldmia   r0, {sp, lr}^              ; Restore user SP & LR (^ = user mode bank)

    msr     spsr_cxsf, r1              ; SPSR_svc = saved user CPSR
    ldmia   sp, {r0 - r12}            ; Restore r0-r12
    add     sp, sp, #\offset + PT_REGS_SIZE  ; Deallocate pt_regs

    ; ---- THE ACTUAL RETURN ----
    movs    pc, lr
    ; SPECIAL ARM32 instruction: S suffix with PC destination:
    ;   1. PC <- LR_svc (return address)
    ;   2. CPSR <- SPSR_svc (restores user CPSR atomically!)
    ; This atomically:
    ;   - Switches SVC mode -> USR mode
    ;   - Restores user CPSR (IRQ enable, Thumb state, condition flags)
    ;   - Jumps to instruction after SVC #0 in user space
.endm
Critical ARM32 Detail: MOVS PC, LR is the ARM32 privileged return instruction. The S suffix with PC as destination atomically copies SPSR -> CPSR and branches to LR. This is the ARM32 equivalent of ARM64's ERET. On ARMv7, SUBS PC, LR, #0 is the preferred encoding.
2.7  Complete ARM32 Flow Diagram
USER SPACE (USR Mode)                   KERNEL SPACE (SVC Mode)
--------------------                    ----------------------

app: open("/dev/x")
  |
  v
glibc: open() wrapper
  | MOV r7, #5 (__NR_open)
  | MOV r0-r2 (args)
  | SVC #0
  |                          +--------------------------------------+
  | === HW: SWI Exception    |                                      |
  | CPSR -> SPSR_svc         |  Vector Table @ VBAR + 0x08          |
  | PC+4 -> LR_svc           |         |                            |
  | Mode -> SVC              |  vector_swi:                         |
  | ========================>|    sub sp, sp, #PT_REGS_SIZE         |
  |                          |    stmia sp, {r0-r12}   <- save regs |
  |                          |    str lr, [sp, #S_PC]  <- save PC   |
  |                          |    mrs r8, spsr                      |
  |                          |    str r8, [sp, #S_PSR] <- save CPSR |
  |                          |    str r0, [sp, #S_OLD_R0]           |
  |                          |                                      |
  |                          |  +-- EABI: mov scno, r7              |
  |                          |  +-- OABI: ldr+decode SWI instruction|
  |                          |                                      |
  |                          |    get_thread_info tsk               |
  |                          |    tst flags, #_TIF_SYSCALL_WORK     |
  |                          |    bne __sys_trace  (slow path)      |
  |                          |                                      |
  |                          |  Fast path:                          |
  |                          |    cmp scno, #NR_syscalls            |
  |                          |    ldrcc pc, [tbl, scno, lsl #2]     |
  |                          |      |                               |
  |                          |    sys_open(r0, r1, r2)              |
  |                          |      -> do_sys_open() -> VFS -> drv  |
  |                          |      return fd (or -errno) in r0     |
  |                          |      |                               |
  |                          |  ret_fast_syscall:                   |
  |                          |    disable_irq                       |
  |                          |    tst flags, #_TIF_WORK_MASK        |
  |                          |    bne -> schedule/signals/etc       |
  |                          |                                      |
  |                          |  restore_user_regs:                  |
  |                          |    ldmia sp, {r0-r12}  <- restore    |
  |                          |    ldm {sp,lr}^        <- user SP/LR |
  |                          |    msr spsr, saved_cpsr              |
  | <===== MOVS PC, LR ===== |    movs pc, lr  <- ATOMIC RETURN     |
  | SPSR_svc -> CPSR         |    (CPSR<-SPSR, PC<-LR, SVC->USR)   |
  | SVC -> USR mode          +--------------------------------------+
  v
glibc: check return
  | cmn r0, #4096  ; if r0 >= -4095: error
  v
app: fd or -1
3. ARM32 vs ARM64: Side-by-Side Comparison
This section provides a consolidated, interview-ready comparison of both architectures covering every step of the system call path.
3.1  Register Convention Comparison
| Aspect | ARM32 (AArch32) | ARM64 (AArch64) |
| Trap instruction | SVC #0 (was SWI #0) | SVC #0 |
| Syscall number register | r7 | x8 |
| Argument registers | r0-r5 | x0-x5 |
| Return value register | r0 | x0 |
| Mode switch | USR -> SVC mode | EL0 -> EL1 |
| Save return address | PC+4 -> LR_svc | PC+4 -> ELR_EL1 |
| Save processor state | CPSR -> SPSR_svc | PSTATE -> SPSR_EL1 |
| Disable interrupts | I-bit set in CPSR | DAIF masked |
| Vector table offset | VBAR + 0x08 (SWI) | VBAR_EL1 + 0x400 (EL0 sync) |
| Handler entry point | vector_swi | el0_sync |
| Identify exception type | Dedicated SWI vector (no check needed) | ESR_EL1 EC field == 0x15 |
| SP for kernel stack | R13_svc (banked) | SP_EL1 |
| User SP access | stmdb r8, {sp,lr}^ (banked) | mrs x21, sp_el0 |
| Dispatch instruction | ldrcc pc, [tbl, scno, lsl #2] | invoke_syscall() C function |
| Spectre mitigation | Conditional ldrcc (partial) | array_index_nospec() |
| Return instruction | movs pc, lr | eret |
| Return mechanism | CPSR <- SPSR_svc, PC <- LR_svc | PSTATE <- SPSR_EL1, PC <- ELR_EL1 |


3.2  The 6-Step Flow Summary (Both Architectures)
Step 1: SVC #0
        ARM32: USR->SVC mode, CPSR->SPSR_svc, PC+4->LR_svc, jump to VBAR+0x08
        ARM64: EL0->EL1, PSTATE->SPSR_EL1, PC+4->ELR_EL1, jump to VBAR+0x400

Step 2: Save all registers -> pt_regs on kernel stack
        ARM32: stmia sp, {r0-r12}; stmdb {sp,lr}^; str lr, [sp,#S_PC]
        ARM64: kernel_entry macro: stp pairs for x0-x30, sp_el0, elr_el1, spsr_el1

Step 3: Extract syscall number, check TIF flags
        ARM32: mov scno, r7 (EABI); tst flags, #_TIF_SYSCALL_WORK
        ARM64: regs->regs[8] (x8); check thread_info->flags

Step 4: sys_call_table[scno] -> call handler
        ARM32: ldrcc pc, [tbl, scno, lsl #2]  (1 instruction!)
        ARM64: invoke_syscall() with array_index_nospec Spectre mitigation

Step 5: Check pending work before return
        TIF_NEED_RESCHED -> schedule()
        TIF_SIGPENDING   -> do_signal() -> possible syscall restart
        TIF_NOTIFY_RESUME -> tracehook_notify_resume()

Step 6: Restore regs from pt_regs, return to user mode
        ARM32: restore_user_regs -> movs pc, lr (atomic CPSR restore + branch)
        ARM64: kernel_exit macro: ldp pairs -> eret (atomic EL1->EL0)
3.3  Interview Q&A: System Call Flow
| Question | Answer |
| Why does the kernel save orig_r0 / orig_x0? | For syscall restart after signal delivery. If a syscall returns -ERESTARTSYS and SA_RESTART is set, the kernel restores the original first argument from orig_r0/orig_x0 and backs up the PC by 4 bytes to re-execute the SVC instruction. |
| What is the Spectre mitigation in ARM64 syscall dispatch? | array_index_nospec() inserts a speculation barrier after the bounds check, preventing speculative out-of-bounds access to the syscall table. ARM32 uses conditional ldrcc which is less powerful. |
| How does ARM32 access user-mode SP and LR while in SVC mode? | Using the ^ suffix on LDM/STM instructions, which accesses the user-bank registers while in a privileged mode. ARM64 has no banked GPRs -- it uses the SP_EL0 system register instead. |
| What happens if an invalid syscall number is passed? | The bounds check fails (CMP scno, #NR_syscalls). The kernel places -ENOSYS in r0/x0 and returns to user space. sys_ni_syscall() handles this on ARM32 fast path. |
| Difference between ret_fast_syscall and ret_slow_syscall on ARM32? | Fast path skips syscall exit tracing (no ptrace/audit active). Slow path calls syscall_trace_exit() before returning. Both check TIF_WORK_MASK for pending work (reschedule, signals). |
| What is OABI vs EABI? | OABI (Old ABI) encodes syscall number inside the SWI instruction (e.g. SWI #0x900005), requiring memory load + decode. EABI uses r7 register for syscall number, faster. EABI is standard since Linux 2.6.16. |
| How is the EL0 synchronous exception identified as a syscall on ARM64? | After el0_sync runs, it reads ESR_EL1 and extracts the Exception Class (EC) field (bits [31:26]). EC = 0x15 means SVC from AArch64. EC = 0x25 means data abort from lower EL, etc. |
| What is VBAR? | Vector Base Address Register. ARM32: set via CP15 MCR instruction. ARM64: VBAR_EL1 system register. Contains the base address of the exception vector table. Set early in boot (head.S). |


4. How the Kernel Routes open() to Your Driver
This section answers the critical question: when user space calls open("/dev/mydevice"), how does the kernel know which driver's .open() function to call? This is the missing link between system call theory and driver development.
4.1  The Short Answer (Interview Elevator Pitch)
| When you call open("/dev/mydevice"), the VFS resolves the path to an inode. For device files, the inode contains a major:minor number. The kernel uses the major number to look up the registered driver's file_operations structure in cdev_map (char devices) or bdev_inode (block devices). That file_operations contains the driver's .open(), .read(), .write(), etc. -- and that's what gets called. |


4.2  Step-by-Step: open("/dev/mydevice") to Driver .open()
Step 1: VFS Path Resolution
sys_open("/dev/mydevice", O_RDWR)
  -> do_filp_open()
    -> path_openat()
      -> link_path_walk("/dev/mydevice")
        -> "/"    -> root dentry
        -> "dev"  -> lookup in root -> /dev dentry (devtmpfs)
        -> "mydevice" -> lookup in /dev -> inode found!

The VFS walks the path component by component using:
  - dentries (directory entry cache)
  - inodes (inode objects with device identity)
Step 2: The Inode Holds the Device Identity
struct inode {
    umode_t         i_mode;     // File type: S_IFCHR (char) or S_IFBLK (block)
    dev_t           i_rdev;     // Device number (major:minor)
    struct cdev     *i_cdev;    // Pointer to char device (if char dev)
    const struct file_operations *i_fop;  // File operations (set by chrdev_open)
    ...                         // inode number, timestamps, size, etc.
};

// MAJOR(i_rdev) = identifies the driver (e.g., 240)
// MINOR(i_rdev) = identifies the specific device instance (e.g., 0, 1, 2...)
Step 3: How i_rdev Gets Set -- Device Node Creation
// Manual creation via mknod:
mknod /dev/mydevice c 240 0    // char device, major=240, minor=0

// Or automatically by the kernel via devtmpfs when driver calls:
device_create(my_class, NULL, MKDEV(240, 0), NULL, "mydevice");
// -> udev/devtmpfs creates /dev/mydevice with correct major:minor in inode
Step 4: VFS Opens the File -- do_dentry_open()
// fs/open.c
static int do_dentry_open(struct file *f, struct inode *inode)
{
    // For device files, the KEY step:
    if (S_ISCHR(inode->i_mode)) {          // Char device?
        f->f_op = &def_chr_fops;            // Temporary: use char device default ops
    } else if (S_ISBLK(inode->i_mode)) {   // Block device?
        f->f_op = &def_blk_fops;
    }

    // Call the open function:
    f->f_op->open(inode, f);
    // For char devices, this calls chrdev_open()
}
Step 5: chrdev_open() — The Magic Lookup (File: fs/char_dev.c)
static int chrdev_open(struct inode *inode, struct file *filp)
{
    struct cdev *p;

    // ---- LOOKUP: major:minor -> cdev ----
    p = inode->i_cdev;
    if (!p) {
        // First open: look up in the global cdev_map hash table
        struct kobject *kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx);
        p = container_of(kobj, struct cdev, kobj);

        inode->i_cdev = p;              // Cache for future opens (avoid lookup)
        inode->i_fop  = p->ops;         // Install DRIVER's file_operations!
        list_add(&inode->i_devices, &p->list);
    }

    // ---- NOW call the DRIVER's .open() ----
    filp->f_op = p->ops;                // Point struct file to driver fops

    if (filp->f_op->open)
        ret = filp->f_op->open(inode, filp);  // *** YOUR driver's open() ***

    return ret;
}
This is the answer: kobj_lookup(cdev_map, inode->i_rdev) uses major:minor to find the struct cdev registered by your driver via cdev_add(). The cdev contains a pointer to your driver's file_operations.
Step 6: How Your Driver Registered Itself (probe() time)
static const struct file_operations my_fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,        // *** YOUR open function ***
    .read    = my_read,
    .write   = my_write,
    .release = my_release,
    .unlocked_ioctl = my_ioctl,
};

static struct cdev my_cdev;

static int __init my_driver_init(void)
{
    dev_t dev;

    // 1. Allocate major:minor numbers (dynamic allocation preferred)
    alloc_chrdev_region(&dev, 0, 1, "mydevice");

    // 2. Initialize cdev with YOUR file_operations
    cdev_init(&my_cdev, &my_fops);    // my_cdev.ops = &my_fops

    // 3. Register cdev in the global cdev_map hash table
    cdev_add(&my_cdev, dev, 1);       // Adds to cdev_map[major]
    // *** cdev_add() is the registration step ***

    // 4. Create device node in /dev (via devtmpfs/udev)
    my_class = class_create(THIS_MODULE, "myclass");
    device_create(my_class, NULL, dev, NULL, "mydevice");
    // -> triggers devtmpfs to create /dev/mydevice with this major:minor

    return 0;
}
4.3  Complete Chain Visual
open("/dev/mydevice")
  |
  v
VFS: path_walk -> find inode for "mydevice"
  |
  |  inode->i_rdev = MKDEV(240, 0)    <- major:minor stored in filesystem
  |  inode->i_mode = S_IFCHR          <- it's a char device
  |
  v
do_dentry_open() -> f->f_op = &def_chr_fops -> calls chrdev_open()
  |
  v
chrdev_open():
  |  kobj_lookup(cdev_map, MKDEV(240,0))
  |       |
  |       v
  |  cdev_map[240] -> struct cdev (my_cdev)
  |       |                |
  |       |                +-- .ops = &my_fops ----------+
  |       v                                              |
  |  inode->i_cdev = &my_cdev                           |
  |  inode->i_fop  = &my_fops                           |
  |  filp->f_op    = &my_fops                           |
  |                                                      |
  v                                                      v
filp->f_op->open(inode, filp) =============> my_open(inode, filp)
                                              *** YOUR DRIVER CODE RUNS ***
4.4  Driver Types: Different Registration Paths
Misc Device (major = 10, simplified registration)
static struct miscdevice my_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "mydevice",
    .fops  = &my_fops,          // Your file_operations
};
misc_register(&my_misc);
// Internally: cdev_add() with major=10, auto-assigned minor
// misc_open() looks up minor in list and swaps in your fops
Platform Device + Char Device (Qualcomm BSP Pattern)
// Device Tree match -> platform_driver.probe()
//   Inside probe():
      alloc_chrdev_region()        // Get major:minor
      cdev_init(&cdev, &my_fops)   // Set file_operations
      cdev_add()                   // Register in cdev_map
      class_create()               // Create sysfs class
      device_create()              // Create /dev node via devtmpfs
4.5  Interview Q&A: VFS to Driver Routing
| Question | Answer |
| How does the kernel know which driver to call for open("/dev/xyz")? | VFS resolves path -> inode. inode->i_rdev has major:minor. For char devices, chrdev_open() calls kobj_lookup(cdev_map, major:minor) -> struct cdev (registered by driver's cdev_add()) -> cdev.ops = file_operations -> .open() is called. |
| What is cdev_map? | A global hash table (struct kobj_map) in fs/char_dev.c that maps major:minor ranges to struct cdev objects. cdev_add() inserts into it; kobj_lookup() searches it. |
| Major vs minor number? | Major identifies the driver (e.g., 240 = my_driver). Minor identifies the specific device instance within that driver. The driver uses iminor(inode) in .open() to determine which instance. |
| What if two drivers register the same major? | register_chrdev_region() fails with -EBUSY. Use alloc_chrdev_region() for dynamic allocation to avoid conflicts. The kernel enforces unique major:minor ranges. |
| How do Qualcomm platform drivers expose a char device? | In probe(): alloc_chrdev_region() -> cdev_init() with driver fops -> cdev_add() -> class_create() -> device_create(). DT match triggers probe which sets up the char device interface. |


5. read() / write() / ioctl() Flow Through to the Driver
After open() completes, filp->f_op already points to your driver's file_operations. All subsequent read(), write(), and ioctl() calls use direct function pointer dispatch -- no more major:minor lookup needed.
5.1  The Key Insight: After open(), It's Direct
// After chrdev_open() completes:
filp->f_op = &my_fops;    // YOUR driver's file_operations
filp->private_data = ...;  // Whatever your .open() set (per-instance context)

// For all subsequent read()/write()/ioctl() on that fd:
// kernel does fdget(fd) -> struct file -> filp->f_op->read/write/ioctl
// Direct function pointer call -- no cdev_map lookup, no path resolution
5.2  The fd to struct file Lookup (fdget)
// How fd (integer) maps to struct file:

struct task_struct (current process)
  +-- struct files_struct *files
        +-- struct fdtable *fdt
              +-- struct file **fd     // Array of file pointers
                    +-- fd[3] -------> struct file {
                                          .f_op = &my_fops,
                                          .f_pos = current_offset,
                                          .f_flags = O_RDWR,
                                          .f_inode = inode,
                                          .private_data = dev_context,
                                        }

// fdget(fd) does:
// 1. current->files->fdt->fd[fd]  -- array index lookup (O(1))
// 2. Checks fd < fdt->max_fds     -- bounds check
// 3. Returns struct file * with reference count management
5.3  read() Flow: User Space to Driver
User Space Call
char buf[256];
int n = read(fd, buf, 256);
// SVC #0: x8=63 (ARM64) or r7=3 (ARM32), x0/r0=fd, x1/r1=buf, x2/r2=count
Kernel Path
sys_read(fd, buf, count)                    // fs/read_write.c
  |
  +-- fdget_pos(fd)                         // fd -> struct fd
  |     +-- current->files->fdt->fd[fd]    // O(1) lookup
  |
  +-- file_pos_read(filp)                   // Get current file position
  |     +-- filp->f_pos
  |
  +-- vfs_read(filp, buf, count, &pos)      // VFS layer
        |
        +-- rw_verify_area(READ, filp, pos, count)  // Security/LSM check
        |     +-- security_file_permission(filp, MAY_READ)
        |           +-- SELinux / AppArmor policy check
        |
        +-- Does filp->f_op->read exist?
        |     YES -> filp->f_op->read(filp, buf, count, &pos)
        |     NO  -> Check filp->f_op->read_iter?
        |              YES -> new_sync_read() -> calls .read_iter()
        |              NO  -> return -EINVAL
        |
        |   *** YOUR DRIVER'S .read() IS CALLED HERE ***
        |
        +-- fsnotify_access(filp)           // inotify notification

+-- file_pos_write(filp, pos)               // Update file position
      +-- filp->f_pos = pos
Inside Your Driver's .read()
static ssize_t my_read(struct file *filp, char __user *buf,
                        size_t count, loff_t *f_pos)
{
    struct my_device *dev = filp->private_data;  // Set during .open()

    // Wait for data if needed
    if (no_data_available(dev)) {
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;                // Non-blocking: return immediately
        wait_event_interruptible(dev->read_queue, data_available(dev));
    }

    // Copy data from kernel buffer to user space
    if (copy_to_user(buf, dev->kbuf, count))
        return -EFAULT;                    // Bad user pointer

    *f_pos += count;                       // Update file position
    return count;                          // Return bytes read
}
copy_to_user() internally calls access_ok() to verify the user pointer, then does the copy with page fault handling. If the user page is swapped out, a page fault occurs, the kernel handles it transparently, and the copy resumes.
5.4  write() Flow: User Space to Driver
int n = write(fd, "hello", 5);

sys_write(fd, buf, count)                   // fs/read_write.c
  +-- fdget_pos(fd)                         // fd -> struct file
  |
  +-- vfs_write(filp, buf, count, &pos)     // VFS layer
        +-- rw_verify_area(WRITE, ...)      // MAY_WRITE security check
        +-- file_start_write(filp)          // Freeze protection (for filesystems)
        +-- filp->f_op->write(filp, buf, count, &pos)
        |   *** YOUR DRIVER'S .write() ***
        +-- file_end_write(filp)
        +-- fsnotify_modify(filp)           // inotify notification
static ssize_t my_write(struct file *filp, const char __user *buf,
                         size_t count, loff_t *f_pos)
{
    struct my_device *dev = filp->private_data;

    // Copy data from user space to kernel buffer
    if (copy_from_user(dev->kbuf, buf, count))
        return -EFAULT;

    // Write to hardware (e.g., I2C/SPI transfer, MMIO register write)
    hw_write(dev, dev->kbuf, count);

    *f_pos += count;
    return count;
}
5.5  ioctl() Flow: The Most Complex One
User Space Call
struct my_config cfg = { .speed = 115200, .mode = 1 };
int ret = ioctl(fd, MY_IOC_SET_CONFIG, &cfg);
Kernel Path (sys_ioctl)
sys_ioctl(fd, cmd, arg)                     // fs/ioctl.c
  |
  +-- fdget(fd)                             // fd -> struct file
  |
  +-- security_file_ioctl(filp, cmd, arg)   // LSM/SELinux check
  |
  +-- do_vfs_ioctl(filp, fd, cmd, arg)
        |
        |  // VFS handles some ioctls ITSELF (never reaches driver):
        +-- FIOCLEX      -> set_close_on_exec(fd)
        +-- FIONCLEX     -> clear close_on_exec
        +-- FIONBIO      -> set/clear O_NONBLOCK on filp->f_flags
        +-- FIOASYNC     -> set/clear O_ASYNC (FASYNC signal)
        +-- FIOQSIZE     -> return file size
        +-- FIFREEZE     -> filesystem freeze (ext4/xfs)
        +-- FITHAW       -> filesystem thaw
        |
        |  // Everything else -> driver:
        +-- vfs_ioctl(filp, cmd, arg)
              +-- filp->f_op->unlocked_ioctl(filp, cmd, arg)
                  *** YOUR DRIVER'S .unlocked_ioctl() ***
_IOC Macro Encoding — The 32-bit Command Word
 31  30  29          16 15           8 7              0
+----+----------------+--------------+----------------+
| dir|     size       |    type      |      nr        |
|2bit|    14 bits     |   8 bits     |    8 bits      |
+----+----------------+--------------+----------------+

dir:  _IOC_NONE=0, _IOC_WRITE=1, _IOC_READ=2, _IOC_READ|_IOC_WRITE=3
type: Magic number (unique per driver, e.g., 'M')
nr:   Command number (sequential within driver)
size: sizeof(data structure being passed)

// Macro definitions (include/uapi/asm-generic/ioctl.h)
#define _IO(type,   nr)       _IOC(_IOC_NONE,  type, nr, 0)
#define _IOR(type,  nr, size) _IOC(_IOC_READ,  type, nr, sizeof(size))
#define _IOW(type,  nr, size) _IOC(_IOC_WRITE, type, nr, sizeof(size))
#define _IOWR(type, nr, size) _IOC(_IOC_READ|_IOC_WRITE, type, nr, sizeof(size))
Direction is from USER's perspective: _IOR means user READS (kernel writes to user buffer). _IOW means user WRITES (kernel reads from user buffer). This is a common interview gotcha.
Driver ioctl Implementation
// ioctl command definitions (in include/uapi header shared with userspace)
#define MY_IOC_MAGIC        'M'
#define MY_IOC_SET_CONFIG   _IOW(MY_IOC_MAGIC, 1, struct my_config)
#define MY_IOC_GET_CONFIG   _IOR(MY_IOC_MAGIC, 2, struct my_config)
#define MY_IOC_RESET        _IO(MY_IOC_MAGIC,  3)

static long my_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct my_device *dev = filp->private_data;
    struct my_config cfg;

    // Validate magic number and command number
    if (_IOC_TYPE(cmd) != MY_IOC_MAGIC)  return -ENOTTY;
    if (_IOC_NR(cmd) > MY_IOC_MAXNR)     return -ENOTTY;

    // Validate user pointer accessibility
    if (_IOC_DIR(cmd) & _IOC_READ)
        if (!access_ok((void __user *)arg, _IOC_SIZE(cmd)))
            return -EFAULT;

    switch (cmd) {
    case MY_IOC_SET_CONFIG:
        if (copy_from_user(&cfg, (void __user *)arg, sizeof(cfg)))
            return -EFAULT;
        return hw_set_config(dev, &cfg);

    case MY_IOC_GET_CONFIG:
        hw_get_config(dev, &cfg);
        if (copy_to_user((void __user *)arg, &cfg, sizeof(cfg)))
            return -EFAULT;
        return 0;

    case MY_IOC_RESET:
        return hw_reset(dev);

    default:
        return -ENOTTY;     // Unknown command: "not a typewriter" (historical)
    }
}
compat_ioctl — 32-bit App on 64-bit Kernel (Qualcomm Important!)
static const struct file_operations my_fops = {
    .unlocked_ioctl = my_ioctl,         // 64-bit user apps
    .compat_ioctl   = my_compat_ioctl,  // 32-bit apps on 64-bit kernel
};

// Called when 32-bit process issues ioctl() on a 64-bit kernel
// Needed because: pointer sizes differ (4 vs 8 bytes),
// struct layout/padding may differ, unsigned long is 4 bytes in 32-bit
static long my_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    // Convert 32-bit user structures to 64-bit kernel structures
    return my_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
5.6  Unified Visual: All Three Operations
User Space                   Kernel VFS                    Your Driver
----------                   ----------                    -----------

read(fd, buf, n)
  | SVC #0
  +---> sys_read()
         +---> fdget(fd) -> struct file
               +---> vfs_read()
                      +-- security check
                      +--> filp->f_op->read() ---------> my_read()
                                                            +-- copy_to_user()
                                                            +-- return bytes

write(fd, buf, n)
  | SVC #0
  +---> sys_write()
         +---> fdget(fd) -> struct file
               +---> vfs_write()
                      +-- security check
                      +--> filp->f_op->write() --------> my_write()
                                                            +-- copy_from_user()
                                                            +-- hw_write()
                                                            +-- return bytes

ioctl(fd, cmd, arg)
  | SVC #0
  +---> sys_ioctl()
         +---> fdget(fd) -> struct file
               +---> do_vfs_ioctl()
                      +-- VFS-handled? (FIONBIO etc.) -> handle internally
                      +--> vfs_ioctl()
                             +--> filp->f_op->unlocked_ioctl() -> my_ioctl()
                                                                    +-- _IOC_TYPE check
                                                                    +-- switch(cmd)
                                                                    +-- copy_from/to_user()
                                                                    +-- hw operation
5.7  Interview Q&A: read/write/ioctl
| Question | Answer |
| After open(), how does read() find the right driver? | open() already set filp->f_op to the driver's file_operations. For read(), fdget(fd) gets struct file, then filp->f_op->read() is called directly. No major:minor lookup -- it's a simple function pointer call. |
| Difference between unlocked_ioctl and old ioctl? | Old .ioctl() was called with the Big Kernel Lock (BKL) held -- serializing ALL ioctls system-wide. .unlocked_ioctl() (since 2.6.36) runs without BKL; driver must handle its own locking. Old .ioctl is completely removed from modern kernels. |
| Why does ioctl return -ENOTTY for unknown commands? | Historical: ENOTTY = "Not a typewriter." Originally ioctl was for terminal (tty) control. -ENOTTY means "this device doesn't support this ioctl command." It is the standard error for unrecognized ioctls. |
| What is private_data in struct file? | A void * that the driver sets in .open() to store per-open-instance context (pointer to device struct, DMA buffers, state). Retrieved via filp->private_data in all subsequent .read(), .write(), .ioctl() calls. |
| How does copy_to_user handle page faults? | Uses exception table mechanism. If user page is not mapped, page fault occurs. The page fault handler checks if faulting address is inside a copy_to_user region (via exception table). If yes, it handles the fault and the copy resumes. If truly invalid, returns bytes not copied; driver returns -EFAULT. |
| .read vs .read_iter? | Modern drivers prefer .read_iter/.write_iter which use struct iov_iter and support scatter-gather, splice, and direct I/O. VFS prefers .read_iter if available, falls back to .read. Simple char drivers can still use .read. |


6. Linux Platform Driver — Complete Deep Dive
This is the most Qualcomm-relevant topic. Every Qualcomm SoC peripheral -- UART, SPI, I2C, GPU, camera, audio codec -- is a platform device. This section covers everything from the motivation through full implementation details.
6.1  What is a Platform Device/Driver?
The Problem It Solves
Some devices are discoverable -- USB and PCI devices announce themselves (vendor ID, device ID). The kernel auto-detects them. But SoC-internal peripherals (UART, I2C, SPI, GPIO, DMA) are NOT discoverable. They are hardwired onto the SoC's internal bus (AHB/APB/AXI). The CPU cannot scan and find them. Someone must tell the kernel they exist.
Platform device = a device that cannot self-identify to the bus. Platform bus = a virtual/pseudo bus invented by Linux to handle non-discoverable devices.
Why Platform Driver (vs Other Approaches)
| Approach | When to Use | Example | Discovery |
| Platform driver | SoC-internal, non-discoverable | UART, I2C, SPI, GPIO, DMA | None (Device Tree) |
| PCI driver | PCI/PCIe bus devices | NVMe, GPU, NIC | PCI config space scan |
| USB driver | USB bus devices | USB storage, HID | USB enumeration |
| I2C client driver | Devices on I2C bus | Sensors, EEPROM, PMIC | DT/board file |
| SPI driver | Devices on SPI bus | Flash, display, ADC | DT/board file |


Key insight: The I2C controller itself is a platform device. The I2C sensor connected to it is an I2C client device. Same for SPI: the SPI controller is platform, the SPI flash chip is an SPI device.
Where It's Used on Qualcomm SoCs
Qualcomm SoC (e.g., QCS6490, SM8550)
+-- UART controllers      -> platform device (qcom,geni-uart)
+-- I2C controllers       -> platform device (qcom,geni-i2c)
+-- SPI controllers       -> platform device (qcom,geni-spi)
+-- GPIO controller       -> platform device (qcom,tlmm)
+-- Clock controller      -> platform device (qcom,gcc-sm8550)
+-- Interconnect          -> platform device (qcom,sm8550-bimc)
+-- Camera (CAMSS)        -> platform device (qcom,camss)
+-- Display (DPU)         -> platform device (qcom,mdss)
+-- Audio (LPASS)         -> platform device (qcom,lpass)
+-- Crypto engine         -> platform device (qcom,qce)
+-- Remoteproc (ADSP)     -> platform device (qcom,sm8550-adsp-pas)
6.2  The Three Core Structures
1. struct platform_device — Describes the Hardware
// include/linux/platform_device.h
struct platform_device {
    const char      *name;          // Device name (for name matching)
    int             id;             // Instance ID (-1 if single instance)
    struct device   dev;            // Embedded generic device
    u32             num_resources;  // Number of resources
    struct resource *resource;      // Memory, IRQ resources (from DT)
    const struct platform_device_id *id_entry;  // Matched ID entry
    // ...
};
2. struct platform_driver — Your Driver Code
// include/linux/platform_device.h
struct platform_driver {
    int  (*probe)(struct platform_device *pdev);    // Called when matched
    int  (*remove)(struct platform_device *pdev);   // Called on unbind
    void (*shutdown)(struct platform_device *pdev); // System shutdown
    int  (*suspend)(struct platform_device *pdev, pm_message_t state);
    int  (*resume)(struct platform_device *pdev);
    struct device_driver driver;                     // Embedded generic driver
    const struct platform_device_id *id_table;       // ID matching table (legacy)
};
3. struct resource — Hardware Resources
struct resource {
    resource_size_t start;  // Start address (MMIO) or IRQ number
    resource_size_t end;    // End address
    const char      *name;  // Resource name (e.g., "reg", "irq")
    unsigned long   flags;  // IORESOURCE_MEM, IORESOURCE_IRQ, etc.
};
6.3  Hardware Description via Device Tree
On Qualcomm platforms (and i.MX6/i.MX8X), hardware is described in Device Tree Source (DTS):
/* Example: Qualcomm GENI UART on SM8550 */
/* arch/arm64/boot/dts/qcom/sm8550.dtsi */

soc {
    compatible = "simple-bus";
    #address-cells = <2>;
    #size-cells = <2>;
    ranges;

    uart2: serial@a94000 {
        compatible = "qcom,geni-uart";       // *** Match string for driver ***
        reg = <0x0 0x00a94000 0x0 0x4000>;   // *** MMIO region ***
        interrupts = <GIC_SPI 358 IRQ_TYPE_LEVEL_HIGH>;  // *** IRQ ***
        clocks = <&gcc GCC_QUPV3_WRAP1_S2_CLK>;
        clock-names = "se";
        pinctrl-0 = <&uart2_default>;
        pinctrl-names = "default";
        power-domains = <&rpmhpd SM8550_CX>;
        status = "okay";                     // "disabled" = not created
    };
};
DT Property to Kernel Mapping
| DT Property | Kernel Representation | Resource Type | Driver Access |
| reg | struct resource | IORESOURCE_MEM | platform_get_resource() |
| interrupts | struct resource | IORESOURCE_IRQ | platform_get_irq() |
| clocks | Clock framework ref | N/A | devm_clk_get() |
| pinctrl-* | Pinctrl framework ref | N/A | Auto-configured |
| power-domains | PM domain reference | N/A | Auto-managed |
| status | "okay" = create device | N/A | Controls DT node |
| compatible | of_device_id match | N/A | Triggers probe() |


7. Platform Driver Registration and Probe Flow
7.1  Boot-Time Flow: Device Tree to Platform Devices
Bootloader (LK/UEFI/U-Boot)
  |  Loads DTB into memory, passes DTB address to kernel
  |
  v
Kernel boot: start_kernel()
  -> setup_arch()
    -> unflatten_device_tree()          // Parse DTB -> device_node tree
    |   Creates: struct device_node for each DT node
    |            struct property for each property
    |
    -> of_platform_default_populate_init()  // arch_initcall_sync
      -> of_platform_default_populate()
        -> of_platform_bus_create()
          |
          |  For each child node of "soc" (compatible="simple-bus"):
          |
          +-- of_platform_device_create_pdata("serial@a94000", ...)
          |     |
          |     +-- of_device_alloc()
          |     |     +-- platform_device_alloc()    // Allocate platform_device
          |     |     +-- Parse "reg" -> struct resource (IORESOURCE_MEM)
          |     |     |     .start = 0x00a94000
          |     |     |     .end   = 0x00a97fff
          |     |     |     .flags = IORESOURCE_MEM
          |     |     +-- Parse "interrupts" -> struct resource (IORESOURCE_IRQ)
          |     |     |     .start = 358 (hwirq -> virq via irqdomain)
          |     |     |     .flags = IORESOURCE_IRQ
          |     |     +-- pdev->dev.of_node = device_node // Link to DT
          |     |
          |     +-- of_device_add(pdev)
          |           -> device_add(&pdev->dev)
          |             -> bus_add_device()     // Add to platform_bus
          |             -> bus_probe_device()   // Try to find matching driver
          |               -> __device_attach()  // Search all registered drivers
          |                 // If no match -> device waits on platform_bus
          |
          +-- of_platform_device_create_pdata("i2c@a98000", ...)
          +-- of_platform_device_create_pdata("spi@a9c000", ...)
          +-- ... (all other SoC peripherals)
7.2  Driver Registration: platform_driver_register()
Driver Definition Example (Qualcomm GENI UART)
// drivers/tty/serial/qcom_geni_serial.c

static const struct of_device_id qcom_geni_serial_match[] = {
    { .compatible = "qcom,geni-uart" },       // *** Must match DT ***
    { .compatible = "qcom,geni-debug-uart" },
    { }                                        // Sentinel: empty entry = end
};
MODULE_DEVICE_TABLE(of, qcom_geni_serial_match);

static struct platform_driver qcom_geni_serial_driver = {
    .probe    = qcom_geni_serial_probe,        // *** Called on match ***
    .remove   = qcom_geni_serial_remove,
    .driver   = {
        .name = "qcom_geni_serial",
        .of_match_table = qcom_geni_serial_match,  // *** DT match table ***
        .pm = &qcom_geni_serial_pm_ops,
    },
};

// Registration:
module_platform_driver(qcom_geni_serial_driver);
// Expands to:
// module_init() -> platform_driver_register(&qcom_geni_serial_driver)
// module_exit() -> platform_driver_unregister(&qcom_geni_serial_driver)
Registration Flow Internals
platform_driver_register(&qcom_geni_serial_driver)
  |
  +-- drv->driver.bus = &platform_bus_type;    // Set bus to platform
  |
  +-- driver_register(&drv->driver)
        |
        +-- bus_add_driver()                    // Add to platform_bus driver list
        |
        +-- driver_attach()                     // Try matching with existing devices
              -> bus_for_each_dev(... __driver_attach ...)
                |
                |  For each platform_device on the bus:
                +-- driver_match_device(drv, dev)
                |     -> platform_bus_type.match(dev, drv)
                |       -> platform_match()   // *** THE MATCHING FUNCTION ***
                |
                |  If match found:
                +-- driver_probe_device(drv, dev)
                      -> really_probe(dev, drv)
                        -> drv->probe(pdev)  // *** YOUR probe() CALLED! ***
7.3  The Matching Mechanism: platform_match()
File: drivers/base/platform.c. The kernel tries 4 matching methods in priority order:
static int platform_match(struct device *dev, struct device_driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    // *** Priority 1: OF (Device Tree) matching *** (used 99% on ARM/Qualcomm)
    if (of_driver_match_device(dev, drv))
        return 1;
    // Compares dev->of_node->compatible with drv->of_match_table[]
    // e.g., "qcom,geni-uart" == "qcom,geni-uart" -> MATCH!

    // *** Priority 2: ACPI matching *** (x86, some ARM servers)
    if (acpi_driver_match_device(dev, drv))
        return 1;

    // *** Priority 3: ID table matching *** (legacy, pre-DT)
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;

    // *** Priority 4: Name string matching *** (oldest, simplest)
    return (strcmp(pdev->name, drv->driver.name) == 0);
}
How DT Matching Works Internally
The compatible property can have multiple strings for fallback matching:
// DT node with multiple compatible strings (fallback chain):
compatible = "qcom,sm8550-geni-uart", "qcom,geni-uart";

// Kernel tries each string left-to-right:
of_match_device(drv->of_match_table, dev)
  -> of_match_node(matches, dev->of_node)
    For each entry in of_match_table[]:
      Compare entry->compatible with each string in DT compatible property
      -> "qcom,sm8550-geni-uart" == "qcom,sm8550-geni-uart"? YES -> MATCH
      -> OR "qcom,geni-uart" == "qcom,geni-uart"? YES -> MATCH (fallback)
Forward compatibility: A driver matching "qcom,geni-uart" will match even if the DT node specifies a more specific "qcom,sm8550-geni-uart" string. This allows older generic drivers to work with newer SoC-specific DT nodes.
7.4  really_probe(): What the Kernel Does Around Your probe()
File: drivers/base/dd.c
static int really_probe(struct device *dev, struct device_driver *drv)
{
    /* 1. Bind driver to device */
    dev->driver = drv;

    /* 2. Pin control setup (from DT pinctrl-0/pinctrl-names) */
    ret = pinctrl_bind_pins(dev);       // Configure UART TX/RX pin muxing

    /* 3. Power domain attach (from DT power-domains) */
    ret = dev_pm_domain_attach(dev);    // Attach CX power domain

    /* 4. DMA configuration (from DT dma-ranges) */
    ret = dma_configure(dev);

    /* 5. *** Call YOUR probe() *** */
    if (dev->bus->probe)
        ret = dev->bus->probe(dev);     // platform_drv_probe()
    // platform_drv_probe() extracts platform_driver:
    //   struct platform_driver *pdrv = to_platform_driver(dev->driver);
    //   return pdrv->probe(to_platform_device(dev));
    //   -> YOUR my_uart_probe(pdev) is called!

    /* 6. If probe succeeds */
    if (ret == 0) {
        driver_bound(dev);
        // Creates sysfs links:
        // /sys/bus/platform/devices/a94000.serial/driver -> ...qcom_geni_serial
        // /sys/bus/platform/drivers/qcom_geni_serial/a94000.serial -> ...
    }

    /* 7. If probe fails: auto-cleanup */
    if (ret) {
        devres_release_all(dev);        // Free ALL devm_ resources
        dev->driver = NULL;
        dev_pm_domain_detach(dev);
        pinctrl_unselect_state(dev);
    }

    return ret;
}
8. The probe() Function: Complete Deep Dive
The probe() function is where your driver comes alive. It is called exactly once when the driver matches a device. Here is a comprehensive real-world implementation with all seven steps a production driver must perform.
8.1  Complete probe() Implementation
static int my_uart_probe(struct platform_device *pdev)
{
    struct my_uart_dev *dev;
    struct resource *res;
    int irq, ret;

    /* ================================================
     * STEP 1: Allocate driver private data
     * ================================================ */
    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;
    // devm_ = device-managed: auto-freed when device is removed
    // No need for manual kfree() in remove()!

    /* ================================================
     * STEP 2: Get memory resource from DT "reg"
     * ================================================ */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    // Returns: .start=0x00a94000, .end=0x00a97fff
    // Index 0 = first "reg" entry

    dev->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(dev->base))
        return PTR_ERR(dev->base);
    // Maps physical 0x00a94000 -> kernel virtual address
    // Also calls request_mem_region() to claim the MMIO range
    // Now you can: readl(dev->base + REG_OFFSET)
    //              writel(val, dev->base + REG_OFFSET)

    /* ================================================
     * STEP 3: Get IRQ from DT "interrupts"
     * ================================================ */
    irq = platform_get_irq(pdev, 0);
    if (irq < 0)
        return irq;
    // Returns Linux virtual IRQ number (mapped from HW IRQ 358)

    ret = devm_request_irq(&pdev->dev, irq, my_uart_isr,
                           IRQF_TRIGGER_HIGH, "my-uart", dev);
    if (ret)
        return ret;

    /* ================================================
     * STEP 4: Get clocks from DT "clocks"
     * ================================================ */
    dev->clk = devm_clk_get(&pdev->dev, "se");
    if (IS_ERR(dev->clk)) {
        if (PTR_ERR(dev->clk) == -EPROBE_DEFER)
            return -EPROBE_DEFER;  // Clock driver not ready yet, retry later
        return PTR_ERR(dev->clk);
    }

    ret = clk_prepare_enable(dev->clk);
    if (ret)
        return ret;

    /* ================================================
     * STEP 5: Parse additional DT properties
     * ================================================ */
    of_property_read_u32(pdev->dev.of_node, "clock-frequency",
                         &dev->baud_rate);

    /* ================================================
     * STEP 6: Initialize hardware
     * ================================================ */
    writel(UART_RESET, dev->base + UART_CR);
    writel(dev->baud_rate, dev->base + UART_BAUD);
    writel(UART_ENABLE, dev->base + UART_CR);

    /* ================================================
     * STEP 7: Register with subsystem / create char device
     * ================================================ */
    alloc_chrdev_region(&dev->devno, 0, 1, "my-uart");
    cdev_init(&dev->cdev, &my_uart_fops);
    cdev_add(&dev->cdev, dev->devno, 1);
    dev->class = class_create(THIS_MODULE, "my-uart");
    device_create(dev->class, &pdev->dev, dev->devno, NULL, "my-uart0");

    /* ================================================
     * STEP 8: Save private data for later use
     * ================================================ */
    platform_set_drvdata(pdev, dev);
    // Equivalent to: dev_set_drvdata(&pdev->dev, dev)
    // Stores pointer in pdev->dev.driver_data
    // Retrieved in remove(), suspend(), resume() via:
    //   struct my_uart_dev *dev = platform_get_drvdata(pdev);

    dev_info(&pdev->dev, "UART probed at 0x%llx, IRQ %d\n",
             (u64)res->start, irq);

    return 0;  // SUCCESS: return 0
    // Non-zero return = probe failed, device stays unbound
}
8.2  The remove() Function
static int my_uart_remove(struct platform_device *pdev)
{
    struct my_uart_dev *dev = platform_get_drvdata(pdev);

    /* Reverse order of probe() */
    device_destroy(dev->class, dev->devno);
    class_destroy(dev->class);
    cdev_del(&dev->cdev);
    unregister_chrdev_region(dev->devno, 1);

    /* Disable hardware */
    writel(UART_DISABLE, dev->base + UART_CR);
    clk_disable_unprepare(dev->clk);

    /* devm_ resources are auto-freed automatically:
     * - devm_kzalloc        -> auto kfree()
     * - devm_ioremap_resource -> auto iounmap() + release_mem_region()
     * - devm_request_irq    -> auto free_irq()
     * - devm_clk_get        -> auto clk_put()
     */

    return 0;
}
8.3  The devm_ Device Managed Resource System
The devm_ functions are one of the most important patterns in modern kernel driver development:
| Operation | Without devm_ | With devm_ |
| Allocate memory | kmalloc() | devm_kzalloc() -- auto freed |
| Map MMIO | ioremap() | devm_ioremap_resource() -- auto unmap |
| Request IRQ | request_irq() | devm_request_irq() -- auto freed |
| Get clock | clk_get() | devm_clk_get() -- auto put |
| Get regulator | regulator_get() | devm_regulator_get() -- auto put |
| Get GPIO | gpio_request() | devm_gpio_request() -- auto freed |


devm_ resources are freed in REVERSE order of allocation when the device is removed OR when probe() fails partway through. This eliminates complex goto error-unwinding in probe and prevents resource leaks. Always prefer devm_ variants in modern drivers.
How devm_ Works Internally
// drivers/base/devres.c
struct devres {
    struct devres_node node;  // Linked into dev->devres_head list
    u8 data[];                // The actual resource (e.g., allocated memory)
};

// Each devm_ call adds an entry to dev->devres_head (linked list)
// On device removal: walk list in REVERSE, call release function for each

devm_kzalloc(&pdev->dev, size, GFP_KERNEL)
  -> devres_alloc(devm_kfree_release, size, GFP_KERNEL)
  -> devres_add(&pdev->dev, dr)    // Add to device's resource list
  -> return dr->data               // Return usable pointer to caller
8.4  EPROBE_DEFER: Deferred Probing
A very common scenario on Qualcomm SoCs with complex dependency chains:
// Problem: UART driver probes before clock controller driver
static int my_probe(struct platform_device *pdev)
{
    dev->clk = devm_clk_get(&pdev->dev, "se");
    if (IS_ERR(dev->clk)) {
        if (PTR_ERR(dev->clk) == -EPROBE_DEFER)
            return -EPROBE_DEFER;   // "Try again later"
        return PTR_ERR(dev->clk);   // Real error
    }
    // ...
}
What Happens with -EPROBE_DEFER
my_probe() returns -EPROBE_DEFER
  -> really_probe() sees EPROBE_DEFER
    -> devres_release_all(dev)      // Free all allocated devm_ resources
    -> driver_deferred_probe_add(dev)  // Add to deferred list
      -> dev added to deferred_probe_pending_list

Later, when the clock driver probes successfully:
  -> driver_bound(clock_dev)
    -> driver_deferred_probe_trigger()
      -> queue_work(deferred_probe_work)
        -> deferred_probe_work_func()
          -> bus_for_each_dev(... deferred_probe_fn ...)
            -> Retries probe for ALL deferred devices
            -> my_probe() called again -> clock now available -> SUCCESS!
On Qualcomm SoCs, probe ordering is non-deterministic. A UART driver might probe before the clock controller or TLMM (pin controller) driver. -EPROBE_DEFER handles this gracefully. Debug deferred devices: cat /sys/kernel/debug/devices_deferred
9. Complete End-to-End Timeline and Interview Summary
9.1  Boot to Runtime: Complete Timeline
============================================================
BOOT TIME
============================================================
1. Bootloader loads DTB -> passes to kernel
2. unflatten_device_tree() -> struct device_node tree in memory
3. of_platform_default_populate() -> creates platform_device for each DT node
   +-- platform_device "a94000.serial" created with:
       +-- resource[0]: MEM 0xa94000-0xa97fff (from "reg")
       +-- resource[1]: IRQ 358 (from "interrupts", mapped via irqdomain)
       +-- dev.of_node -> DT node with compatible="qcom,geni-uart"
4. Device added to platform_bus -> no matching driver yet -> device waits

============================================================
DRIVER INIT (module_init or built-in initcall)
============================================================
5. platform_driver_register(&qcom_geni_serial_driver)
   +-- driver_attach() -> scan all platform devices
       +-- platform_match("a94000.serial", qcom_geni_serial_driver)
           +-- of_match: "qcom,geni-uart" == "qcom,geni-uart" -> MATCH!

============================================================
PROBE
============================================================
6. really_probe()
   +-- pinctrl_bind_pins()     -> configure UART TX/RX pin muxing
   +-- dev_pm_domain_attach()  -> attach CX power domain
   +-- pdrv->probe(pdev)      -> qcom_geni_serial_probe()
       +-- devm_ioremap_resource()  -> map 0xa94000 to virtual address
       +-- platform_get_irq()       -> get IRQ
       +-- devm_clk_get()           -> get clock (or EPROBE_DEFER)
       +-- Initialize hardware
       +-- Register with tty/uart subsystem (or cdev_add)
       +-- return 0 -> SUCCESS

============================================================
RUNTIME
============================================================
7. User: open("/dev/ttyMSM0")
   -> VFS -> chrdev_open() -> driver's .open()
8. User: write(fd, data, len)
   -> VFS -> driver's .write() -> HW TX FIFO
9. HW RX interrupt -> driver's ISR -> push data to tty layer
10. User: read(fd, buf, len)
    -> tty layer -> buffered data -> copy_to_user -> user

============================================================
REMOVAL (module unload or unbind)
============================================================
11. platform_driver_unregister()
    OR: echo "a94000.serial" > /sys/bus/platform/drivers/.../unbind
    +-- pdrv->remove(pdev)
        +-- Unregister from subsystem
        +-- Disable hardware
        +-- devm_ resources auto-freed in reverse allocation order
9.2  Sysfs Representation After Probe
# Device view
/sys/bus/platform/devices/a94000.serial/
+-- driver -> ../../../drivers/qcom_geni_serial    # Bound driver
+-- of_node -> ../../../firmware/devicetree/.../serial@a94000
+-- modalias    # "of:NserialT(null)Cqcom,geni-uart"
+-- uevent
+-- power/
+-- subsystem -> ../../../bus/platform

# Driver view
/sys/bus/platform/drivers/qcom_geni_serial/
+-- a94000.serial -> .../devices/.../a94000.serial  # Bound device
+-- bind            # echo device name to force bind
+-- unbind          # echo device name to force unbind

# Useful debug commands:
cat /sys/bus/platform/devices/a94000.serial/driver/name
cat /sys/kernel/debug/devices_deferred    # See deferred probes
echo "a94000.serial" > /sys/bus/platform/drivers/qcom_geni_serial/unbind
echo "a94000.serial" > /sys/bus/platform/drivers/qcom_geni_serial/bind
9.3  Platform Driver Interview Q&A
| Question | Answer |
| What is a platform device and why do we need it? | SoC-internal peripherals are memory-mapped and non-discoverable -- unlike USB/PCI, they can't announce themselves. The platform bus is a virtual bus to describe these via Device Tree and match them with drivers via compatible string. |
| Explain the probe flow from Device Tree to driver. | DTB parsed -> device_node tree -> of_platform_populate() creates platform_device for each node (extracts reg->IORESOURCE_MEM, interrupts->IORESOURCE_IRQ). When platform_driver registers, platform_match() compares of_match_table compatible strings. On match, really_probe() calls probe(). |
| What is EPROBE_DEFER? | If probe() depends on a resource (clock, regulator, GPIO) whose provider hasn't probed yet, return -EPROBE_DEFER. Kernel adds device to deferred list, retries when any driver successfully binds. Handles non-deterministic probe ordering. |
| What are devm_ functions? | Device-managed resource functions that automatically free resources when device is removed or probe fails. Freed in reverse allocation order. Eliminates manual cleanup code and prevents resource leaks. Always prefer devm_ in modern drivers. |
| How to debug a platform driver that is not probing? | 1) cat /sys/kernel/debug/devices_deferred -- is it deferred? 2) Check status="okay" in DT. 3) Verify compatible string matches exactly. 4) Check dmesg for probe errors. 5) Verify driver is built (CONFIG_xxx=y/m). 6) Check /sys/bus/platform/devices/ -- does device exist? 7) Check /sys/bus/platform/drivers/ -- is driver registered? |
| What does really_probe() do before calling your probe()? | Sets dev->driver, calls pinctrl_bind_pins() (configures pin muxing from DT pinctrl-0), dev_pm_domain_attach() (attaches power domain), dma_configure(). If probe fails, calls devres_release_all() to free all devm_ resources. |
| Difference between platform_get_resource() and platform_get_irq()? | platform_get_resource(pdev, IORESOURCE_MEM, 0) returns raw struct resource. platform_get_irq(pdev, 0) returns Linux virtual IRQ number (mapped from hardware IRQ through interrupt controller irqdomain). Always use platform_get_irq() for IRQ resources, not platform_get_resource(IORESOURCE_IRQ). |
| How does DT "compatible" forward compatibility work? | DT compatible property can list multiple strings: compatible = "qcom,sm8550-geni-uart", "qcom,geni-uart". The kernel tries each string left-to-right. A driver matching only "qcom,geni-uart" still matches, enabling generic drivers to handle new SoC variants. |


10. The Big Picture and Quick Reference
10.1  Complete System Architecture Diagram
+----------------------------------------------------------------+
|                        USER SPACE                              |
|  app: fd=open("/dev/x")  read(fd)  write(fd)  ioctl(fd)       |
|         |                  |           |            |          |
|         SVC #0            SVC #0      SVC #0       SVC #0     |
+----------|-----------------|-----------|-----------|-----------+
           |                 |           |           |
==== HW EXCEPTION (EL0->EL1 / USR->SVC) ===========================
           |                 |           |           |
+----------v-----------------v-----------v-----------v-----------+
|                     KERNEL VFS LAYER                           |
|  sys_open()          sys_read()  sys_write()  sys_ioctl()     |
|    |                    |            |              |          |
|    path_walk->inode     fdget(fd)->filp             |          |
|    |                    |            |              |          |
|    chrdev_open()        |            |              |          |
|    |                    |            |              |          |
|    cdev_map[major]      |            |              |          |
|    |                    v            v              v          |
|    struct cdev   filp->f_op->read  .write   .unlocked_ioctl   |
|    |                    |            |              |          |
|    v                    |            |              |          |
|    filp->f_op = &my_fops|            |              |          |
+----+--------------------+-----------+--------------+-----------+
     |                    |           |              |
+----v--------------------v-----------v--------------v-----------+
|                     YOUR DRIVER                                |
|  .open()            .read()     .write()       .ioctl()       |
|  |                  |            |              |              |
|  set private_data   copy_to_user copy_from_user switch(cmd)   |
|                     |            |              |              |
|                     v            v              v              |
|              +-----------------------------------------------+ |
|              |         HARDWARE (MMIO Registers)             | |
|              |   readl(base+OFF)    writel(val, base+OFF)    | |
|              +-----------------------------------------------+ |
|                                                                |
|  Registered via platform_driver:                              |
|    DT compatible match -> probe() -> ioremap + irq + cdev_add |
+----------------------------------------------------------------+
10.2  Quick-Fire Interview Reference Table
| Question | One-Line Answer |
| How does kernel know which driver to call for open()? | major:minor -> cdev_map -> struct cdev -> file_operations |
| Why save orig_r0 / orig_x0? | Syscall restart after signal interruption |
| ARM32 return instruction? | movs pc, lr -- atomically: PC<-LR, CPSR<-SPSR |
| ARM64 Spectre mitigation in syscalls? | array_index_nospec() -- bounds check + speculation barrier |
| What triggers platform probe()? | compatible string match between DT node and driver's of_match_table |
| What is platform bus? | Virtual bus for non-discoverable SoC peripherals |
| unlocked_ioctl vs old ioctl? | Old held BKL (global lock); unlocked does not -- driver does own locking |
| Return -ENOTTY in ioctl? | "Device doesn't support this ioctl command" (historical: not a typewriter) |
| copy_to_user vs memcpy? | copy_to_user validates pointer + handles page faults; memcpy crashes on bad address |
| How to debug probe not happening? | Check: DT status=okay, compatible match, driver built/loaded, devices_deferred, dmesg |
| filp->private_data purpose? | Per-open-instance context set in .open(), used in all subsequent ops |
| What does really_probe() do before your probe()? | pinctrl_bind_pins, dev_pm_domain_attach, dma_configure, then calls your probe |
| What is VBAR? | Vector Base Address Register -- base of exception vector table |
| EC field in ESR_EL1? | Exception Class: 0x15 = SVC from AArch64, 0x25 = data abort from lower EL |
| devm_ resource freed when? | On remove() or if probe() fails -- auto in reverse allocation order |
| ARM32 ^ suffix on LDM/STM? | Accesses user-bank registers (SP, LR) while in SVC mode |
| TIF_SIGPENDING action on return? | Call do_signal() -> deliver signal -> possible syscall restart |
| OABI vs EABI difference? | OABI: syscall# in SWI instruction (slow). EABI: syscall# in r7 (fast, standard) |


10.3  Quick-Reference Flow Summary
User app: open()
  -> glibc: r7/x8 = syscall#, r0-r5/x0-x5 = args, SVC #0
    -> HW: USR->SVC / EL0->EL1, save PC & CPSR/PSTATE, jump to vector
      -> vector_swi / el0_sync: save all regs -> pt_regs on kernel stack
        -> Extract syscall# (r7 or x8), check TIF flags
          -> Fast: sys_call_table[scno] -> handler (VFS, driver, etc.)
          -> Slow: trace_enter -> handler -> trace_exit
        -> ret_fast_syscall / ret_to_user:
          -> Check TIF_NEED_RESCHED -> schedule()
          -> Check TIF_SIGPENDING -> do_signal() -> possible restart
          -> Restore regs from pt_regs
          -> movs pc,lr / eret -> HW: SVC->USR / EL1->EL0
  -> glibc: check r0/x0, set errno if negative
-> app: fd or -1

VFS routing after open():
  -> inode->i_rdev -> cdev_map[major:minor] -> struct cdev
  -> cdev->ops = &my_fops -> filp->f_op = &my_fops
  -> my_fops->open(inode, filp) // YOUR DRIVER'S open()

After open(), for read/write/ioctl:
  -> fdget(fd) -> struct file -> filp->f_op->read/write/ioctl
  -> Direct function pointer call // NO lookup needed

Platform driver probe():
  -> DT: compatible="qcom,geni-uart" -> platform_device created at boot
  -> Driver: of_match_table = { .compatible = "qcom,geni-uart" }
  -> platform_match() -> really_probe() -> your probe(pdev)
  -> probe(): ioremap + get_irq + get_clk + hw_init + cdev_add

| Part 2 (Next Document) will cover: Interrupt Handling (ARM GIC), Device Tree Deep Dive, DMA Framework, Power Management (PM Domain, Runtime PM, suspend/resume), and Memory Management (kmalloc, vmalloc, DMA-coherent memory). |


Crash Dump Analysis Guide
Part 1: Crash Data Types & Analysis Workflow

Technical Reference for Embedded Linux & Qualcomm SoC Platforms
Kernel & User-Space Crash Data • 7-Phase Analysis Workflow • Root Cause Patterns

|  |


| Attribute | Details |
| Document | Crash Dump Analysis Guide — Part 1 of 3 |
| Scope | Crash Data Types, Kernel/User-Level Data Sources, 7-Phase Analysis Workflow |
| Architecture | ARM64 (AArch64), ARM32 — Qualcomm SoC Platforms |
| Tools Covered | crash utility, T32 (Lauterbach), CrashScope, GDB, Ramparse, addr2line |
| Audience | Embedded Linux / BSP Engineers, Kernel Developers, Staff/Senior Engineers |


# Section 1: What is Crashdump, Ramdump & Kernel Crash Data
When a Linux kernel or user-space process crashes on an embedded or mobile platform, the system generates diagnostic data that engineers can use to determine the root cause. This section provides a comprehensive breakdown of every type of crash data the kernel provides, from low-level kernel panic dumps to Android tombstones, organized by source level.
## 1.1  Kernel-Level Crash Data Types
The following data types are generated when the kernel itself crashes or detects a fatal condition:
### 1.1.1  Crash Dump (kdump)
A full or filtered copy of system memory captured at the time of a kernel panic. Uses the kexec mechanism to boot a secondary "capture" kernel that saves the dump.
- Produces a vmcore file (ELF format) analyzable with crash utility or GDB
- Contains: all physical memory pages, kernel data structures, task states, register contents
- Configurable filtering via makedumpfile (exclude zero/free/cache pages)
- Key config: CONFIG_KEXEC + crashkernel= boot parameter

### 1.1.2  RAM Dump (Ramdump)
A raw binary dump of the entire physical RAM taken when the system crashes. Common on mobile/embedded platforms. Qualcomm uses this extensively via the warm boot path.
- On Qualcomm SoCs: device enters download mode on crash; tools like QPST/QFIL or T32 extract full RAM
- Parsed using CrashScope, Ramparse, or T32 scripts to reconstruct kernel state
- Contains everything: kernel memory, user-space memory, device memory mappings, DMA buffers
- Collection: Sahara protocol over USB — DDR preserved in self-refresh during warm reset

### 1.1.3  Kernel Panic / Oops Logs
Oops: Non-fatal kernel fault — logs register state, backtrace, faulting instruction, and affected task.
Panic: Fatal crash — same info as Oops plus the system halts.
Data captured in a kernel Oops/Panic:
- CPU registers (PC, LR, SP, CPSR/PSTATE on ARM64)
- Call stack / backtrace
- Loaded modules list
- Memory info around the faulting address
- PID and command name of current process

### 1.1.4  pstore (Persistent Store)
Survives reboots by writing to persistent RAM (ramoops), EFI variables, or block devices.
| pstore Entry | Content |
| dmesg | Last kernel log before crash |
| console | Console output at crash time |
| pmsg | User-space messages (Android logcat) |
| ftrace | Last function trace entries before crash |


### 1.1.5  Last Kmsg / ramoops
A reserved memory region that preserves the kernel ring buffer (dmesg) across a crash/reboot.
- Accessible post-reboot at /sys/fs/pstore/dmesg-ramoops-* or /proc/last_kmsg (older kernels)
- Config: CONFIG_PSTORE + CONFIG_PSTORE_RAM + reserve memory in DT

### 1.1.6  ftrace / Event Trace Buffers
If enabled, per-CPU trace buffers capture function calls, scheduling events, and interrupts.
- Can be preserved via pstore for post-mortem analysis
- Access: /sys/kernel/debug/tracing/ during live system or via pstore after reboot
- Key feature: Function graph tracer shows call hierarchy with timestamps — invaluable for timing issues

## 1.2  User-Level Crash Data Types
When a user-space process crashes (receives a fatal signal such as SIGSEGV, SIGABRT, or SIGBUS), the following data sources are available:
### 1.2.1  Core Dump
Generated when a user-space process crashes. Contains the full process memory state at the time of crash in ELF format.
Contents of a core dump:
- Process memory mappings (heap, stack, shared libraries, mmap regions)
- CPU register state at crash point
- Signal information (signal number, signal info structure)
- Thread states (all threads including their individual stacks)
- Auxiliary vector (AT_*) — runtime linker information
- File descriptors (optional, via /proc/PID/fd)

Format: ELF core file — analyzable with GDB, eu-readelf, readelf
Control via: ulimit -c, /proc/sys/kernel/core_pattern, coredump_filter

# Enable core dumps (unlimited size)
ulimit -c unlimited

# Set core dump pattern (filename)
echo "/tmp/core.%e.%p.%t" > /proc/sys/kernel/core_pattern

# Check coredump filter (default 0x33)
cat /proc/<PID>/coredump_filter

# Analyze with GDB
gdb /path/to/binary /tmp/core.my_binary.1234.1700000000

### 1.2.2  /proc/PID/ Information (Pre-Crash Capture)
When a process hangs or before it crashes, the following /proc entries contain critical diagnostic information:
| /proc Entry | Information Provided |
| /proc/PID/maps | Memory layout — all mapped regions with addresses, permissions, and backing files |
| /proc/PID/smaps | Detailed per-region memory usage including RSS, PSS, shared/private split |
| /proc/PID/status | Task state, signals pending/blocked/ignored, thread count, UID/GID |
| /proc/PID/stack | Kernel stack of a stuck process (shows where it is sleeping in kernel) |
| /proc/PID/wchan | Wait channel — kernel function where process is currently blocked |
| /proc/PID/fd | Open file descriptors with symlinks to actual files/sockets/pipes |
| /proc/PID/fdinfo | File descriptor flags and position for each open fd |


### 1.2.3  Tombstones (Android-Specific)
Android's crash handler (debuggerd) generates a tombstone file when a native process crashes.
- Contains: registers, backtrace, memory around registers, open FDs, signal info, logcat snippets
- Stored in /data/tombstones/ (typically 10 most recent tombstones kept)
- Backtrace is symbolicated if debug symbols are present on the device
- Includes memory map, abort message (for SIGABRT), and system build info

# View tombstones on Android device
adb shell ls /data/tombstones/
adb pull /data/tombstones/tombstone_00

# Symbolicate a tombstone with ndk-stack
ndk-stack -sym ./symbols/system/lib64/ < tombstone_00

# Live tombstone output
adb logcat -d | grep -A 50 "*** FATAL EXCEPTION"

## 1.3  Other Kernel Diagnostic Mechanisms
Beyond crash dumps, the Linux kernel provides several built-in diagnostic mechanisms that capture specific failure modes:
| Mechanism | What It Captures | Level |
| BUG_ON / WARN_ON | Stack trace + register dump when assertion fails; BUG_ON is fatal, WARN_ON continues | Kernel |
| softlockup / hardlockup detector | Stuck CPU detection: softlockup >20s without schedule(), hardlockup >10s without NMI | Kernel |
| hung_task detector | Tasks in uninterruptible (D) state beyond configurable threshold (default 120s) | Kernel |
| RCU stall detector | RCU callback stalls with per-CPU state, grace period info, and offloaded callbacks | Kernel |
| watchdog reset | Hardware watchdog captures reset reason + limited CPU state before asserting reset line | Kernel/HW |
| MCE (Machine Check Exception) | Hardware error logs from CPU (corrected/uncorrected errors, DRAM ECC, cache errors) | HW/Kernel |
| KASAN / KFENCE | Memory corruption: use-after-free, out-of-bounds with full allocation and free stack traces | Kernel (debug) |
| lockdep | Deadlock detection: lock acquisition order tracking, circular wait chains, lock class validation | Kernel (debug) |
| minidump (Qualcomm) | Selective memory regions (stacks, key structures) instead of full ramdump — production safe | Kernel |


### Key Mechanism Details
KASAN (Kernel Address Sanitizer): Uses shadow memory to detect heap/stack/global buffer overflows and use-after-free errors at run time. Reports exact allocation and free stack traces. Enabled via CONFIG_KASAN=y.

lockdep: Validates locking order at run time. Detects AB-BA deadlock patterns, lock class inversions, and incorrect lock usage (e.g., taking spinlock in interrupt with IRQ disabled). Enabled via CONFIG_PROVE_LOCKING=y.

softlockup / hardlockup: Separate watchdog threads on each CPU. softlockup fires when a CPU does not call schedule() for >20 seconds. hardlockup fires when no NMI occurs for >10 seconds, indicating the CPU is completely stuck (IRQs disabled).
## 1.4  Qualcomm-Specific Crash Data Context
Qualcomm SoC platforms (Snapdragon series) provide additional crash data infrastructure beyond standard Linux. These mechanisms are critical for production crash collection and deep hardware-level debugging.
| Qualcomm Mechanism | Description & Purpose |
| Minidump | Captures only pre-registered memory regions (task stacks, kernel log buffer, key global structures). Much smaller than full ramdump (50–200 MB vs. GBs). Production-safe and fast to collect. Enabled via CONFIG_QCOM_MINIDUMP. |
| Secure Watchdog Bite | Trustzone (TZ) firmware captures CPU context before full reset when the secure watchdog expires. Provides register state from TZ/HYP perspective that is not visible from kernel. |
| IMEM Cookie | Internal Memory (IMEM) stores crash reason and magic values. Bootloader reads these magic values at next boot to decide between download mode (dump collection) and normal reboot path. |
| SCM Calls | Secure Channel Manager calls: used to communicate download mode intent to TrustZone before triggering warm reset. Ensures TZ is aware of the crash and prepares for dump mode. |
| DCC (Data Capture & Compare) | Hardware-level register capture block. Configured at boot with a list of register addresses. Autonomously captures all programmed register values at crash time with no software intervention needed. Output stored in DCC_SRAM, accessible in any dump type. |
| RTB (Register Trace Buffer) | Per-CPU ring buffer that records last N events (schedule, IRQ, SMC calls). Stored in minidump region KRTB. Provides execution history without ftrace overhead. |
| OCIMEM | On-Chip Internal Memory: captured as OCIMEM.BIN in ramdump. Contains IPC message RAM, shared memory (SMEM), and TZ data critical for subsystem communication analysis. |


### 1.4.1  IMEM Cookie Mechanism Detail
The IMEM region is a small internal memory block that survives warm reset. Critical cookie offsets:
IMEM (Internal Memory) Layout:
┌────────────────────────────────────────┐
│ Offset 0x0:  restart_reason            │ ← Magic values
│ Offset 0x4:  download_mode_cookie      │ ← 0x12345678 = download mode
│ Offset 0x8:  minidump_cookie           │ ← 0x87654321 = minidump mode
│ Offset 0xC:  pon_reason                │
│ Offset 0x10: dload_type                │ ← Full dump vs. mini
│ Offset 0x6C: boot_misc_info            │
└────────────────────────────────────────┘

On crash:
  1. Kernel sets download_mode_cookie = 0x12345678 via SCM call
  2. Warm reset triggered (PSHOLD deassert)
  3. XBL reads IMEM cookie → detects crash → enters Sahara mode

### 1.4.2  DCC (Data Capture and Compare) Detail
DCC is a hardware debugging block that captures register values autonomously at crash time without any software intervention after initial programming:
- Programmed at boot with a list of register addresses to monitor (GCC clocks, TLMM GPIOs, RPM status, NOC registers)
- Triggered automatically on watchdog bite or crash signal
- Captures all programmed registers into DCC_SRAM
- Available in both full ramdump and minidump as DCC_SRAM region
- Critical for NOC (Network On Chip) errors where bus state must be captured before CPU can report it

# Section 2: Crash Dump Analysis Workflow
A structured, reproducible workflow is essential for efficient crash analysis. The following 7-phase methodology covers the complete process from initial crash detection through post-mortem documentation, applicable to both kernel and user-space crashes on embedded Linux and Qualcomm platforms.
### Workflow Overview
┌─────────────────────────────────────────────────────────────┐
│           7-PHASE CRASH DUMP ANALYSIS WORKFLOW              │
├─────────────────────────────────────────────────────────────┤
│  Phase 1  │  Crash Detection & Collection                   │
│  Phase 2  │  Environment Setup                              │
│  Phase 3  │  Initial Triage (First 5 Minutes)               │
│  Phase 4  │  Detailed Analysis with Tools                   │
│  Phase 5  │  Root Cause Investigation Patterns              │
│  Phase 6  │  Verification & Fix                             │
│  Phase 7  │  Post-Mortem Documentation                      │
└─────────────────────────────────────────────────────────────┘

## Phase 1: Crash Detection & Collection
Before analysis can begin, the crash must be detected and the appropriate data collected. Different crash signatures indicate different collection methods.
### 1.1  Identifying the Crash Type
| Indicator / Observable | Crash Type | Collection Method |
| Device reboots, enters download mode | Kernel panic / watchdog bite | Ramdump via QPST / QFIL / T32 (Sahara protocol) |
| last_kmsg / pstore available after reboot | Kernel Oops / Panic | Read from /sys/fs/pstore/ or /proc/last_kmsg |
| Tombstone file generated in /data/tombstones/ | User-space native crash | Pull from /data/tombstones/ via adb |
| Core dump file created | User-space crash (SIGSEGV/SIGABRT) | Retrieve from path set in /proc/sys/kernel/core_pattern |
| Minidump collected (smaller binary files) | Selective kernel crash | Extract via Sahara protocol, use Ramparse |
| App crash (Java/Kotlin on Android) | JVM exception / ANR | logcat, /data/anr/traces.txt, bugreport |


### 1.2  Collection Steps
Step 1 — Full Ramdump: Connect device via USB. Enter download mode (warm reset path). Use QPST/QFIL to pull raw memory. All DDR banks saved as BIN files with address metadata.

Step 2 — Minidump: Collected via Sahara protocol. Smaller (50–200 MB). Contains only regions registered via msm_minidump_add_region(). Use Ramparse with --minidump-dir option.

Step 3 — kdump/vmcore: If configured, secondary kernel saves vmcore to disk/network after panic. Launch analysis with crash vmlinux vmcore.

Step 4 — Core dumps: Retrieved from the path set in /proc/sys/kernel/core_pattern. Analyze with gdb <binary> <core>.

Step 5 — Tombstones: adb pull /data/tombstones/tombstone_XX. Symbolicate with ndk-stack or addr2line.

## Phase 2: Environment Setup
A properly configured analysis environment is critical. Missing or mismatched symbols will produce incorrect backtraces and waste investigation time.
### 2.1  Required Tools
# Required tools for crash dump analysis
├── crash utility (kernel ramdump/vmcore analysis)
├── T32 (Lauterbach TRACE32)              ← Primary tool for Qualcomm ramdump
├── CrashScope (Qualcomm proprietary)     ← Auto-parse + GUI navigation
├── Ramparse (Qualcomm Python parser)     ← Extracts dmesg, tasks, backtraces
├── addr2line / objdump / readelf         ← Symbol resolution & disassembly
├── pahole / dwarfdump                    ← Struct layout analysis
├── vmlinux          ← Unstripped kernel with debug symbols (MANDATORY)
├── System.map       ← Kernel symbol address map
└── Matching kernel source tree           ← For source-level correlation

### 2.2  Symbol Setup Checklist
| Checklist Item | Details & Commands |
| Verify vmlinux build ID matches | file vmlinux | grep "Build ID"Must match crash-time kernel exactly |
| Load kernel module symbols at correct addresses | crash> mod -S /path/to/modules/Module load addresses from /proc/modules or crash> mod |
| User-space: obtain unstripped binaries matching build | Symbols must match crash-time library versions exactlyUse build fingerprint to identify matching symbol set |
| Verify kernel config (for debug features) | strings vmlinux | grep "CONFIG_KASAN"Confirms which debug features were active |
| Prepare ramdump address map (Qualcomm) | Load DDRCS_info.txt or dump_info.txt for address rangesT32: Data.Load.Binary per BIN file with correct address |


## Phase 3: Initial Triage (First 5 Minutes)
The first 5 minutes of analysis should focus on quickly classifying the crash, identifying the faulting function, and determining a direction for deeper investigation.
### 3.1  Kernel Crash Initial Triage Steps
When analyzing a kernel crash from last_kmsg, pstore, or the crash utility:
Step 1: Identify crash signature
         → Look at the panic/oops message:
           "Unable to handle kernel paging request"  → NULL/bad pointer
           "Kernel BUG at..."                        → BUG_ON triggered
           "NULL pointer dereference"                → struct member of NULL ptr
           "Bad mode in ... handler"                 → Wrong exception level
           "BUG: soft lockup - CPU#X stuck for XXs!" → CPU stuck in loop

Step 2: Note the faulting address (PC/IP)
         → Convert to symbol: addr2line -e vmlinux <address>
         → Or: crash> dis <address>
         → Identify: kernel function or module? Which line?

Step 3: Check the call stack / backtrace
         → Identify the code path leading to the crash
         → Find where the bad pointer was last valid
         → Note any workqueue / timer / interrupt context

Step 4: Note the current process (PID, comm)
         → Is it a kernel thread (kworker, kthread) or user-space context?
         → Is this a known problematic driver or subsystem?

Step 5: Check crash frequency
         → One-off vs. reproducible → guides investigation depth
         → Time-based (at boot? after X hours?) → memory leak or race?

### 3.2  User-Space Crash Initial Triage Steps
For user-space crashes identified via core dump, tombstone, or GDB:
Step 1: Identify the signal
         SIGSEGV → Segmentation fault (invalid memory access)
         SIGABRT → Abort (assert failure, heap corruption detected)
         SIGBUS  → Bus error (alignment or memory-mapped file issue)
         SIGFPE  → Floating point exception (div by zero, overflow)

Step 2: Note faulting address and instruction
         → Tombstone: "signal 11 (SIGSEGV), fault addr 0x..."
         → Core: gdb> info signal → x/1i $pc

Step 3: Check thread that crashed + its backtrace
         → Which thread? (main thread vs. worker thread)
         → backtrace (bt) in GDB or tombstone frames section

Step 4: Examine memory map (valid mapping at fault address?)
         → gdb> info proc mappings
         → Is fault address NULL? Stack overflow? Unmapped region?

Step 5: Check register values for corruption patterns
         → 0x0: NULL pointer
         → 0xDEADBEEF / 0xABADCAFE: Debug sentinel/poison values
         → Small integers in pointer context: wrong type cast

## Phase 4: Detailed Analysis with Tools
After initial triage establishes the crash context, detailed analysis uses specialized tools to reconstruct the system state and identify the root cause.
### 4.1  Using the crash Utility (Kernel Analysis)
# Launch crash utility
crash vmlinux vmcore          # For kdump vmcore
crash vmlinux @ramdump        # For ramdump (with ramparse output)
crash vmlinux ramdump.elf     # ELF-format ramdump

# ── Essential crash commands ──────────────────────────────────
crash> log                    # Kernel log buffer (full dmesg)
crash> bt                     # Backtrace of panicking task
crash> bt -a                  # Backtrace of ALL CPUs (critical for lockups)
crash> bt -F                  # Full frame info including frame sizes
crash> ps                     # Process list at crash time
crash> ps -k                  # Kernel threads only
crash> task <address>         # Examine task_struct in detail
crash> vm <pid>               # Virtual memory mappings for a process
crash> kmem -s                # Slab allocator state
crash> kmem -i                # General memory info
crash> rd <address> <count>   # Read raw memory (N quadwords)
crash> dis <function>         # Disassemble function
crash> dis -l <address>       # Disassemble with source line info
crash> files <pid>            # Open file descriptors for process
crash> runq                   # Run queues per CPU (scheduler state)
crash> irq -a                 # All IRQ states
crash> mod                    # Loaded modules at crash time
crash> mod -S /path/to/mods/  # Load module symbols
crash> search -u <pattern>    # Search memory for pattern
crash> struct task_struct      # Display task_struct layout
crash> p init_task             # Print a global kernel variable
crash> whatis <symbol>         # Show type of a symbol

### 4.2  Using T32 (Lauterbach TRACE32)
T32 provides a GUI-based environment for ramdump analysis with MMU reconstruction and source-level browsing:
; ── Load symbols ─────────────────────────────────────────────
Data.Load.Elf vmlinux /NoCODE /NoClear

; ── Load ramdump binary segments ────────────────────────────
Data.Load.Binary DDRCS0_0.BIN 0x80000000--0xBFFFFFFF /NoClear
Data.Load.Binary DDRCS0_1.BIN 0xC0000000--0xFFFFFFFF /NoClear
Data.Load.Binary OCIMEM.BIN   0x14680000--0x1469FFFF /NoClear

; ── Reconstruct MMU (essential for virtual address resolution) ─
MMU.FORMAT LINUXTABLES swapper_pg_dir
MMU.SCAN
MMU.ON

; ── Analysis commands ───────────────────────────────────────
Register.view             ; CPU registers at crash
Frame.view /Caller        ; Call stack with caller info
List                      ; Source code at PC
Data.dump <address>       ; Raw memory view
Var.View <structure>      ; View kernel data structure
MMU.DUMP.TLB             ; TLB state at crash
MMU.DUMP.PageTable        ; Full page table walk

; ── Load module at specific address ─────────────────────────
Data.Load.Elf my_module.ko /NoCODE /NoClear /StripPART 4 0x<module_addr>

### 4.3  Using CrashScope (Qualcomm)
CrashScope is Qualcomm's proprietary GUI tool that automates ramdump parsing and provides structured navigation of crash data:
1. Load ramdump files + vmlinux into CrashScope
   → File → Open Dump → select all BIN files + vmlinux

2. CrashScope auto-parses:
   → Crash reason & signature (panic string, ESR value)
   → Per-CPU register state (all cores)
   → Kernel log (dmesg reconstruction)
   → Task list + individual backtraces
   → Workqueue states (pending/active work items)
   → IRQ states (pending, in-progress, disabled)
   → Scheduler info (run queues, current tasks)
   → Memory info (slab, buddy allocator)

3. Navigation workflow:
   Crash Summary → identifies crash type automatically
   Backtrace tab  → per-CPU and crashed task call chains
   Source view    → correlate to source lines (with vmlinux)
   Memory tab     → browse physical/virtual memory
   Tasks tab      → examine all task_structs

## Phase 5: Root Cause Investigation Patterns
Most kernel and user-space crashes fall into recognizable patterns. Identifying the pattern early allows targeted analysis and avoids time-consuming exploration of irrelevant data.
### Pattern A: NULL Pointer Dereference
| Aspect | Details |
| Symptom | "Unable to handle kernel NULL pointer dereference at virtual address 0x000000XX" — fault address is close to 0x0 (NULL + struct member offset) |
| Analysis Step 1 | Look at faulting instruction (Code: line, instruction in parentheses) → identify which register held the base pointer |
| Analysis Step 2 | Walk backtrace to find where the pointer was last assigned — look for deferred init, error paths, or concurrent free |
| Analysis Step 3 | Check: race condition? Missing NULL check? Use-after-free? Was object already freed? |
| Analysis Step 4 | Use pahole -C <struct_name> vmlinux to identify which struct member is at the fault offset |
| Key Command | crash> struct <type_name> | addr2line -e vmlinux <PC> | pahole -C struct_name vmlinux |


### Pattern B: Use-After-Free
| Aspect | Details |
| Symptom | Access to freed slab object. Poison patterns: 0x6b6b6b6b (SLUB freed) or KASAN "use-after-free" report with stack traces |
| Analysis Step 1 | Identify the slab cache → which object type? (crash> kmem -s or KASAN report) |
| Analysis Step 2 | Find the allocation site: SLUB debug alloc_traces, or KASAN "allocated by" stack |
| Analysis Step 3 | Find the free site: free_traces from SLUB debug, or KASAN "freed by" stack |
| Analysis Step 4 | Race between free and access → check locking and reference counting (kref / refcount_t) |
| Key Config | CONFIG_KASAN=y | CONFIG_SLUB_DEBUG=y | slub_debug=FZ on cmdline |


### Pattern C: Stack Overflow
| Aspect | Details |
| Symptom | Corruption at thread_info boundary. "stack-protector: Kernel stack is corrupted in: <function>". SP points outside valid stack range. |
| Analysis Step 1 | Check stack usage: crash> bt -F → shows frame sizes per call. Identify deepest frames. |
| Analysis Step 2 | Look for deep recursion or large local variables (VLAs, large struct allocations on stack) |
| Analysis Step 3 | Check VMAP_STACK guard page hit — CONFIG_VMAP_STACK provides guard pages that trigger fault on overflow |
| Analysis Step 4 | Examine thread_info magic value corruption — if THREAD_SIZE_ORDER canary is overwritten, stack overflow confirmed |
| Key Config | CONFIG_VMAP_STACK=y | CONFIG_STACK_VALIDATION=y | objtool stack checking |


### Pattern D: Deadlock / Soft Lockup
| Aspect | Details |
| Symptom | "BUG: soft lockup - CPU#X stuck for XXs!" or "INFO: task <name>:<pid> blocked for more than 120 seconds" or watchdog bite |
| Analysis Step 1 | crash> bt -a → Check what ALL CPUs are doing simultaneously. Identify which CPUs are stuck. |
| Analysis Step 2 | Look for spinlock holders: examine lock variable contents. Check spin_lock() in backtrace. |
| Analysis Step 3 | Check for circular wait patterns (AB-BA deadlock) across different CPUs |
| Analysis Step 4 | Examine IRQ-disabled duration — preempt_count, local_irq_save paths |
| Analysis Step 5 | lockdep report (if enabled) gives the exact lock chain causing deadlock |
| Key Commands | crash> bt -a | crash> runq | crash> irq -a | lockdep trace in dmesg |


### Pattern E: User-Space Crash (SIGSEGV)
Analyzing user-space crashes with GDB from a core dump or tombstone:
# Launch GDB with binary and core dump
$ gdb <binary> <core>

(gdb) bt                        # Full backtrace of crashing thread
(gdb) bt full                   # Backtrace with local variables
(gdb) info registers            # All register values at crash
(gdb) info proc mappings        # Memory map (check if fault addr is mapped)
(gdb) x/10i $pc                 # 10 instructions at crash point
(gdb) print *<pointer>          # Dereference a pointer variable
(gdb) print sizeof(struct foo)  # Check struct size
(gdb) thread apply all bt       # Backtraces of ALL threads
(gdb) info threads              # List all threads with states
(gdb) frame <N>                 # Switch to specific stack frame
(gdb) info locals               # Local variables in current frame

# Check for heap corruption (common with SIGABRT)
(gdb) x/20wx $sp                # Examine stack memory
(gdb) p __libc_single_threaded  # Threading info

# addr2line for tombstone symbolication
$ addr2line -e /path/to/binary -f 0x<fault_address>
$ addr2line -e /path/to/lib.so -f 0x<relative_address>

## Phase 6: Verification & Fix
Once the root cause is identified through dump analysis, the fix must be rigorously verified before submission. Inadequate verification is a primary cause of recurring bugs.
### 6.1  Fix Development & Verification Steps
┌─────────────────────────────────────────────────────────────┐
│         VERIFICATION & FIX CHECKLIST                        │
├─────────────────────────────────────────────────────────────┤
│  Step 1: Form hypothesis from analysis                      │
│          → State clearly: "The bug is X because Y"          │
│          → Identify all code paths affected                 │
│                                                             │
│  Step 2: Correlate with source code review                  │
│          → Read the full function, not just the crash site  │
│          → Look for similar patterns elsewhere in codebase  │
│                                                             │
│  Step 3: Check git log for recent changes in area           │
│          → git log --oneline <file>                         │
│          → git blame <file>  → find who changed the code    │
│          → bisect if needed: git bisect start               │
│                                                             │
│  Step 4: Reproduce (if possible) with debug tools enabled   │
│          → KASAN: CONFIG_KASAN=y → catches memory bugs      │
│          → KCSAN: CONFIG_KCSAN=y → catches data races       │
│          → lockdep: CONFIG_PROVE_LOCKING=y → deadlocks      │
│          → SLUB_DEBUG=y + slub_debug=FZ → heap corruption   │
│                                                             │
│  Step 5: Develop fix                                        │
│          → Minimal change that addresses root cause         │
│          → Avoid masking symptoms (NULL check at use site)  │
│          → Fix the initialization/lifecycle issue           │
│                                                             │
│  Step 6: Verify fix does not regress                        │
│          → Run existing kernel test suites                  │
│          → LTP, kselftest, driver-specific tests            │
│          → Boot test on real hardware                       │
│                                                             │
│  Step 7: Stress test the fix                                │
│          → Trinity (syscall fuzzer) if user/kernel boundary │
│          → Module load/unload loops                         │
│          → Concurrent access with artificially injected     │
│            delays to expose race window                     │
└─────────────────────────────────────────────────────────────┘

### 6.2  Debug Tool Commands for Reproduction
# Enable KASAN (memory error detection)
# In kernel config: CONFIG_KASAN=y, CONFIG_KASAN_OUTLINE=y
# Boot arg: kasan.fault=panic

# Enable lockdep
# In kernel config: CONFIG_PROVE_LOCKING=y, CONFIG_DEBUG_LOCKDEP=y

# Enable SLUB debug
echo 1 > /sys/kernel/debug/slab/<slab_name>/validate
# Or boot with: slub_debug=FZ,<slab_name>

# Trigger kernel panic for testing dump flow
echo c > /proc/sysrq-trigger       # Immediate kernel panic
echo 9 > /proc/sysrq-trigger       # Immediate shutdown

# Check for memory leaks with kmemleak
echo scan > /sys/kernel/debug/kmemleak
cat /sys/kernel/debug/kmemleak

## Phase 7: Post-Mortem Documentation
Thorough documentation ensures that knowledge is preserved, reviewers can understand the analysis, and similar crashes can be identified faster in the future.
### 7.1  Crash Report Template
CRASH REPORT TEMPLATE
═══════════════════════════════════════════════════════════════

├── SUMMARY (1-liner)
│   Example: "Kernel panic due to NULL pointer dereference in
│   my_driver_irq_handler() when IRQ fires during driver remove"

├── IMPACT
│   Severity : Critical / High / Medium / Low
│   Frequency: Always / Intermittent (rate: 1/100 boots)
│   Affected builds: Android 14 / kernel 5.15.78 onwards
│   Customer impact: Device reboot / data loss / service outage

├── ENVIRONMENT
│   Device   : Qualcomm SM8550 (Snapdragon 8 Gen 2)
│   Build    : LA.VENDOR.1.0.r1-00XX-genXXX.0
│   Kernel   : 5.15.78 (commit: abc1234)
│   Android  : 14.0 (build: XXXX)

├── CRASH SIGNATURE
│   Panic message: "Unable to handle kernel paging request..."
│   PC address   : my_buggy_function+0x48/0x120 [my_module]
│   Call trace   :
│     my_buggy_function+0x48/0x120 [my_module]
│     my_caller_function+0x84/0xf0 [my_module]
│     process_one_work+0x1e8/0x390

├── ROOT CAUSE ANALYSIS
│   The pointer <ptr_name> in struct <struct_name> can be NULL
│   when <condition>. Function <function_name> accesses member
│   at offset 0xXX without a NULL check. Race between <thread_A>
│   freeing the object and <thread_B> accessing it.

├── FIX DESCRIPTION
│   Add reference counting to <object_type> using kref.
│   Acquire reference before access, release after use.
│   commit: [fix commit hash]

├── VERIFICATION STEPS
│   1. Reproduce scenario manually (N/100 boots before fix)
│   2. Confirm zero crashes over 500 boot cycles with fix
│   3. KASAN clean in stress test: module load/unload x1000

└── PREVENTION
    Static analysis rule: Add checkpatch warning for <pattern>
    Test addition: add test_<feature>_teardown_race to test suite
    Code review guideline: document lifetime rules in header

### 7.2  Documentation Best Practices
| Practice | Why It Matters |
| Always include the exact panic/oops string | Enables grep-based search across crash databases and future dumps |
| Include both PC and LR addresses | LR (return address) often reveals the true caller when backtraces are incomplete |
| Document the build ID / commit hash | Ensures the fix can be precisely backported or excluded from cherry-picks |
| Attach the raw dump or last_kmsg | Enables independent re-analysis if the initial conclusion turns out to be wrong |
| State what was ruled out | Documents investigation breadth and prevents duplicate analysis |
| Include a minimal reproducer if found | Critical for validation of fix and regression testing |


## Quick Reference: Analysis Decision Tree
Use this decision tree to quickly identify the correct analysis path for any crash scenario:
Crash Occurred
    │
    ├── Device in download mode?
    │   ├── YES → Collect ramdump → T32 / CrashScope / crash utility
    │   └── NO  → Check pstore / last_kmsg
    │
    ├── Kernel or User crash?
    │   ├── KERNEL → crash utility + vmlinux
    │   │   ├── Memory corruption? → Enable KASAN, check slab (kmem -s)
    │   │   ├── Hang / lockup?     → bt -a, check spinlocks, runq
    │   │   ├── Page fault?        → Walk page tables (MMU.DUMP), check mappings
    │   │   └── Watchdog?          → Check IRQ-disabled paths, DCC output
    │   │
    │   └── USER → GDB + core dump
    │       ├── SIGSEGV → Check pointer validity, mappings (info proc maps)
    │       ├── SIGABRT → Check assert/abort reason in logcat/dmesg
    │       └── SIGBUS  → Alignment issue or memory-mapped file problem
    │
    └── Can reproduce?
        ├── YES → Add debug (ftrace, tracepoints, printk, KASAN, lockdep)
        └── NO  → Rely on dump analysis + code review + static analysis

──────────────────────────────────────────────────────────────
Fault Address Interpretation:
    0x000000XX         → NULL dereference (struct offset = XX)
    0xDEAD0000XXXXXX   → LIST_POISON (freed list_head accessed)
    0x6B6B6B6B6B6B6B6B → SLUB_FREED poison (use-after-free)
    0xA5A5A5A5A5A5A5A5 → Allocated but uninitialized (POISON_INUSE)
    ffff800XXXXXXXXX   → Valid kernel address (linear map)
    ffffc0XXXXXXXXXX   → Valid kernel address (vmalloc region)
    bf0XXXXX           → Kernel module address (ARM32)

──────────────────────────────────────────────────────────────
ESR Quick Decode (ARM64):
    EC=0x25 → Data Abort from current EL (kernel memory fault)
    EC=0x21 → Data Abort from lower EL (user memory fault)
    EC=0x20 → Instruction Abort from lower EL
    WnR=0   → READ fault  │  WnR=1 → WRITE fault
    FSC=0x05 → Level 1 translation fault (unmapped page)
    FSC=0x0F → Level 3 permission fault (access violation)
    FSC=0x21 → Alignment fault

──────────────────────────────────────────────────────────────
addr2line Usage:
    addr2line -e vmlinux -f 0xFFFFFF8012345678
    addr2line -e my_module.ko -f 0x<offset_within_module>
    objdump -d vmlinux | grep -A5 "<function_name>"

──────────────────────────────────────────────────────────────
Common crash> commands quick reference:
    crash> log         # dmesg at crash time
    crash> bt          # backtrace of crashed task
    crash> bt -a       # ALL CPUs backtraces
    crash> ps          # process list
    crash> kmem -s     # slab state
    crash> runq        # scheduler run queues
    crash> mod         # loaded modules
    crash> dis <func>  # disassemble

## Summary: Crash Data Types at a Glance
| Data Type | Level | Best For | Key Tool |
| Full Ramdump | Kernel | Complex crashes, memory corruption analysis | T32, CrashScope, crash |
| Minidump | Kernel | Production, quick triage, field crash data | Ramparse, CrashScope |
| kdump / vmcore | Kernel | Server/embedded with kexec support | crash utility, GDB |
| pstore / ramoops | Kernel | Quick dmesg check, no tools needed | cat /sys/fs/pstore/ |
| Core Dump | User | Full user-space process state analysis | GDB, readelf |
| Tombstone | User (Android) | Native crash quick triage on Android | ndk-stack, addr2line |
| KASAN Report | Kernel (debug) | Memory corruption root cause with exact stacks | dmesg analysis |
| DCC (Qualcomm) | Hardware | NOC errors, clock/power state at crash | T32 dcc_parser.cmm |


  ℹ  Continue to Part 2: Qualcomm Ramdump/Minidump Deep Dive — covering Sahara protocol, IMEM/SMEM architecture, minidump registration API, DCC integration, and advanced Ramparse/T32 parsing workflows.

QUALCOMM
Linux Kernel Staff Engineer
Interview Preparation Guide
PART 2 OF 3
Topics 5–9: Boot Optimization • ARM/ARM64 Architecture • Performance Profiling & Debugging Tools • Upstream Linux Development • Virtualization
|  |


| Candidate | Sandeep Kumar |
| Position | Engineer, Staff — Linux Kernel |
| Company | Qualcomm India Private Limited |
| Interview Date | April 22, 2026 |
| Coverage | Topics 5–9 of 14 (Boot Optimization, ARM/ARM64, Profiling, Upstream, Virtualization) |
| Document | Part 2 of 3 — Refer to Part 1 for Topics 1–4, Part 3 for Topics 10–14 |


Prepared for Interview Excellence — Deep Technical Coverage

TABLE OF CONTENTS

# Topic 5: Boot Optimization
Boot time reduction is a critical requirement for embedded and mobile Linux systems. Qualcomm SoCs must boot quickly for consumer experience and system responsiveness. This topic covers the full ARM64 boot flow, measurement tools, and concrete optimization techniques at every stage.

## 5.1 Linux Boot Flow on ARM64
Understanding the ARM64 boot sequence is essential for identifying optimization bottlenecks. The flow proceeds: firmware → bootloader → kernel decompression → early kernel init → driver init → userspace init.
### Firmware Stage (EL3/Secure World)
- ARM Trusted Firmware-A (ATF) runs at EL3; initializes TrustZone, sets up PSCI, branches to BL31/BL32
- UEFI firmware (EDK2) or U-Boot SPL handles early hardware initialization (DDR training, clocks, PLLs)
- Handoff to bootloader at EL2 (if hypervisor) or EL1 (if no virtualization)
- Key: DDR training is often the biggest firmware latency; Qualcomm uses XBL (eXtensible Bootloader)
### Bootloader Stage (U-Boot / UEFI / ABL)
- Android Boot Loader (ABL) on Qualcomm: loads kernel image, DTBs, ramdisk from storage
- Passes boot parameters via UEFI protocols or ATAGs/FDT (Flattened Device Tree)
- Kernel entry: x0 = FDT blob address, x1–x3 = 0 (ARM64 boot protocol in Documentation/arm64/booting.rst)
- Optimization: preload ramdisk into memory during firmware, reduce storage probe time
### Kernel Decompression
- ARM64 kernels are typically Image.gz or Image; decompression happens in-place
- Self-decompressing stub calls decompress_kernel(), then branches to start_code
- Optimization: use LZ4 instead of gzip for 2-3x faster decompression at slight size cost
- CONFIG_KERNEL_LZ4 recommended for boot-time critical systems
### Early Kernel Initialization (head.S → start_kernel())
- head.S (arch/arm64/kernel/head.S): MMU setup, early page tables (idmap + swapper), exception vectors
- start_kernel(): lockdep_init(), early_boot_irqs_disabled_end(), setup_arch()
- setup_arch(): parses device tree, initializes memory, sets up CPU features
- mm_init() → mem_init() → kmem_cache_init() → page allocator ready
- sched_init() → IRQ init → timers → console_init() → rest_init()
### Initcall Sequence
The initcall mechanism runs driver init functions in order: early_initcall → core_initcall → postcore_initcall → arch_initcall → subsys_initcall → fs_initcall → device_initcall → late_initcall. This is the primary battleground for boot optimization.

| Interview Tip — Boot Flow QuestionInterviewers often ask: "Walk me through what happens from power-on to the login prompt on an ARM64 system." Cover all stages: ATF/firmware, bootloader, kernel head.S, start_kernel(), initcalls, and init process. Mention deferred probing and async probing as optimization levers. |


## 5.2 Boot-Time Measurement and Profiling
You cannot optimize what you cannot measure. Linux provides several tools to profile boot time at different granularities.
### initcall_debug
- Kernel parameter: initcall_debug on the kernel command line
- Prints timing of each initcall function: "initcall xxx_init returned 0 after N usecs"
- Use dmesg | grep "initcall" | sort -t@ -k2 -n to find slowest initcalls
- Combined with printk.time=1 for absolute timestamps
### Ftrace Boot-Time Tracing
- trace_event=initcall:* on command line enables initcall tracing via ftrace
- ftrace_boot_snapshot=1 captures trace at the end of boot for later analysis
- Can trace any kernel function during boot with boot_tracer=function
### Bootchart
- Userspace tool (bootchart2/systemd-bootchart) records process tree and CPU/IO usage during boot
- Visualizes boot as a Gantt chart — shows parallelism and serialization bottlenecks
- Install: apt install bootchart2; add initcall to kernel cmdline
### systemd-analyze
- systemd-analyze: shows total boot time split into firmware + loader + kernel + userspace
- systemd-analyze blame: lists services sorted by startup time
- systemd-analyze critical-chain: shows the critical path of service dependencies
- systemd-analyze plot > boot.svg: generates full boot timeline SVG
### Kernel Timing with printk
- Add printk(KERN_INFO "checkpoint: %llu us
", ktime_get_boot_ns()/1000) to custom code
- CONFIG_PRINTK_TIME=y adds timestamps to all printk output
- Use early_printk for output before console init

## 5.3 Boot Optimization Techniques
Multiple layers of optimization exist, from firmware to userspace. A systematic approach covers each layer independently.
### Deferred Probing
- driver_probe_device() returns -EPROBE_DEFER if a dependency is not yet available
- Kernel re-queues the device for later probing; this avoids serialization
- Use devm_clk_get() / devm_regulator_get() — they automatically defer if not ready
- Debug: echo deferred_probe_timeout > /sys/kernel/debug/devices or check /sys/kernel/debug/probe_result
- Common issue: deferred probe loop — device A waits for B, B waits for A — check dependencies
### Asynchronous Probing
- DRIVER_ATTR(async_probe): mark a driver as async-probeable
- driver_register() with probe_type = PROBE_PREFER_ASYNCHRONOUS
- Kernel spawns a kworker thread for async device probing in parallel with serial devices
- CONFIG_ASYNCHRONOUS_PROBING=y: global async probe for all drivers (risky for ordering-sensitive drivers)
- device_enable_async_probe(dev) in driver init for per-driver async probing
### Initcall Optimization
- Move non-critical drivers to late_initcall or module_init
- Convert built-in drivers to loadable modules (CONFIG_xxx=m vs =y) and load post-boot
- Use initcall_debug to identify and eliminate unnecessary early initcalls
- Reorder initcalls using __define_initcall() with appropriate initcall level
### Kernel Command-Line and Config Optimizations
- quiet: suppress non-critical boot messages (saves serial output time)
- loglevel=0: minimal log output during production boot
- Disable CONFIG_KALLSYMS, CONFIG_DEBUG_INFO in production for smaller kernel
- Enable CONFIG_EMBEDDED, CONFIG_EXPERT to trim unnecessary kernel features
- Use CONFIG_HZ_100 (100Hz tick) instead of 250Hz or 1000Hz for less timer overhead
### Compressed Kernel and initramfs
- LZ4 compression: faster decompression than gzip/lzma at minor size penalty
- Minimize initramfs: include only critical early-boot binaries (udev minimal set)
- Use CONFIG_INITRAMFS_SOURCE to build initramfs into kernel image — eliminates initrd loading step
- Consider no-initramfs boot (rootfs directly on eMMC) for deeply embedded systems
### Device Tree Optimization
- Trim unused nodes from production DTB (eliminates unused driver probing)
- status = "disabled" for all unused peripherals in DT
- Use dtb overlays carefully — late overlay application avoids early boot overhead
- Reduce DT size by removing debug/trace nodes in production image

## 5.4 Boot Optimization Summary Table
| Stage | Technique | Expected Improvement |
| Firmware | DDR training cache / skip on warm boot | 100–500ms reduction |
| Bootloader | Parallel storage access, preload ramdisk | 50–200ms reduction |
| Decompress | Switch gzip → LZ4 compression | 50–150ms reduction |
| Kernel Init | Async probing, deferred probing | 200–800ms reduction |
| Initcalls | Move to late_initcall / module | 100–400ms reduction |
| Userspace | Systemd service parallelism, socket activation | 500ms–2s reduction |



# Topic 6: ARM / ARM64 Architecture
Deep knowledge of ARM/ARM64 architecture is a core requirement for Qualcomm Linux kernel work. This covers exception levels, register sets, cache coherency, big.LITTLE/DynamIQ, interrupt controllers, SMMU, and TrustZone — all directly relevant to Qualcomm SoC kernel development.

## 6.1 ARMv8-A Architecture Overview
ARMv8-A is the 64-bit ARM architecture baseline. It introduces AArch64 (64-bit execution state) alongside AArch32 (32-bit compatibility). Qualcomm Snapdragon SoCs use ARMv8-A and ARMv9-A (Cortex-X/A/Kryo series).
### Execution States
- AArch64: 64-bit execution with 31 general-purpose registers (X0–X30), 64-bit addresses
- AArch32: 32-bit backward-compatible state (Thumb, ARM ISA) — limited use in modern kernels
- State switch via exception returns (ERET) — cannot switch mid-execution
- ARMv9-A adds: Realm Management Extension (RME), SVE2, SME (Scalable Matrix Extension)
## 6.2 Exception Levels (EL0–EL3)
ARM64 defines four privilege levels called Exception Levels. Higher EL = higher privilege. The Linux kernel runs at EL1; user applications at EL0.
| Level | Name | What Runs Here | Notes |
| EL0 | Application | User processes (apps, libc) | Lowest privilege |
| EL1 | Kernel / OS | Linux kernel, kernel modules | Most kernel work here |
| EL2 | Hypervisor | KVM host, Xen, pKVM (Android) | Optional; VHE merges EL1+EL2 |
| EL3 | Secure Monitor | ARM Trusted Firmware (ATF), PSCI | Highest privilege; TrustZone entry |


### Exception Handling Flow
- Exceptions (interrupts, synchronous faults, SVC) cause CPU to jump to the exception vector table (VBAR_EL1/EL2/EL3)
- ARM64 vector table has 16 entries (4 exception types x 4 EL/SP combinations)
- Linux vector table: arch/arm64/kernel/entry.S — kernel_ventry macro
- ESR_EL1 (Exception Syndrome Register): encodes exception class (EC) and syndrome
- FAR_EL1 (Fault Address Register): virtual address that caused the fault
- ELR_EL1 (Exception Link Register): return address after exception handling
## 6.3 ARM64 Registers and Calling Convention
### Register Set
- X0–X7: function arguments and return values (AAPCS64)
- X8: indirect result location register (struct return pointer)
- X9–X15: caller-saved temporary registers
- X16–X17: intra-procedure-call scratch registers (IP0, IP1)
- X18: platform register (reserved in some ABIs)
- X19–X28: callee-saved registers
- X29 (FP): frame pointer; X30 (LR): link register; SP: stack pointer; PC: program counter
### System Registers (Key Ones)
- SCTLR_EL1: system control (MMU enable, cache enable, endianness)
- TCR_EL1: translation control (page granule, address size, ASID bits)
- TTBR0_EL1/TTBR1_EL1: translation table base registers (user/kernel page tables)
- MAIR_EL1: memory attribute indirection register (Normal/Device/etc.)
- SPSR_EL1: saved program status register (NZCV flags, exception mask bits)
- DAIF: Debug/SError/IRQ/FIQ mask bits; MSR DAIFSet/DAIFClr to enable/disable interrupts
- CNTVCT_EL0: virtual count register (high-resolution timer); CNTFRQ_EL0: timer frequency
## 6.4 Cache Architecture and Coherency
ARM64 implements a hierarchical cache architecture. Understanding coherency is essential for multicore kernel development and DMA buffer management on Qualcomm SoCs.
### Cache Hierarchy
- L1 I-cache and D-cache: per-core, typically 32KB–64KB, PIPT (Physically Indexed Physically Tagged)
- L2 cache: per-core or per-cluster, 256KB–1MB, unified
- L3 / LLC (Last Level Cache): shared across cluster or SoC, 4MB–32MB on modern Snapdragons
- System cache (DSU — DynamIQ Shared Unit): ARM v8.2+ shared cache for big.LITTLE clusters
### Cache Coherency Protocols
- MESI protocol: Modified, Exclusive, Shared, Invalid — maintains coherency between cores
- MOESI (ARM extension): Owned state allows sharing dirty data without writeback
- ARM CoreLink CMN (Cache and Memory Network): interconnect implementing ACE/CHI protocol
- ACE (AXI Coherency Extensions): full coherency; ACE-Lite: one-way coherency (for GPU/DSP)
- CCI (Cache Coherent Interconnect) on older Snapdragon; NoC on newer
### Cache Operations in Linux
- clean_dcache_area(addr, size): writeback D-cache range to PoU (Point of Unification)
- __flush_dcache_area(): flush to PoC (Point of Coherency) for DMA
- dma_sync_single_for_device(): arch-specific DMA cache management
- I-cache coherency after code patching: flush_icache_range() ensures instruction fetch sees new code
- Non-cacheable mappings for device memory: use DEVICE_nGnRE attribute in MAIR_EL1

| Key Interview Point — DMA and Cache CoherencyOn Qualcomm SoCs, peripheral devices (camera ISP, modem, DSP) are often not cache-coherent. DMA buffers must be explicitly cleaned (writeback) before DMA_TO_DEVICE and invalidated after DMA_FROM_DEVICE. dma_alloc_coherent() provides uncached memory; dma_map_single() handles flushing for streaming DMA. This is a common source of subtle memory corruption bugs. |


## 6.5 ARM big.LITTLE and DynamIQ (Heterogeneous Multiprocessing)
Qualcomm Snapdragon SoCs use heterogeneous CPU clusters (Cortex-X "Prime" + Cortex-A "Performance" + Cortex-A "Efficiency"), analogous to ARM big.LITTLE. Understanding the topology is essential for EAS, cpufreq, and scheduler work.
### big.LITTLE Architecture
- big cluster: high-performance out-of-order CPUs (high IPC, high power, high frequency)
- LITTLE cluster: efficient in-order CPUs (low IPC, low power, lower frequency)
- Kernel models topology via struct sched_domain and struct cpu_topology
- CPU capacity: relative performance capacity stored per-CPU; used by EAS for task placement
- arch_scale_cpu_capacity() returns normalized capacity from energy model
### DynamIQ
- DynamIQ Shared Unit (DSU): single cluster can mix big+LITTLE cores with shared L3 cache
- Each core can run at independent voltage and frequency (unlike older ARM clusters)
- Supports up to 8 cores per cluster with per-core or per-cluster power domains
- AMU (Activity Monitor Unit): per-core hardware counters for capacity estimation
- AMU integration with EAS: arch_scale_freq_capacity() uses AMU counters for accurate freq scaling
### Qualcomm-Specific Topology (Snapdragon 8 Gen series)
- 1x Cortex-X (Prime, e.g., X3/X4): highest performance, ~3.0+ GHz
- 3-4x Cortex-A "Performance" cores (e.g., A715/A720): balanced perf/power
- 4x Cortex-A "Efficiency" cores (e.g., A510/A520): lowest power idle handling
- Adreno GPU, Hexagon DSP (NPU), Snapdragon X modem as separate compute blocks
## 6.6 GIC — Generic Interrupt Controller
The ARM Generic Interrupt Controller (GIC) is the standard interrupt controller for ARM SoCs. Linux supports GICv2, GICv3, and GICv4 via the irqchip subsystem.
### GICv3 Architecture
- Distributor (GICD): global interrupt configuration and routing; one per system
- Redistributor (GICR): per-CPU interface for SGI/PPI and power management
- CPU Interface (ICC_*_EL1 system registers in GICv3): replaces memory-mapped GICC in GICv2
- Interrupt types: SGI (Software Generated, 0–15), PPI (Private Peripheral, 16–31), SPI (Shared Peripheral, 32–1019), LPI (Message-based, GICv3+)
### IRQ Handling in Linux
- arch/arm64/kernel/entry.S: el1_irq / el0_irq vectors call handle_arch_irq
- GIC driver reads IAR (Interrupt Acknowledge Register): ICC_IAR1_EL1 in GICv3
- handle_domain_irq() dispatches to registered irq_desc handler
- IPI (Inter-Processor Interrupt) via SGI: used for scheduler kick, TLB flush, cache invalidation
- IRQ affinity: /proc/irq/N/smp_affinity_list; irq_set_affinity() kernel API
### GICv4 — Direct Injection of Virtual Interrupts
- GICv4: allows direct injection of virtual interrupts into guest VMs without hypervisor intervention
- LPI routing via ITS (Interrupt Translation Service): PCIe MSI/MSI-X support
- Relevant for KVM on ARM: reduces VM-exit overhead for high-frequency interrupts
## 6.7 SMMU — System Memory Management Unit (IOMMU)
The ARM SMMU provides address translation and access control for non-CPU bus masters (GPU, camera, DMA engines). Essential for security isolation and DMA remapping on Qualcomm SoCs.
- SMMU performs stage-1 (virtual → intermediate physical) and stage-2 (IPA → physical) translation
- Linux driver: drivers/iommu/arm/arm-smmu-v3/ (SMMUv3, current), arm-smmu/ (SMMUv2)
- IOMMU domains: each device gets an iommu_domain with its own page tables
- DMA API integration: iommu_map(), iommu_unmap() called by dma_map_*() when IOMMU is present
- Qualcomm SMMU (qcom_iommu): used in older SoCs; ARM SMMUv3 in newer Snapdragon
- Security: prevents rogue DMA (DMA attacks); enforced by ARM TrustZone in Qualcomm TEE
- SVA (Shared Virtual Addressing): allows userspace virtual addresses for accelerator DMA (PASID)
## 6.8 TrustZone and Secure World
ARM TrustZone creates a hardware-enforced isolation between "Normal World" (Linux/Android) and "Secure World" (TEE — Trusted Execution Environment). On Qualcomm, QTEE (Qualcomm TEE) implements secure services.
- EL3 (Secure Monitor): mediates world switches; SMC (Secure Monitor Call) instruction triggers EL3 entry
- Secure World: isolated memory, peripherals, crypto accelerators; runs QTEE (based on QSEE)
- Normal World Linux interacts via SMC calls (PSCI, SCM driver in drivers/firmware/qcom_scm.c)
- PSCI (Power State Coordination Interface): EL3 firmware interface for CPU on/off, suspend, reset
- Secure DMA: TZ restricts certain memory regions from Normal World DMA via SMMU configuration
- OP-TEE: open-source TEE implementation; Qualcomm uses proprietary QTEE
- Kernel interface: arm_smccc_smc() / arm_smccc_hvc() for SMC/HVC calls from kernel

| ARM Architecture Interview TipFor Qualcomm interviews, be prepared to explain: (1) What happens when a Linux driver calls pm_runtime_suspend()? (Trace through EL1 → PSCI SMC → EL3 ATF → CPU power down). (2) How does DMA isolation work on an SMMU-equipped SoC? (3) How does a GICv3 interrupt flow from hardware pin to Linux interrupt handler? |



# Topic 7: Performance Profiling & Debugging Tools
Qualcomm Linux engineers are expected to use the full Linux performance toolkit to characterize, debug, and optimize system behavior. This section covers perf, ftrace, BPF/eBPF, kgdb, crash, and standard system observability tools.

## 7.1 perf — Linux Performance Counters Tool
perf is the primary Linux performance analysis tool, using hardware performance counters (PMUs), software counters, and kernel tracepoints. It is indispensable for CPU performance analysis on Qualcomm ARM64 SoCs.
### perf stat — Event Counting
- perf stat -e cycles,instructions,cache-misses,branch-misses ./workload
- Reports IPC (Instructions Per Cycle): IPC < 1 suggests memory-bound, > 2 compute-bound
- ARM PMU events: arm_pmu driver; events like l1d_cache_refill, ll_cache_miss, bus_cycles
- perf stat -a: system-wide counting across all CPUs
- perf stat --per-core: per-core breakdown for big.LITTLE analysis
### perf record / report — Sampling Profiler
- perf record -g -F 999 -e cycles:u ./workload: sample at 999 Hz, capture call graphs
- perf report: interactive TUI showing hottest functions and call chains
- perf report --stdio --sort=dso,sym: scripted output sorted by DSO and symbol
- Kernel sampling: perf record -g -e cycles:k (requires CAP_SYS_ADMIN or /proc/sys/kernel/perf_event_paranoid <= 1)
- Frame pointer vs DWARF unwinding: -g dwarf for apps without frame pointers
- LBR (Last Branch Record): x86 only; ARM equivalent is SPE (Statistical Profiling Extension)
### perf top — Live Profiling
- perf top: live view of hottest kernel/user functions (like htop but per-function)
- perf top -e cache-misses: live cache miss hotspot view
### ARM Statistical Profiling Extension (SPE)
- ARM SPE (ARMv8.2+): hardware instruction-level sampling with memory access attribution
- Records: virtual address, physical address, latency, operation type (load/store/branch)
- perf record -e arm_spe_0/... for SPE events
- Enables memory-level profiling: identifies cacheline hotspots, TLB misses, DRAM bandwidth
- Available on Qualcomm Cortex-X/A-series cores in recent Snapdragon SoCs
### perf trace — System Call Tracing
- perf trace: live strace-like output with low overhead using tracepoints
- perf trace -a -e syscalls:sys_enter_futex: monitor futex calls system-wide
- Much lower overhead than strace (no ptrace); suitable for production debugging
### Key perf Commands Reference
| Command | Purpose |
| perf stat -e cycles,instructions | Hardware counter measurement; compute IPC |
| perf record -g -F 999 | CPU cycle sampling with call graphs at 999Hz |
| perf report --sort=dso,sym | Analyze hotspots in perf.data |
| perf annotate <func> | Instruction-level annotation with cycle counts |
| perf sched record/latency | Scheduler latency and wakeup analysis |
| perf mem record/report | Memory access profiling (load/store latency) |
| perf lock record/report | Lock contention analysis |
| perf c2c record/report | Cache-to-cache transfer / false sharing detection |


## 7.2 ftrace — Kernel Function Tracer
ftrace (Function Tracer) is the in-kernel tracing framework. It provides zero-overhead function tracing, event tracing, and latency measurement. All trace data is accessed via /sys/kernel/debug/tracing/.
### ftrace Tracers
- function: traces every kernel function call (uses mcount/fentry instrumentation)
- function_graph: traces function entry AND exit with indented call graph and latency
- irqsoff: traces maximum IRQ-disabled latency (critical for RT analysis)
- preemptoff: traces maximum preemption-disabled duration
- wakeup / wakeup_rt: traces scheduler wakeup-to-run latency
- blk: block I/O tracer
### Using ftrace
# Mount debugfs
mount -t debugfs nodev /sys/kernel/debug
# Enable function_graph tracer
echo function_graph > /sys/kernel/debug/tracing/current_tracer
echo 1 > /sys/kernel/debug/tracing/tracing_on
# Filter to specific function
echo "cpufreq_*" > /sys/kernel/debug/tracing/set_ftrace_filter
cat /sys/kernel/debug/tracing/trace
### Trace Events
- Trace events are static instrumentation points in the kernel; defined via TRACE_EVENT() macro
- Enable: echo 1 > /sys/kernel/debug/tracing/events/sched/sched_switch/enable
- Key event categories: sched/*, irq/*, kmem/*, power/*, net/*, block/*, cpufreq/*
- Filter: echo "prev_comm == bash" > /sys/kernel/debug/tracing/events/sched/sched_switch/filter
### trace-cmd — ftrace Frontend
- trace-cmd record -e sched:* -e irq:* ./workload: records trace events to kernel.dat
- trace-cmd report: human-readable output from kernel.dat
- KernelShark: GUI frontend for trace-cmd output (visual timeline)
- trace-cmd stream: live streaming trace output
### Dynamic Function Filtering
- set_ftrace_filter: trace only specified functions (glob patterns supported)
- set_graph_function: for function_graph, trace call graph rooted at specified function
- set_ftrace_notrace: exclude functions from tracing
- Per-CPU tracing: trace_options, tracing_cpumask for selective CPU tracing
## 7.3 BPF / eBPF — Programmable Kernel Observability
eBPF (extended Berkeley Packet Filter) allows safe, sandboxed programs to run in the Linux kernel for tracing, networking, and security. It is the most powerful modern Linux observability tool and is increasingly important for performance work at Qualcomm.
### eBPF Architecture
- eBPF programs are JIT-compiled to native ARM64 bytecode and verified by the BPF verifier
- Program types: kprobe, tracepoint, perf_event, XDP, TC, LSM, sched_ext
- BPF maps: shared data structures between BPF programs and userspace (hash, array, ringbuf, LRU)
- BPF helpers: safe kernel functions callable from BPF programs (bpf_ktime_get_ns, bpf_get_current_pid_tgid, etc.)
- BTF (BPF Type Format): CO-RE (Compile Once – Run Everywhere) — portable BPF programs
### bpftrace — High-Level BPF Scripting
- One-liner syntax: bpftrace -e 'kprobe:do_sys_open { printf("%s
", str(arg1)); }'
- Tracepoint: bpftrace -e 'tracepoint:sched:sched_switch { @[args->next_comm] = count(); }'
- Histogram: bpftrace -e 'kretprobe:vfs_read { @latency = hist(retval); }'
- Profile (sampling): bpftrace -e 'profile:hz:99 { @[kstack] = count(); }'
### BCC — BPF Compiler Collection
- biolatency: block I/O latency histogram
- funclatency: latency histogram for any kernel function
- offcputime: time processes spend off-CPU (blocked) with stack traces
- runqlat: scheduler run queue latency histogram
- tcplife: TCP connection lifetimes with throughput
- memleak: kernel/userspace memory leak detection
### libbpf — Production BPF Development
- C-based library for loading and managing BPF programs programmatically
- Used by sched_ext schedulers, Cilium (networking), Falco (security)
- skeleton generation: bpftool gen skeleton prog.o > prog.skel.h for type-safe CO-RE programs

## 7.4 kgdb / kdb — Kernel Debugger
kgdb (Kernel GNU Debugger) provides source-level kernel debugging via a serial or network connection to GDB. kdb is the built-in kernel debugger without external GDB requirement.
### kgdb Setup
- Enable: CONFIG_KGDB=y, CONFIG_KGDB_SERIAL_CONSOLE=y
- Kernel cmdline: kgdboc=ttyS0,115200 kgdbwait (waits for GDB connection at boot)
- Trigger breakpoint: sysrq+g (echo g > /proc/sysrq-trigger) to enter kgdb
- GDB connection: target remote /dev/ttyUSB0 or target remote :1234 (for QEMU)
- Requires vmlinux with debug symbols (CONFIG_DEBUG_INFO=y)
### kdb — Built-in Kernel Debugger
- No external GDB needed; interactive shell at kernel crash or via sysrq
- bt: backtrace; ps: process list; md addr: memory dump; mm addr val: memory write
- go: continue; ss: single step; bp addr: set breakpoint
- Useful for quick inspection without setting up GDB cross-debugging
## 7.5 crash — Post-Mortem Kernel Dump Analysis
The crash utility enables post-mortem analysis of kernel crash dumps (vmcore). Essential for debugging kernel panics, OOM events, and production incidents on Qualcomm devices.
- crash /usr/lib/debug/vmlinux-$(uname -r) /proc/kcore: live kernel inspection
- crash vmlinux vmcore: analyze a crash dump from a previous kernel panic
- bt: current task backtrace; bt -a: all tasks; bt -p PID: specific process
- log: kernel dmesg log from crash; dis function: disassemble kernel function
- struct task_struct ffff...: print task_struct contents
- kmem -s: slab cache statistics; vm: virtual memory info for current task
- Kdump configuration: CONFIG_KEXEC=y + kdump-tools to capture vmcore on panic
- On Qualcomm devices: ramdump via EDL mode or DLOAD subsystem for memory capture
## 7.6 System Observability — /proc, /sys, and Standard Tools
### Essential /proc Interfaces
- /proc/cpuinfo: CPU model, features, cache topology
- /proc/interrupts: interrupt counts per CPU; /proc/softirqs: softirq counts
- /proc/schedstat: per-CPU scheduler statistics (run time, wait time)
- /proc/meminfo: memory zones, reclaim counters, slab usage
- /proc/vmstat: page allocator, reclaim, compaction, THP statistics
- /proc/buddyinfo: buddy allocator free page distribution per zone
- /proc/slabinfo: per-cache object counts and sizes
- /proc/PID/smaps: per-mapping memory usage (RSS, PSS, anonymous, file-backed)
### Essential /sys Interfaces
- /sys/devices/system/cpu/cpuN/cpufreq/: frequency scaling controls and current freq
- /sys/kernel/debug/sched/: scheduler tunables and statistics
- /sys/class/thermal/: thermal zones, temperatures, cooling devices
- /sys/bus/platform/devices/: platform device tree hierarchy
### Standard Linux Performance Tools
| Tool | Category | Key Use Case |
| top / htop | CPU & Process | Live CPU usage, load average, process CPU% |
| mpstat | CPU | Per-CPU utilization breakdown (usr/sys/irq/idle) |
| vmstat | Memory / IO | Memory, swap, block IO, context switches per second |
| iostat | Storage IO | Block device utilization, throughput, latency |
| sar | Historical | Record and replay system performance over time |
| numastat | NUMA | Per-node memory allocation and hit/miss rates |
| turbostat | Power | Per-CPU frequency, C-state residency, power (x86 + ARM) |
| dmesg -w -H | Kernel Logs | Live kernel messages with timestamps; debug driver issues |
| strace / ltrace | Syscall Trace | Syscall tracing (strace) and library call tracing (ltrace) |


| Interview Tip — Perf Debugging ScenarioCommon question: "A userspace application suddenly shows high latency spikes. How do you debug it?" Answer: Start with perf stat to check IPC/cache metrics; use perf record -g to find CPU hotspots; use ftrace irqsoff to check interrupt latency; use bpftrace offcputime to find blocking; use perf sched latency to check scheduler delays. Mention /proc/PID/sched for per-task stats. |



# Topic 8: Upstream Linux Development
Contributing to the upstream Linux kernel is explicitly listed in the Qualcomm job description. This covers the Git workflow, patch submission process, coding standards, mailing list etiquette, and the release cycle. Qualcomm engineers are expected to upstream ARM/SoC platform code.

## 8.1 Linux Kernel Development Workflow
Kernel development follows a disciplined patch-based workflow using Git and email. There is no GitHub pull-request model — all contributions are submitted as email patches to mailing lists.
### Release Cycle
- Linus Torvalds releases a new kernel every ~9–10 weeks
- Merge Window (2 weeks): subsystem trees merged into Linus's tree; new features only
- RC (Release Candidate) phase: rc1 through rc7/rc8; bug fixes only, no new features
- Stable releases: Greg KH maintains stable/ and longterm/ branches with backported fixes
- Qualcomm maintains linux-next integration, ARM SoC tree (maintained by Arnd Bergmann / Olof Johansson)
### Subsystem Trees
- Each subsystem has a maintainer who runs a Git tree (e.g., linux-pm.git for power management)
- Patches flow: developer → subsystem maintainer tree → linux-next → Linus merge window
- ARM SoC tree: arch/arm64/boot/dts/*, arch/arm64/ changes go through arm-soc tree
- MAINTAINERS file: lists maintainers, mailing lists, and git trees for every subsystem
- get_maintainer.pl: ./scripts/get_maintainer.pl <patch> to find correct maintainers/lists
## 8.2 Git Workflow for Kernel Development
### Setting Up
git clone https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
git remote add arm-soc https://git.kernel.org/pub/scm/linux/kernel/git/soc/soc.git
git config sendemail.smtpserver smtp.gmail.com
git config sendemail.smtpserverport 587
### Patch Development
- Create a branch: git checkout -b fix/cpufreq-qualcomm-throttle origin/master
- Develop fix; keep commits small and focused — one logical change per patch
- Commit message format: <subsystem>: <short summary under 72 chars>
- Body: explain WHAT changed and WHY (not how); reference bug/Fixes: tag if applicable
- Signed-off-by: Your Name <email> required for all patches (Developer Certificate of Origin)
### Generating and Sending Patches
# Generate patch series
git format-patch -v2 --cover-letter -3 origin/master
# Outputs: 0000-cover-letter.patch, 0001-fix.patch, etc.
# Check patch style
./scripts/checkpatch.pl --strict 0001-*.patch
# Send patches via email
git send-email --to=linux-pm@vger.kernel.org \
  --cc=viresh.kumar@linaro.org \
  --cc=linux-kernel@vger.kernel.org \
  0001-*.patch
### Fixes and Backports
- Fixes: tag: Fixes: <12-char commit SHA> ("Short subject of original commit")
- Tells stable kernel maintainers this patch should be backported
- git cherry-pick -x <commit>: backport to stable branch with attribution
- stable@vger.kernel.org: email for requesting stable backports
## 8.3 Kernel Coding Style
Linux has a strict coding style documented in Documentation/process/coding-style.rst. Violations are a common reason for patch rejection on mailing lists.
### Key Style Rules
- Tabs (not spaces) for indentation; 8-space tab width (use git show to verify)
- 80-column limit (relaxed to 100 in practice but never exceed without justification)
- K&R brace style: opening brace on same line for functions, except function definitions
- No typedef for structs unless an opaque type is genuinely needed
- snake_case for everything (no camelCase); UPPER_CASE for macros and constants
- Error path cleanup: use goto for error unwinding in functions
- Avoid global variables; use static for file-scope symbols
### checkpatch.pl
- ./scripts/checkpatch.pl --strict --color=always 0001-*.patch
- Checks style, whitespace, comment format, line length, Signed-off-by presence
- Must have 0 errors; warnings should be addressed unless technically justified
- Also check C files: ./scripts/checkpatch.pl --file drivers/cpufreq/qcom-cpufreq-hw.c
## 8.4 Mailing List Etiquette
Linux kernel development happens primarily on mailing lists (vger.kernel.org, kernel.org lists). Proper mailing list behavior is critical for getting patches accepted.
- LKML (linux-kernel@vger.kernel.org): main kernel list; CC for broad patches
- Always CC relevant subsystem list (linux-pm, linux-arm-kernel, linux-arm-msm, etc.)
- Use plain text email (no HTML); configure mutt, git send-email, or Thunderbird with plain text
- Reply inline (interleaved), not top-post; trim quoted text to relevant sections
- Respond to reviewer feedback promptly; "LGTM" or "Reviewed-by:" acknowledgment
- Reviewed-by: Name <email>: reviewer endorsement (strong positive signal for maintainer)
- Tested-by: Name <email>: tester confirmation (especially valued for bug fixes)
- Acked-by: Name <email>: acknowledgment from affected subsystem maintainer
## 8.5 Code Review Process
- Re-send revised patches with version increment: git format-patch -v2 (produces [PATCH v2])
- Cover letter: describe the series, list changes from v1→v2, provide test evidence
- Respond to every reviewer comment; justify if you disagree with a suggestion
- patchwork.kernel.org: tracks patch state (New, Under Review, Accepted, Rejected)
- b4: modern tool for managing kernel patch series from patchwork/mailing lists
- lore.kernel.org: searchable archive of all kernel mailing list traffic

| Upstream Development Interview TipInterviewers will ask about your upstream contributions. Prepare to discuss: which mailing lists you target (e.g., linux-arm-msm@vger.kernel.org for Qualcomm SoC patches), how you handle reviewer feedback on a patch series, and how you determine the correct maintainer tree for a patch. Mention specific real contributions if you have them. |


## 8.6 ARM SoC Development Specifics
- ARM SoC tree: maintained by Arnd Bergmann — all arch/arm64/ SoC-specific changes
- Device Tree bindings must go to dt-bindings list; reviewed by Rob Herring / Conor Dooley
- YAML schema validation: ./scripts/dtschema/check-dtschema.sh DT-source.yaml
- Qualcomm SoC subsystem: linux-arm-msm@vger.kernel.org; drivers/soc/qcom/, drivers/clk/qcom/, etc.
- Power management for Qualcomm: drivers/cpufreq/qcom-cpufreq-nvmem.c, qcom-cpufreq-hw.c
- Key Qualcomm upstream maintainers: Bjorn Andersson, Dmitry Baryshkov, Konrad Dybcio

# Topic 9: Virtualization
The Qualcomm job description explicitly mentions KVM experience. This topic covers KVM on ARM64, ARM Virtualization Host Extensions (VHE), QEMU integration, virtio paravirtualization, and hypervisor concepts — with focus on the Linux kernel’s KVM implementation.

## 9.1 Hypervisor Concepts
### Type 1 vs. Type 2 Hypervisors
| Type | Description | Examples |
| Type 1 (Bare-Metal) | Runs directly on hardware; no host OS below it. Maximum performance and isolation. | Xen, VMware ESXi, KVM (debatable) |
| Type 2 (Hosted) | Runs on top of a host OS; uses OS services. Simpler deployment, some overhead. | VirtualBox, VMware Workstation |
| KVM Classification | KVM turns Linux into a Type 1 hypervisor — Linux becomes the hypervisor; VMs are Linux processes. | KVM + QEMU (Linux-based) |


## 9.2 KVM — Kernel-Based Virtual Machine
KVM (CONFIG_KVM) is a Linux kernel module that exposes virtualization capabilities to userspace. It uses CPU hardware virtualization extensions (VT-x on Intel, VHE on ARM64) to run guest VMs with near-native performance.
### KVM Architecture on ARM64
- kvm.ko + kvm-arm64.ko: core KVM module + ARM64 architecture backend
- /dev/kvm: character device; userspace (QEMU) uses ioctl() calls to manage VMs
- Key ioctls: KVM_CREATE_VM, KVM_CREATE_VCPU, KVM_SET_USER_MEMORY_REGION, KVM_RUN
- VCPU: virtual CPU represented as struct kvm_vcpu; each VCPU runs in a host kernel thread
- VM-entry: kvm_arch_vcpu_ioctl_run() arms stage-2 tables and does eret to EL1 (guest kernel)
- VM-exit: guest trap causes exception to EL2 hypervisor; KVM handles and returns to host
### ARM VHE — Virtualization Host Extensions
- VHE (ARMv8.1): allows host kernel to run at EL2 instead of EL1
- Without VHE: Linux host at EL1, KVM hypervisor context switches to EL2 for every VM-entry
- With VHE: Linux host directly at EL2; eliminates EL1↔EL2 context switch overhead
- Huge performance improvement: eliminates ~1000s of cycles per VM-exit for world switch
- CONFIG_ARM64_VHE detected at boot; enabled if CPU supports HCR_EL2.E2H=1
- nVHE (non-VHE): used by pKVM (protected KVM) for Android confidential VMs
### Stage-2 Address Translation
- Stage-2 MMU: IPA (Intermediate Physical Address) → PA (Physical Address) translation
- VTTBR_EL2: stage-2 translation table base for the current VM
- Guest page faults on unmapped IPA cause VM-exit; KVM maps the page and resumes guest
- Second-stage fault handling: kvm_handle_guest_abort() in arch/arm64/kvm/mmu.c
- Memory overcommit: KVM can overcommit physical memory using demand paging at stage-2
## 9.3 QEMU + KVM Integration
QEMU is the most common userspace component paired with KVM. QEMU provides device emulation, firmware loading, and VM management; KVM provides CPU and memory virtualization acceleration.
### QEMU ARM64 VM Launch
qemu-system-aarch64 \
  -machine virt,gic-version=3 \
  -cpu cortex-a72 \
  -enable-kvm \
  -m 4G \
  -nographic \
  -kernel Image \
  -dtb virt.dtb \
  -initrd rootfs.cpio.gz \
  -append "console=ttyAMA0 root=/dev/vda"
- -enable-kvm: use KVM acceleration (5–10x faster than pure emulation)
- -machine virt: QEMU virtual ARM platform with virtio devices
- -cpu host: passthrough host CPU model for native performance
### KVM Debugging and Tracing
- KVM statistics: /sys/kernel/debug/kvm/ — per-VM and per-VCPU counters
- VM-exit reasons: trace_kvm_exit tracepoint in KVM trace events
- perf kvm stat record: records KVM-specific performance data including VM-exit breakdown
- perf kvm stat report: shows which VM-exit reasons consume the most time
- Common exit reasons: MMIO, hypercall, MSR access, page fault, IRQ/FIQ injection
## 9.4 Virtio — Paravirtualization Standard
Virtio is the standard paravirtualization interface for I/O between guest VMs and the host. It provides near-native I/O performance by avoiding full device emulation. Qualcomm Android VMs (pKVM) use virtio extensively.
### Virtio Architecture
- Frontend driver: runs in guest kernel (drivers/virtio/ in Linux)
- Backend: runs in host (QEMU vhost, in-kernel vhost, or userspace)
- Virtqueue: shared ring buffer mechanism for I/O requests between guest and host
- Transport: PCI (virtio-pci), MMIO (virtio-mmio), channel I/O
### Common Virtio Devices
- virtio-blk: virtual block device (disk); virtio-net: virtual network interface
- virtio-rng: entropy source; virtio-console: guest–host console channel
- virtio-mem: dynamic memory hotplug for VMs; virtio-pmem: persistent memory
- virtio-gpu: virtual GPU; virtio-input: input device passthrough
### vhost — In-Kernel Virtio Backend
- vhost-net: in-kernel virtio-net backend (avoids QEMU userspace for each packet)
- vhost-blk: in-kernel block backend; vhost-vsock: VM-to-host socket communication
- vhost-user: move backend to separate userspace process (e.g., DPDK for networking)
- Huge performance improvement: vhost reduces packet path from 4 context switches to 1
## 9.5 pKVM — Protected KVM (Android Hypervisor)
pKVM is Android's implementation of a confidential computing hypervisor based on nVHE KVM. It creates isolated "protected VMs" whose memory is inaccessible even to the Android host kernel. Directly relevant to Qualcomm/Android work.
- Uses nVHE (non-VHE) KVM: hypervisor stub at EL2, host Linux at EL1
- Protected VM: guest memory encrypted/isolated; host cannot access guest physical memory
- Hypervisor mediates all stage-2 mappings; host Linux cannot map protected VM pages
- Use case: secure Android enclaves (DRM, payment, biometric processing) isolated from host OS
- CONFIG_KVM_ARM_HYP_DEBUG_UART for pKVM debugging; CONFIG_PROTECTED_GUEST
- EL2 code must be minimal and verifiable — strict size budget for security TCB

| Virtualization Interview TipFor Qualcomm, the key KVM topics are: (1) How does VHE reduce VM-exit overhead? (2) What is stage-2 page fault handling flow? (3) What is pKVM and how does it differ from regular KVM? (4) How does virtio-blk achieve near-native I/O performance? Also know how to use QEMU + KVM to build and test upstream kernel patches for ARM64. |



# Part 2 Quick Reference Summary
This page provides a concise quick-reference for all five topics covered in Part 2. Use this for last-minute review before your April 22 interview.

| Topic | Key Concepts | Critical Interview Points |
| Topic 5: Boot Optimization | initcall_debug, ftrace boot, bootchartDeferred + async probingLZ4 compression, minimal initramfs | Walk through ARM64 boot stages; name specific optimization for each layer |
| Topic 6: ARM/ARM64 | EL0–EL3, exception vectors, ESR/FARCache coherency (MESI/MOESI, ACE)GICv3, SMMU, TrustZone, PSCI | Trace IRQ from hardware to handler; explain DMA coherency; describe EL levels for real use cases |
| Topic 7: Profiling & Debug | perf stat/record/report/sched/memftrace: function_graph, irqsoff, eventsbpftrace, BCC tools, crash utility | Debug latency spike scenario end-to-end; explain perf vs ftrace vs eBPF trade-offs |
| Topic 8: Upstream Dev | git format-patch, git send-emailcheckpatch.pl, MAINTAINERS, get_maintainer.pllinux-arm-msm list, Fixes: tag, stable backport | Submit a patch scenario: how you find maintainer, format, send, respond to review, version bump |
| Topic 9: Virtualization | KVM on ARM64, VHE, stage-2 MMUQEMU + KVM workflow, perf kvm statVirtio architecture, vhost, pKVM | Explain VHE benefit; describe VM-exit flow; contrast pKVM vs regular KVM for Android security |


## Top 15 Expected Interview Questions — Part 2 Topics

| # | Topic | Question |
| Q1 | Boot | Walk me through the boot process of a Qualcomm ARM64 SoC from power-on to shell prompt. |
| Q2 | Boot | How would you reduce boot time by 500ms on an embedded ARM64 product? |
| Q3 | Boot | What is deferred probing and how does it improve boot parallelism? |
| Q4 | ARM64 | What happens at each exception level during a system call on ARM64? |
| Q5 | ARM64 | Explain how a GICv3 interrupt flows from hardware pin to the Linux interrupt handler. |
| Q6 | ARM64 | Why does a driver need to flush DMA buffers on Qualcomm SoCs? What APIs are used? |
| Q7 | ARM64 | Explain ARM big.LITTLE topology and how EAS uses CPU capacity for task placement. |
| Q8 | Profiling | A critical kernel path shows latency spikes in production. How do you debug it? |
| Q9 | Profiling | Compare perf, ftrace, and eBPF. When would you use each? |
| Q10 | Profiling | How do you analyze a kernel crash dump? Walk me through the crash utility workflow. |
| Q11 | Upstream | Walk me through submitting a bug fix patch to the upstream Linux kernel. |
| Q12 | Upstream | How do you determine which mailing list to send a Qualcomm driver patch to? |
| Q13 | Upstream | What is the kernel release cycle? How does the merge window work? |
| Q14 | KVM | How does KVM work on ARM64? What is the role of EL2 and VHE? |
| Q15 | KVM | What is pKVM (Protected KVM)? How does it achieve guest memory isolation? |


| QUALCOMM LINUX KERNEL INTERVIEW PREP — PART 2 OF 3Sandeep Kumar • April 22, 2026 • Topics 5–9 • See Part 1 for Topics 1–4 • See Part 3 for Topics 10–14 |


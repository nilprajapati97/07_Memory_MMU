

Qualcomm Staff Linux Kernel
Engineer
Interview Study Guide
Part 3 of 3: Topics 10–14 + 5-Day Study Plan

| Prepared for: Sandeep KumarPosition: Engineer, Staff — Linux KernelCompany: Qualcomm India Private LimitedInterview Date: April 22, 2026 |


| Topic | Coverage |
| 10. Containers | Docker, Kubernetes, namespaces, cgroups v2, security |
| 11. Device Drivers | Platform driver model, DT, clocks, GPIO, DMA, I2C/SPI |
| 12. sched-ext Deep Dive | BPF scheduler framework, hooks, scx schedulers |
| 13. AI/ML Basics | Inference workloads, NPU/GPU offload, edge AI |
| 14. Userspace Resource Mgmt | cgroups, systemd, power/perf trade-offs |
| 5-Day Study Plan | Prioritized day-by-day schedule for April 22 interview |


# Topic 10: Containers
Containers are a critical technology in the modern Linux ecosystem. For the Qualcomm Staff Linux Kernel Engineer role, you need deep understanding of how containers work from a kernel perspective — specifically namespaces, cgroups, and security mechanisms.
## 10.1 Container Fundamentals
A container is a lightweight process isolation mechanism built using two core Linux kernel features: namespaces (for resource visibility isolation) and cgroups (for resource usage control). Unlike VMs, containers share the host kernel.
- Containers = Namespaces + cgroups + Layered filesystem (OverlayFS)
- No separate kernel per container — all containers share the host kernel
- Copy-on-Write (CoW) filesystem layers for storage efficiency
- Container runtimes: Docker (containerd/runc), Podman, LXC
- OCI (Open Container Initiative) standard defines image and runtime specs

| 📌 Interview TipWhen asked "how does Docker work at the kernel level?" answer: it uses clone(2) with namespace flags (CLONE_NEWPID, CLONE_NEWNET, etc.) to create isolated processes, and cgroups to enforce resource limits. The container image layers are stacked with OverlayFS. |

## 10.2 Linux Namespaces
Namespaces provide process-level isolation by partitioning global kernel resources. Each namespace type controls a different resource dimension. A new namespace is created via clone(2) or unshare(2) system calls.
| Namespace | Flag | What it Isolates |
| PID | CLONE_NEWPID | Process IDs — PID 1 inside container |
| Network | CLONE_NEWNET | Network interfaces, routing tables, firewall rules |
| Mount | CLONE_NEWNS | Filesystem mount points (pivot_root) |
| UTS | CLONE_NEWUTS | Hostname and NIS domain name |
| IPC | CLONE_NEWIPC | SysV IPC, POSIX message queues |
| User | CLONE_NEWUSER | User/group IDs — rootless containers |
| Cgroup | CLONE_NEWCGROUP | cgroup root — prevents host cgroup visibility |
| Time | CLONE_NEWTIME | System clock offsets (Linux 5.6+) |


- Inspect namespaces: ls -la /proc/<pid>/ns/
- Enter namespace: nsenter -t <pid> --pid --net --mount
- Create namespace: unshare --pid --fork --mount-proc bash
- Namespace file descriptors remain open as long as a process uses them
## 10.3 cgroups (Control Groups)
cgroups limit, account for, and isolate resource usage of process groups. The kernel exposes cgroup interfaces as a filesystem hierarchy. Cgroups v2 (unified hierarchy) is now the default in modern distributions.
### cgroups v1 vs cgroups v2
| Feature | cgroups v1 | cgroups v2 (Recommended) |
| Hierarchy | Multiple independent hierarchies per subsystem | Single unified hierarchy for all controllers |
| Controller attach | Process can be in different cgroups per controller | Process in exactly one cgroup (all controllers) |
| Memory | memory.limit_in_bytes | memory.max, memory.high, memory.low |
| CPU | cpu.shares, cpu.cfs_quota_us | cpu.weight, cpu.max (period+quota) |
| PSI | Not available | pressure.{cpu,memory,io} — stall metrics |
| Freezer | freezer controller | cgroup.freeze interface |


### Key cgroup v2 Interfaces
- cpu.weight: Relative CPU weight (default 100, range 1-10000)
- cpu.max: "quota period" — e.g., "200000 1000000" = 20% of one CPU
- memory.max: Hard memory limit, triggers OOM killer
- memory.high: Soft limit, triggers reclaim and throttling
- io.max: Per-device I/O bandwidth and IOPS limits
- cgroup.procs: List of PIDs in this cgroup
- cgroup.controllers: Available controllers for child cgroups

| 📌 Interview Tip — cgroup DelegationFor containers, the systemd "slices" concept uses cgroups: system.slice (system services), user.slice (user sessions), machine.slice (VMs/containers). Docker uses cgroup v2 by default since Docker 20.10. Key interview Q: "What happens when memory.max is exceeded?" — the OOM killer is invoked within that cgroup. |

## 10.4 Docker Architecture
Docker is a container platform built on top of Linux kernel primitives. Its architecture separates concerns: Docker CLI, dockerd daemon, containerd (container lifecycle), and runc (OCI runtime that calls clone/cgroups).
- Docker CLI → dockerd (daemon) → containerd → runc → kernel (namespaces + cgroups)
- containerd: High-level runtime managing image pull, container lifecycle
- runc: OCI-compliant low-level runtime — calls clone(2) with namespace flags
- Image layers use OverlayFS: lowerdir (read-only) + upperdir (writable) + merged
- Docker networking: bridge (default), host, overlay (Swarm), macvlan
- Volume mounts use bind mounts at the kernel level (MS_BIND flag to mount(2))
- Relevant commands: docker run, exec, inspect, stats, system df

### OverlayFS in Detail
OverlayFS (overlay2 in Docker) stacks read-only image layers on top of each other with a writable layer on top. The kernel merges them transparently. File modifications trigger copy-on-write.
- mount -t overlay overlay -o lowerdir=L1:L2,upperdir=U,workdir=W /merged
- lowerdir: Read-only image layers (stacked, :-separated, top-first)
- upperdir: Container-writable layer (all modifications go here)
- workdir: Temp directory for atomic operations (must be on same fs as upperdir)
- CoW: First write to a lower-layer file copies it to upperdir
## 10.5 Kubernetes Basics
Kubernetes (k8s) is a container orchestration platform. While deep Kubernetes knowledge is not the primary focus, understanding its interaction with the Linux kernel — especially resource management — is important.
| Concept | Description |
| Pod | Smallest deployable unit; one or more containers sharing a network namespace and storage |
| Node | Physical/virtual machine running kubelet + container runtime (containerd/CRI-O) |
| Limits | cpu/memory limits map directly to cgroup cpu.max and memory.max |
| Requests | Scheduling hint; maps to cgroup cpu.weight and memory.min (guaranteed) |
| QoS Classes | Guaranteed (requests=limits), Burstable (partial), BestEffort (no limits) — affects OOM kill priority |
| DaemonSet | Runs one pod per node — useful for monitoring agents, CNI plugins |


- CRI (Container Runtime Interface): kubelet → CRI → containerd/CRI-O → runc
- CNI (Container Network Interface): Flannel, Calico, Cilium (eBPF-based)
- Cilium uses eBPF for kernel-level networking without iptables overhead
- Resource quotas enforce aggregate cgroup limits across namespaces
## 10.6 Container Security
Container security operates at multiple kernel levels. Even with namespace isolation, containers share the kernel, so a kernel vulnerability could break container isolation.
### Seccomp (Secure Computing Mode)
Seccomp filters system calls using BPF programs. Docker applies a default seccomp profile that blocks ~44 dangerous syscalls (e.g., keyctl, ptrace, kexec_load).
- Mode 1 (SECCOMP_MODE_STRICT): Only read, write, exit, sigreturn allowed
- Mode 2 (SECCOMP_MODE_FILTER): BPF filter program specifies allowed syscalls
- prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) to apply filter
- Docker default profile: /etc/docker/seccomp.json
### AppArmor and SELinux
Mandatory Access Control (MAC) systems that restrict container processes beyond DAC (discretionary access control). They implement the principle of least privilege at the kernel LSM layer.
- AppArmor: Path-based profiles loaded via /sys/kernel/security/apparmor/
- SELinux: Label-based; every process and file has a security context
- Docker SELinux labels: container_t (process), container_file_t (volumes)
- Capabilities: Docker drops most capabilities (NET_RAW, SYS_ADMIN, etc.) by default
- User namespaces: Map container root (uid=0) to unprivileged host user

| 📌 Interview Tip — Container Security LayersIf asked "how secure are containers?" explain defense-in-depth: (1) Namespaces for visibility isolation, (2) cgroups for resource limits, (3) Capabilities for privilege reduction, (4) Seccomp for syscall filtering, (5) AppArmor/SELinux for MAC, (6) User namespaces for UID mapping. No single layer is sufficient alone. |

# Topic 11: Device Drivers (Relevant Basics)
Device driver knowledge is fundamental for a Linux Kernel Engineer at Qualcomm. Qualcomm SoCs have hundreds of peripherals — understanding how to write, debug, and optimize drivers is essential.
## 11.1 Platform Device Driver Model
The platform device model is used for devices that cannot be auto-discovered (unlike PCI/USB). Most SoC peripherals — UARTs, I2C controllers, clock controllers, GPIO banks — use this model.
- Device is described in Device Tree (DTS) or ACPI tables
- Platform bus matches devices to drivers using compatible strings
- Driver registers with platform_driver_register(&my_driver)
- Probe is called when device and driver are matched

### Driver Registration and Probe
static struct platform_driver my_driver = {
    .probe  = my_probe,       /* called on device match */
    .remove = my_remove,      /* called on device removal */
    .driver = {
        .name  = "my-device",
        .of_match_table = my_of_match, /* DT matching */
        .pm = &my_pm_ops,    /* power management callbacks */
    },
};
module_platform_driver(my_driver); /* replaces init/exit */

- devm_* APIs: Device-managed resources, auto-freed on driver detach
- devm_ioremap_resource(): Map device register space from DT
- devm_request_irq(): Request IRQ with auto-free
- dev_err(dev, ...): Preferred over pr_err — includes device name in log
## 11.2 Device Tree Bindings
Device Tree (DT) describes hardware topology to the kernel. Qualcomm SoCs rely heavily on DT bindings. Writing and reviewing DT bindings is a common upstream contribution task.
### Device Tree Node Example
uart0: serial@7af0000 {
    compatible = "qcom,geni-uart";
    reg = <0x07af0000 0x4000>;        /* register base, size */
    interrupts = <GIC_SPI 26 IRQ_TYPE_LEVEL_HIGH>;
    clocks = <&gcc GCC_QUPV3_UART0_CLK>;  /* clock consumer */
    clock-names = "core";
    pinctrl-0 = <&uart0_default_state>;  /* pin mux state */
    status = "okay";
};

- Binding schema defined in Documentation/devicetree/bindings/ (YAML since 5.x)
- dt-bindings: Compatible string format "vendor,device" (e.g., "qcom,sc8180x-camcc")
- of_match_table matches DT compatible to driver
- of_property_read_u32(), of_get_named_gpio(), of_parse_clkspec() — DT parsing APIs
- DT overlays (.dtbo): Runtime DT modification for board variants
## 11.3 Clock Framework
The Linux clock framework (Common Clock Framework, CCF) manages clock sources, PLLs, dividers, gates, and muxes. On Qualcomm SoCs, the clock controller (GCC, CAMCC, DISPCC, etc.) provides hundreds of clocks.
- clk_get() / devm_clk_get(): Acquire a clock handle
- clk_prepare_enable() / clk_disable_unprepare(): Two-phase enable/disable
- clk_set_rate(): Request a specific frequency (actual rate may differ)
- clk_round_rate(): Query achievable rate without setting it
- CLK_SET_RATE_PARENT: Propagate rate change to parent clock
- debugfs: /sys/kernel/debug/clk/clk_summary — view all clocks and rates
- Qualcomm: GCC (Global Clock Controller) is the main clock provider
## 11.4 Pinctrl and GPIO
Most SoC pins are multiplexed — they can serve as GPIO, I2C, SPI, UART, etc. The pinctrl subsystem manages pin function selection (muxing) and configuration (pull-up/down, drive strength).
- Pin functions defined in DT: pinctrl-0 (default), pinctrl-1 (sleep), etc.
- pinctrl_select_state(): Switch pin state (called during probe, suspend)
- GPIO: devm_gpiod_get(), gpiod_direction_output(), gpiod_set_value()
- TLMM (Top Level Mode Multiplexer): Qualcomm pin controller
- gpio-keys driver: Translates GPIO interrupts to input events
## 11.5 DMA Framework
The DMA Engine subsystem provides a generic API for DMA operations. Qualcomm uses GPI (General Purpose Interface) DMA for peripheral DMA and BAM (Bus Access Manager) for high-bandwidth DMA.
- dma_alloc_coherent(): Allocate coherent (non-cached) DMA buffer — CPU and device see same data
- dma_map_single(): Map existing buffer for DMA — cache-flush/invalidate handled
- dma_sync_single_for_cpu(): After DMA completes, sync buffer to CPU cache
- DMA Engine API: dmaengine_prep_slave_sg(), dmaengine_submit(), dma_async_issue_pending()
- IOMMU/SMMU integration: DMA addresses go through SMMU for device isolation
- dma_direct_* vs IOMMU-mapped DMA: differs in physical vs virtual DMA address
## 11.6 I2C, SPI, and UART Drivers
I2C, SPI, and UART are the most common low-speed peripheral interfaces on Qualcomm SoCs. Understanding their kernel subsystem structure is important for driver development.
| Bus | Kernel Subsystem | Driver API | Qualcomm Example |
| I2C | i2c-core | i2c_driver, probe/remove, i2c_transfer() | qcom-geni-i2c |
| SPI | spi-core | spi_driver, spi_transfer, spi_message | spi-geni-qcom |
| UART | tty-serial | uart_driver, uart_ops | msm_geni_serial |
| I3C | i3c-core | i3c_driver, i3c_device_send_hdr_cmds() | Next-gen Qualcomm |


- regmap: Generic register map abstraction over I2C/SPI/MMIO
- regmap_read(), regmap_write(), regmap_update_bits() — common APIs
- I2C smbus functions: i2c_smbus_read_byte_data(), write_byte_data()

| 📌 Interview Tip — Driver DebuggingCommon driver debugging flow: (1) Check dmesg for probe errors, (2) Use "echo 8 > /proc/sys/kernel/printk" for maximum verbosity, (3) Use dynamic debug: "echo 'module mydrv +p' > /sys/kernel/debug/dynamic_debug/control", (4) Check /sys/bus/platform/devices/ for device binding, (5) Use "cat /sys/kernel/debug/clk/clk_summary" for clock states. |

# Topic 12: sched-ext (BPF Extensible Scheduler) — Deep Dive
sched-ext is a Preferred Qualification for this role — study this topic thoroughly. It allows custom CPU schedulers to be loaded at runtime as BPF programs without kernel recompilation. Merged in Linux 6.12 (late 2024).

| ⭐ Priority Notesched-ext is explicitly listed as a Preferred Qualification in the Qualcomm JD. Expect at least one deep-dive question. Be prepared to discuss the architecture, hooks, and at least one scheduler (scx_rusty or scx_lavd) in detail. |

## 12.1 BPF/eBPF Fundamentals
eBPF (extended Berkeley Packet Filter) is a kernel subsystem that allows safe, verifiable programs to run in kernel space without modifying kernel source code. It is the foundation of sched-ext.
| eBPF Component | Description |
| Verifier | Statically analyzes BPF bytecode: no unbounded loops, no null deref, type safety |
| JIT Compiler | Translates BPF bytecode to native machine code (x86_64/ARM64) for performance |
| Maps | Key-value stores shared between BPF programs and userspace (hash, array, percpu, ringbuf) |
| Helpers | Kernel functions callable from BPF (bpf_ktime_get_ns, bpf_get_current_pid_tgid, etc.) |
| Kfuncs | Kernel-exported functions for BPF use; sched-ext exposes scx_bpf_* kfuncs |
| BTF | BPF Type Format: kernel type info enabling CO-RE (Compile Once, Run Everywhere) |


- BPF programs are loaded via bpf(BPF_PROG_LOAD, ...) syscall
- BPF maps created with bpf(BPF_MAP_CREATE, ...) and accessed from both sides
- libbpf: Userspace library for BPF program loading and management
- bpftool: CLI tool for BPF program/map inspection
- bpftrace: High-level BPF tracing language (similar to awk for kernel events)
## 12.2 sched-ext Architecture
sched-ext introduces a new scheduling class (ext_sched_class) in the kernel. When a sched-ext scheduler is loaded, it takes over scheduling decisions for tasks using SCHED_EXT policy or optionally all tasks.
### Scheduling Class Hierarchy (with sched-ext)
| Priority | Class | Description |
| 1 (Highest) | stop_sched_class | Migration thread, CPU stop tasks (not schedulable by users) |
| 2 | dl_sched_class | SCHED_DEADLINE: Sporadic tasks with deadline/period/runtime |
| 3 | rt_sched_class | SCHED_FIFO / SCHED_RR: Real-time tasks, priority 1-99 |
| 4 (NEW) | ext_sched_class | sched-ext: BPF-implemented scheduler for SCHED_EXT tasks |
| 5 | fair_sched_class | SCHED_NORMAL/BATCH: CFS/EEVDF for most user tasks |
| 6 (Lowest) | idle_sched_class | SCHED_IDLE: Runs only when no other task is runnable |


- sched-ext can optionally override scheduling for ALL tasks (not just SCHED_EXT)
- BPF scheduler runs as a kernel BPF program — low overhead, no context switches
- If BPF scheduler crashes, kernel safely falls back to the default scheduler
- Safety: BPF verifier ensures no infinite loops, no kernel memory corruption
## 12.3 sched-ext Hooks and Callbacks
The sched-ext framework exposes BPF struct_ops callbacks. The BPF scheduler implements the callbacks it needs and leaves others as defaults. Key callbacks are described below.
| Callback | Description and Usage |
| enqueue(task, enq_flags) | Called when a task becomes runnable. Place task in a DSQ using scx_bpf_dispatch() |
| dequeue(task, deq_flags) | Called when a task stops being runnable (blocks, exits). Remove from custom data structures. |
| dispatch(cpu, prev) | Called when CPU needs next task. Pick from DSQ using scx_bpf_consume(). Core scheduling decision point. |
| select_cpu(task, prev_cpu, wake_flags) | Choose which CPU to wake task on. Return target CPU. Affects cache locality and NUMA balance. |
| running(task) | Task is now running on CPU. Update accounting, statistics. |
| stopping(task, runnable) | Task is being preempted or blocking. Finalize runtime accounting. |
| init_task(task, fork) | Task created (fork). Allocate per-task BPF data structures. |
| exit_task(task) | Task exiting. Free per-task data. |
| cpu_online/offline | CPU hotplug events. Update scheduler topology state. |


### Dispatch Queues (DSQs)
DSQs are FIFO queues used to stage tasks for dispatch. They decouple enqueueing (decision when task becomes runnable) from dispatch (execution when CPU is ready).
- SCX_DSQ_GLOBAL: System-wide FIFO shared across all CPUs
- SCX_DSQ_LOCAL: Per-CPU local DSQ (fastest dispatch)
- scx_bpf_create_dsq(dsq_id, node): Create custom DSQ (e.g., per-priority)
- scx_bpf_dispatch(task, dsq_id, slice_ns, enq_flags): Place task in DSQ
- scx_bpf_consume(dsq_id): Pull task from DSQ to local CPU queue
- scx_bpf_dispatch_vtime(): Dispatch with virtual time ordering (for fairness)
## 12.4 Writing a Basic sched-ext Scheduler
A minimal sched-ext scheduler in C (skeleton shown). Real schedulers use scx_utils (Rust library) or libbpf (C library).
/* BPF-side skeleton (simplified) */
#include <scx/common.bpf.h>
void BPF_STRUCT_OPS(my_enqueue, struct task_struct *p, u64 enq_flags) {
    scx_bpf_dispatch(p, SCX_DSQ_GLOBAL, SCX_SLICE_DFL, enq_flags);
}
void BPF_STRUCT_OPS(my_dispatch, s32 cpu, struct task_struct *prev) {
    scx_bpf_consume(SCX_DSQ_GLOBAL); /* pull task to this CPU */
}
SEC(".struct_ops.link")
struct sched_ext_ops my_ops = {
    .enqueue = (void *)my_enqueue,
    .dispatch = (void *)my_dispatch,
    .name = "my_simple_sched",
};

- Load scheduler: sudo scx_simple (or scxtop for monitoring)
- Check status: cat /sys/kernel/sched_ext/state (idle/enabled/disabled)
- Unload: SIGTERM to the loader process, kernel reverts to default scheduler
## 12.5 Production sched-ext Schedulers
Several production-quality sched-ext schedulers are available in the sched_ext/scx repository. Each targets different workload characteristics.
| Scheduler | Language | Design Philosophy and Use Cases |
| scx_rusty | Rust + BPF | NUMA-aware, load-balancing scheduler. Userspace makes global decisions; BPF does per-CPU dispatch. Good for servers and NUMA systems. |
| scx_lavd | Rust + BPF | Latency-Aware Virtual Deadline: Optimized for interactive/gaming. Considers latency criticality and virtual deadlines. Core-to-core latency aware. |
| scx_bpfland | BPF only | Prioritizes interactive tasks (those waiting on I/O or sleeping often). Good for desktop responsiveness. |
| scx_central | BPF only | Centralizes scheduling on CPU 0. Other CPUs run with preemption disabled, reducing overhead for HPC workloads. |
| scx_simple | BPF only | Minimal reference implementation: global FIFO or weighted FIFO. Good starting point for learning sched-ext. |
| scx_flatcg | BPF only | Flattens cgroup hierarchy for scheduling. Reduces overhead of deep cgroup trees in container workloads. |


- scx_utils (Rust): Common library for sched-ext schedulers (topology, stats, CPU info)
- scxtop: Real-time monitoring of sched-ext scheduler activity
- Repository: github.com/sched-ext/scx
- Kernel config: CONFIG_SCHED_CLASS_EXT=y required

| 📌 Interview Tip — sched-ext for Qualcomm Use CasesFrame sched-ext for Qualcomm context: (1) Power-aware scheduling on heterogeneous Kryo cores without kernel rebuilds, (2) Quickly prototype EAS alternatives for specific SoC workloads, (3) Game-mode scheduling that prioritizes foreground app threads, (4) Thermal-aware CPU placement using BPF maps fed with temperature data from thermal framework, (5) AI/ML inference workload prioritization on P-cores. |

# Topic 13: AI/ML Basics (Preferred Qualification)
AI/ML kernel-level knowledge is a preferred qualification. Focus on how AI/ML inference workloads interact with the Linux kernel subsystems you already know.
## 13.1 AI/ML Inference Workloads
ML inference is the process of running a trained model to make predictions. Unlike training (done in cloud/datacenter), inference runs on-device (Qualcomm SoCs). Key characteristics:
- Compute-intensive: Matrix multiplications, convolutions, activation functions
- Memory-intensive: Model weights (100MB-10GB), intermediate activations
- Latency-critical: Real-time inference (speech, camera AI) requires under 20ms
- Heterogeneous compute: CPU + GPU + NPU/DSP all involved in inference pipeline
- Batching: Group multiple requests to maximize hardware utilization

| Accelerator | Qualcomm Component | Linux Interface |
| CPU | Kryo cores | Normal task scheduling, NEON SIMD |
| GPU | Adreno GPU | DRM/KMS + Freedreno driver, OpenCL via Mesa |
| NPU/HTA | Hexagon DSP/NPU | misc char device, custom IOCTL interface, IOMMU |
| DSP | CDSP/ADSP | fastrpc driver, remoteproc subsystem |
| Memory | LPDDR5, LLCC | CMA for contiguous buffers, ION/dma-heap |

## 13.2 AI/ML Frameworks and Kernel Interaction
Multiple ML inference frameworks run on Qualcomm platforms. Understanding how they interface with the kernel is the key focus area.
### TensorFlow Lite (TFLite)
- Designed for mobile/embedded inference
- Delegates: GPU Delegate (OpenCL/OpenGL), NNAPI Delegate, Hexagon Delegate
- NNAPI (Android Neural Networks API): Kernel-userspace interface for ML accelerators
- Kernel side: Sync file framework (sync_file, sw_sync) for cross-hardware synchronization
### ONNX Runtime
- Cross-platform inference engine supporting ONNX model format
- Execution Providers: QNN (Qualcomm Neural Network) EP, DirectML, CUDA
- QNN SDK: Qualcomm ML acceleration SDK, uses kernel fastrpc + IOMMU
### PyTorch Mobile
- torch.jit.script() / torch.jit.trace() for model export
- Vulkan backend: Uses Qualcomm Adreno GPU via Vulkan compute shaders
## 13.3 Kernel Subsystems for AI/ML
Several kernel subsystems are directly involved in AI/ML workloads on Qualcomm platforms. Understanding these is where kernel expertise meets AI/ML.
### Memory Management for AI/ML
- CMA (Contiguous Memory Allocator): ML models need large contiguous buffers
- ION/dma-heap: Zero-copy buffer sharing between CPU and accelerators
- dma-buf: Cross-device buffer sharing without copy (CPU to GPU to NPU pipeline)
- Huge pages: THP reduces TLB pressure for large model weights
- mlock(): Pin model in memory to prevent paging during inference
### Scheduling for AI/ML
- AI inference threads should be SCHED_FIFO or high-priority SCHED_NORMAL
- sched-ext: Can prioritize inference threads during active inference windows
- CPU affinity: Pin preprocessing to efficiency cores, inference to performance cores
- CPU boost: cpufreq governor hint via sched_boost on Android/Qualcomm
- Deadline scheduling (SCHED_DEADLINE): Guarantee inference completes within deadline
### Power Management for AI/ML
- Voltage corners: NPU/DSP have turbo voltage corners for peak performance
- Power budgeting: IPA (Intelligent Power Allocation) controls throttling during AI
- NPU clock scaling: OPPs for the DSP/NPU (higher freq = more power = more heat)
- Dynamic power management: Runtime PM to power-gate NPU when idle
- Bandwidth: DDR bandwidth is often the bottleneck — BWMON driver tracks usage
## 13.4 FastRPC and Hexagon DSP
Qualcomm Hexagon DSP is accessed via the FastRPC mechanism — a remote procedure call framework that transparently marshals function calls and data from the CPU to the DSP.
- fastrpc kernel driver: drivers/char/adsprpc.c
- Userspace: Opens /dev/adsprpc-smd or /dev/fastrpc-adsp device node
- IOCTL interface: FASTRPC_IOCTL_INVOKE to call DSP functions
- Memory sharing: DMA-buf based zero-copy buffer passing to DSP
- SMMU: DSP DMA is protected by IOMMU context banks
- Remote heap: ADSP has its own memory, managed separately from Linux heap

| 📌 Interview Tip — AI/ML + Kernel AngleWhen discussing AI/ML in interviews, always connect it to kernel expertise: AI workloads on Qualcomm SoCs require large contiguous memory (CMA), cross-device buffer sharing (dma-buf), DSP access via FastRPC, thermal management via IPA, and scheduling policies to meet latency targets. sched-ext could enable a custom AI-aware scheduler that prioritizes inference threads during active inference windows. |

## 13.5 Edge AI and Kernel Optimization
Edge AI (running AI on-device rather than cloud) has unique optimization requirements that a Linux Kernel Engineer can directly impact.
- Prefetching: Linux readahead for model weight loading from storage
- NUMA placement: Place model weights on NUMA node closest to NPU
- Cache warming: madvise(MADV_WILLNEED) to prefetch model into cache before inference
- Interrupt coalescing: Reduce interrupt rate from NPU completion interrupts
- CPU-DSP latency: Minimize IPC overhead; use shared memory + lightweight notifications
- Power-vs-latency: EAS + sched-ext can select P-core vs E-core for AI preprocessing
# Topic 14: Userspace Resource Management
Userspace resource management uses kernel interfaces (cgroups, namespaces, scheduling APIs) to control and optimize application resource usage. At Qualcomm, this is critical for mobile/compute power-performance KPIs.
## 14.1 cgroups for Resource Control
cgroups v2 (unified hierarchy) is the standard. Resource controllers are enabled at the mount level and delegated to sub-hierarchies.
### cgroups v2 CPU Control
- cpu.weight: Relative scheduling weight (1-10000, default 100)
- cpu.weight.nice: Same expressed as nice value (-20 to +19)
- cpu.max: quota period — e.g., 500000 1000000 means 50% of one CPU per second
- cpu.uclamp.min: Minimum CPU utilization clamp (frequency floor)
- cpu.uclamp.max: Maximum CPU utilization clamp (frequency ceiling)
- cpu.stat: Per-cgroup CPU usage statistics

### cgroups v2 Memory Control
- memory.max: Hard OOM limit. Exceeding it kills processes in the cgroup.
- memory.high: Soft limit. Processes are throttled and memory is reclaimed.
- memory.low: Memory protection level. Kernel avoids reclaiming below this.
- memory.min: Hard memory guarantee. Never reclaimed regardless of pressure.
- memory.swap.max: Swap usage limit.
- memory.oom.group: Kill entire cgroup on OOM (vs. individual process).
- memory.pressure: PSI pressure stall information for this cgroup.

### cgroups v2 I/O Control
- io.max: "dev rbps wbps riops wiops" — bandwidth and IOPS limits per device
- io.weight: Relative I/O weight for proportional I/O scheduling (BFQ/mq-deadline)
- io.stat: Per-device I/O statistics for the cgroup
- io.latency: Target latency for I/O operations (latency-based throttling)
## 14.2 systemd Resource Management
systemd is the init system and service manager on modern Linux. It uses cgroups v2 natively to manage resources for services, users, and machines.
| systemd Unit Property | cgroup v2 Interface | Example Value |
| CPUWeight= | cpu.weight | CPUWeight=500 (5x normal priority) |
| CPUQuota= | cpu.max | CPUQuota=50% (half a CPU) |
| MemoryMax= | memory.max | MemoryMax=2G |
| MemoryHigh= | memory.high | MemoryHigh=1G (soft limit) |
| IOWeight= | io.weight | IOWeight=1000 (high I/O priority) |
| IOReadBandwidthMax= | io.max rbps | IOReadBandwidthMax=/dev/sda 50M |
| TasksMax= | pids.max | TasksMax=100 (fork bomb protection) |


### systemd Slice Hierarchy
- -.slice: Root slice
- system.slice: All system services (sshd, nginx, etc.)
- user.slice: Per-user sessions (user@1000.service)
- machine.slice: VMs and containers (systemd-machined)
- app.slice: Interactive applications (used in Android-like configs)
- Inspect: systemd-cgls, systemctl status, systemd-cgtop
- Transient units: systemd-run --scope --slice=app.slice myapp
## 14.3 Uclamp (Utilization Clamping)
Uclamp allows per-task hints to the CPU frequency governor. It directly influences which OPP is selected for a CPU, enabling fine-grained power-performance control.
- uclamp_min: Minimum utilization hint — sets a floor on CPU frequency
- uclamp_max: Maximum utilization hint — caps CPU frequency for power saving
- API: sched_setattr() with sched_util_min and sched_util_max fields
- Range: 0-1024 (0 = no hint, 1024 = max frequency)
- cgroup interface: cpu.uclamp.min / cpu.uclamp.max
- Kernel implementation: task_util_est() feeds uclamp into schedutil governor
- Use case: Foreground app gets uclamp_min=512 (ensures at least mid frequency)
- Power saving: Background tasks get uclamp_max=256 (capped at ~25% of max freq)
## 14.4 PSI (Pressure Stall Information)
PSI measures resource contention by tracking how long tasks are stalled waiting for CPU, memory, or I/O. Available in /proc/pressure/ and cgroup pressure files.
- CPU pressure: Tasks runnable but waiting for CPU (scheduling contention)
- Memory pressure: Tasks stalled waiting for memory reclaim
- I/O pressure: Tasks stalled waiting for I/O completion
- /proc/pressure/cpu: "some" (at least one task stalled) and "full" (all tasks stalled)
- PSI monitors: pollfd on /proc/pressure/* for threshold-based alerts
- Use case: Trigger LMKD when memory pressure exceeds threshold
- systemd uses PSI for early OOM detection and resource policy enforcement
## 14.5 Android-Specific Resource Management
For Qualcomm mobile platforms, Android adds its own resource management layer on top of Linux kernel primitives. Understanding these is relevant even for the upstream/compute role.
| Component | Description |
| LMKD | Low Memory Killer Daemon: Monitors PSI, kills processes by oom_score_adj when memory is low. Replaced kernel LMK. |
| Thermal HAL | Thermal Hardware Abstraction Layer: Interfaces kernel thermal framework to Android thermal policy engine |
| Power HAL | Controls CPU boost, cluster policies, uclamp hints for app launch, touch response, gaming modes |
| cpuset | Android uses cpuset cgroup to partition CPUs: top-app (big cores), foreground, background, restricted |
| Freezer | cgroup.freeze to suspend background app processes without killing them — saves memory and CPU |
| eBPF netd | Android networking daemon uses eBPF for per-UID traffic accounting and firewall rules in kernel |


| 📌 Interview Tip — Power/Perf KPIs at QualcommQualcomm evaluates kernel engineers on Power and Performance KPIs. Be ready to discuss: (1) How uclamp sets frequency floors/ceilings for apps, (2) How cpuset isolates foreground app on big cores, (3) How PSI-driven LMKD prevents OOM while minimizing kills, (4) How thermal throttling (IPA governor) balances performance vs thermal limits, (5) How sched-ext could implement a performance mode scheduler for key scenarios. |

## 14.6 Performance vs Power Trade-off Frameworks
The Linux kernel provides several frameworks that allow dynamic trade-offs between performance and power consumption. These are central to Qualcomm power/performance optimization work.
| Framework | How it Trades Performance for Power (or vice versa) |
| CPUFreq schedutil | Tracks CPU utilization via PELT, selects OPP via energy model. Tunable: rate_limit_us, iowait_boost_max |
| EAS (Energy-Aware Sched) | Places tasks on the most energy-efficient CPU considering capacity, utilization, and energy model |
| uclamp | Per-task/cgroup min/max utilization hints directly override EAS/schedutil frequency decisions |
| IPA (Intelligent Power Alloc.) | PID controller distributes power budget across thermal actors (CPU, GPU) to stay below SoC power limit |
| cpuidle TEO governor | Predicts next wakeup using timer events to select deepest safe idle state for maximum power savings |
| DVFS | V*f scaling: lower voltage quadratically reduces dynamic power (P = C * V squared * f) |

# 5-Day Study Plan for April 22 Interview
Based on the Qualcomm Staff Linux Kernel Engineer JD and your background in embedded Linux/ARM systems, here is a prioritized 5-day preparation plan. The interview date is April 22 — use the days preceding it wisely.

| 🎯 Interview StrategyFor a Staff-level role, interviewers expect depth over breadth. Be ready to go 3 levels deep on any topic. Always tie theoretical knowledge to practical experience: "I worked on X which involved Y — here is how I approached it and what I learned." Qualcomm values upstream contributions and power/performance debugging stories. |

## DAY 1 (April 17): Scheduling + sched-ext
Scheduling is the #1 topic given the sched-ext preferred qualification. This day focuses entirely on mastering the scheduler stack.
| Time Block | Activity |
| Morning (2h) | Review CFS: vruntime, red-black tree, min_vruntime, group scheduling. Draw the data structures from memory. |
| Late Morning (1.5h) | EEVDF: Why it replaced CFS (lag-tracking, eligible time), key improvements for latency-sensitive workloads. |
| Afternoon (2h) | sched-ext deep dive: Architecture, ALL callbacks (enqueue/dequeue/dispatch/select_cpu), DSQ types, scx_rusty vs scx_lavd design. |
| Late Afternoon (1.5h) | EAS (Energy-Aware Scheduling): Energy model, capacity-aware placement, interaction with schedutil, big.LITTLE topology. |
| Evening (1h) | Practice: Answer "explain CFS", "what is sched-ext?", "how would you write a power-aware BPF scheduler?" out loud. |


### Day 1 Key Concepts to Master
- CFS: How vruntime ensures fairness across different task weights
- EEVDF: The "eligible" constraint and how lag prevents starvation
- sched-ext: The three key hooks — enqueue, dispatch, select_cpu
- EAS: find_energy_efficient_cpu() flow and energy model data structures
- RT scheduling: Priority inversion, PI mutex, RT throttling (95%/100ms default)
## DAY 2 (April 18): Memory Management + Power Management
Memory and power management are always heavily tested at Qualcomm. These are the most complex kernel subsystems and require careful, systematic review.
| Time Block | Activity |
| Morning (2h) | ARM64 MMU: TTBR0/TTBR1, 4-level page tables, ASID, MAIR attributes. Draw the page table walk on paper. |
| Late Morning (1.5h) | Buddy allocator + SLUB: GFP flags, memory zones, kmalloc vs vmalloc vs ioremap. OOM killer scoring. |
| Afternoon (2h) | CPUFreq + CPUIdle + DVFS: schedutil internals, PSCI states, OPP tables, idle state selection algorithms. |
| Late Afternoon (1.5h) | Thermal framework: Trip points, IPA governor (PID controller), thermal zones, cooling devices, throttling chain. |
| Evening (1h) | Practice debugging scenarios: "How do you find a memory leak?", "Why is the system throttling?", "How do you optimize power on ARM?" |


### Day 2 Key Concepts to Master
- Page reclaim: kswapd, direct reclaim, LRU lists, refault distance
- CMA: How it reserves memory and serves contiguous allocation requests
- schedutil: How PELT signals (util_avg) translate to OPP selection
- IPA: How it distributes power budget and enforces thermal limits
- Runtime PM: autosuspend, the callback sequence, and common pitfalls
## DAY 3 (April 19): ARM64 Architecture + Performance Tools + Boot
ARM64 architectural knowledge is foundational for all other topics. Performance tools (perf, ftrace, eBPF) are essential for the debugging and profiling aspects of the role.
| Time Block | Activity |
| Morning (2h) | ARM64: EL0-EL3, exception model, GICv3 internals, SMMU/IOMMU, cache coherency (ACE/CHI). Review ARMv8 TRM key sections. |
| Late Morning (1.5h) | perf: Hardware PMU events, perf stat/record/report, flame graphs. Practice: perf stat -e cycles,instructions,cache-misses <cmd> |
| Afternoon (2h) | ftrace: function_graph, irqsoff, wakeup tracers, trace events. Practice: trace-cmd record -e sched:sched_switch |
| Late Afternoon (1h) | eBPF: Verifier, maps, kprobes/uprobes. Quick review of bpftrace one-liners for common debugging scenarios. |
| Evening (1.5h) | Boot optimization: ARM64 boot flow, initcall_debug, deferred probing, async probe. Review a real Qualcomm bootchart. |


### Day 3 Key Concepts to Master
- ARM64 exception model: What happens on interrupt? (vector → EL1 handler → GIC ACK)
- Cache maintenance: When to clean/invalidate/flush, PoC vs PoU
- DynamIQ topology: How cluster + core capacity affects EAS
- perf: Understand PMU multiplexing and sampling bias
- ftrace: How to enable/disable at runtime without reboot
## DAY 4 (April 20): Kernel Internals + Upstream + Virtualization
Day 4 consolidates kernel fundamentals, upstream process knowledge, and virtualization — all tested regularly at Qualcomm for a Staff-level engineer.
| Time Block | Activity |
| Morning (2h) | Synchronization: RCU (grace period, quiescent state, rcu_read_lock/unlock), lockdep internals, memory barriers (smp_mb, smp_rmb, smp_wmb). |
| Late Morning (1.5h) | Interrupt handling: Hardirq vs threaded IRQ, workqueue, tasklet trade-offs. IRQ affinity and balancing for performance. |
| Afternoon (1.5h) | Upstream process: git format-patch, checkpatch.pl, get_maintainer.pl, LKML submission. Review 2-3 real linux-arm-msm patches on patchwork. |
| Late Afternoon (1.5h) | KVM on ARM64: VHE, stage-2 page tables, vGIC, PSCI emulation. pKVM architecture for confidential computing. |
| Evening (1.5h) | Containers: Namespaces deep dive, cgroup v2 delegation, container security (seccomp BPF, capabilities). Device drivers: platform driver probe sequence. |


### Day 4 Key Concepts to Master
- RCU: Why it is faster than rwlock for read-heavy workloads
- Memory barriers: Why needed on ARM64 (weak memory model), when to use each
- KVM VHE: How EL2 host mode eliminates world-switch overhead
- Upstream: Know the exact Qualcomm subsystem mailing lists (linux-arm-msm@vger.kernel.org)
- cgroup v2: Understand "no internal process" constraint and delegation model
## DAY 5 (April 21): Mock Interview + Review + Rest
The day before the interview: no new learning, only consolidation and active recall. Mental freshness is as important as knowledge.
| Time Block | Activity |
| Morning (2h) | Self mock interview: Answer all 20 questions in the Quick Reference section below out loud, timing yourself at 3-4 minutes per answer. |
| Late Morning (1h) | Review weak areas identified during mock interview. Focus on concepts where you hesitated or gave incomplete answers. |
| Afternoon (1.5h) | Review your own work experience: Map your past projects to Qualcomm JD requirements. Prepare 2-3 "story" answers with STAR format. |
| Late Afternoon (1h) | Prepare questions for interviewers: Ask about team, product, power/perf challenges, kernel versions used, upstream activities. |
| Evening | Setup Microsoft Teams. REST. No kernel docs after 8 PM. Ensure good sleep — cognitive performance depends on it. |


### Day 5 — STAR Stories to Prepare
- Story 1: A time you debugged a complex kernel performance regression (perf/ftrace journey)
- Story 2: A time you optimized power consumption on an ARM platform (quantified result)
- Story 3: A time you submitted or reviewed an upstream kernel patch
- Story 4: A time you mentored a junior engineer or led a technical decision
- Story 5: A time you worked under conflicting priorities (performance vs power vs schedule)
# Part 3 — Top 20 Interview Questions with Guidance
These questions are most likely to be asked based on the Qualcomm JD and common Staff-level Linux kernel interview patterns. Practice answering each in 3-4 minutes.

| Q1: What is sched-ext and how does it work? |

| Define: runtime-loadable BPF scheduler. Cover: new scheduling class (ext_sched_class), BPF struct_ops callbacks (enqueue/dispatch/select_cpu), DSQs, safety via BPF verifier, Linux 6.12 merge. Mention scx_rusty or scx_lavd. |


| Q2: How would you write a power-aware BPF scheduler for Qualcomm big.LITTLE? |

| sched-ext select_cpu callback: read CPU capacity and current utilization from BPF maps. Place latency-sensitive tasks on big cores (high capacity), background tasks on LITTLE cores. Read thermal state from BPF map updated by userspace thermal monitor. Dispatch to appropriate per-cluster DSQ. |


| Q3: Explain the ARM64 virtual memory system. |

| Two translation regimes: TTBR0 (user, EL0), TTBR1 (kernel, EL1). 4-level page tables (PGD/PUD/PMD/PTE) for 48-bit VA. ASID for TLB tagging. MAIR for memory attributes. Stage-2 for hypervisor. GFP flags affect which zone pages come from. |


| Q4: How does the OOM killer decide which process to kill? |

| Scores each process: oom_score = (rss + swap) * 1000 / total_memory. Adjusted by oom_score_adj (-1000 to +1000). Higher score = more likely to die. oom_score_adj=-1000 makes process unkillable. cgroup memory.oom.group kills all group members together. |


| Q5: Explain CPUFreq schedutil governor in detail. |

| Uses PELT util_avg signal from scheduler. On task wakeup/enqueue, computes next_freq = C * util / max_cpu_capacity * max_freq. Applies iowait boost. Rate-limited by rate_limit_us (default 25us). Feeds frequency request to cpufreq driver which calls clk_set_rate() and regulator_set_voltage(). |


| Q6: How does Energy-Aware Scheduling work on DynamIQ? |

| find_energy_efficient_cpu() called on task wakeup. For each candidate CPU (prev, idlest in LLC, wake-affine): compute energy_delta = (util+task_util)/capacity * power. Pick CPU with lowest energy_delta that has sufficient capacity. Requires energy model (em_perf_domain) and CONFIG_ENERGY_MODEL=y. |


| Q7: Describe the ARM64 exception handling flow. |

| Exception occurs (IRQ/SVC/data abort): CPU reads VBAR_EL1 for vector table base. Saves state to stack (pushes x0-x30, sp, pc, pstate). Jumps to vector entry. Kernel handler reads ESR_EL1 (syndrome), FAR_EL1 (faulting address). For IRQ: calls GIC driver, ACKs interrupt, invokes handler, EOI. ERET returns. |


| Q8: How does RCU work? When would you use it vs rwlock? |

| RCU: readers never block; writers copy-update-swap. Grace period ensures old version read before freed. rcu_read_lock/unlock bracket read-side. call_rcu() defers free after grace period. Use RCU when reads >> writes, and read-side latency matters. rwlock blocks readers during write; unacceptable in interrupt context. |


| Q9: What is the IPA thermal governor and how does it work? |

| Intelligent Power Allocation: PID controller that distributes a power budget across cooling devices (CPU, GPU). Measures power consumption via power_actor.get_requested_power(). If SoC power > limit, reduces power_budget proportionally. CPU power actor uses freq → power lookup table from energy model. Prevents thermal throttling by proactive frequency reduction. |


| Q10: How does KVM work on ARM64 with VHE? |

| Without VHE: KVM at EL2, Linux kernel at EL1 (expensive world-switch). With VHE (ARMv8.1): Host kernel runs at EL2, guests at EL1/EL0. Eliminates EL2/EL1 world-switch overhead. Stage-2 page tables map IPA to PA for guest memory isolation. vGIC emulates GICv3 for guest interrupt delivery. VCPU = kernel thread + hypervisor state. |


| Q11: Explain how cgroups v2 differs from v1 and why it matters. |

| v1: Multiple hierarchies, process can be in different cgroups per controller (inconsistent state). v2: Single hierarchy, all controllers see same process tree. Key: no-internal-process rule (process only in leaf nodes). Better accounting, PSI support, delegation model. Docker supports v2 since 20.10. systemd uses v2 for unified resource management. |


| Q12: How would you debug a scheduling latency issue? |

| Tools: ftrace irqsoff/preemptoff tracers to find latency sources. ftrace wakeup tracer measures wakeup-to-run latency. perf sched latency to see worst-case latencies. Check /proc/sys/kernel/sched_rt_runtime_us (RT throttling). Use bpftrace to track time between sched_wakeup and sched_switch tracepoints. Look for priority inversion, lock contention. |


| Q13: How do containers differ from VMs at the kernel level? |

| VMs: separate kernel per VM, hardware-assisted virtualization (EL2/stage-2), full OS isolation but heavy. Containers: share host kernel, use namespaces for visibility isolation, cgroups for resource control, no overhead of VM-exit/VM-entry. Container security is weaker: kernel vulnerability can break container isolation. Defense-in-depth: seccomp + capabilities + AppArmor + user NS. |


| Q14: How does Linux boot on ARM64? What are the optimization techniques? |

| TF-A (EL3) → U-Boot/ABL (EL2/EL1) → kernel (head.S, EL2) → start_kernel() → rest_init() → init. Optimization: async probe (PROBE_PREFER_ASYNCHRONOUS), deferred probe (defer non-critical devices), initcall ordering, LZ4 compression, DT status=disabled for unused peripherals, initramfs minimization, LTO. |


| Q15: Explain uclamp and how it affects CPU frequency. |

| Utilization clamping: per-task min/max bounds on util_est signal. uclamp_min = frequency floor (prevents DVFS from going too low even when utilization is low). uclamp_max = frequency ceiling (caps frequency for power saving). schedutil reads effective_util = clamp(task_util, uclamp_min, uclamp_max) for OPP selection. API: sched_setattr SCHED_FLAG_UTIL_CLAMP_MIN/MAX. |


| Q16: How would you find and fix a memory leak in a kernel driver? |

| Use kmemleak: Enable CONFIG_DEBUG_KMEMLEAK, run echo scan > /sys/kernel/debug/kmemleak, check /sys/kernel/debug/kmemleak for unreferenced objects. kasan catches use-after-free and out-of-bounds. slabinfo tracks per-cache allocations. valgrind equivalent: kmemleak + kasan together. Fix: use devm_* APIs for automatic resource cleanup on driver unbind. |


| Q17: Describe the platform driver probe sequence. |

| Device registered (DT parsed → of_platform_populate → platform_device_register). Driver registered (platform_driver_register). Bus match: of_match_device() compares compatible strings. If match: driver.probe() called with struct platform_device. Inside probe: devm_ioremap_resource(), devm_request_irq(), clk_prepare_enable(), request_firmware(). On error: return errno, devm_* frees automatically. |


| Q18: How does PSI work and how is it used in practice? |

| PSI tracks time fraction spent stalled on CPU/memory/I/O. "Some" = at least one task stalled. "Full" = all non-idle tasks stalled (no progress). Implemented as per-cgroup and system-wide (/proc/pressure/). Practical use: LMKD polls memory.pressure; triggers process killing when pressure.some > threshold. systemd uses PSI for early OOM detection before hard limit hit. |


| Q19: What upstream kernel contributions have you made or are familiar with? |

| Describe real patches if applicable. Otherwise: Explain the process — identify issue, write fix, run checkpatch.pl, get_maintainer.pl, git send-email to subsystem list (linux-arm-msm, linux-pm, etc.). Describe reviewing others patches: Reviewed-by vs Acked-by vs Tested-by. Mention stable backports, Fixes: tag format. |


| Q20: How would you implement a performance-mode scheduler using sched-ext? |

| BPF enqueue: check task cgroup; if in "top-app" cgroup, set high-priority DSQ, high uclamp hint via bpf_sched_setscheduler. select_cpu: prefer big cores (read capacity from BPF map). dispatch: service high-priority DSQ first, then normal DSQ. Userspace: monitor app launch events (ActivityManager), toggle "perf mode" by writing to BPF map. Revert to normal after 500ms of inactivity. |


# Part 3 Quick Reference — Topic Summary
| Topic | Priority | Key APIs / Concepts |
| 10. Containers | 🟢 Good to Know | namespaces (7 types), cgroups v2, OverlayFS, seccomp, AppArmor, runc |
| 11. Device Drivers | 🟢 Good to Know | platform_driver, devm_*, of_match_table, CCF clocks, DMA, regmap |
| 12. sched-ext | ⭐ MUST KNOW | BPF struct_ops, enqueue/dispatch/select_cpu, DSQs, scx_rusty/lavd |
| 13. AI/ML Basics | 🟡 Medium | Hexagon NPU, FastRPC, dma-buf, CMA, NNAPI, inference scheduling |
| 14. Userspace Rsrc | 🔴 High Priority | cgroups v2 (cpu/mem/io), uclamp, PSI, systemd slices, LMKD, IPA |
| 5-Day Plan | 🟡 Use This! | Day1:Sched, Day2:MM+PM, Day3:ARM64+Tools, Day4:Upstream+KVM, Day5:Mock |


| 🚀 Final Note from Your Study GuideYou have the background (8+ years Linux/ARM, kernel development, embedded systems) that Qualcomm is looking for. The interview is about demonstrating depth of knowledge and the ability to reason through complex problems. Use the STAR method for behavioral questions. Connect every technical answer to real-world impact: power savings, boot time reduction, latency improvement. Good luck, Sandeep — you have got this! |


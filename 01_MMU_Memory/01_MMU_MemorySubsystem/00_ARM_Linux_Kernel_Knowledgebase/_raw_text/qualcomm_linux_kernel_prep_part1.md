Qualcomm Staff Linux Kernel Engineer
Interview Preparation Guide
| PART 1 OF 3 — TOPICS 1–4 |


| Topic # | Subject Area |
| Topic 1 | Linux Kernel Fundamentals |
| Topic 2 | CPU Scheduling |
| Topic 3 | Memory Management |
| Topic 4 | Power Management |


|  | 📌 Role: Engineer, Staff — Linux Kernel | Qualcomm India Private Limited📌 Focus: Power & performance optimizations on compute chipsets using upstream Linux📌 Priority: HIGH — these four topics are core to the Qualcomm interview |


# Topic 1: Linux Kernel Fundamentals
Relevance: Deep kernel internals knowledge is the foundation for every Qualcomm interview question. Expect architecture, boot-flow, and synchronization questions.
## 1.1 Kernel Architecture
Linux is a monolithic kernel with modular extensions. All kernel code runs in a single address space (kernel space) at EL1 on ARM64, providing high performance at the cost of stricter isolation compared to microkernels.
| Concept | Key Points |
| Monolithic Design | All subsystems (MM, VFS, net, drivers) share one address space; function calls, not IPC |
| Kernel Modules (.ko) | Loaded/unloaded at runtime via insmod/rmmod; same privilege as built-in code |
| User vs Kernel Space | User at EL0, Kernel at EL1 on ARM64; crossing via syscall (SVC #0) |
| Kconfig / Kbuild | Kconfig=feature selection; Kbuild=recursive make; defconfig for ARM64 cross-builds |

## 1.2 Kernel Boot Flow (ARM64)
|  | Bootloader (e.g., U-Boot / ABL) → loads kernel Image + DTB → jumps to _head (arch/arm64/kernel/head.S)head.S: MMU off, sets up page tables, enables MMU → jumps to start_kernel() (init/main.c)start_kernel(): sets up architecture, memory, scheduler, IRQs, drivers (do_initcalls) → launches init processinit/PID-1 spawns systemd or busybox; user-space continues mount/service bring-up |

Key boot-time profiling mechanisms: initcall_debug kernel param, bootchart, systemd-analyze — critical for Qualcomm boot-time KPIs.
## 1.3 System Calls
How syscalls work on ARM64: User code places syscall number in x8, arguments in x0–x5, then executes SVC #0. CPU traps to EL1; kernel dispatches via sys_call_table[].
- Entry path: el0_svc (entry.S) → el0_svc_common → invoke_syscall → handler
- Exit path: ret_to_user, potential reschedule / signal delivery on return
- Adding a new syscall: add to include/uapi/asm-generic/unistd.h, SYSCALL_DEFINE macro, update syscall table
## 1.4 Interrupt Handling
Linux uses a two-half model to keep interrupt latency low: the top half (hard IRQ handler) does minimal work and schedules the bottom half for deferred processing.
| Mechanism | Context | Use Case |
| Hard IRQ handler | Interrupt ctx (no sleep) | ACK hardware, wake tasklet/workqueue |
| Softirq | Soft interrupt ctx | Networking (NET_RX), block I/O (BLOCK_SOFTIRQ) |
| Tasklet | Softirq ctx | One-shot deferred work; cannot sleep |
| Workqueue | Process ctx (kthread) | Can sleep; CMWQ / alloc_workqueue |

## 1.5 Kernel Synchronization Primitives
Choosing the right lock is critical for both correctness and performance in a multi-core embedded SoC.
| Primitive | Sleep? | IRQ-safe? | Best For |
| spinlock_t | No (spin) | With _irqsave | Short critical sections, IRQ ctx |
| mutex | Yes | No (process ctx only) | Protecting data with sleeping ops |
| rwlock / rwsem | rwsem: yes | rwlock: yes | Read-heavy data (driver config) |
| RCU | rcu_read_lock: No | Yes | Mostly-read data structures, routing tables |
| atomic_t / atomic64_t | No | Yes | Reference counts, stats, flags |

RCU deep insight: Read-Copy-Update allows lock-free reads via rcu_read_lock/rcu_read_unlock; writers use rcu_assign_pointer + synchronize_rcu or call_rcu for deferred reclamation. Heavily used in scheduler, networking, and device driver subsystems.
## 1.6 Kernel Data Structures
- Linked list (list_head): Intrusive doubly-linked list embedded in objects; list_for_each_entry macro
- Red-Black tree (rb_root/rb_node): O(log n) ordered; used in CFS run queue, virtual memory areas (VMA), timers
- Radix tree / XArray: Page cache, IDR (ID allocation); XArray replaced radix tree in 4.20+
- Hash tables (hlist): PID hash, inode hash, dentry cache lookup; O(1) average lookups
- Per-CPU variables: DEFINE_PER_CPU; eliminates cache-line bouncing on SMP; used in scheduler, networking
## 1.7 proc, sysfs and debugfs
- /proc: Kernel-to-user interface; /proc/cpuinfo, /proc/meminfo, /proc/<pid>/maps, /proc/interrupts
- /sys (sysfs): Exposes kernel object model; driver attributes, power management, device topology
- /sys/kernel/debug (debugfs): Developer-only; tracing, scheduler stats, regulator states; mounted at boot on debug builds
## 1.8 Kernel Debugging Techniques
| Tool / Feature | How to Use & What It Finds |
| printk / pr_* | Severity levels (KERN_ERR, KERN_DEBUG); dev_dbg for device-linked messages; dmesg to read |
| dynamic_debug | Enable/disable pr_debug at runtime via /sys/kernel/debug/dynamic_debug/control without recompile |
| KASAN | Kernel Address Sanitizer; catches heap use-after-free, out-of-bounds; CONFIG_KASAN=y |
| UBSAN | Undefined Behaviour Sanitizer; catches integer overflow, misaligned access; CONFIG_UBSAN=y |
| lockdep | Lock dependency validator; detects deadlock cycles at runtime; CONFIG_PROVE_LOCKING=y |
| KGDB / KDB | Live kernel debugger over serial; set breakpoints, inspect registers and memory |

# Topic 2: CPU Scheduling
Relevance: Qualcomm explicitly asks about CFS, EEVDF, sched-ext (a preferred qualification), and Energy-Aware Scheduling on big.LITTLE/DynamIQ SoCs.
## 2.1 Completely Fair Scheduler (CFS)
CFS is the default task scheduler for SCHED_NORMAL/SCHED_BATCH tasks. It models "ideal multi-tasking" by tracking virtual runtime (vruntime) per task.
|  | Key idea: every task should run as if on a perfectly fair CPU; actual scheduling is approximated.vruntime: task runtime normalized by priority (nice) weight; lower vruntime = higher urgencyRun queue: per-CPU red-black tree ordered by vruntime; leftmost node = next-to-runTick: update vruntime on each scheduler tick; if current vruntime exceeds min_vruntime by sched_latency, preempt |

| CFS Parameter | Meaning & Tuning |
| sched_latency_ns | Target scheduling period (default 6ms–24ms); all runnable tasks get one slot per period |
| sched_min_granularity_ns | Minimum task runtime before preemption (default 0.75ms); prevents context-switch storm |
| sched_wakeup_granularity_ns | Amount vruntime must exceed current task to trigger preemption on wakeup |
| nice / weight | nice -20 to +19; mapped to weights; delta_vruntime = delta_time * (NICE_0_WEIGHT / weight) |

## 2.2 EEVDF — Earliest Eligible Virtual Deadline First
EEVDF (merged in Linux 6.6) replaces the pure vruntime selection of CFS with a deadline-based selection, improving latency fairness.
- Each task has a virtual deadline based on vruntime + slice; eligible tasks (vruntime ≤ min_vruntime) are candidate picks.
- Scheduler picks the eligible task with the earliest virtual deadline, not simply the leftmost node.
- Benefit: Better tail latency for interactive workloads; fairer slice distribution under mixed loads
- Tuning: sched_base_slice_ns controls minimum slice; can be tuned via /sys/kernel/debug/sched/
## 2.3 sched-ext: BPF-Extensible Scheduler Class
Merged in Linux 6.12. This is a preferred qualification at Qualcomm — be prepared for detailed questions.
|  | sched-ext exposes a set of BPF scheduling hooks that replace the scheduler class ops.A BPF program (scheduler) is loaded at runtime without kernel recompile or reboot.BPF verifier ensures safety; if the scheduler crashes or fails, kernel falls back to CFS.Primary use cases: power-aware scheduling, gaming latency, workload-specific SoC policies |

### 2.3.1 sched-ext Hooks (Key Callbacks)
| Hook / Op | Triggered When / Responsibility |
| ops.enqueue | Task becomes runnable; BPF program decides which DSQ (dispatch queue) to place it on |
| ops.dequeue | Task removed from runqueue (migrated, sleeping); optional cleanup |
| ops.dispatch | CPU needs next task; BPF moves tasks from custom DSQ to LOCAL_DSQ for execution |
| ops.running / .stopping | Task starts/stops executing; used for per-task time accounting |
| ops.select_cpu | Wakeup fast-path; BPF hints which CPU to wake the task on (cache-hot / idle preference) |

### 2.3.2 Notable sched-ext Schedulers
- scx_rusty: Rust-based; NUMA-aware load balancing; per-domain task stealing; production-used in Meta
- scx_lavd: Latency-Aware Virtual Deadline; gaming/interactive; measures "urgency" from wakeup freq
- scx_simple: Minimal FIFO-within-priority; great learning reference for writing your own
- scx_central: Centralizes scheduling decisions on core 0; reduces inter-core scheduling overhead
- Writing your own: Implement in C or Rust using libbpf / scx_utils; load with scx_loader or bpftool
## 2.4 Real-Time Scheduling Policies
| Policy | Priority | Behavior |
| SCHED_FIFO | 1–MAX (99) | Runs until voluntarily yields or preempted by higher-priority RT task; no time slicing |
| SCHED_RR | 1–MAX (99) | Like FIFO but with round-robin slice (default 100ms) among same-priority tasks |
| SCHED_DEADLINE | Above RT | Earliest Deadline First (EDF); specify runtime, deadline, period via sched_attr |
| SCHED_NORMAL (CFS) | nice -20..+19 | Default; fair time-sharing via vruntime/EEVDF |

RT throttling: kernel.sched_rt_period_us / sched_rt_runtime_us prevent RT tasks from starving normal tasks (default 95% CPU to RT).
## 2.5 CPU Affinity, cpusets and cgroups
- sched_setaffinity(): Pins process to CPU set; kernel migration only within allowed mask
- cpusets (cgroup subsystem): cpuset.cpus restricts which CPUs a cgroup can use; used in Android for foreground/background isolation
- CPU cgroup bandwidth: cpu.cfs_period_us + cpu.cfs_quota_us throttle group CPU time; cpu.shares for relative weight
- Topology awareness: sched_domain hierarchy: SMT → MC (per-die) → NUMA; load balancing respects boundaries
## 2.6 Load Balancing Across CPUs and NUMA
The CFS load balancer runs periodically (triggered by idle CPUs or load-imbalance) and during task wakeup to distribute tasks across CPUs.
- Periodic balancer: run_rebalance_domains called from scheduler tick; checks each sched_domain level
- Idle balancer: newidle_balance pulls tasks to freshly-idle CPU (latency-optimized, immediate)
- NUMA balancing: CONFIG_NUMA_BALANCING; scans task memory, migrates pages to node where task runs; task_numa_fault
- Misfit migration: For big.LITTLE: tasks needing more compute than small core provides are migrated to big core
## 2.7 Energy-Aware Scheduling (EAS) on ARM big.LITTLE / DynamIQ
EAS is critical for Qualcomm SoCs with heterogeneous CPU clusters. It extends the load balancer with an energy model to pick the most energy-efficient CPU for waking tasks.
|  | Prerequisite: CONFIG_ENERGY_MODEL=y; energy model supplied via device tree or EM framework API.EAS is activated when: (1) schedutil governor is active, (2) energy model is registered, (3) system is asymmetric (big.LITTLE).select_cpu (wakeup path): calls find_energy_efficient_cpu(); computes energy impact of adding task to each candidate CPU.Energy impact = (capacity_of(cpu) — util_after) * cost_per_unit: prefers spare capacity on already-active clusters.Migration triggers: misfit migration moves tasks to powerful cores; overutilized state disables EAS, falls back to raw LB. |

# Topic 3: Memory Management
Relevance: Qualcomm JD explicitly lists memory allocation, pressure, fragmentation, and latency. Expect deep-dive questions on buddy allocator, slab, and NUMA policies.
## 3.1 Virtual Memory & Page Tables
Linux uses multi-level page tables to map virtual addresses to physical pages. On ARM64 with 4KB granule, the default is a 4-level table (PGD → PUD → PMD → PTE); 5-level is supported for 57-bit VA.
| Component | Detail |
| PGD (L0) | Page Global Directory; TTBR0 (user) or TTBR1 (kernel) points to PGD base on ARM64 |
| PUD (L1) | Page Upper Directory; may be folded on 3-level configs |
| PMD (L2) | Page Middle Directory; 2MB huge pages terminate here (PMD-level huge page) |
| PTE (L3) | Page Table Entry; 4KB page; contains PFN + access/dirty/PXN/UXN bits on ARM64 |
| TLB | Caches VA→PA translations; ASID disambiguates user spaces; TLBI instructions for flush |

## 3.2 ARM MMU Internals
- TTBR0_EL1: User-space page table base; switched on context switch (ASID in TTBR0 avoids full TLB flush)
- TTBR1_EL1: Kernel page table base; constant across all processes (kernel VA is shared)
- Granule sizes: 4KB (default), 16KB, 64KB; determines page size and table walk steps
- Memory attributes (MAIR_EL1): Device-nGnRnE, Normal WB/RA/WA, Normal NC; set via mmap flags / ioremap variants
- Stage-2 (Hypervisor): Second level of translation for VMs; IPA → PA; controlled by VTTBR_EL2
## 3.3 Page Allocator — Buddy System
The buddy allocator manages physical memory in power-of-2 blocks (order 0 = 4KB to order MAX_ORDER-1 = typically 4MB). Free pages are maintained in per-order free lists per NUMA zone.
|  | Allocation: alloc_pages(GFP_flags, order) → finds smallest sufficient free block; splits higher orders as needed.Free: __free_pages() → merges with buddy block; walks up orders until no buddy available.GFP flags: GFP_KERNEL (can sleep, reclaim), GFP_ATOMIC (no sleep, IRQ-safe), GFP_DMA (low-mem zone), __GFP_ZERO.Zone hierarchy: ZONE_DMA (<16MB), ZONE_DMA32 (<4GB), ZONE_NORMAL, ZONE_HIGHMEM (32-bit only). |

## 3.4 Slab Allocator (SLUB / SLAB / SLOB)
The slab allocator sits above the buddy system and provides efficient allocation of small, fixed-size kernel objects (structs, descriptors, etc.).
| Allocator | Design | Best For / Notes |
| SLUB | Per-CPU slabs; minimal metadata; default | Default in all modern kernels; lowest overhead on SMP |
| SLAB | Original; per-CPU caches + shared pool | Legacy; more tuneable but heavier; SLUB generally preferred |
| SLOB | Simple first-fit; tiny code size | Embedded / very small systems only; not suitable for SMP |

Key APIs: kmalloc(size, GFP_KERNEL) for generic small alloc; kmem_cache_create/alloc/free for typed object caches; kzalloc for zero-init alloc.
## 3.5 vmalloc, kmalloc, ioremap — Comparison
| Function | Physical? | Can Sleep? | Use Case |
| kmalloc | Physically contiguous | Depends on GFP | DMA buffers, small kernel structs (<= few pages) |
| vmalloc | Virtually contiguous only | Yes | Large allocations, module text/data, VM areas |
| ioremap | Maps MMIO PA to VA | Yes | Device register access; sets Device memory attributes |
| alloc_pages | Physically contiguous | Depends on GFP | Raw page allocation; used by MM subsystems |

## 3.6 Memory Zones and NUMA
- NUMA node: Each CPU socket / cluster has local memory; remote access has higher latency
- NUMA policy: MPOL_BIND (local only), MPOL_PREFERRED (prefer local), MPOL_INTERLEAVE (round-robin)
- Kernel NUMA path: alloc_pages_node(nid, gfp, order); cpuset_allowed_nodes for cgroup-aware allocation
- NUMA stats: /proc/buddyinfo, /sys/devices/system/node/nodeN/meminfo, numastat tool
## 3.7 Memory Pressure, OOM Killer and Reclaim
When free memory falls below watermarks, the kernel reclaims pages through kswapd (background) or direct reclaim (synchronous, in allocating process context).
|  | Watermarks per zone: pages_min (trigger OOM), pages_low (wake kswapd), pages_high (kswapd sleeps).LRU lists: active/inactive anon and file lists; clock-algorithm approximates LRU reclaim order.Swap: anon pages can be swapped out; swap pressure increases via vm.swappiness (0=avoid, 100=aggressive).OOM killer: triggered when all reclaim paths fail; selects victim via oom_score / oom_score_adj; sends SIGKILL.Memory cgroups: memory.limit_in_bytes, memory.memsw.limit; per-cgroup OOM kills before global OOM. |

## 3.8 Memory Fragmentation, Compaction, and CMA
- Fragmentation: External (no contiguous free block despite enough total free); worsens DMA alloc at high order
- Compaction: kcompactd and direct compaction; migrates movable pages to create contiguous free regions
- Page mobility: MIGRATE_UNMOVABLE (kernel), MIGRATE_MOVABLE (user pages, file cache), MIGRATE_RECLAIMABLE
- CMA (Contiguous Memory Allocator): Reserves a region at boot; allows user page reuse until DMA driver requests the block
- CMA setup: cma=64M kernel cmdline or dma_contiguous_reserve(); used by V4L2, ION, GPU drivers on Qualcomm SoCs
## 3.9 Huge Pages & Transparent Huge Pages (THP)
- 2MB PMD-level huge pages (ARM64): Reduce TLB pressure for large mappings; ~10-20% perf gain for memory-intensive workloads
- THP (madvise/always/never): Kernel automatically promotes 4KB regions to 2MB when aligned; khugepaged scans and collapses
- THP downsides: Memory waste (2MB for small allocs), latency spikes during collapse; tune via /sys/kernel/mm/transparent_hugepage/
- HugeTLB (explicit): Preallocated huge pages reserved at boot; used by databases (e.g., PostgreSQL shared_buffers)
## 3.10 Memory Profiling Tools
- /proc/meminfo: MemFree, Buffers, Cached, Slab, VmallocUsed, AnonPages, Mapped
- /proc/<pid>/smaps: Per-VMA anonymous/shared/swapped breakdown; Private_Clean/Dirty key metrics
- slabinfo / slabtop: Per-cache object counts, sizes, memory consumed; identify slab leaks
- perf mem: Hardware memory access latency sampling; identifies cache miss hotspots
- vmstat -s: Page fault, swap, page reclaim counters
- kmemleak: CONFIG_DEBUG_KMEMLEAK; scans for unreferenced kernel allocations
# Topic 4: Power Management
Relevance: Power & performance optimization is the core of this Qualcomm role. Expect detailed questions on CPUFreq, CPUIdle, DVFS, thermal management, and the energy model.
## 4.1 CPUFreq Framework
CPUFreq manages CPU operating frequencies. It decouples frequency selection (governors) from hardware control (drivers), allowing policy changes without driver modifications.
|  | Architecture: cpufreq_policy per CPU cluster; governor selects target freq; driver applies it.Key files: /sys/devices/system/cpu/cpuN/cpufreq/{scaling_governor, scaling_available_frequencies, scaling_cur_freq}.Frequency domains: on big.LITTLE, each cluster is an independent domain (e.g., cpu0-3 little, cpu4-7 big).OPP table: Operating Performance Points defined in device tree (opp-hz, opp-microvolt) or firmware. |

| Governor | Behavior & Best Use |
| performance | Always max frequency; benchmarking, latency-critical; ignores power |
| powersave | Always min frequency; idle/standby scenarios; ignores performance |
| ondemand | Reacts to load; jumps to max on >80% util; legacy; CPU-intensive polling overhead |
| conservative | Like ondemand but gradually increases/decreases frequency; smoother transitions |
| schedutil | RECOMMENDED: uses scheduler utilization signals (PELT) directly; EAS prerequisite; lowest latency decision |
| userspace | Manual frequency control from user space; used by power HALs and benchmarking scripts |

schedutil deep-dive: Triggered on every scheduler utilization update (PELT tick); computes target freq = max_freq * util / capacity; applied via sugov_update_single/shared; rate_limit_us prevents too-frequent changes.
## 4.2 CPUIdle Framework
CPUIdle allows CPUs to enter low-power idle states (C-states on x86, WFI/WFE on ARM) when no tasks are runnable.
| Component | Role | ARM64 Specifics |
| cpuidle driver | Defines available idle states and their latency/power characteristics | arm_cpuidle_driver or platform-specific (e.g., qcom-cpuidle) |
| cpuidle governor | Selects which idle state to enter based on expected idle duration | menu (default) or teo (timer event-oriented, better for mobile) |
| Idle state | Power state with target residency and exit latency | WFI (C1, shallow), cluster power-off (deep), LPM levels |
| PSCI | Power State Coordination Interface (ARM firmware spec) | CPU_SUSPEND, CPU_OFF, SYSTEM_SUSPEND via SMC calls to EL3 |

- Key tunables: /sys/devices/system/cpu/cpuN/cpuidle/stateN/{disable, latency, residency, usage}
- Tracing: cpu_idle tracepoint (trace_cpu_idle); powertop idle state histogram; perf stat -e power/energy-cores/
## 4.3 DVFS — Dynamic Voltage and Frequency Scaling
DVFS combines CPUFreq (frequency) and regulator (voltage) adjustments to minimize power while meeting performance targets. Power ∝ V² × f.
|  | Voltage scaling: lower voltage at lower frequency reduces dynamic power quadratically.Regulator framework: regulator_set_voltage() + regulator_enable(); paired with CPUFreq OPP table.OPP (Operating Performance Points): each {freq, voltage} pair validated by hardware characterization.DVFS flow: governor requests freq → cpufreq notifier fires → driver raises voltage (if increasing) → changes PLL → lowers voltage (if decreasing).On Qualcomm SoCs: CPRh (Core Power Reduction hardened) hw dynamically adjusts voltage per workload. |

## 4.4 Runtime Power Management (Runtime PM)
Runtime PM allows individual devices to be suspended/resumed independently at runtime, without system-wide suspend. Critical for power efficiency on SoCs with many peripherals.
- Key APIs: pm_runtime_enable(), pm_runtime_get_sync() (resume+get), pm_runtime_put_autosuspend() (release)
- Autosuspend: Device suspended after idle timeout; pm_runtime_set_autosuspend_delay(dev, ms)
- Callbacks: driver .runtime_suspend / .runtime_resume / .runtime_idle; must be registered in dev_pm_ops
- Reference counting: rpm_get/rpm_put track usage; device suspended only at count==0
Qualcomm relevance: DSP, ISP, modem, GPU, NPU on Qualcomm SoCs are all managed via runtime PM + IOMMU power domains; bugs here cause both power regressions and functional failures.
## 4.5 System Suspend / Resume (S2R, S2Idle)
| State | Description & ARM64 Path |
| s2idle (S0ix) | CPU enters WFI deep-idle; no DRAM self-refresh; devices frozen; fast wake (< 100ms) |
| standby (S1) | Light sleep; limited driver suspend; rarely used on modern ARM |
| suspend-to-RAM (S3) | All CPUs off except one; DRAM in self-refresh; PSCI CPU_SUSPEND to platform LPM; ~10-50mW |
| hibernate (S4) | Memory image written to disk (swap); full power off; slow resume; not common on mobile |

Suspend flow: echo mem > /sys/power/state → freeze tasks → suspend devices (pm_noirq, pm_late) → disable non-boot CPUs → CPU_SUSPEND PSCI. Resume: reverse order.
## 4.6 Thermal Management Framework
The kernel thermal framework monitors temperatures and applies cooling actions to prevent hardware damage and meet thermal design power (TDP) budgets.
|  | Thermal zone: represents a temperature sensor (TSENS on Qualcomm); polling or interrupt-driven.Cooling device: an action to reduce heat — CPUFreq limit (passive), fan speed, display brightness.Thermal governor: connects zones to cooling devices; trips define thresholds.Governors: step_wise (default), power_allocator (preferred for mobile — budget-based), user_space.power_allocator: uses PID controller; distributes power budget across cooling devices proportionally. |

| Trip Point Type | Temperature | Action |
| THERMAL_TRIP_PASSIVE | e.g., 85°C | Trigger passive cooling (freq cap); power_allocator kicks in |
| THERMAL_TRIP_HOT | e.g., 95°C | Notify user space; aggressive freq throttle |
| THERMAL_TRIP_CRITICAL | e.g., 105°C | Emergency shutdown (orderly_poweroff) |

## 4.7 Power Domains and PM Domains
- PM domains (genpd): Group devices sharing a power rail; pm_genpd_add_device(); power on/off entire domain
- Qualcomm GDSC: Global Distributed Switch Controller; hardware power domain gates for subsystems (GPU, video, camera)
- Power domain hierarchy: Child domain cannot be off if parent is off; pm_genpd_add_subdomain() links hierarchy
- Device tree binding: power-domains = <&clock_gcc GCC_GPU_GDSC>; required for GDSC-based drivers
## 4.8 Energy Model and Power-Aware Scheduling
The Energy Model (EM) framework provides a per-CPU or per-cluster power cost table consumed by EAS (Section 2.7) to make energy-efficient wakeup decisions.
- EM registration: em_dev_register_perf_domain(dev, nr_states, ops, cpumask); ops provides cost per OPP
- Cost table: em_perf_state {frequency, power, cost}; cost = power / frequency for energy-per-task computation
- DT-based EM: capacity-dmips-mhz in cpu node for asymmetric capacity; dynamic-power-coefficient for power model
- Validation: cat /sys/devices/system/cpu/cpuN/cpufreq/energy_performance_available_preferences; compare perf/watt
## 4.9 Regulator Framework
- Purpose: Manages voltage/current regulators (PMIC rails) used by SoC subsystems
- Key APIs: regulator_get/put, regulator_enable/disable, regulator_set_voltage, regulator_get_voltage
- PMIC integration: Qualcomm PMICs (PM8998, PM8550 etc.) exposed via spmi-regulator driver; DT consumer binding via regulator-names
- Constraints: min/max voltage, max current in device tree; kernel enforces via regulator_set_voltage range checks
## 4.10 Power & Performance Profiling
The Qualcomm JD specifically mentions debugging performance regressions — these tools are essential:
| Tool | How to Use for Power/Performance |
| perf stat | perf stat -e power/energy-cores/,power/energy-pkg/ workload; measures energy in Joules |
| ftrace (trace_cpu_idle) | echo 1 > tracing/events/power/cpu_idle/enable; trace idle state residency histograms |
| powertop | Interactive TUI; idle state stats, wakeup sources, device power, tune suggestions |
| turbostat (x86) / energy_perf_bias | Per-core freq, idle %, power; ARM equivalent via perf/sysfs |
| Simpleperf / Perfetto | Android-specific; Perfetto records CPU freq, idle, sched traces systemwide |
| /sys/class/power_supply | Battery current/voltage/capacity; compute average power draw over test window |

# Quick-Reference: Part 1 Summary
| Topic | Key Subsystem / File | Must-Know Concepts |
| 1. Kernel Fundamentals | init/main.c, arch/arm64/ | Boot flow, syscall, IRQ top/bottom half, RCU, lockdep, KASAN |
| 2. CPU Scheduling | kernel/sched/ | CFS vruntime, EEVDF, sched-ext hooks, EAS, RT throttling, NUMA LB |
| 3. Memory Management | mm/ | Buddy system, SLUB, THP, CMA, OOM, NUMA policies, kmemleak |
| 4. Power Management | drivers/cpufreq/, drivers/thermal/ | schedutil, CPUIdle/PSCI, DVFS, Runtime PM, thermal governor, EM |


|  | 📖 Study Plan: Part 2 covers Topics 5–9 (Boot Optimization, ARM64 Architecture, Perf Tools, Upstream Development, Virtualization)📖 Study Plan: Part 3 covers Topics 10–14 (Containers, Device Drivers, sched-ext Deep Dive, AI/ML, Userspace Resource Mgmt)✅ Interview Date: April 22, 2026 | Role: Engineer, Staff — Linux Kernel | Qualcomm India Private Limited |


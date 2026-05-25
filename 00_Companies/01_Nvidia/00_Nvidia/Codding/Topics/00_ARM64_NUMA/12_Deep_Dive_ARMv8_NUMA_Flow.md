
# Deep Dive: ARMv8 NUMA Initialization Flow (Step-by-Step)

This document provides a comprehensive, interview-ready walkthrough of how NUMA is initialized in the Linux kernel on ARMv8 platforms, with code and architecture references.

---

## 1. Bootloader Loads Kernel and Hardware Tables
- The bootloader (e.g., UEFI, U-Boot) loads the kernel image and device tree (DT) or ACPI tables into memory.
- The CPU jumps to the kernel entry point (e.g., `arch/arm64/kernel/head.S`).

## 2. Kernel Entry: `start_kernel()` (init/main.c)
- The main C entry point for the kernel.
- Calls `setup_arch(&command_line)` for architecture-specific setup.

## 3. Architecture Setup: `setup_arch()` (arch/arm64/kernel/setup.c)
- Parses hardware tables (DT/ACPI) to discover CPUs, memory, and NUMA nodes.
- Calls `arm64_numa_init()` if NUMA is enabled.

## 4. NUMA Discovery: `arm64_numa_init()` (arch/arm64/mm/numa.c)
- Walks through hardware tables, grouping CPUs and memory into nodes.
- Builds CPU-to-node and memory-to-node maps.
- Stores these mappings for use by `early_cpu_to_node()`.

## 5. Early NUMA Setup: `early_numa_node_init()` (init/main.c)
- For each possible CPU, calls `early_cpu_to_node(cpu)` and sets the per-CPU node ID.
- Ensures that memory allocation and scheduling are NUMA-aware from the start.

## 6. Memory Allocator and Scheduler
- The page allocator uses the per-CPU NUMA node ID to prefer local memory.
- The scheduler uses the mapping to keep tasks close to their memory.

---

## Code Flow Diagram (Mermaid)

```mermaid
flowchart TD
    Bootloader[Bootloader] --> KernelEntry[Kernel Entry]
    KernelEntry --> StartKernel[start_kernel()]
    StartKernel --> SetupArch[setup_arch()]
    SetupArch --> Arm64NumaInit[arm64_numa_init()]
    Arm64NumaInit --> BuildMaps[Build CPU/Node Maps]
    BuildMaps --> EarlyNumaNodeInit[early_numa_node_init()]
    EarlyNumaNodeInit --> SetCpuNumaNode[set_cpu_numa_node()]
    SetCpuNumaNode --> AllocatorScheduler[Memory Allocator & Scheduler]
```

---

## Key Code References
- `init/main.c` (for `start_kernel()`, `early_numa_node_init()`)
- `arch/arm64/kernel/setup.c` (for `setup_arch()`)
- `arch/arm64/mm/numa.c` (for NUMA discovery and mapping)
- `include/linux/topology.h` (for per-CPU NUMA variables)

---

## Interview Tips
- Be able to walk through this flow, referencing both the architecture and the code.
- Understand why NUMA must be set up early (before memory allocation and scheduling).
- Know how to debug and verify NUMA topology on real hardware (see Debugging slide).

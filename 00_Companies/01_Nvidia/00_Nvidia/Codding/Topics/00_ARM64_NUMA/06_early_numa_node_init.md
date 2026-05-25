
# Deep Dive: early_numa_node_init() (From Scratch)

## What is early_numa_node_init()?

`early_numa_node_init()` is a critical function in the Linux kernel boot process. Its job is to set up the NUMA node ID for each CPU as early as possible, so that all subsequent memory allocations and scheduling decisions are NUMA-aware.

## Where is it Called?
- It is called from `start_kernel()` in `init/main.c`, right after architecture setup and before most kernel subsystems are initialized.

## Why is it Needed?
- The kernel must know, for each CPU, which NUMA node it belongs to.
- This mapping is needed before the memory allocator and scheduler are initialized.
- Without this, early allocations or task placement could be suboptimal, hurting performance.

## Code Walkthrough (init/main.c)

```c
static void __init early_numa_node_init(void)
{
#ifdef CONFIG_USE_PERCPU_NUMA_NODE_ID
#ifndef cpu_to_node
    int cpu;
    // The early_cpu_to_node() should be ready here.
    for_each_possible_cpu(cpu)
        set_cpu_numa_node(cpu, early_cpu_to_node(cpu));
#endif
#endif
}
```

### What Does This Do?
- For each possible CPU, it calls `early_cpu_to_node(cpu)` to get the NUMA node for that CPU.
- It then calls `set_cpu_numa_node(cpu, node)` to store this mapping in a per-CPU variable.
- On ARMv8, `early_cpu_to_node()` is set up during architecture initialization (see `arch/arm64/mm/numa.c`).

## Why So Early?
- The page allocator and scheduler need this mapping to make NUMA-aware decisions from the very start.
- If this is done too late, early allocations may go to the wrong node, causing performance issues.

---

## Diagram: How early_numa_node_init() Works

```mermaid
flowchart TD
    CPUList[All CPUs] --> EarlyCpuToNode[early_cpu_to_node(cpu)]
    EarlyCpuToNode --> SetCpuNumaNode[set_cpu_numa_node(cpu, node)]
    SetCpuNumaNode --> PerCpuMap[Per-CPU NUMA Node Map]
```

---

**Interview Tip:**
Be ready to explain why this function must run before the memory allocator and scheduler, and how it interacts with architecture-specific code.

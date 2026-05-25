
# Interview Q&A: NUMA and early_numa_node_init() (Nvidia/ARMv8 Focus)

## Conceptual Questions (with Answers)

**Q1:** What is NUMA and why is it important?
- **A:** NUMA (Non-Uniform Memory Access) is a memory architecture where each CPU has fast access to its local memory and slower access to remote memory. It's critical for scaling performance on large multi-core systems like Nvidia Grace.

**Q2:** How does the Linux kernel discover NUMA topology on ARMv8?
- **A:** The kernel parses hardware tables (ACPI SRAT or Device Tree) during early boot to discover nodes, CPUs, and memory regions.

**Q3:** What is the role of `early_numa_node_init()`?
- **A:** It sets up the per-CPU NUMA node IDs very early in boot, so memory allocation and scheduling are NUMA-aware from the start.

**Q4:** How does NUMA affect memory allocation and scheduling?
- **A:** The kernel prefers allocating memory from the local node and tries to schedule tasks on CPUs close to their memory.

**Q5:** What are NUMA policies and how can user-space control them?
- **A:** Policies control how memory is allocated (local, interleave, preferred, bind). User-space can set them with `numactl`, `mbind()`, or `set_mempolicy()`.

---

## Code/Kernel Questions (with Answers)

**Q1:** Where is `early_numa_node_init()` called in the boot sequence?
- **A:** From `start_kernel()` in `init/main.c`, after architecture setup and before most subsystems.

**Q2:** How does the kernel map CPUs to NUMA nodes on ARMv8?
- **A:** During `setup_arch()`, the kernel builds a CPU-to-node map using hardware tables, then sets per-CPU node IDs.

**Q3:** What happens if the NUMA topology is incorrect or missing?
- **A:** The kernel may treat the system as UMA (single node), which can hurt performance. Debug with `dmesg` and `/sys/devices/system/node/`.

**Q4:** How does the kernel fallback if local memory is exhausted?
- **A:** The page allocator tries remote nodes in order of proximity.

**Q5:** What are the main data structures for NUMA in the kernel?
- **A:** `node_to_cpumask[]`, `memnodemap[]`, per-CPU `numa_node_id`, and `zonelist[]`.

---

## Debugging/Practical Questions (with Answers)

**Q1:** How do you check NUMA topology on a running system?
- **A:** Use `numactl --hardware`, `lscpu`, or check `/sys/devices/system/node/`.

**Q2:** How do you debug performance issues related to NUMA?
- **A:** Check for remote memory allocations, use `perf` to measure latency, and tune with `numactl` or kernel boot parameters.

**Q3:** What tools can you use to tune NUMA behavior?
- **A:** `numactl`, `taskset`, `hwloc`, kernel boot parameters (`numa=off`, `numa_balancing=off`).

---

**Tip:** Practice explaining the flow from hardware discovery to kernel initialization to memory management and scheduling. Be ready to draw diagrams and reference code.

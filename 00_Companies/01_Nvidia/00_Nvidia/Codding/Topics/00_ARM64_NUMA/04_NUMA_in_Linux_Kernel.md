
# NUMA in the Linux Kernel: Data Structures and Awareness

## How Does the Kernel See NUMA?

The Linux kernel must understand the NUMA topology to:
- Allocate memory efficiently (prefer local node)
- Schedule tasks for locality
- Expose topology to user-space for tuning

### What Does the Kernel Need to Know?
- How many NUMA nodes exist?
- Which CPUs belong to which nodes?
- Which memory regions belong to which nodes?

---

## Key Kernel Data Structures

| Structure                | Purpose                                                      |
|--------------------------|--------------------------------------------------------------|
| `node_to_cpumask[]`      | Maps each node to its CPUs                                   |
| `memnodemap[]`           | Maps memory ranges to nodes                                  |
| Per-CPU `numa_node_id`   | Each CPU stores its own node ID for fast lookup              |
| `zonelist[]`             | List of memory zones (Normal, DMA, etc.) per node            |

---

## How Does the Kernel Use NUMA?

- **Memory allocation:**
  - The page allocator (`alloc_pages_node()`) prefers local node memory.
  - If local memory is full, it falls back to remote nodes.
- **Scheduling:**
  - The scheduler tries to run tasks on CPUs close to their memory.
  - NUMA balancing can migrate tasks or memory to improve locality.
- **APIs:**
  - `numa_node_id()`, `set_cpu_numa_node()`, `alloc_pages_node()`

---

## NUMA Policy (Kernel and User-space)

- The kernel and user-space can set policies:
  - Prefer local node
  - Interleave across nodes
  - Bind to a specific node

---

## Diagram: Kernel NUMA Mapping

| CPU ID | NUMA Node | Local Memory Region |
|--------|-----------|--------------------|
|   0    |     0     |   0x0000-0x7fff    |
|   1    |     0     |   0x0000-0x7fff    |
|   2    |     1     |   0x8000-0xffff    |

---

**Interview Tip:**
Be ready to describe how the kernel builds and uses these mappings, and how they affect performance.


# NUMA Memory Management in Linux: How Locality is Enforced

## How Does the Kernel Allocate Memory with NUMA?

### The Page Allocator
- Each NUMA node has its own set of memory zones (Normal, DMA, etc.).
- When a process running on CPU X requests memory, the kernel tries to allocate from the local node first.
- If local memory is full, the kernel falls back to remote nodes (with a performance penalty).

### Zone Lists
- Each node has a prioritized list of zones to try for allocations.
- The order: local node → nearby nodes → remote nodes.

---

## NUMA Policies: How Allocation is Controlled

| Policy      | Behavior                                                      |
|-------------|---------------------------------------------------------------|
| Default     | Prefer local node                                             |
| Interleave  | Spread allocations round-robin across all nodes               |
| Preferred   | Prefer a specific node, fallback to others if needed          |
| Bind        | Restrict allocations to a set of nodes (fail if not possible) |

### Who Sets Policy?
- The kernel sets a default policy, but user-space can override it per-process or per-allocation.

---

## User-space Control of NUMA

- Tools: `numactl`, `taskset`, `hwloc`
- System calls: `mbind()`, `set_mempolicy()`, `get_mempolicy()`
- Example: `numactl --membind=0 ./myapp` forces allocations to node 0

---

## Kernel APIs for NUMA

- `alloc_pages_node(node, ...)` — Allocate pages from a specific node
- `numa_node_id()` — Get the current CPU's node
- `set_mems_allowed()` — Set allowed nodes for allocation

---

## Diagram: NUMA Memory Allocation

```mermaid
flowchart TD
	Process[Process on CPU 2 (Node 1)] --> LocalAlloc[Try Local Node 1]
	LocalAlloc -- Success --> Memory[Memory Allocated on Node 1]
	LocalAlloc -- Fail --> RemoteAlloc[Try Remote Node 0]
	RemoteAlloc --> Memory[Memory Allocated on Node 0]
```

---

**Interview Tip:**
Be ready to explain how the kernel enforces locality, what happens when local memory is full, and how user-space can control policy.

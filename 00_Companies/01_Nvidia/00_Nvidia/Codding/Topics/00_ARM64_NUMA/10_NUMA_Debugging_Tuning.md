
# NUMA Debugging and Tuning: Tools and Pitfalls

## How to Debug NUMA Issues

### Kernel Interfaces
- `/proc/zoneinfo` — Shows memory zones and node assignments
- `/proc/meminfo` — Overall memory usage
- `/sys/devices/system/node/` — Lists nodes, CPUs, and memory

### Kernel Boot Logs
- `dmesg | grep -i numa` — Shows NUMA topology detected at boot

### User-space Tools
- `numactl --hardware` — Shows detected NUMA nodes and CPUs
- `lscpu` — Shows CPU/node mapping
- `hwloc-ls` — Visualizes hardware topology

---

## Tuning NUMA Behavior

### Kernel Boot Parameters
- `numa=off` — Disable NUMA
- `numa_balancing=off` — Disable automatic NUMA balancing

### User-space Tools
- `numactl`, `taskset`, `hwloc`

### Performance Counters
- `perf`, `toplev`, `pcm-memory`

---

## Common Pitfalls

- Memory allocation from remote nodes (performance hit)
- Task migration thrashing (tasks bouncing between nodes)
- Incorrect firmware tables (ACPI/DT) causing wrong topology

---

## Table: NUMA Debugging Tools

| Tool/Interface           | Purpose                        | Example Usage                  |
|-------------------------|--------------------------------|-------------------------------|
| `/proc/zoneinfo`        | Show memory zones/nodes        | `cat /proc/zoneinfo`          |
| `numactl --hardware`    | Show NUMA nodes/CPUs           | `numactl --hardware`          |
| `dmesg`                 | Boot log NUMA info             | `dmesg | grep -i numa`         |
| `hwloc-ls`              | Visualize topology             | `hwloc-ls`                    |
| `perf`                  | Measure memory access latency  | `perf stat -e ...`            |

---

**Interview Tip:**
Be ready to describe how you would debug a NUMA performance issue and what tools you would use.

# Scheduler Topology and Domain Hierarchy

## 1. Design Intent
Scheduler topology models hardware locality and shared resources so balancing and placement decisions align with cache, NUMA, SMT, and capacity structure.

## 2. Primary Code Regions
- `kernel/sched/topology.c`
- topology interactions in `kernel/sched/fair.c`

Key functions:
- domain build/rebuild paths
- initialization and hotplug update routines
- energy-domain integration hooks

## 3. Domain and Group Model
A sched_domain tree spans CPU scopes (core, cluster, package, NUMA levels). Each domain contains groups used for load comparison and migration targets.

## 4. Rebuild Triggers
Topology/domain rebuilds can occur due to:
- CPU hotplug.
- cpuset and partitioning changes.
- energy model/topology updates.

These operations require careful synchronization and RCU-safe pointer replacement.

## 5. Locality and Cost Awareness
Domain boundaries encode balancing cost assumptions:
- near domains prioritize low migration cost and cache locality.
- far domains permit broader balancing when local correction is insufficient.

## 6. Asymmetry and Capacity
Asymmetric CPUs require domain metadata and capacity signals to avoid naive equal-load balancing that hurts performance.

## 7. Failure Modes
1. Stale or incorrect domains after dynamic topology changes.
2. Over-balancing across expensive boundaries.
3. NUMA-unaware migration increasing memory access latency.
4. Inconsistent domain assumptions with cpuset partitioning.

## 8. Validation Strategy
- verify domain tree reflects expected hardware levels.
- stress hotplug while tracing balancing and migration correctness.
- compare locality-sensitive benchmarks before/after domain policy changes.
- confirm cpuset partition behavior remains deterministic.

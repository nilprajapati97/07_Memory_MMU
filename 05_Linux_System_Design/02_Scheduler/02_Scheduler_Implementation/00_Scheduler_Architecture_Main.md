# Linux Scheduler Implementation Design (Main)

## 1. Audience and Intent
This document targets experienced Linux kernel engineers. It focuses on implementation-level behavior in current kernel scheduler code, with emphasis on invariants, concurrency, call paths, and tuning tradeoffs rather than introductory scheduling theory.

## 2. Scope
- Scheduling classes and class arbitration in the core loop.
- Per-CPU runqueue model and synchronization.
- End-to-end execution paths: wakeup, enqueue/dequeue, pick-next, and context-switch.
- SMP load balancing, topology, and capacity awareness.
- Cross-subsystem integrations: cpufreq schedutil, cgroup CPU control, uclamp, tracing/debugging.

Out of scope:
- Beginner OS background.
- Vendor-private policy plugins.
- Benchmark recipes specific to one platform.

## 3. Scheduler Architecture Overview
The scheduler is organized around per-CPU runqueues (`struct rq`) and pluggable `sched_class` implementations. Core arbitration is class-ordered and lock-centric.

Class order (highest to lowest effective precedence in pick path):
1. stop
2. deadline
3. rt
4. fair
5. idle
6. ext (when enabled/configured)

The central scheduling loop is `__schedule()` in `kernel/sched/core.c`, which:
1. Stabilizes state under rq lock and updates clocks.
2. Selects the next task through class dispatch (`pick_next_task()` path).
3. Performs context-switch (`context_switch()` + `switch_to()`).
4. Finalizes previous task accounting and lock handoff.

## 4. Core Data Structures
### 4.1 `struct rq` (per CPU)
Key responsibilities:
- Owns runnable state for each class (`cfs`, `rt`, `dl`, plus optional ext state).
- Maintains scheduler clocks and capacity/utilization snapshots.
- Serves as the primary serialization domain through `rq->lock`.

### 4.2 `struct sched_class`
Class vtable that defines behavior for:
- `enqueue_task`, `dequeue_task`
- `pick_next_task`, `put_prev_task`, `set_next_task`
- `check_preempt_curr`, `task_tick`, migration hooks

### 4.3 Class-specific queues
- CFS: `struct cfs_rq` with RB-tree timeline keyed by vruntime/EEVDF state.
- RT: priority array and push/pull machinery.
- DL: deadline-ordered RB-tree and bandwidth enforcement.

## 5. End-to-End Critical Flows
### 5.1 Wakeup flow
Primary entry: `try_to_wake_up()`.
High-level phases:
1. Validate task state and lock ordering (`p->pi_lock`, target rq lock semantics).
2. CPU selection (`select_task_rq*`) based on policy class, affinity, capacity/topology.
3. Activation (`ttwu_do_activate`) and enqueue into class queue.
4. Preemption test (`check_preempt_curr`) and resched signaling.

### 5.2 Enqueue / Dequeue abstraction
Generic wrappers (`enqueue_task`, `dequeue_task`) dispatch into class logic. This keeps common state management centralized while allowing policy specialization.

### 5.3 Pick-next and switch
`pick_next_task()` iterates classes in precedence order and returns first runnable candidate from class-specific picker.
`context_switch()` handles mm switch and architecture switch primitives with strict lock sequencing.

## 6. Invariants that Must Hold
1. A runnable task is represented in exactly one valid runqueue context at a time.
2. Runqueue lock ordering is globally consistent (including double-rq lock operations).
3. Class transitions and migration preserve task state machine consistency.
4. Scheduler clock and utilization signals remain monotonic enough for policy assumptions.
5. `rq->curr`/donor semantics are coherent with configuration (including proxy execution variants).

## 7. Concurrency and Locking Model
Primary lock hierarchy:
1. `rq->lock` (raw spinlock, per CPU)
2. `p->pi_lock` (task lock for PI and wake/migration coordination)
3. double rq lock in deterministic address order

RCU is used for topology/domain pointers and rebuild safety (`sched_domain` and related structures), with explicit synchronization during topology reconfiguration.

## 8. SMP, Topology, and Capacity Awareness
The scheduler computes balancing and placement decisions from:
- sched_domain hierarchy.
- asym capacity and architecture-provided capacity scaling.
- energy model for EAS (where present).
- NOHZ state and remote balancing constraints.

The placement decision is not purely load based; modern code uses utilization, capacity headroom, and energy constraints.

## 9. Cross-Subsystem Integrations
### 9.1 cpufreq schedutil
Scheduler utilization signals drive DVFS frequency selection through schedutil update hooks.

### 9.2 cgroup CPU controller
CPU bandwidth/weight constraints are represented through task groups and hierarchical runqueues.

### 9.3 uclamp
Per-task and cgroup utilization clamp bounds influence placement and frequency behavior.

### 9.4 PSI and tracing
Scheduler state feeds pressure and observability interfaces for production diagnosis.

## 10. Observability and Diagnostics
Primary surfaces:
- Tracepoints in scheduler core/fair/pelt paths.
- `/proc/<pid>/sched`, `/proc/<pid>/schedstat`.
- debugfs scheduler dumps for runqueue and entity state.
- Sysctl and cgroup knobs for policy tuning.

Production workflow should correlate scheduler tracepoints with CPU frequency traces, IRQ pressure, and memory locality events before making policy changes.

## 11. Performance and Failure-Mode Themes
- Migration storms can reduce throughput despite improved instantaneous balance.
- Wrong topology or energy model assumptions can degrade heterogeneous platforms.
- PI/lock contention and wakeup races can produce latency cliffs under mixed RT/fair workloads.
- Utilization lag can under-drive frequency during bursty phases.

## 12. Document Map
- `01_CFS_EEVDF_Internals.md`
- `02_RT_Scheduler_Internals.md`
- `03_Deadline_Scheduler_Internals.md`
- `04_Load_Balancing_and_NOHZ.md`
- `05_CPU_Placement_EAS_and_Capacity.md`
- `06_PELT_and_Utilization_Signals.md`
- `07_Schedutil_DVFS_Integration.md`
- `08_Topology_and_Sched_Domains.md`
- `09_Sched_Ext_BPF_Design.md`
- `10_Core_Scheduling_SMT.md`
- `11_Concurrency_Locking_and_Migration.md`
- `12_uclamp_and_CPU_Controller.md`

## 13. Engineering Checklist for Changes
Before merging scheduler changes, validate:
1. Lock ordering and migration state transitions under stress.
2. Cross-class starvation behavior.
3. Capacity-aware placement on asymmetric CPUs.
4. Utilization-to-frequency response during burst and sustained load.
5. cgroup and uclamp correctness for constrained workloads.
6. Trace-based evidence that claimed wins are not instrumentation artifacts.

---

# 14. Custom Scheduler Implementation Plan

## 14.1 Implementation Overview
This section begins the implementation of a custom scheduler, following the architecture and design described above. The implementation will be modular, with clear separation between core logic, scheduling classes, runqueue management, and observability hooks.

## 14.2 Core Data Structures
- **struct my_sched_task**: Represents a schedulable entity (task/thread), including priority, deadline, affinity, state, and statistics.
- **struct my_sched_class**: Function table (vtable) for scheduling class operations (enqueue, dequeue, pick_next, etc.).
- **struct my_runqueue**: Per-core runqueue, holding class-specific queues and core-local state.
- **struct my_core_state**: Tracks current task, load, and capabilities for each CPU core.

## 14.3 Module Breakdown
- **Task Lifecycle Management**: Creation, state transitions, and destruction of tasks.
- **Scheduling Class Logic**: Separate modules for RT, interactive, and batch scheduling policies.
- **Runqueue Management**: Enqueue, dequeue, and pick-next logic for each class and core.
- **Core Assignment & Load Balancing**: Affinity, NUMA, and load balancing algorithms.
- **Preemption & Context Switching**: Mechanisms for switching tasks and handling preemption.
- **Metrics & Observability**: Hooks for collecting and exposing scheduler statistics.

## 14.4 Next Steps
- Define the core data structures in detail.
- Implement the task lifecycle and runqueue management modules.
- Develop scheduling class logic for RT, interactive, and batch tasks.
- Integrate core assignment, load balancing, and observability features.

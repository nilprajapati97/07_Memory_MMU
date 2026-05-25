# CFS / EEVDF Implementation Deep Dive

## 1. Design Intent
CFS provides proportional CPU sharing for normal tasks. Modern kernels implement EEVDF-style selection semantics on top of fair scheduling fundamentals, using virtual time accounting and entity eligibility/deadline concepts.

## 2. Primary Code Regions
- `kernel/sched/fair.c`
- `kernel/sched/sched.h`
- `kernel/sched/pelt.c`

Key functions:
- `enqueue_task_fair`, `dequeue_task_fair`
- `pick_next_task_fair`
- `update_curr`, `task_tick_fair`
- `select_task_rq_fair`
- balancing helpers such as `load_balance`, `idle_balance`

## 3. Core Structures
### 3.1 `struct cfs_rq`
Holds CFS runnable entities for one hierarchy level on one CPU:
- RB-tree timeline cache.
- aggregate load/util/runnable signals.
- vruntime baseline (`min_vruntime`).
- pointers to current and next entities.

### 3.2 `struct sched_entity`
Per-task or group entity state:
- virtual runtime progression.
- load weight and averaged utilization.
- hierarchical links for group scheduling.

## 4. Enqueue / Dequeue Path
On activation/wakeup:
1. `enqueue_task_fair` updates clocks and average signals.
2. Entity is inserted into CFS timeline with eligibility semantics.
3. Hierarchical propagation updates parent entities and task group state.

On sleep/block/migration:
1. `dequeue_task_fair` removes entity and adjusts aggregates.
2. Hierarchy is updated upward to preserve proportional accounting.

Correctness depends on clock update ordering and avoiding stale entity position in RB-tree.

## 5. Pick-Next Mechanics
`pick_next_task_fair`:
1. Ensures previous entity is accounted (`put_prev_entity` path).
2. Chooses best eligible entity from timeline.
3. Installs selected entity (`set_next_entity`) and updates run state.

Engineering focus:
- selection fairness over long windows.
- low-latency response for newly woken interactive tasks.
- bounded overhead with deep cgroup hierarchies.

## 6. Virtual Time and Fairness
Virtual runtime advances according to actual runtime weighted by nice/weight. Heavier-weight entities accumulate vruntime more slowly, yielding more CPU share.

A practical fairness check in production is long-window ratio alignment:
- If entities A and B have weights `wA` and `wB`, expected runtime ratio tends toward `wA:wB` subject to wake/sleep and topology effects.

## 7. Hierarchical Fair Group Scheduling
With task groups, each level has its own `cfs_rq`. A task competes first in its local group, then group entities compete at higher levels.

Implications:
- Deep hierarchy adds accounting and traversal cost.
- Misconfigured group weights can create perceived unfairness even when algorithmic invariants hold.

## 8. Latency, Granularity, and Preemption
Important tuning dimensions:
- base granularity/slice behavior.
- wakeup preemption thresholds.
- migration cost hysteresis.

Too aggressive preemption improves tail latency but can increase context-switch and cache miss cost.

## 9. CFS and Utilization Signals
CFS entity and rq utilization (`sched_avg`) feeds:
- capacity-aware placement.
- cpufreq schedutil DVFS decisions.
- overutilization and energy-aware signals.

Signal lag is intrinsic to averaging and should be compensated using util-est and clamp hints where appropriate.

## 10. Common Failure Patterns
1. Over-migration causing LLC churn.
2. Hierarchy imbalance from extreme cgroup weight settings.
3. Frequency under-drive on bursty interactive workloads.
4. Latency regressions from incorrect wakeup preemption tuning.

## 11. Validation Strategy
- Trace enqueue/dequeue and pick decisions under mixed CPU-bound/IO-bound load.
- Validate fairness ratios at steady state.
- Compare tail latency before/after tuning with cache and migration counters.
- Audit cgroup hierarchy effects with per-group runtime statistics.

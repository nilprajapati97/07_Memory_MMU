# Load Balancing and NOHZ Internals

## 1. Design Intent
Load balancing redistributes runnable work across CPUs to improve throughput and latency while respecting affinity, topology, capacity, and migration cost constraints. NOHZ support allows balancing decisions with reduced periodic ticks.

## 2. Primary Code Regions
- `kernel/sched/fair.c`
- `kernel/sched/topology.c`
- `kernel/sched/core.c`

Key functions:
- `load_balance`, `idle_balance`, `load_balance_newidle`
- `find_busiest_group`, `find_busiest_queue`
- `can_migrate_task`, task move helpers
- `nohz_idle_balance` and related NOHZ paths

## 3. Scheduler Domain Walk
Balancing walks sched_domain levels from near to far topology scopes. At each level:
1. Evaluate imbalance metrics.
2. Identify busiest group/queue.
3. Select migratable tasks with affinity and policy checks.
4. Move tasks with lock-safe enqueue/dequeue sequences.

## 4. Migration Constraints
A task can be migrated only if all checks pass:
- cpumask affinity allows destination.
- policy/class constraints remain valid.
- migration-disabled or pinned states are respected.
- balancing does not violate lock or state invariants.

## 5. Active vs Passive Balancing
- Passive balancing occurs during periodic or idle triggers.
- Active balancing can invoke stopper-assisted movement for stubborn imbalance conditions.

Active balancing is expensive and should remain exceptional.

## 6. NOHZ Balance Model
In NOHZ modes, idle CPUs may suppress ticks. The scheduler uses remote triggering and deferred balancing to maintain global health without full periodic accounting on every CPU.

Engineering tradeoff:
- lower overhead and power for idle CPUs versus delayed balancing visibility.

## 7. Pathological Behaviors
1. Migration storms from over-aggressive imbalance thresholds.
2. Cache-thrash due to cross-LLC bouncing.
3. Delayed corrections when NOHZ state hides imbalance too long.
4. Affinity fragmentation causing local overload despite idle remote CPUs.

## 8. Tuning and Validation
- Validate with mixed short and long-running tasks.
- Correlate migrations with LLC miss and context-switch costs.
- Inspect NOHZ balance traces under partial-idle clusters.
- Compare throughput gains against tail latency regressions.

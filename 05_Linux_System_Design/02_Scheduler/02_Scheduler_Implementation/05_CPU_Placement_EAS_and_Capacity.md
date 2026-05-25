# CPU Placement, Capacity Awareness, and EAS

## 1. Design Intent
CPU placement chooses destination CPU for wakeups and migrations using utilization, capacity, topology locality, and energy costs where available.

## 2. Primary Code Regions
- `kernel/sched/fair.c`
- `kernel/sched/topology.c`
- architecture capacity scaling hooks

Key functions:
- `select_task_rq_fair`
- `select_idle_sibling`
- `find_energy_efficient_cpu`
- capacity and overutilization checks

## 3. Placement Pipeline
Typical CFS wakeup placement:
1. Start with previous/affine candidate.
2. Apply affinity and cpuset constraints.
3. Prefer local idle siblings where beneficial.
4. If EAS path is active, evaluate candidate CPUs by estimated energy cost.
5. Choose CPU with acceptable performance and energy tradeoff.

## 4. Capacity Model
Utilization and capacity are normalized (common scale) for fair comparisons. Asymmetric systems rely on architecture-provided capacity values.

Key risk:
- incorrect capacity inputs produce systematically poor placement.

## 5. EAS Decision Logic
EAS uses performance domain information and energy model data to estimate placement cost. It is most effective when EM and topology accurately represent hardware behavior.

## 6. Idle-Preference vs Throughput
Aggressive idle preference can reduce wake latency but may hurt throughput if it increases migration or disrupts cache locality. Placement heuristics balance these competing outcomes.

## 7. Heterogeneous Systems
big.LITTLE-like systems require:
- accurate capacity gradients.
- task utilization signals with bounded lag.
- clamp policies to avoid underpowered placement for latency-sensitive workloads.

## 8. Operational Pitfalls
1. EM mismatch causing energy regressions.
2. Overusing high-capacity cores for low-value background tasks.
3. Bursty tasks misclassified due to delayed utilization signal convergence.
4. Affinity constraints neutralizing EAS benefits.

## 9. Validation Strategy
- Compare placement decisions against expected capacity classes.
- Measure performance-per-watt before/after policy changes.
- Audit wakeup latency and migration overhead jointly.
- Test constrained affinity and cgroup-isolated scenarios.

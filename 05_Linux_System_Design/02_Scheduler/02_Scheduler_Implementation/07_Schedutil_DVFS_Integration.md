# Schedutil and DVFS Integration

## 1. Design Intent
Schedutil couples scheduler utilization with cpufreq frequency decisions, reducing control-loop latency relative to purely sampling-based governors.

## 2. Primary Code Regions
- `kernel/sched/cpufreq_schedutil.c`
- `kernel/sched/cpufreq.c`
- scheduler signal producers in fair/pelt paths

Key symbols:
- update hooks (`sugov_update_*` family)
- target frequency computation (`get_next_freq` style logic)
- shared-policy handling paths

## 3. Signal-to-Frequency Pipeline
1. Scheduler updates utilization for rq/entity.
2. Schedutil callback receives updated demand context.
3. Governor computes target frequency with policy constraints.
4. cpufreq driver applies transition subject to hardware limits.

## 4. Shared vs Per-CPU Policies
On shared frequency domains, one decision serves multiple CPUs. This can improve efficiency but complicates fairness between mixed-demand CPUs.

## 5. Interactions with Placement and Clamps
Placement and uclamp influence schedutil inputs:
- concentrated high-util tasks can drive rapid boosts.
- clamp minimums protect latency-sensitive tasks from deep downclock.
- clamp maximums cap energy usage for background workloads.

## 6. Failure Modes
1. Under-frequency during burst entry due to signal lag.
2. Over-frequency from stale high util after workload phase shifts.
3. Thrashing from frequent target changes on noisy workloads.
4. Cross-CPU interference on shared policy domains.

## 7. Engineering Guidance
- treat scheduler and cpufreq as one control system.
- tune with trace evidence from both scheduler and frequency paths.
- avoid policy changes that improve one benchmark but destabilize mixed workloads.

## 8. Validation Strategy
- correlate util, requested freq, and applied freq timelines.
- test interactive bursts, sustained compute, and mixed background pressure.
- validate thermally constrained behavior.
- verify behavior under cgroup/uclamp policy boundaries.

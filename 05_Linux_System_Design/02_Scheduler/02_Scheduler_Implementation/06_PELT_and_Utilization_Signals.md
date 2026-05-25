# PELT and Utilization Signal Pipeline

## 1. Design Intent
PELT provides smoothed load/utilization signals for scheduling and frequency decisions. It trades immediate responsiveness for stability and noise resistance.

## 2. Primary Code Regions
- `kernel/sched/pelt.c`
- `kernel/sched/pelt.h`
- signal consumers in `kernel/sched/fair.c` and `kernel/sched/cpufreq_schedutil.c`

Key functions:
- `___update_load_avg`
- entity/rq update helpers
- rq PELT clock update routines

## 3. Signal Components
Typical tracked dimensions:
- load contribution.
- runnable contribution.
- utilization contribution.

Signals exist per-entity and per-runqueue and are aggregated through hierarchy.

## 4. Update Triggers
PELT updates happen on key scheduler events:
- enqueue/dequeue.
- tick/accounting updates.
- context switch related paths.

Clock correctness is essential; stale clocks distort decay and utilization estimates.

## 5. Consumer Paths
PELT outputs directly impact:
- CFS balancing and placement decisions.
- overutilization detection.
- schedutil DVFS target frequency.
- uclamp and energy-aware policy behavior.

## 6. Responsiveness vs Stability Tradeoff
Faster response can track bursts better but increases jitter and oscillation risk. Slower response smooths noise but can underreact to sudden demand changes.

Practical mitigation uses util-est and clamp hints to improve burst response without discarding smoothing properties.

## 7. Common Failure Patterns
1. Frequency lag on short bursts causing user-visible latency.
2. Oscillation when combined with aggressive governors or placement changes.
3. Misinterpreting averaged util as instantaneous demand in diagnostics.
4. Hierarchical aggregation surprises under complex cgroup trees.

## 8. Validation Strategy
- Overlay PELT signal evolution with actual runtime and freq transitions.
- Confirm monotonic/decay expectations during sleep/wakeup cycles.
- Stress-test mixed bursty and CPU-bound workloads.
- Validate cgroup hierarchy effects on group util propagation.

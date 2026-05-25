# Core Scheduling for SMT Systems

## 1. Design Intent
Core scheduling coordinates runnable tasks across SMT siblings to enforce security and performance constraints, especially where co-scheduling rules are required.

## 2. Primary Code Regions
- `kernel/sched/core_sched.c`
- integration hooks in `kernel/sched/core.c`

## 3. Model
Instead of treating SMT threads as fully independent, core scheduling may enforce compatibility relationships among co-running tasks on sibling logical CPUs.

## 4. Selection Implications
Task selection must consider:
- sibling occupancy and compatibility/cookie constraints.
- potential force-idle states when no compatible partner exists.
- throughput loss versus isolation/security goals.

## 5. Interactions with Other Classes
Core scheduling logic intersects class pick decisions and may alter effective runnable opportunities on SMT siblings.

## 6. Operational Tradeoffs
- stronger isolation can reduce SMT utilization.
- strict compatibility can increase idle time despite runnable tasks.
- performance impact depends on workload composition and sibling pairing quality.

## 7. Failure Modes
1. Unexpected throughput drop from frequent force-idle periods.
2. Misdiagnosis of idle CPUs when incompatibility is the true blocker.
3. Policy conflicts with affinity/cpuset restrictions.
4. Insufficient observability for compatibility mismatches.

## 8. Validation Strategy
- benchmark with and without core scheduling on SMT-heavy workloads.
- inspect sibling utilization versus runnable queue depth.
- verify intended isolation semantics under adversarial co-run scenarios.
- ensure production telemetry distinguishes true idle from forced idle.

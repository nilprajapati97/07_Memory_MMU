# Utilization Clamping and cgroup CPU Controller

## 1. Design Intent
uclamp and cgroup CPU controls provide policy constraints that shape scheduler placement and frequency behavior beyond raw runnable demand.

## 2. Primary Code Regions
- `kernel/sched/syscalls.c`
- `kernel/sched/fair.c`
- `kernel/sched/core.c`
- cgroup CPU accounting/control paths

## 3. uclamp Semantics
uclamp defines minimum and maximum utilization bounds:
- min clamp protects latency-sensitive tasks from under-provisioning.
- max clamp caps aggressive boost behavior for background or power-sensitive work.

Aggregation occurs across tasks/runqueues with class-aware application rules.

## 4. cgroup CPU Controller Model
cgroup v2 CPU controller exposes weight and bandwidth knobs (such as `cpu.weight` and `cpu.max`) implemented through task-group hierarchical scheduling.

This affects:
- CFS entity competition at group boundaries.
- effective runtime distribution across services.
- interaction with per-task policy and affinity.

## 5. Interaction Matrix
1. uclamp min + high-priority interactive workload: improves responsiveness, increases energy draw.
2. uclamp max + batch services: controls power, can reduce throughput.
3. tight `cpu.max`: enforces quota, may increase latency and induce throttling artifacts.
4. extreme group weights: can produce surprising fairness perception while still being algorithmically correct.

## 6. Operational Pitfalls
1. Treating clamps as absolute performance guarantees.
2. Misconfigured cgroup hierarchy causing priority inversion at service level.
3. Confusing quota throttling effects with scheduler regression.
4. Ignoring shared-domain frequency interactions when tuning clamps.

## 7. Validation Strategy
- validate service-level SLOs with realistic mixed workloads.
- inspect per-cgroup runtime and throttling statistics.
- correlate clamp settings with actual frequency and placement behavior.
- test failure isolation when one group becomes CPU-saturated.

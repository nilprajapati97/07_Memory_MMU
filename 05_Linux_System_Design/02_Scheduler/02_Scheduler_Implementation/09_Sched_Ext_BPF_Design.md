# sched_ext (BPF Scheduler Class) Design

## 1. Design Intent
sched_ext enables programmable scheduling policy in BPF while preserving kernel safety and fallback behavior.

## 2. Primary Code Regions
- `kernel/sched/ext.c`
- `kernel/sched/ext_idle.c`
- generic scheduler hooks in core paths

## 3. Architectural Model
The ext class provides:
- BPF-controlled dispatch logic.
- dispatch queue abstractions.
- kernel-managed safety mechanisms and fallback transitions.

## 4. Integration Points
Core scheduler invokes ext hooks through class interface for enqueue/select/dispatch transitions when ext is active.

Key engineering principle: extensibility must not compromise scheduler liveness.

## 5. Safety and Recovery
Expected protections include:
- stall/watchdog detection for non-progressing policy logic.
- bypass/fallback to built-in scheduling behavior on failure.
- bounded interaction contracts between kernel and BPF logic.

## 6. Performance Considerations
Programmability adds overhead and complexity:
- dispatch queue management cost.
- verification and runtime safety checks.
- policy-level bugs that can induce starvation if safeguards are weak.

## 7. Failure Modes
1. Starvation due to incorrect BPF dispatch policy.
2. Excessive overhead from high-frequency control decisions.
3. Unexpected interactions with cgroup or affinity constraints.
4. Operational instability from immature policy rollout.

## 8. Validation Strategy
- begin with conservative policy prototypes.
- use stress and starvation tests with watchdog monitoring.
- validate fallback behavior intentionally (fault injection style).
- compare against baseline CFS/RT behavior on representative workloads.

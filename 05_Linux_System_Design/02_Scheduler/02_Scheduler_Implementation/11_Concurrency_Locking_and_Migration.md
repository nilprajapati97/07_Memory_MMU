# Concurrency, Locking, and Task Migration

## 1. Design Intent
Scheduler correctness depends on strict lock ordering, coherent task state transitions, and race-safe migration logic across CPUs.

## 2. Primary Code Regions
- `kernel/sched/core.c`
- `kernel/sched/fair.c`
- `kernel/sched/rt.c`
- `kernel/sched/sched.h`

## 3. Locking Foundations
Primary locks:
- per-CPU `rq->lock` for runqueue state.
- task `p->pi_lock` for PI-sensitive transitions.

Lock-order discipline must be deterministic, especially in two-rq operations.

## 4. Migration State and Correctness
Migration involves enqueue/dequeue and CPU ownership transitions under coordinated locks. The task must never be visible as runnable in multiple queues simultaneously.

Critical risk areas:
- wakeup versus migration races.
- migration-disabled task handling.
- affinity changes concurrent with balancing.

## 5. Double-Runqueue Operations
Cross-CPU moves can require locking both source and destination runqueues. Ordering is address-based to avoid deadlock.

## 6. Memory Ordering and Barriers
Scheduler state transitions rely on precise atomic/locking semantics and barrier placement. Weak ordering bugs can appear only under stress and high core counts.

## 7. CPU Hotplug and RCU
Domain and topology pointers are RCU-protected; rebuild and migration behavior during hotplug must guarantee no stale pointer dereference or invalid CPU target decisions.

## 8. Common Pitfalls
1. Deadlock from lock-order inversion in rare migration paths.
2. Double enqueue/dequeue from race windows in wakeup state handling.
3. Long lock hold times causing latency spikes.
4. Hidden contention on heavily loaded runqueues.

## 9. Validation Strategy
- lockdep-enabled stress with aggressive affinity churn.
- migration-heavy workloads on large CPU counts.
- hotplug stress with balancing active.
- tracing of wakeup/migration/cswitch timeline to confirm state consistency.

# Deadline Scheduler Implementation Deep Dive

## 1. Design Intent
`SCHED_DEADLINE` provides EDF-based scheduling with CBS-style bandwidth accounting to offer temporal guarantees for periodic or constrained-latency workloads.

## 2. Primary Code Regions
- `kernel/sched/deadline.c`
- `kernel/sched/core.c`
- `kernel/sched/sched.h`

Key symbols:
- `enqueue_task_dl`, `dequeue_task_dl`
- `pick_next_task_dl`
- replenishment and throttle paths
- pushable deadline task handling

## 3. Core Model
Each DL entity is parameterized by runtime/deadline/period attributes. The class selects earliest eligible deadline first while enforcing reserved bandwidth constraints.

## 4. Queue and Selection
`struct dl_rq` maintains deadline-ordered entities in RB-tree form. Selection picks earliest absolute deadline task that is runnable and not throttled.

## 5. Runtime Accounting and Replenishment
CBS principles:
1. Runtime is consumed during execution.
2. On depletion, task may be throttled until replenishment conditions.
3. Replenishment restores budget according to period/deadline policy.

Correct accounting is essential to avoid temporal contract violations.

## 6. SMP and Migration
DL class includes migration logic for balancing and deadline feasibility. Cross-CPU movement must preserve admission and runtime invariants.

## 7. Admission and Overload Behavior
Feasibility depends on aggregate utilization and system topology constraints. Overload can trigger misses even if individual tasks are correctly configured.

Engineering guidance:
- isolate critical DL workloads where possible.
- minimize interference from unrelated RT/fair traffic.

## 8. Interactions with Other Classes
DL preempts RT/fair/idle classes but still contends with stop-class activities and system-level constraints.

## 9. Operational Pitfalls
1. Incorrect runtime/period values causing chronic throttling.
2. Assuming deadline guarantees without accounting for IRQ and lock interference.
3. Oversubscription on asymmetric CPU systems.
4. Migration-induced cache penalties breaking practical latency targets.

## 10. Validation Strategy
- Verify budget consumption and replenishment trace timelines.
- Measure miss ratio under controlled background interference.
- Confirm admission assumptions against real topology and isolation settings.
- Audit interaction with cpuset/affinity constraints.

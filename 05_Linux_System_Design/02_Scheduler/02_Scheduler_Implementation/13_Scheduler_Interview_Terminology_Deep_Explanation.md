# Linux Scheduler Interview Terminology (Deep Explanation, No Code)

## 1. Purpose
This document is designed for interview preparation at senior Linux kernel engineering level. It explains scheduler terminology with implementation-oriented depth, but without code snippets.

## 2. How to Use in Interviews
1. Start from architecture terms (runqueue, scheduling class, preemption).
2. Move to decision terms (placement, balancing, capacity, energy).
3. Close with correctness terms (locking, race windows, starvation, inversion).
4. For every term, explain both definition and engineering tradeoff.

## 3. Core Architecture Terms
### 3.1 Scheduler
The scheduler is the kernel subsystem that decides which runnable task executes on each CPU at a given instant. In modern Linux, this is not one single algorithm but a coordinated framework of scheduling classes, per-CPU state, and balancing logic.

### 3.2 Scheduling Class
A scheduling class is a policy family with its own queueing and pick rules. Class ordering defines which policy dominates when multiple classes have runnable tasks.

Interview depth point:
Class ordering is a correctness and latency contract. It is not just an implementation detail, because it defines system behavior under contention.

### 3.3 Runqueue
A runqueue is the CPU-local representation of runnable work and scheduling metadata. Think of it as the decision context for one CPU, including policy-specific subqueues and timing signals.

Interview depth point:
Per-CPU runqueues reduce global lock contention but create migration complexity and balancing overhead.

### 3.4 Context Switch
A context switch is the transition from one running task to another. It includes register state transfer, memory context considerations, and scheduling/accounting transitions.

Interview depth point:
A context switch is not free. Excessive switching damages cache locality and can dominate latency under fine-grained workloads.

## 4. Fair Scheduling Terminology
### 4.1 Fairness
Fairness means CPU time converges to configured proportional share over a long enough window, not necessarily equal instantaneous execution.

### 4.2 Virtual Runtime
Virtual runtime is normalized execution progress used to compare entities fairly despite different weights. It lets the scheduler reason in a policy domain where heavier weights advance more slowly.

Interview depth point:
Virtual time provides deterministic ordering semantics while allowing weighted fairness.

### 4.3 EEVDF
EEVDF (Earliest Eligible Virtual Deadline First) is the modern conceptual framework in fair scheduling selection. It improves latency/fairness behavior by combining eligibility and virtual deadline ideas.

Interview depth point:
Discuss EEVDF as evolution of fairness decision quality, not as a complete replacement for all legacy CFS concepts.

### 4.4 Granularity
Granularity is the practical minimum time chunk before re-evaluation. It prevents pathological over-preemption while controlling interactive latency.

Tradeoff:
- Lower granularity: better responsiveness, higher switching overhead.
- Higher granularity: better throughput/cache locality, potentially worse tail latency.

## 5. Real-Time and Deadline Terms
### 5.1 Real-Time Priority
Real-time priority enforces strict ordering where higher-priority RT tasks can preempt lower classes quickly. It optimizes determinism, not global fairness.

### 5.2 FIFO vs Round-Robin (RR)
- FIFO: runs until block/yield or higher-priority preemption.
- RR: equal-priority peers share CPU using a quantum.

Interview depth point:
The key difference is peer-level fairness within the same RT priority.

### 5.3 Deadline Scheduling
Deadline scheduling models execution as runtime/deadline/period contracts and uses earliest-deadline-first principles with bandwidth control.

### 5.4 Admission Control
Admission control is feasibility reasoning before accepting real-time/deadline workload demand. Without it, deadline misses become systemic rather than occasional.

## 6. Placement and Balancing Terms
### 6.1 CPU Affinity
Affinity is a placement constraint indicating allowed CPUs for a task. It can preserve locality but may block optimal balance.

### 6.2 Load Balancing
Load balancing migrates runnable work between CPUs to reduce hotspots and improve utilization.

Interview depth point:
Good balancing is constrained optimization, not equalization. It must respect affinity, topology, class rules, and migration cost.

### 6.3 Active Balance vs Passive Balance
- Passive balance: periodic or idle-triggered correction.
- Active balance: stronger migration actions when passive methods cannot fix imbalance.

### 6.4 Migration Cost
Migration cost is the performance penalty of moving execution to another CPU, primarily from cache and locality disruption.

### 6.5 Wakeup Placement
Wakeup placement is the initial CPU choice when a sleeping task becomes runnable. It strongly affects latency and cache behavior.

## 7. Topology and Capacity Terms
### 7.1 Scheduler Domain
A scheduler domain is a topology layer used for balancing decisions (for example, sibling, package, NUMA scope).

### 7.2 Scheduling Group
A scheduling group is a set of CPUs compared as a unit during load-balance decisions.

### 7.3 Capacity Awareness
Capacity awareness means scheduler decisions account for unequal CPU compute capability. On heterogeneous systems this is mandatory for correctness-like behavior in performance terms.

### 7.4 Asymmetric CPU System
An asymmetric system has CPUs with different performance/efficiency profiles. Equal load distribution can be harmful in this model.

### 7.5 EAS (Energy Aware Scheduling)
EAS chooses placements using energy-cost modeling plus utilization/capacity constraints.

Interview depth point:
EAS quality depends on model fidelity. If model is inaccurate, policy appears correct but outcomes regress.

## 8. Utilization Signal Terms
### 8.1 Utilization
Utilization is scheduler-estimated compute demand, not direct CPU percentage in user-facing tools.

### 8.2 PELT
PELT is an exponentially decayed signal model used to track load/runnable/utilization trends over time.

Interview depth point:
PELT is intentionally smooth, so instantaneous spikes and averaged demand diverge by design.

### 8.3 util-est
util-est is a mechanism to improve responsiveness for bursty behavior where pure averaged signals react too slowly.

### 8.4 Overutilization
Overutilization indicates demand approaching or exceeding effective capacity thresholds, used for placement/frequency decision inflection.

## 9. Frequency and Power Terms
### 9.1 DVFS
Dynamic voltage and frequency scaling adjusts CPU operating points to match workload demand and power/thermal goals.

### 9.2 schedutil
schedutil is the cpufreq governor path tightly coupled with scheduler utilization signals.

Interview depth point:
Treat scheduler plus governor as one control loop; tuning one side without the other often causes oscillation or sluggishness.

### 9.3 Thermal Throttling
Thermal throttling limits achievable frequency independent of scheduler intent. Many apparent scheduler regressions are actually thermal budget artifacts.

## 10. Isolation and Security Terms
### 10.1 Core Scheduling
Core scheduling coordinates SMT sibling co-run behavior to enforce compatibility/isolation constraints.

### 10.2 Force Idle
Force idle is the condition where a sibling thread remains idle because no compatible co-schedule candidate exists.

### 10.3 CPU Isolation
CPU isolation reserves CPUs from general balancing/noise to protect latency-critical workloads.

## 11. Control and Policy Terms
### 11.1 cgroup CPU Weight
Weight expresses proportional share between groups in contention.

### 11.2 cgroup CPU Max (Quota)
CPU max is a hard bandwidth cap across a period. It enforces isolation but can create throttling-induced latency.

### 11.3 uclamp Min
uclamp min sets a lower utilization bound to protect responsiveness and frequency floor behavior.

### 11.4 uclamp Max
uclamp max sets an upper bound to constrain power/performance aggressiveness.

Interview depth point:
Clamps shape scheduler and frequency behavior; they do not bypass fundamental capacity, topology, or thermal limits.

## 12. Correctness and Concurrency Terms
### 12.1 Preemption
Preemption is interruption of current task execution to run a more urgent or better candidate task.

### 12.2 Starvation
Starvation is indefinite deferral of runnable work due to policy dominance or misconfiguration.

### 12.3 Priority Inversion
Priority inversion occurs when a high-priority task waits on a lower-priority task holding a needed resource.

### 12.4 Priority Inheritance (PI)
Priority inheritance temporarily raises lock-holder priority to bound inversion duration.

### 12.5 Race Window
A race window is the interval where concurrent events can observe or create inconsistent state if ordering/locking is wrong.

### 12.6 Deadlock
Deadlock is cyclic waiting where progress halts. In scheduler context, lock-ordering discipline is the primary prevention strategy.

## 13. Observability Terms
### 13.1 Tracepoint
A tracepoint is a stable instrumentation hook to observe scheduler transitions and decisions with timeline accuracy.

### 13.2 Scheduling Latency
Scheduling latency is delay between task becoming runnable and actually executing.

### 13.3 Tail Latency
Tail latency is high-percentile delay behavior (for example, p99/p999), often more operationally relevant than average latency.

### 13.4 Throughput
Throughput is total useful work completed per unit time; scheduler tuning can improve latency while reducing throughput, or vice versa.

## 14. Interview Question Patterns and Strong Responses
### Pattern 1: "Fairness vs Latency"
Strong answer structure:
1. Define fairness over windowed proportional share.
2. Explain granularity/preemption tradeoff.
3. Mention cache/migration side effects.
4. Conclude with measurement approach (tail latency + throughput together).

### Pattern 2: "Why tasks migrate too much"
Strong answer structure:
1. Explain balancing goals and migration constraints.
2. Mention topology locality and migration cost.
3. Describe symptoms (cache miss spikes, context-switch growth).
4. Propose trace-based validation before tuning.

### Pattern 3: "Why performance dropped on big.LITTLE"
Strong answer structure:
1. Check capacity model and placement policy.
2. Verify utilization signal quality and clamp policy.
3. Evaluate EAS model fidelity and affinity restrictions.
4. Separate scheduler issue from thermal/frequency limits.

### Pattern 4: "RT task misses deadline"
Strong answer structure:
1. Distinguish RT FIFO/RR from deadline contracts.
2. Examine interference, lock contention, and IRQ pressure.
3. Validate admission/isolation assumptions.
4. Use timeline evidence, not only aggregate counters.

## 15. Practical Vocabulary for Senior-Level Communication
Use these terms precisely in interviews:
- "policy precedence" instead of "priority only"
- "windowed fairness" instead of "equal CPU"
- "placement under constraints" instead of "best CPU"
- "signal lag versus responsiveness" instead of "wrong utilization"
- "liveness and starvation boundaries" instead of "slow scheduler"

## 16. Final Interview Advice
A strong scheduler interview answer always includes:
1. Definition.
2. Why it exists.
3. What tradeoff it introduces.
4. How to observe/validate it in production.

This 4-step framing demonstrates architectural understanding, operational maturity, and debugging discipline expected from senior kernel engineers.

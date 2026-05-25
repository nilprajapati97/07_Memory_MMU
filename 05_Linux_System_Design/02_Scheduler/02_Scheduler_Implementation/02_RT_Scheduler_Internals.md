# Real-Time Scheduler Implementation Deep Dive

## 1. Design Intent
The RT class provides deterministic priority-based scheduling for `SCHED_FIFO` and `SCHED_RR`, preempting lower classes and enforcing strict priority ordering while integrating with SMP migration and bandwidth controls.

## 2. Primary Code Regions
- `kernel/sched/rt.c`
- `kernel/sched/core.c`
- `kernel/sched/sched.h`

Key symbols:
- `enqueue_task_rt`, `dequeue_task_rt`
- `pick_next_task_rt`
- `push_rt_task`, `pull_rt_task`
- RT bandwidth enforcement logic

## 3. RT Runqueue Model
`struct rt_rq` includes:
- active priority array and bitmap.
- runnable RT counters.
- pushable task list for load balancing.

The highest-priority runnable RT task is selected first; FIFO/RR semantics differentiate time-slice behavior among equal priority tasks.

## 4. Scheduling Semantics
### 4.1 `SCHED_FIFO`
- Runs until block/yield/preempted by higher-priority task.
- No timeslice expiry among same-priority tasks unless explicit yield/block behavior occurs.

### 4.2 `SCHED_RR`
- Same priority ordering as FIFO.
- Round-robin timeslice among peers at identical priority.

## 5. Enqueue/Dequeue and Preemption
Wakeup path enqueues RT tasks into priority array and triggers immediate preemption check against current task. Cross-class preemption guarantees RT dominance over fair tasks.

## 6. SMP Balancing for RT
RT balancing is push/pull oriented:
- push attempts to move lower-priority or pushable RT tasks away from overloaded CPU.
- pull allows idle/underloaded CPU to acquire runnable RT tasks.

Stopper/kthread-assisted migration is used for correctness in difficult affinity scenarios.

## 7. Bandwidth and Starvation Control
RT runtime throttling limits CPU monopolization by RT tasks and protects lower classes from complete starvation.

Design tradeoff:
- strict RT response versus global system liveness for non-RT work.

## 8. Interactions with PI and Locking
RT workloads are sensitive to priority inversion. PI-aware lock paths and `p->pi_lock` coordination are mandatory to avoid unbounded inversion.

## 9. Operational Pitfalls
1. Unbounded FIFO loops causing starvation of fair tasks.
2. Affinity restrictions preventing effective push/pull migration.
3. Misconfigured runtime quotas causing either starvation or deadline misses in mixed workloads.
4. Excessive RT migration overhead on high core-count systems.

## 10. Validation Strategy
- Run mixed RT/fair stress with controlled affinity masks.
- Verify highest-priority response latency under IRQ load.
- Validate throttling behavior and recovery.
- Inspect migration tracepoints for pathological ping-pong patterns.

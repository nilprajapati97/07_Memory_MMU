# 14 — Scheduler (Cooperative, RR, Priority, EDF)

## Problem
Choose which ready task runs next on a CPU. Implement the policy efficiently and predictably.

## Why It Matters
Real-time, embedded, kernel and runtime systems all need a scheduler. The choice of algorithm determines latency, fairness, determinism, and whether you can prove deadlines are met.

## Approaches

### Approach 1 — Cooperative (Run-to-Completion / Yield)
Each task runs until it voluntarily calls `yield()` or finishes.
```text
loop:
    t = next_runnable_task()
    run(t)            // returns only when t yields or finishes
```
- Trivial: no preemption, no locking needed inside a single task.
- Misbehaving task hogs the CPU.
- Used in many small embedded systems and protothreads.

### Approach 2 — Round-Robin (Preemptive, Equal Quantum)
Timer interrupt every `Q` ms; current task moved to tail of run queue; head dequeued.
```text
timer_isr:
    save context
    enqueue(current)
    current = dequeue()
    restore context
```
- Fair, simple.
- Ignores priorities and deadlines.
- Quantum choice trades context-switch overhead vs latency.

### Approach 3 — Priority-Based (Fixed Priority Preemptive)
Highest-priority ready task always runs. Lower-priority preempted on arrival of higher.
- Standard in RTOSes (FreeRTOS, VxWorks, Zephyr).
- Predictable; needs **priority inheritance** to avoid inversion.
- Starvation possible if a high-priority task never blocks.

### Approach 4 — Multilevel Feedback Queue (MLFQ)
Several priority queues; CPU-bound tasks demoted, I/O-bound promoted.
- Approximates "give I/O-bound jobs priority" without knowing job class in advance.
- Used in classic UNIX schedulers.

### Approach 5 — O(1) / Bitmap Scheduler
Priority queue implemented as: bitmap of "non-empty priorities" + per-priority FIFO. `find_first_set` on the bitmap locates the highest priority in **O(1)**.
```text
bitmap |= (1 << prio); queue[prio].push(t)
prio = __builtin_ctz(bitmap); t = queue[prio].pop()
if queue[prio].empty: bitmap &= ~(1 << prio)
```
- Constant-time selection regardless of task count.
- Linux 2.6 O(1) scheduler used this; later replaced by CFS.

### Approach 6 — Completely Fair Scheduler (CFS)
Each task has `vruntime` (virtual runtime); pick task with smallest `vruntime` from a red-black tree. Approximates "perfect" fair sharing.
- O(log N) selection.
- Linux mainline since 2.6.23.

### Approach 7 — Rate-Monotonic (RMS) — Real-Time
Fixed priority assigned by **period**: shorter period → higher priority. Optimal for fixed-priority schedulability under specific assumptions (Liu & Layland).
- Schedulable if total utilisation ≤ `n(2^{1/n} - 1)` (≈ 69% for many tasks).

### Approach 8 — Earliest Deadline First (EDF) — Real-Time
Dynamic priority: task with closest deadline runs.
- Optimal among dynamic-priority single-processor schedulers; can use 100% utilisation.
- Harder to analyse under overload (cascading misses); harder in multi-core.

## Comparison
| Scheduler | Selection | Fairness | RT predictability | Use case |
|---|---|---|---|---|
| Cooperative | O(1) | trust-based | high if tasks behave | small embedded |
| Round-robin | O(1) | equal time | no priorities | timesharing fairness |
| Fixed-priority preemptive | O(P) or O(1) bitmap | none | high | most RTOSes |
| MLFQ | O(1) per queue | adaptive | low | general-purpose OS |
| O(1) bitmap | O(1) | per-prio | high | Linux 2.6 |
| CFS | O(log N) | proportional | medium | Linux mainline |
| RMS | O(1) bitmap | n/a | provable | hard RT periodic |
| EDF | O(log N) | n/a | provable, full util | hard RT |

## Context Switch Skeleton
```
save_context(prev)        # callee-saved regs, sp, pc → prev->tcb
prev = current
current = next
restore_context(current)  # reload regs, sp, pc from current->tcb
return                     # returns into the new task
```
Costs (~µs): register save/restore + cache/TLB pollution + (if process switch) page-table change.

## Key Insight
- Scheduling is about **predictability vs throughput vs fairness** — you cannot maximise all three.
- For real-time work, **fixed-priority preemptive** + careful priority assignment is the default; EDF gets you to 100% utilisation at the cost of analysis difficulty.
- O(1) bitmap selection is a beautiful application of `ffs`/`ctz` — constant-time priority queue when priorities fit in a word.

## Pitfalls
- Priority inversion without inheritance → high-priority task waits forever
- Choosing too small a timer quantum → context-switch overhead dominates
- Long ISR holding off the scheduler → all tasks delayed
- Spinlock held across a preemptible region → unbounded latency
- Forgetting to update `vruntime`/run-stats when a task blocks → starvation on wake
- Floating-point context save costly — only save FPU regs for tasks that use them ("lazy FP")
- Multi-core: per-CPU run queues + load balancer; naïve global queue scales badly

## Interview Tips
1. Walk through cooperative → RR → fixed-priority — establishes vocabulary.
2. Volunteer priority inversion + inheritance (Mars Pathfinder).
3. Cite RMS test and EDF optimality if asked about RT theory.
4. Show the O(1) bitmap trick (`__builtin_ctz`) — small but impressive.
5. Mention Linux moved from O(1) bitmap to CFS (red-black tree) — and **why** (fairness, not speed).

## Related / Follow-ups
- [04_spinlock](../04_spinlock/), [08_mutex_semaphore_spinlock](../08_mutex_semaphore_spinlock/)
- [13_state_machine](../13_state_machine/) — task states (READY/RUN/BLOCKED)
- Liu & Layland 1973, RM scheduling paper
- Linux CFS source, FreeRTOS scheduler
- Real-Time Systems by Jane Liu

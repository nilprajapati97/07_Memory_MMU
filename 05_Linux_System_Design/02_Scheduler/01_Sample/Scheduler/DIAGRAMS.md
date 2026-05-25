# Visual Diagrams - Linux Scheduler

## Process State Diagram

```
    ┌─────────────────────────────────────────────────┐
    │                                                 │
    │                    NEW                          │
    │              (Process Created)                  │
    │                                                 │
    └────────────────────┬────────────────────────────┘
                         │
                         │ Admitted
                         ▼
    ┌─────────────────────────────────────────────────┐
    │                                                 │
    │                   READY                         │
    │            (Waiting for CPU)                    │
    │                                                 │
    └────────────────────┬────────────────────────────┘
                         │
                         │ Scheduler Dispatch
                         ▼
    ┌─────────────────────────────────────────────────┐
    │                                                 │
    │                  RUNNING                        │
    │            (Executing on CPU)                   │
    │                                                 │
    └─────┬──────────────┬──────────────┬─────────────┘
          │              │              │
          │              │              │ Exit
          │              │              ▼
          │              │         ┌──────────┐
          │              │         │TERMINATED│
          │              │         └──────────┘
          │              │
          │              │ I/O or Event Wait
          │              ▼
          │         ┌─────────────┐
          │         │   WAITING   │
          │         │  (Blocked)  │
          │         └──────┬──────┘
          │                │
          │                │ I/O Complete
          │                ▼
          │         Back to READY
          │
          │ Time Quantum Expired
          └──────────► Back to READY
```

## CFS Scheduling Algorithm

```
Run Queue (Red-Black Tree sorted by vruntime)
┌────────────────────────────────────────────┐
│                                            │
│  Process A: vruntime = 10  ◄── Minimum!   │
│  Process B: vruntime = 25                 │
│  Process C: vruntime = 40                 │
│  Process D: vruntime = 55                 │
│                                            │
└────────────────────────────────────────────┘
                    │
                    │ Pick minimum vruntime
                    ▼
            ┌───────────────┐
            │  Process A    │
            │   RUNNING     │
            └───────┬───────┘
                    │
                    │ Execute for time quantum
                    ▼
            Update vruntime:
            vruntime += exec_time * (20 - priority)
                    │
                    ▼
            Insert back to run queue
```

## Virtual Runtime Calculation

```
Priority: 18 (High)
─────────────────────────────────────────────
Time Quantum: 4 units
Weight: 20 - 18 = 2
vruntime increase: 4 * 2 = 8

Before:  vruntime = 0
After:   vruntime = 8  ◄── Slow growth


Priority: 10 (Low)
─────────────────────────────────────────────
Time Quantum: 4 units
Weight: 20 - 10 = 10
vruntime increase: 4 * 10 = 40

Before:  vruntime = 0
After:   vruntime = 40  ◄── Fast growth


Result: High priority process gets scheduled more often!
```

## Thread Synchronization Flow

```
Thread 1 (ODD)                    Thread 2 (EVEN)
─────────────                     ────────────────

START                             START
  │                                 │
  ├─► Lock mutex                    │
  │                                 │
  ├─► Check: is_odd_turn?           │
  │   YES ✓                         │
  │                                 │
  ├─► Print: 1                      │
  │                                 │
  ├─► counter = 2                   │
  ├─► is_odd_turn = false           │
  │                                 │
  ├─► Signal EVEN                   │
  │   │                             │
  │   └────────────────────────────►├─► Wake up
  │                                 │
  ├─► Unlock mutex                  ├─► Lock mutex
  │                                 │
  ├─► Wait on condition             ├─► Check: !is_odd_turn?
  │   (WAITING state)                │   YES ✓
  │                                 │
  │                                 ├─► Print: 2
  │                                 │
  │                                 ├─► counter = 3
  │                                 ├─► is_odd_turn = true
  │                                 │
  │                                 ├─► Signal ODD
  │   ◄────────────────────────────┤
  │                                 │
  ├─► Wake up                       ├─► Unlock mutex
  │                                 │
  ├─► Lock mutex                    ├─► Wait on condition
  │                                 │   (WAITING state)
  ├─► Print: 3                      │
  │                                 │
  └─► Continue...                   └─► Continue...
```

## Context Switch Process

```
┌──────────────────────────────────────────────────┐
│         Process A (RUNNING)                      │
│  Registers: R1=10, R2=20, PC=0x1000             │
│  Stack Pointer: 0x7fff                           │
└──────────────────┬───────────────────────────────┘
                   │
                   │ Timer Interrupt
                   ▼
┌──────────────────────────────────────────────────┐
│              CONTEXT SWITCH                      │
│                                                  │
│  1. Save Process A state to PCB                 │
│     - Registers → PCB_A                          │
│     - PC → PCB_A                                 │
│     - Stack Pointer → PCB_A                      │
│                                                  │
│  2. Update state: A.state = READY               │
│                                                  │
│  3. Select next: Process B (min vruntime)       │
│                                                  │
│  4. Load Process B state from PCB               │
│     - PCB_B → Registers                          │
│     - PCB_B → PC                                 │
│     - PCB_B → Stack Pointer                      │
│                                                  │
│  5. Update state: B.state = RUNNING             │
│                                                  │
└──────────────────┬───────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────┐
│         Process B (RUNNING)                      │
│  Registers: R1=5, R2=15, PC=0x2000              │
│  Stack Pointer: 0x7ffe                           │
└──────────────────────────────────────────────────┘
```

## Mutex Lock Mechanism

```
Thread 1                          Thread 2
────────                          ────────

pthread_mutex_lock(&mutex)
  │
  ├─► Is mutex available?
  │   YES ✓
  │
  ├─► Acquire lock
  │   mutex.owner = Thread1
  │   mutex.locked = true
  │
  ├─► Enter critical section        pthread_mutex_lock(&mutex)
  │   shared_counter++                 │
  │                                    ├─► Is mutex available?
  │                                    │   NO ✗
  │                                    │
  │                                    ├─► Thread2.state = WAITING
  │                                    │   Add to mutex wait queue
  │                                    │
  │                                    ├─► Sleep (context switch)
  │                                    │
pthread_mutex_unlock(&mutex)          │
  │                                    │
  ├─► Release lock                    │
  │   mutex.locked = false             │
  │                                    │
  ├─► Wake up waiting thread          │
  │   │                                │
  │   └───────────────────────────────►├─► Wake up
  │                                    │
  │                                    ├─► Acquire lock
  │                                    │   mutex.owner = Thread2
  │                                    │
  │                                    ├─► Enter critical section
  │                                    │   shared_counter++
  │                                    │
  │                                    pthread_mutex_unlock(&mutex)
```

## Condition Variable Wait/Signal

```
Thread 1: Wait                    Thread 2: Signal
─────────────                     ────────────────

pthread_mutex_lock(&mutex)        pthread_mutex_lock(&mutex)
  │                                 │
  ├─► Check condition               ├─► Modify shared state
  │   while (!condition)            │   condition = true
  │                                 │
  ├─► Condition false               ├─► Signal waiting thread
  │   Call pthread_cond_wait()      │   pthread_cond_signal(&cond)
  │                                 │   │
  ├─► Atomically:                   │   └──────────┐
  │   1. Release mutex              │              │
  │   2. Sleep (WAITING)            │              │
  │   3. Add to cond wait queue     │              │
  │                                 │              │
  │   ┌─────────────────────────────┘              │
  │   │                             │              │
  │   │ Wake up                     pthread_mutex_unlock(&mutex)
  │   │                             │
  │   ├─► Reacquire mutex           │
  │   │   (may block if locked)     │
  │   │                             │
  │   ├─► Return from wait          │
  │   │                             │
  │   ├─► Check condition again     │
  │   │   Condition true ✓          │
  │   │                             │
  │   ├─► Exit loop                 │
  │                                 │
pthread_mutex_unlock(&mutex)        │
```

## Run Queue Evolution (CFS)

```
Time 0: Initial State
┌────────────────────────────────────┐
│ PID:1 Browser   | vruntime: 0     │
│ PID:2 Player    | vruntime: 0     │
│ PID:3 Editor    | vruntime: 0     │
│ PID:4 Compiler  | vruntime: 0     │
└────────────────────────────────────┘
         │
         │ Pick PID:1 (first with vruntime=0)
         ▼

Time 4: After Browser runs
┌────────────────────────────────────┐
│ PID:2 Player    | vruntime: 0     │ ◄── Minimum
│ PID:3 Editor    | vruntime: 0     │
│ PID:4 Compiler  | vruntime: 0     │
│ PID:1 Browser   | vruntime: 20    │
└────────────────────────────────────┘
         │
         │ Pick PID:2 (min vruntime)
         ▼

Time 8: After Player runs
┌────────────────────────────────────┐
│ PID:3 Editor    | vruntime: 0     │ ◄── Minimum
│ PID:4 Compiler  | vruntime: 0     │
│ PID:1 Browser   | vruntime: 20    │
│ PID:2 Player    | vruntime: 40    │
└────────────────────────────────────┘
         │
         │ Pick PID:3 (min vruntime)
         ▼

Time 12: After Editor runs
┌────────────────────────────────────┐
│ PID:4 Compiler  | vruntime: 0     │ ◄── Minimum
│ PID:3 Editor    | vruntime: 8     │
│ PID:1 Browser   | vruntime: 20    │
│ PID:2 Player    | vruntime: 40    │
└────────────────────────────────────┘

Pattern: Always pick minimum vruntime → Fairness!
```

## Thread Lifecycle

```
                    pthread_create()
                          │
                          ▼
                    ┌──────────┐
                    │   NEW    │
                    └─────┬────┘
                          │
                          │ Scheduler admits
                          ▼
    ┌─────────────────────────────────────┐
    │              READY                  │
    │      (In run queue)                 │
    └──────┬──────────────────────────────┘
           │                    ▲
           │ Scheduled          │
           ▼                    │
    ┌─────────────┐             │
    │   RUNNING   │             │
    │             │             │
    └──────┬──────┘             │
           │                    │
           ├────────────────────┘
           │ Time quantum expired
           │
           ├─► pthread_mutex_lock() (if locked)
           │   pthread_cond_wait()
           │         │
           │         ▼
           │   ┌──────────┐
           │   │ WAITING  │
           │   │(Blocked) │
           │   └─────┬────┘
           │         │
           │         │ Unlocked/Signaled
           │         └──────► Back to READY
           │
           │ pthread_exit()
           │ return from thread function
           ▼
    ┌──────────────┐
    │  TERMINATED  │
    └──────┬───────┘
           │
           │ pthread_join()
           ▼
    Cleaned up by main thread
```

## Memory Layout - Process vs Thread

```
Process 1                         Process 2
─────────────────────             ─────────────────────
┌─────────────────┐               ┌─────────────────┐
│  Code Segment   │               │  Code Segment   │
├─────────────────┤               ├─────────────────┤
│  Data Segment   │               │  Data Segment   │
├─────────────────┤               ├─────────────────┤
│  Heap           │               │  Heap           │
├─────────────────┤               ├─────────────────┤
│  Stack          │               │  Stack          │
└─────────────────┘               └─────────────────┘
   Separate Memory                   Separate Memory


Single Process with Threads
─────────────────────────────────
┌─────────────────────────────┐
│     Code Segment            │ ◄── Shared
├─────────────────────────────┤
│     Data Segment            │ ◄── Shared
├─────────────────────────────┤
│     Heap                    │ ◄── Shared
├─────────────────────────────┤
│  Thread 1 Stack             │ ◄── Private
├─────────────────────────────┤
│  Thread 2 Stack             │ ◄── Private
└─────────────────────────────┘
```

## Priority Impact on Scheduling

```
High Priority (18)          Low Priority (10)
──────────────────          ─────────────────

vruntime growth: SLOW       vruntime growth: FAST
Weight: 2                   Weight: 10

Time 0:  vruntime = 0       Time 0:  vruntime = 0
Time 4:  vruntime = 8       Time 4:  vruntime = 40
Time 8:  vruntime = 16      Time 8:  vruntime = 80
Time 12: vruntime = 24      Time 12: vruntime = 120

Result: High priority scheduled MORE often
        Low priority scheduled LESS often

Visualization:
High: ████████████████████████ (24 units)
Low:  ████████ (8 units)
```

## Complete Odd/Even Thread Flow

```
Counter: 1  2  3  4  5  6  7  8  9  10 ... 100
         │  │  │  │  │  │  │  │  │  │      │
ODD:     █  ░  █  ░  █  ░  █  ░  █  ░  ... █
EVEN:    ░  █  ░  █  ░  █  ░  █  ░  █  ... ░

█ = Thread executing
░ = Thread waiting

Flow:
1. ODD prints 1, signals EVEN, waits
2. EVEN prints 2, signals ODD, waits
3. ODD prints 3, signals EVEN, waits
4. EVEN prints 4, signals ODD, waits
...
99. ODD prints 99, signals EVEN, waits
100. EVEN prints 100, exits
101. ODD exits
102. Both join to main
103. Main exits
```

---

These diagrams illustrate the core concepts of the Linux scheduler implementation.
Refer to the source code for actual implementation details.

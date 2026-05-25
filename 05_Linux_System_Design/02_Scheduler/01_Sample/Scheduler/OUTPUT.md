# Shell Output Examples

This document contains the actual output from running all the scheduler implementations.

## Table of Contents
1. [Process Scheduler Output](#1-process-scheduler-output)
2. [Simple Thread Output](#2-simple-thread-output)
3. [Detailed Thread Scheduler Output](#3-detailed-thread-scheduler-output)
4. [Advanced Thread Implementation Output](#4-advanced-thread-implementation-output)

---

## 1. Process Scheduler Output

### Command
```bash
./scheduler
```

### Output
```
[SYSTEM] Initializing processes...

[SCHEDULER] Process WebBrowser (PID:1) added to run queue
[SCHEDULER] Process VideoPlayer (PID:2) added to run queue
[SCHEDULER] Process TextEditor (PID:3) added to run queue
[SCHEDULER] Process Compiler (PID:4) added to run queue

========================================
   Linux CFS Scheduler Simulation
========================================
Time Quantum: 4 units
Total Processes: 4
========================================


[RUN QUEUE STATE] Processes: 4
  PID:1 WebBrowser   | VRuntime:0    | Remaining:12 | Priority:15
  PID:2 VideoPlayer  | VRuntime:0    | Remaining:8  | Priority:10
  PID:3 TextEditor   | VRuntime:0    | Remaining:6  | Priority:18
  PID:4 Compiler     | VRuntime:0    | Remaining:10 | Priority:12


[TIME   0] Running Process: WebBrowser (PID:1)
           Priority: 15 | Nice: 5 | VRuntime: 0
           Remaining Time: 12 | Wait Time: 0

           [Time Quantum Expired]
             [CONTEXT SWITCH] Saving state of WebBrowser

[RUN QUEUE STATE] Processes: 4
  PID:1 WebBrowser   | VRuntime:20   | Remaining:8  | Priority:15
  PID:2 VideoPlayer  | VRuntime:0    | Remaining:8  | Priority:10
  PID:3 TextEditor   | VRuntime:0    | Remaining:6  | Priority:18
  PID:4 Compiler     | VRuntime:0    | Remaining:10 | Priority:12


[TIME   4] Running Process: VideoPlayer (PID:2)
           Priority: 10 | Nice: 10 | VRuntime: 0
           Remaining Time: 8 | Wait Time: 4

           [Time Quantum Expired]
             [CONTEXT SWITCH] Saving state of VideoPlayer

[RUN QUEUE STATE] Processes: 4
  PID:1 WebBrowser   | VRuntime:20   | Remaining:8  | Priority:15
  PID:2 VideoPlayer  | VRuntime:40   | Remaining:4  | Priority:10
  PID:3 TextEditor   | VRuntime:0    | Remaining:6  | Priority:18
  PID:4 Compiler     | VRuntime:0    | Remaining:10 | Priority:12


[TIME   8] Running Process: TextEditor (PID:3)
           Priority: 18 | Nice: 2 | VRuntime: 0
           Remaining Time: 6 | Wait Time: 8

           [Time Quantum Expired]
             [CONTEXT SWITCH] Saving state of TextEditor

[RUN QUEUE STATE] Processes: 4
  PID:1 WebBrowser   | VRuntime:20   | Remaining:8  | Priority:15
  PID:2 VideoPlayer  | VRuntime:40   | Remaining:4  | Priority:10
  PID:3 TextEditor   | VRuntime:8    | Remaining:2  | Priority:18
  PID:4 Compiler     | VRuntime:0    | Remaining:10 | Priority:12


[TIME  12] Running Process: Compiler (PID:4)
           Priority: 12 | Nice: 8 | VRuntime: 0
           Remaining Time: 10 | Wait Time: 12

           [Time Quantum Expired]
             [CONTEXT SWITCH] Saving state of Compiler

[RUN QUEUE STATE] Processes: 4
  PID:1 WebBrowser   | VRuntime:20   | Remaining:8  | Priority:15
  PID:2 VideoPlayer  | VRuntime:40   | Remaining:4  | Priority:10
  PID:3 TextEditor   | VRuntime:8    | Remaining:2  | Priority:18
  PID:4 Compiler     | VRuntime:32   | Remaining:6  | Priority:12


[TIME  16] Running Process: TextEditor (PID:3)
           Priority: 18 | Nice: 2 | VRuntime: 8
           Remaining Time: 2 | Wait Time: 12

           >>> Process TextEditor COMPLETED <<<
           Total Wait Time: 12
           Turnaround Time: 18

[RUN QUEUE STATE] Processes: 3
  PID:1 WebBrowser   | VRuntime:20   | Remaining:8  | Priority:15
  PID:2 VideoPlayer  | VRuntime:40   | Remaining:4  | Priority:10
  PID:4 Compiler     | VRuntime:32   | Remaining:6  | Priority:12


[TIME  18] Running Process: WebBrowser (PID:1)
           Priority: 15 | Nice: 5 | VRuntime: 20
           Remaining Time: 8 | Wait Time: 14

           [Time Quantum Expired]
             [CONTEXT SWITCH] Saving state of WebBrowser

[RUN QUEUE STATE] Processes: 3
  PID:1 WebBrowser   | VRuntime:40   | Remaining:4  | Priority:15
  PID:2 VideoPlayer  | VRuntime:40   | Remaining:4  | Priority:10
  PID:4 Compiler     | VRuntime:32   | Remaining:6  | Priority:12


[TIME  22] Running Process: Compiler (PID:4)
           Priority: 12 | Nice: 8 | VRuntime: 32
           Remaining Time: 6 | Wait Time: 18

           [Time Quantum Expired]
             [CONTEXT SWITCH] Saving state of Compiler

[RUN QUEUE STATE] Processes: 3
  PID:1 WebBrowser   | VRuntime:40   | Remaining:4  | Priority:15
  PID:2 VideoPlayer  | VRuntime:40   | Remaining:4  | Priority:10
  PID:4 Compiler     | VRuntime:64   | Remaining:2  | Priority:12


[TIME  26] Running Process: WebBrowser (PID:1)
           Priority: 15 | Nice: 5 | VRuntime: 40
           Remaining Time: 4 | Wait Time: 18

           >>> Process WebBrowser COMPLETED <<<
           Total Wait Time: 18
           Turnaround Time: 30

[RUN QUEUE STATE] Processes: 2
  PID:2 VideoPlayer  | VRuntime:40   | Remaining:4  | Priority:10
  PID:4 Compiler     | VRuntime:64   | Remaining:2  | Priority:12


[TIME  30] Running Process: VideoPlayer (PID:2)
           Priority: 10 | Nice: 10 | VRuntime: 40
           Remaining Time: 4 | Wait Time: 26

           >>> Process VideoPlayer COMPLETED <<<
           Total Wait Time: 26
           Turnaround Time: 34

[RUN QUEUE STATE] Processes: 1
  PID:4 Compiler     | VRuntime:64   | Remaining:2  | Priority:12


[TIME  34] Running Process: Compiler (PID:4)
           Priority: 12 | Nice: 8 | VRuntime: 64
           Remaining Time: 2 | Wait Time: 26

           >>> Process Compiler COMPLETED <<<
           Total Wait Time: 26
           Turnaround Time: 36

========================================
   Scheduling Complete
========================================
Total Time: 36 units
Context Switches: 6
Average Wait Time: 20.50 units
Average Turnaround Time: 29.50 units
========================================
```

### Key Observations
1. **TextEditor (Priority 18)** completes first despite starting with same vruntime
   - Higher priority = slower vruntime growth (vruntime += 4 * 2 = 8)
   - Gets scheduled more frequently

2. **VideoPlayer (Priority 10)** has fastest vruntime growth
   - Lower priority = faster vruntime growth (vruntime += 4 * 10 = 40)
   - Gets less CPU time

3. **CFS Fairness**: Process with lowest vruntime always runs next

4. **Context Switches**: 6 total switches when time quantum expires

---

## 2. Simple Thread Output

### Command
```bash
./thread_simple
```

### Output
```
=== Multithreading: Odd/Even Number Printing ===

ODD Thread:  1
EVEN Thread: 2
ODD Thread:  3
EVEN Thread: 4
ODD Thread:  5
EVEN Thread: 6
ODD Thread:  7
EVEN Thread: 8
ODD Thread:  9
EVEN Thread: 10
ODD Thread:  11
EVEN Thread: 12
ODD Thread:  13
EVEN Thread: 14
ODD Thread:  15
EVEN Thread: 16
ODD Thread:  17
EVEN Thread: 18
ODD Thread:  19
EVEN Thread: 20
ODD Thread:  21
EVEN Thread: 22
ODD Thread:  23
EVEN Thread: 24
ODD Thread:  25
EVEN Thread: 26
ODD Thread:  27
EVEN Thread: 28
ODD Thread:  29
EVEN Thread: 30
ODD Thread:  31
EVEN Thread: 32
ODD Thread:  33
EVEN Thread: 34
ODD Thread:  35
EVEN Thread: 36
ODD Thread:  37
EVEN Thread: 38
ODD Thread:  39
EVEN Thread: 40
ODD Thread:  41
EVEN Thread: 42
ODD Thread:  43
EVEN Thread: 44
ODD Thread:  45
EVEN Thread: 46
ODD Thread:  47
EVEN Thread: 48
ODD Thread:  49
EVEN Thread: 50
ODD Thread:  51
EVEN Thread: 52
ODD Thread:  53
EVEN Thread: 54
ODD Thread:  55
EVEN Thread: 56
ODD Thread:  57
EVEN Thread: 58
ODD Thread:  59
EVEN Thread: 60
ODD Thread:  61
EVEN Thread: 62
ODD Thread:  63
EVEN Thread: 64
ODD Thread:  65
EVEN Thread: 66
ODD Thread:  67
EVEN Thread: 68
ODD Thread:  69
EVEN Thread: 70
ODD Thread:  71
EVEN Thread: 72
ODD Thread:  73
EVEN Thread: 74
ODD Thread:  75
EVEN Thread: 76
ODD Thread:  77
EVEN Thread: 78
ODD Thread:  79
EVEN Thread: 80
ODD Thread:  81
EVEN Thread: 82
ODD Thread:  83
EVEN Thread: 84
ODD Thread:  85
EVEN Thread: 86
ODD Thread:  87
EVEN Thread: 88
ODD Thread:  89
EVEN Thread: 90
ODD Thread:  91
EVEN Thread: 92
ODD Thread:  93
EVEN Thread: 94
ODD Thread:  95
EVEN Thread: 96
ODD Thread:  97
EVEN Thread: 98
ODD Thread:  99
EVEN Thread: 100

=== All threads completed and joined to main ===
Main thread exiting...
```

### Key Observations
1. **Perfect Alternation**: Threads alternate printing odd/even numbers
2. **Synchronization**: Mutex and condition variables ensure correct order
3. **Completion**: Both threads reach 100 and join to main
4. **Clean Output**: Simple, easy to verify correctness

---

## 3. Detailed Thread Scheduler Output

### Command
```bash
./thread_scheduler
```

### Output (First 100 lines)
```
========================================
  Linux Thread Scheduler Simulation
========================================
Demonstrating:
  - Thread creation and management
  - Mutex locks for synchronization
  - Condition variables for coordination
  - Thread state transitions
  - Context switching
========================================

[MAIN THREAD] Initializing synchronization primitives
    Mutex initialized
    Condition variable initialized
    Starting counter: 1

[MAIN THREAD] Creating worker threads...

[THREAD CREATED] ODD_THREAD (TID: 123969384478400) - State: READY
    [LOCK ACQUIRED] ODD_THREAD acquired mutex

>>> [ODD_THREAD] Printing: 1
    [SIGNAL] ODD_THREAD signaling EVEN_THREAD
    [LOCK RELEASED] ODD_THREAD released mutex


[MAIN THREAD] Both threads created successfully
[MAIN THREAD] State: RUNNING -> WAITING (waiting for threads to join)

[MAIN THREAD] Waiting for ODD_THREAD to join...

[THREAD CREATED] EVEN_THREAD (TID: 123969376085696) - State: READY
    [LOCK ACQUIRED] EVEN_THREAD acquired mutex

>>> [EVEN_THREAD] Printing: 2
    [SIGNAL] EVEN_THREAD signaling ODD_THREAD
    [LOCK RELEASED] EVEN_THREAD released mutex

    [LOCK ACQUIRED] ODD_THREAD acquired mutex

>>> [ODD_THREAD] Printing: 3
    [SIGNAL] ODD_THREAD signaling EVEN_THREAD
    [LOCK RELEASED] ODD_THREAD released mutex

    [LOCK ACQUIRED] EVEN_THREAD acquired mutex

>>> [EVEN_THREAD] Printing: 4
    [SIGNAL] EVEN_THREAD signaling ODD_THREAD
    [LOCK RELEASED] EVEN_THREAD released mutex

    [LOCK ACQUIRED] EVEN_THREAD acquired mutex
    [WAITING] EVEN_THREAD waiting for condition (counter=5)
    [STATE CHANGE] EVEN_THREAD: RUNNING -> WAITING
    [LOCK ACQUIRED] ODD_THREAD acquired mutex

>>> [ODD_THREAD] Printing: 5
    [SIGNAL] ODD_THREAD signaling EVEN_THREAD
    [LOCK RELEASED] ODD_THREAD released mutex

    [STATE CHANGE] EVEN_THREAD: WAITING -> READY -> RUNNING
        [SCHEDULER] Context switch for EVEN_THREAD (Total: 1)

>>> [EVEN_THREAD] Printing: 6
    [SIGNAL] EVEN_THREAD signaling ODD_THREAD
    [LOCK RELEASED] EVEN_THREAD released mutex

    [LOCK ACQUIRED] ODD_THREAD acquired mutex

>>> [ODD_THREAD] Printing: 7
    [SIGNAL] ODD_THREAD signaling EVEN_THREAD
    [LOCK RELEASED] ODD_THREAD released mutex

    [LOCK ACQUIRED] EVEN_THREAD acquired mutex

>>> [EVEN_THREAD] Printing: 8
    [SIGNAL] EVEN_THREAD signaling ODD_THREAD
    [LOCK RELEASED] EVEN_THREAD released mutex

    [LOCK ACQUIRED] EVEN_THREAD acquired mutex
    [WAITING] EVEN_THREAD waiting for condition (counter=9)
    [STATE CHANGE] EVEN_THREAD: RUNNING -> WAITING
    [LOCK ACQUIRED] ODD_THREAD acquired mutex

>>> [ODD_THREAD] Printing: 9
    [SIGNAL] ODD_THREAD signaling EVEN_THREAD
    [LOCK RELEASED] ODD_THREAD released mutex

    [STATE CHANGE] EVEN_THREAD: WAITING -> READY -> RUNNING
        [SCHEDULER] Context switch for EVEN_THREAD (Total: 2)

>>> [EVEN_THREAD] Printing: 10
    [SIGNAL] EVEN_THREAD signaling ODD_THREAD
    [LOCK RELEASED] EVEN_THREAD released mutex

    [LOCK ACQUIRED] ODD_THREAD acquired mutex

>>> [ODD_THREAD] Printing: 11
    [SIGNAL] ODD_THREAD signaling EVEN_THREAD
    [LOCK RELEASED] ODD_THREAD released mutex
```

### Key Observations
1. **State Transitions**: Shows RUNNING → WAITING → READY → RUNNING
2. **Context Switches**: Tracked and counted when threads wait/signal
3. **Lock Operations**: Every mutex acquire/release is logged
4. **Synchronization**: Condition variable wait/signal operations visible
5. **Thread IDs**: Real pthread TIDs shown

---

## 4. Advanced Thread Implementation Output

### Command
```bash
./thread_advanced
```

### Output (First 120 lines)
```
================================================================
     Linux Thread Scheduler - Complete Implementation
================================================================
Demonstrating:
  • Thread Control Blocks (TCB)
  • Thread State Transitions (NEW->READY->RUNNING->WAITING)
  • Mutex Locks (Critical Section Protection)
  • Condition Variables (Thread Synchronization)
  • Context Switching
  • Virtual Runtime (CFS Scheduling)
  • Priority-based Scheduling
================================================================

[MAIN] Initializing synchronization primitives...
  • Mutex initialized
  • Condition variable initialized
  • Starting counter: 1
  • Max number: 100

[MAIN] Creating threads...
    [SCHEDULER] ODD_THREAD: NEW -> READY

[THREAD START] ODD_THREAD created (TID: 131258965292736)
    [SCHEDULER] ODD_THREAD: READY -> RUNNING

[MUTEX] ODD_THREAD acquired lock
[EXECUTE] ODD_THREAD printing: 1
[SIGNAL] ODD_THREAD signaling EVEN thread
[MUTEX] ODD_THREAD released lock
    [SCHEDULER] ODD_THREAD: RUNNING -> READY

[MAIN] Main thread entering WAITING state (pthread_join)...
    [SCHEDULER] EVEN_THREAD: NEW -> READY

[THREAD START] EVEN_THREAD created (TID: 131258956900032)
    [SCHEDULER] EVEN_THREAD: READY -> RUNNING

[MUTEX] EVEN_THREAD acquired lock
[EXECUTE] EVEN_THREAD printing: 2
[SIGNAL] EVEN_THREAD signaling ODD thread
[MUTEX] EVEN_THREAD released lock
    [SCHEDULER] EVEN_THREAD: RUNNING -> READY
    [SCHEDULER] ODD_THREAD: READY -> RUNNING

[MUTEX] ODD_THREAD acquired lock
[EXECUTE] ODD_THREAD printing: 3
[SIGNAL] ODD_THREAD signaling EVEN thread
[MUTEX] ODD_THREAD released lock
    [SCHEDULER] EVEN_THREAD: READY -> RUNNING

[MUTEX] EVEN_THREAD acquired lock
[EXECUTE] EVEN_THREAD printing: 4
[SIGNAL] EVEN_THREAD signaling ODD thread
[MUTEX] EVEN_THREAD released lock
    [SCHEDULER] EVEN_THREAD: RUNNING -> READY
    [SCHEDULER] ODD_THREAD: RUNNING -> READY
    [SCHEDULER] EVEN_THREAD: READY -> RUNNING

[MUTEX] EVEN_THREAD acquired lock
    [SCHEDULER] EVEN_THREAD: RUNNING -> WAITING
[WAIT] EVEN_THREAD waiting on condition variable (counter=5)
    [SCHEDULER] ODD_THREAD: READY -> RUNNING

[MUTEX] ODD_THREAD acquired lock
[EXECUTE] ODD_THREAD printing: 5
[SIGNAL] ODD_THREAD signaling EVEN thread
[MUTEX] ODD_THREAD released lock
    [SCHEDULER] ODD_THREAD: RUNNING -> READY
    [SCHEDULER] EVEN_THREAD: WAITING -> READY
    [CONTEXT SWITCH #1] Switching from EVEN_THREAD (vruntime=100)
    [SCHEDULER] EVEN_THREAD: READY -> RUNNING
[EXECUTE] EVEN_THREAD printing: 6
[SIGNAL] EVEN_THREAD signaling ODD thread
[MUTEX] EVEN_THREAD released lock
    [SCHEDULER] EVEN_THREAD: RUNNING -> READY
    [SCHEDULER] ODD_THREAD: READY -> RUNNING

[MUTEX] ODD_THREAD acquired lock
[EXECUTE] ODD_THREAD printing: 7
[SIGNAL] ODD_THREAD signaling EVEN thread
[MUTEX] ODD_THREAD released lock
    [SCHEDULER] ODD_THREAD: RUNNING -> READY
    [SCHEDULER] EVEN_THREAD: READY -> RUNNING

[MUTEX] EVEN_THREAD acquired lock
[EXECUTE] EVEN_THREAD printing: 8
[SIGNAL] EVEN_THREAD signaling ODD thread
[MUTEX] EVEN_THREAD released lock
    [SCHEDULER] EVEN_THREAD: RUNNING -> READY
    [SCHEDULER] ODD_THREAD: READY -> RUNNING

[MUTEX] ODD_THREAD acquired lock
    [SCHEDULER] EVEN_THREAD: READY -> RUNNING
[EXECUTE] ODD_THREAD printing: 9
[SIGNAL] ODD_THREAD signaling EVEN thread
[MUTEX] ODD_THREAD released lock
    [SCHEDULER] ODD_THREAD: RUNNING -> READY

[MUTEX] EVEN_THREAD acquired lock
[EXECUTE] EVEN_THREAD printing: 10
[SIGNAL] EVEN_THREAD signaling ODD thread
[MUTEX] EVEN_THREAD released lock
    [SCHEDULER] EVEN_THREAD: RUNNING -> READY
    [SCHEDULER] ODD_THREAD: READY -> RUNNING

[MUTEX] ODD_THREAD acquired lock
[EXECUTE] ODD_THREAD printing: 11
[SIGNAL] ODD_THREAD signaling EVEN thread
[MUTEX] ODD_THREAD released lock
    [SCHEDULER] ODD_THREAD: RUNNING -> READY
    [SCHEDULER] EVEN_THREAD: READY -> RUNNING

[MUTEX] EVEN_THREAD acquired lock
[EXECUTE] EVEN_THREAD printing: 12
[SIGNAL] EVEN_THREAD signaling ODD thread
[MUTEX] EVEN_THREAD released lock
    [SCHEDULER] EVEN_THREAD: RUNNING -> READY
    [SCHEDULER] ODD_THREAD: READY -> RUNNING
```

### Key Observations
1. **Thread Control Blocks**: Each thread has TCB with state, priority, vruntime
2. **Complete State Machine**: NEW → READY → RUNNING → WAITING → READY
3. **Virtual Runtime**: Tracks vruntime for CFS-like scheduling
4. **Context Switches**: Numbered and tracked with vruntime values
5. **Priority-based**: Priority affects vruntime calculation
6. **Main Thread State**: Shows main thread entering WAITING during pthread_join

---

## Summary of All Implementations

| Feature | scheduler.c | thread_simple.c | thread_scheduler.c | thread_advanced.c |
|---------|-------------|-----------------|-------------------|-------------------|
| Process/Thread | Process | Thread | Thread | Thread |
| CFS Algorithm | ✓ | ✗ | ✗ | ✓ |
| Virtual Runtime | ✓ | ✗ | ✗ | ✓ |
| Priority | ✓ | ✗ | ✗ | ✓ |
| State Transitions | ✓ | ✗ | ✓ | ✓ |
| Context Switches | ✓ | ✗ | ✓ | ✓ |
| Mutex/Cond Var | ✗ | ✓ | ✓ | ✓ |
| Statistics | ✓ | ✗ | ✓ | ✓ |
| TCB/PCB | ✓ | ✗ | ✗ | ✓ |
| Output Verbosity | Medium | Low | High | High |

## How to Run All Examples

```bash
# Compile all
make

# Run process scheduler
./scheduler

# Run simple threading
./thread_simple

# Run detailed threading
./thread_scheduler

# Run advanced threading
./thread_advanced

# Clean up
make clean
```

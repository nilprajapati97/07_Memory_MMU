# Linux Scheduler Implementation

Comprehensive implementations demonstrating how the Linux scheduler works with process scheduling, thread management, and synchronization mechanisms.

## Files Overview

### Process Scheduling
- **`scheduler.c`** - Complete CFS (Completely Fair Scheduler) implementation
  - Process states (READY, RUNNING, WAITING, TERMINATED)
  - Virtual runtime (vruntime) tracking
  - Priority-based scheduling
  - Context switching simulation
  - Run queue management
  - Detailed statistics

### Thread Synchronization
- **`thread_simple.c`** - Basic odd/even number printing with threads
  - Clean, minimal implementation
  - Mutex locks
  - Condition variables
  - Thread joining

- **`thread_scheduler.c`** - Detailed thread scheduling simulation
  - Thread state transitions
  - Context switch tracking
  - Synchronization visualization
  - Thread statistics

- **`thread_advanced.c`** - Complete Linux thread scheduler implementation
  - Thread Control Blocks (TCB)
  - Virtual runtime for threads
  - Priority-based thread scheduling
  - Comprehensive state management
  - Performance metrics

## Key Concepts Demonstrated

### Process Scheduling (CFS)
1. **Virtual Runtime (vruntime)**: Tracks CPU time per process
2. **Priority-based Scheduling**: Higher priority = more CPU time
3. **Time Quantum**: Fixed time slice before context switch
4. **Fair Scheduling**: Process with lowest vruntime runs next
5. **Run Queue**: Manages ready processes

### Thread Synchronization
1. **Mutex Locks**: Protect critical sections
2. **Condition Variables**: Coordinate thread execution
3. **Thread States**: NEW → READY → RUNNING → WAITING → TERMINATED
4. **Context Switching**: Save/restore thread state
5. **Thread Joining**: Wait for thread completion

## How to Build

### Using Makefile (Recommended)
```bash
# Build all programs
make

# Build specific program
make scheduler
make thread_simple
make thread_scheduler
make thread_advanced

# Clean build files
make clean
```

### Manual Compilation
```bash
# Process scheduler
gcc -Wall scheduler.c -o scheduler

# Thread programs (need pthread library)
gcc -Wall -pthread thread_simple.c -o thread_simple
gcc -Wall -pthread thread_scheduler.c -o thread_scheduler
gcc -Wall -pthread thread_advanced.c -o thread_advanced
```

## How to Run

### Process Scheduler
```bash
./scheduler
# or
make run_scheduler
```

### Thread Programs
```bash
# Simple version (clean output)
./thread_simple
# or
make run_simple

# Detailed version (with scheduler info)
./thread_scheduler
# or
make run_scheduler_thread

# Advanced version (complete implementation)
./thread_advanced
# or
make run_advanced
```

## How It Works

### Process Scheduler (scheduler.c)
1. Creates 4 processes with different priorities and burst times
2. Maintains run queue with all ready processes
3. Picks process with minimum vruntime (CFS algorithm)
4. Executes for time quantum (4 units)
5. Updates vruntime: `vruntime += exec_time * (20 - priority)`
6. Context switches or terminates process
7. Tracks wait time, turnaround time, and statistics

### Thread Synchronization (thread_*.c)
1. Creates two threads: ODD and EVEN
2. Shared counter starts at 1
3. Threads alternate printing numbers:
   - ODD thread prints odd numbers (1, 3, 5, ...)
   - EVEN thread prints even numbers (2, 4, 6, ...)
4. Uses mutex to protect shared counter
5. Uses condition variable to coordinate turns
6. Continues until counter reaches 100
7. Both threads join back to main thread
8. Main thread exits

## Example Output

### Process Scheduler
```
[TIME   0] Running Process: WebBrowser (PID:1)
           Priority: 15 | Nice: 5 | VRuntime: 0
           Remaining Time: 12 | Wait Time: 0

           [Time Quantum Expired]
           [CONTEXT SWITCH] Saving state of WebBrowser

[RUN QUEUE STATE] Processes: 4
  PID:1 WebBrowser   | VRuntime:20   | Remaining:8  | Priority:15
  PID:2 VideoPlayer  | VRuntime:0    | Remaining:8  | Priority:10
  ...
```

### Thread Synchronization
```
[THREAD START] ODD_THREAD created (TID: 140234567)
[MUTEX] ODD_THREAD acquired lock
[EXECUTE] ODD_THREAD printing: 1
[SIGNAL] ODD_THREAD signaling EVEN thread
[MUTEX] ODD_THREAD released lock

[MUTEX] EVEN_THREAD acquired lock
[EXECUTE] EVEN_THREAD printing: 2
[SIGNAL] EVEN_THREAD signaling ODD thread
...
```

## Linux Scheduler Concepts Explained

### Process States
- **READY**: Process is loaded in RAM, waiting for CPU
- **RUNNING**: Currently executing on CPU
- **WAITING**: Blocked on I/O or synchronization
- **TERMINATED**: Finished execution

### Synchronization Primitives
- **Mutex (Mutual Exclusion)**: Ensures only one thread accesses critical section
- **Condition Variable**: Allows threads to wait for specific conditions
- **Semaphore**: Controls access to shared resources (not shown here)

### Context Switch
When scheduler switches between processes/threads:
1. Save current process state (registers, PC, stack pointer)
2. Update process state (RUNNING → READY)
3. Select next process (lowest vruntime)
4. Load next process state
5. Update process state (READY → RUNNING)
6. Resume execution

### Virtual Runtime (vruntime)
- Tracks "fair" CPU time for each process
- Formula: `vruntime += actual_time * weight`
- Weight based on priority (nice value)
- Process with lowest vruntime runs next
- Ensures fairness across all processes

## Experiment Ideas

### Process Scheduler
- Change `TIME_QUANTUM` (1, 2, 8, 16)
- Modify process priorities (0-19)
- Add more processes
- Change burst times
- Observe how vruntime affects scheduling order

### Thread Synchronization
- Change `MAX_NUMBER` (50, 200, 1000)
- Add a third thread for multiples of 3
- Modify thread priorities
- Add delays to simulate I/O
- Remove mutex and observe race conditions

## Learning Path

1. **Start with**: `thread_simple.c` - Understand basic threading
2. **Then**: `scheduler.c` - Learn process scheduling
3. **Next**: `thread_scheduler.c` - See detailed thread behavior
4. **Finally**: `thread_advanced.c` - Complete implementation

## Key Takeaways

1. **Scheduler ensures fairness** using virtual runtime
2. **Priority affects CPU time** allocation
3. **Context switches** have overhead
4. **Synchronization primitives** prevent race conditions
5. **Thread states** transition based on scheduler decisions
6. **Mutex protects** critical sections
7. **Condition variables** coordinate thread execution
8. **Thread joining** ensures proper cleanup

# How Linux Scheduler Works - Complete Guide

## Overview

This repository contains complete C implementations demonstrating how the Linux scheduler manages processes and threads in RAM, including synchronization mechanisms.

## What Happens When Process Gets Execution in RAM

### 1. Process Loading
- Process binary loaded from disk into RAM
- Kernel creates Process Control Block (PCB) / task_struct
- Assigned initial priority and scheduling class
- Process enters READY state in run queue

### 2. Scheduler Components

#### Run Queue
- Per-CPU data structure holding ready processes
- Organized by scheduling policy (CFS uses red-black tree)
- Tracks minimum vruntime for fairness

#### Process States
```
NEW → READY → RUNNING → WAITING → TERMINATED
```

- **NEW**: Process created, not yet in run queue
- **READY**: In RAM, waiting for CPU time
- **RUNNING**: Currently executing on CPU
- **WAITING**: Blocked on I/O or synchronization
- **TERMINATED**: Finished execution

### 3. CFS (Completely Fair Scheduler)

#### Virtual Runtime (vruntime)
- Tracks "fair" CPU time for each process
- Formula: `vruntime += actual_runtime * weight`
- Weight based on nice value (-20 to +19)
- Process with lowest vruntime runs next

#### Scheduling Decision
```c
1. Timer interrupt (every ~1ms)
2. Scheduler checks if current process should continue
3. If not, pick process with minimum vruntime
4. Context switch to new process
5. Update statistics
```

### 4. Context Switch

When switching between processes:
```
1. Save current process state:
   - CPU registers
   - Program counter
   - Stack pointer
   - Memory mappings

2. Update process state (RUNNING → READY)

3. Select next process (lowest vruntime)

4. Load next process state:
   - Restore registers
   - Update page tables
   - Set program counter

5. Update state (READY → RUNNING)

6. Resume execution
```

### 5. Priority and Nice Values

- **Nice value**: -20 (highest) to +19 (lowest)
- **Priority**: 0-139 (0-99 real-time, 100-139 normal)
- Higher priority = more CPU time = slower vruntime growth

```c
// In our implementation
vruntime += exec_time * (20 - priority)

// Priority 18: vruntime += 4 * 2 = 8
// Priority 10: vruntime += 4 * 10 = 40
// Priority 18 gets more CPU time!
```

## Thread Synchronization

### 1. Thread Creation
- Threads share same address space (RAM)
- Each thread has own stack and registers
- Managed by same scheduler as processes
- Lighter weight than processes

### 2. Synchronization Primitives

#### Mutex (Mutual Exclusion)
```c
pthread_mutex_lock(&mutex);
// Critical section - only one thread at a time
shared_counter++;
pthread_mutex_unlock(&mutex);
```

**How it works:**
- Thread tries to acquire lock
- If locked: thread enters WAITING state
- If unlocked: thread acquires lock, enters RUNNING
- On unlock: wakes up one waiting thread

#### Condition Variables
```c
// Thread 1: Wait for condition
pthread_mutex_lock(&mutex);
while (!condition) {
    pthread_cond_wait(&cond, &mutex);  // Releases mutex, waits
}
// Condition met, mutex reacquired
pthread_mutex_unlock(&mutex);

// Thread 2: Signal condition
pthread_mutex_lock(&mutex);
condition = true;
pthread_cond_signal(&cond);  // Wake up one waiting thread
pthread_mutex_unlock(&mutex);
```

**How it works:**
- `pthread_cond_wait()`: Atomically releases mutex and sleeps
- Thread enters WAITING state
- `pthread_cond_signal()`: Wakes one waiting thread
- Woken thread reacquires mutex before continuing

### 3. Thread States in Our Implementation

```
Thread 1 (ODD):                    Thread 2 (EVEN):
READY                              READY
  ↓                                  ↓
RUNNING (acquires mutex)           WAITING (mutex locked)
  ↓                                  ↓
WAITING (cond_wait)                RUNNING (mutex acquired)
  ↓                                  ↓
READY (signaled)                   WAITING (cond_wait)
  ↓                                  ↓
RUNNING (mutex reacquired)         READY (signaled)
```

## Implementation Files

### 1. scheduler.c - Process Scheduler
**Demonstrates:**
- Process Control Blocks
- Run queue management
- CFS algorithm (vruntime)
- Priority-based scheduling
- Context switching
- Wait time and turnaround time

**Key Functions:**
- `init_process()` - Create process with priority
- `pick_next_task()` - Select process with min vruntime
- `schedule()` - Main scheduling loop
- `context_switch()` - Simulate state save/restore

### 2. thread_simple.c - Basic Threading
**Demonstrates:**
- Thread creation with pthread_create()
- Mutex for critical section protection
- Condition variables for coordination
- Thread joining with pthread_join()

**Key Concepts:**
- Two threads alternate printing odd/even numbers
- Mutex protects shared counter
- Condition variable ensures alternation
- Both threads join to main before exit

### 3. thread_scheduler.c - Detailed Threading
**Demonstrates:**
- Thread state transitions
- Context switch tracking
- Synchronization visualization
- Thread statistics (waits, switches)

**Additional Features:**
- Verbose logging of all operations
- State change notifications
- Lock acquisition/release tracking
- Signal/wait operation logging

### 4. thread_advanced.c - Complete Implementation
**Demonstrates:**
- Thread Control Blocks (TCB)
- Virtual runtime for threads
- Priority-based thread scheduling
- Complete state management
- Performance metrics

**Advanced Features:**
- TCB similar to kernel task_struct
- Virtual runtime tracking per thread
- Priority affects thread scheduling
- Comprehensive statistics

## How to Use

### Compile All
```bash
make
```

### Run Examples

#### 1. Process Scheduler
```bash
./scheduler
```
Shows how CFS schedules processes based on vruntime and priority.

#### 2. Simple Threading
```bash
./thread_simple
```
Clean output showing odd/even number printing with synchronization.

#### 3. Detailed Threading
```bash
./thread_scheduler
```
Verbose output showing all scheduler operations and state transitions.

#### 4. Advanced Threading
```bash
./thread_advanced
```
Complete implementation with TCBs, vruntime, and full statistics.

## Key Observations

### Process Scheduling
1. **Fairness**: CFS ensures all processes get fair CPU time
2. **Priority**: Higher priority processes get more CPU time
3. **Vruntime**: Tracks accumulated CPU time per process
4. **Context Switch**: Overhead when switching between processes
5. **Time Quantum**: Fixed time slice prevents starvation

### Thread Synchronization
1. **Mutex**: Prevents race conditions in critical sections
2. **Condition Variables**: Coordinate thread execution order
3. **State Transitions**: Threads move between READY/RUNNING/WAITING
4. **Context Switches**: Occur on lock contention and signals
5. **Thread Joining**: Ensures proper cleanup before exit

## Real Linux Kernel Comparison

### Our Implementation vs Real Kernel

| Feature | Our Implementation | Real Linux Kernel |
|---------|-------------------|-------------------|
| Scheduling | Simple CFS with vruntime | Full CFS with red-black tree |
| Time Quantum | Fixed 4 units | Dynamic based on load |
| Priority | 0-19 | 0-139 (RT + Normal) |
| Context Switch | Simulated | Full register save/restore |
| Run Queue | Array | Per-CPU red-black tree |
| Synchronization | pthread (userspace) | Futex (kernel) |

### What's Simplified
- No real-time scheduling classes
- No load balancing across CPUs
- No CPU affinity
- No cgroups or namespaces
- No I/O scheduling
- Simplified context switch

### What's Accurate
- CFS vruntime concept
- Priority-based scheduling
- Mutex and condition variables
- Thread state transitions
- Context switch overhead
- Fair scheduling algorithm

## Learning Outcomes

After studying these implementations, you understand:

1. **How scheduler picks next process** (lowest vruntime)
2. **How priority affects CPU time** (weight in vruntime calculation)
3. **What happens during context switch** (save/restore state)
4. **How threads synchronize** (mutex + condition variables)
5. **Why synchronization is needed** (prevent race conditions)
6. **How threads coordinate** (wait/signal pattern)
7. **What thread states mean** (READY/RUNNING/WAITING)
8. **How fairness is achieved** (virtual runtime tracking)

## Experiment Ideas

1. **Change priorities** - See how it affects scheduling order
2. **Modify time quantum** - Observe context switch frequency
3. **Add more processes/threads** - Test scalability
4. **Remove mutex** - Observe race conditions
5. **Change MAX_NUMBER** - Test with different workloads
6. **Add delays** - Simulate I/O operations
7. **Measure performance** - Compare different configurations

## Conclusion

These implementations demonstrate core Linux scheduler concepts:
- **CFS ensures fairness** through virtual runtime
- **Priority affects scheduling** through weight calculations
- **Context switches** enable multitasking
- **Synchronization primitives** prevent race conditions
- **Thread states** reflect scheduler decisions
- **Proper cleanup** requires thread joining

Understanding these concepts is fundamental to systems programming, operating systems design, and concurrent programming.

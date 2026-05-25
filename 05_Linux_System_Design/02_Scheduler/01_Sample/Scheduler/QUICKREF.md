# Quick Reference Guide

## Build Commands

```bash
# Build all programs
make

# Build individual programs
make scheduler
make thread_simple
make thread_scheduler
make thread_advanced

# Clean build artifacts
make clean
```

## Run Commands

```bash
# Process scheduler
./scheduler
make run_scheduler

# Simple threading
./thread_simple
make run_simple

# Detailed threading
./thread_scheduler
make run_scheduler_thread

# Advanced threading
./thread_advanced
make run_advanced
```

## File Structure

```
Scheduler/
├── scheduler.c              # Process scheduler with CFS
├── thread_simple.c          # Basic odd/even threading
├── thread_scheduler.c       # Detailed thread scheduling
├── thread_advanced.c        # Complete TCB implementation
├── Makefile                 # Build automation
├── README.md                # Main documentation
├── EXPLANATION.md           # Theory and concepts
├── OUTPUT.md                # Shell output examples
└── QUICKREF.md             # This file
```

## Key Concepts

### Process States
```
NEW → READY → RUNNING → WAITING → TERMINATED
```

### CFS Algorithm
```c
// Pick process with minimum vruntime
Process* next = min_vruntime_process(run_queue);

// Execute for time quantum
execute(next, TIME_QUANTUM);

// Update vruntime
next->vruntime += exec_time * (20 - priority);
```

### Thread Synchronization

#### Mutex (Critical Section)
```c
pthread_mutex_lock(&mutex);
// Only one thread can be here
shared_data++;
pthread_mutex_unlock(&mutex);
```

#### Condition Variable (Coordination)
```c
// Wait for condition
pthread_mutex_lock(&mutex);
while (!condition) {
    pthread_cond_wait(&cond, &mutex);
}
pthread_mutex_unlock(&mutex);

// Signal condition
pthread_mutex_lock(&mutex);
condition = true;
pthread_cond_signal(&cond);
pthread_mutex_unlock(&mutex);
```

### Context Switch
```
1. Save current process state
2. Update state (RUNNING → READY)
3. Select next process (lowest vruntime)
4. Load next process state
5. Update state (READY → RUNNING)
6. Resume execution
```

## Important Formulas

### Virtual Runtime
```
vruntime += actual_time * weight
weight = (20 - priority)
```

### Priority to Nice
```
nice = 20 - priority
priority range: 0-19
nice range: -20 to +19
```

### Scheduling Metrics
```
Turnaround Time = Completion Time - Arrival Time
Wait Time = Turnaround Time - Burst Time
Response Time = First Run Time - Arrival Time
```

## Common Modifications

### Change Time Quantum
```c
#define TIME_QUANTUM 8  // Change from 4 to 8
```

### Change Max Number
```c
#define MAX_NUMBER 200  // Change from 100 to 200
```

### Add More Processes
```c
Process p5;
init_process(&p5, 5, "NewProcess", 14, 15, SCHED_NORMAL);
add_to_queue(&queue, &p5);
```

### Change Priority
```c
// Higher number = higher priority = more CPU time
init_process(&p1, 1, "HighPriority", 19, 10, SCHED_NORMAL);
init_process(&p2, 2, "LowPriority", 5, 10, SCHED_NORMAL);
```

## Debugging Tips

### Enable More Logging
Add printf statements at key points:
```c
printf("[DEBUG] vruntime=%lld, priority=%d\n", p->vruntime, p->priority);
```

### Check Thread Synchronization
```c
// Verify mutex is protecting critical section
pthread_mutex_lock(&mutex);
printf("[LOCK] Thread %d acquired\n", thread_id);
// Critical section
pthread_mutex_unlock(&mutex);
printf("[UNLOCK] Thread %d released\n", thread_id);
```

### Monitor Context Switches
```c
void context_switch(Process* prev, Process* next) {
    printf("[SWITCH] %s -> %s\n", prev->name, next->name);
    context_switch_count++;
}
```

## Performance Analysis

### Measure Execution Time
```bash
time ./scheduler
time ./thread_simple
```

### Count Context Switches
```bash
./scheduler | grep "CONTEXT SWITCH" | wc -l
```

### Check Thread Statistics
```bash
./thread_advanced | grep "Statistics" -A 10
```

## Common Issues

### Race Condition
**Problem**: Threads accessing shared data without mutex
**Solution**: Always use mutex around shared data
```c
pthread_mutex_lock(&mutex);
shared_counter++;  // Protected
pthread_mutex_unlock(&mutex);
```

### Deadlock
**Problem**: Threads waiting for each other
**Solution**: Always acquire locks in same order
```c
// Thread 1 and 2 both do:
pthread_mutex_lock(&mutex1);
pthread_mutex_lock(&mutex2);
// work
pthread_mutex_unlock(&mutex2);
pthread_mutex_unlock(&mutex1);
```

### Starvation
**Problem**: Low priority process never runs
**Solution**: CFS prevents this with vruntime
```c
// Low priority process will eventually have lowest vruntime
// and will be scheduled
```

## Testing Scenarios

### Test 1: Equal Priority
```c
// All processes same priority
init_process(&p1, 1, "P1", 10, 8, SCHED_NORMAL);
init_process(&p2, 2, "P2", 10, 8, SCHED_NORMAL);
init_process(&p3, 3, "P3", 10, 8, SCHED_NORMAL);
// Should get equal CPU time
```

### Test 2: Priority Inversion
```c
// High priority with long burst
init_process(&p1, 1, "High", 19, 20, SCHED_NORMAL);
// Low priority with short burst
init_process(&p2, 2, "Low", 5, 4, SCHED_NORMAL);
// High priority should complete first
```

### Test 3: Many Threads
```c
#define MAX_NUMBER 1000  // Increase workload
// Observe context switch overhead
```

## Useful GDB Commands

```bash
# Compile with debug symbols
gcc -g -pthread thread_simple.c -o thread_simple

# Run with GDB
gdb ./thread_simple

# Set breakpoints
(gdb) break pthread_mutex_lock
(gdb) break pthread_cond_wait

# Run program
(gdb) run

# Check thread info
(gdb) info threads

# Switch threads
(gdb) thread 2

# Print variables
(gdb) print shared_data.counter
```

## Valgrind Memory Check

```bash
# Check for memory leaks
valgrind --leak-check=full ./thread_simple

# Check for thread errors
valgrind --tool=helgrind ./thread_simple
```

## Learning Path

1. **Start**: Read README.md
2. **Understand**: Read EXPLANATION.md
3. **See Output**: Read OUTPUT.md
4. **Run**: Execute thread_simple
5. **Explore**: Run scheduler
6. **Deep Dive**: Run thread_advanced
7. **Experiment**: Modify and recompile
8. **Debug**: Use GDB and Valgrind

## Additional Resources

### Linux Kernel Documentation
- CFS Scheduler: kernel.org/doc/html/latest/scheduler/
- Process Management: kernel.org/doc/html/latest/process/

### Books
- "Operating System Concepts" by Silberschatz
- "Linux Kernel Development" by Robert Love
- "The Linux Programming Interface" by Michael Kerrisk

### Man Pages
```bash
man pthread_create
man pthread_mutex_lock
man pthread_cond_wait
man sched
```

## Quick Troubleshooting

| Problem | Solution |
|---------|----------|
| Compilation error | Check gcc version, install pthread |
| Segmentation fault | Check array bounds, null pointers |
| Threads not alternating | Check condition variable logic |
| Deadlock | Check mutex lock/unlock order |
| High CPU usage | Add usleep() delays |
| Inconsistent output | Add mutex around printf |

## Contact & Contribution

For questions or improvements:
1. Check EXPLANATION.md for theory
2. Check OUTPUT.md for expected behavior
3. Modify code and experiment
4. Document your findings

Happy Learning! 🚀

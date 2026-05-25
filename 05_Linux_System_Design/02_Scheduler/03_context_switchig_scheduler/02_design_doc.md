# TCB-Based Cooperative Scheduler — System Design Document

> **File:** `01_tcb_scheduler.c`  
> **Build:** `gcc -Wall -Wextra -pthread 01_tcb_scheduler.c -o tcb_sched`  
> **Run:** `./tcb_sched`

---

## 1. Overview

This implementation builds a **cooperative, round-robin scheduler** entirely from
scratch around two pthreads.  Instead of relying on the OS to manage which thread
runs next, our scheduler explicitly:

1. Owns a **Task Control Block (TCB)** per thread — the single source of truth for
   all per-thread state, including a simulated CPU register save area.
2. Calls **TCB LOAD** to hand the CPU to a thread (restores the context frame,
   signals the thread's condvar).
3. Calls **TCB UNLOAD** when the thread gives up the CPU (saves the context frame,
   transitions state, re-enqueues or blocks the thread).
4. Drives a **round-robin ready queue** to select the next thread.

---

## 2. Architecture

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │                          main() thread                              │
  │                                                                     │
  │  scheduler_init()  →  scheduler_register_thread() × 2               │
  │         │                        │                                  │
  │         │                 allocates TCB[0] ←── ODD_THREAD  ──┐      │
  │         │                 allocates TCB[1] ←── EVEN_THREAD ──┤      │
  │         │                                                    │      │
  │         └──────────────── scheduler_run() ───────────────────┘      │
  │                                │                                    │
  │                    ┌───────────▼────────────┐                       │
  │                    │      Scheduler_t       │                       │
  │                    │  ┌─────────────────┐   │                       │
  │                    │  │  ready_queue[]  │   │  circular FIFO        │
  │                    │  │  [TID0, TID1]   │   │  round-robin          │
  │                    │  └────────┬────────┘   │                       │
  │                    │           │ dequeue    │                       │
  │                    │  ┌────────▼────────┐   │                       │
  │                    │  │  tcb_load(next) │   │  ← TCB LOAD           │
  │                    │  └────────┬────────┘   │                       │
  │                    │           │ signal     │                       │
  │                    │           │ sched_cond │                       │
  │                    └───────────┼────────────┘                       │
  │                                │                                    │
  │               ┌────────────────┴─────────────────┐                  │
  │               ▼                                  ▼                  │
  │       ┌──────────────┐                  ┌──────────────┐            │
  │       │  TCB[0]      │                  │  TCB[1]      │            │
  │       │  ODD_THREAD  │                  │  EVEN_THREAD │            │
  │       │  state=READY │                  │  state=READY │            │
  │       │  sim_PC=...  │                  │  sim_PC=...  │            │
  │       │  sim_SP=...  │                  │  sim_SP=...  │            │
  │       └──────┬───────┘                  └──────┬───────┘            │
  │              │ pthread                         │ pthread            │
  │              ▼                                 ▼                    │
  │       [odd_thread_func]               [even_thread_func]            │
  │       prints: 1,3,5,...               prints: 2,4,6,...             │
  └─────────────────────────────────────────────────────────────────────┘
```

---

## 3. TCB Memory Layout

Every registered thread gets exactly one TCB on the heap (`calloc`).

```
  Offset  Field                Type            Description
  ──────  ─────────────────    ──────────────  ──────────────────────────────
     0    tid                  int             Scheduler-assigned thread ID
     4    name[32]             char[]          Human-readable label
    36    state                thread_state_t  Current FSM state (enum int)
    40    pthread_id           pthread_t       Underlying OS thread handle
    48    func                 void*(*)(void*) Entry function pointer
    56    arg                  void*           Argument for entry function
    ────  ── Context Frame ─────────────────  ── Saved on UNLOAD, read on LOAD ──
    64    sim_pc               unsigned long   Program Counter (resume address)
    72    sim_sp               unsigned long   Stack Pointer (stack top)
    80    sim_lr               unsigned long   Link Register (return address)
    88    sim_r0               int             General reg R0 (counter value)
    92    sim_r1               int             General reg R1 (odd_turn flag)
    ────  ── Scheduling Metadata ──────────  ──────────────────────────────
    96    priority             int             1 (low) – 10 (high)
   100    time_quantum_us      int             Allotted time slice in µs
    ────  ── Statistics ───────────────────  ──────────────────────────────
   104    n_context_saves      int             Times tcb_unload() was called
   108    n_context_loads      int             Times tcb_load() was called
   112    n_yields             int             Times scheduler_yield() was called
   116    n_blocks             int             Times scheduler_block() was called
   120    total_cpu_time_us    long            Cumulative CPU time consumed
   128    last_scheduled       struct timeval  Timestamp of last TCB load
```

> In Linux `struct task_struct`, the equivalent register save area is
> `struct thread_struct thread` which holds `sp` (stack pointer) and
> is populated by the `cpu_switch_to()` assembly macro.

---

## 4. Scheduler Memory Layout

```
  Scheduler_t (global g_sched)
  ┌────────────────────────────────────────────────────────┐
  │  tcb_pool[0] ──────────────────────► TCB[0] (ODD)      │
  │  tcb_pool[1] ──────────────────────► TCB[1] (EVEN)     │
  │  n_threads   = 2                                       │
  ├────────────────────────────────────────────────────────┤
  │  Ready Queue (circular FIFO)                           │
  │                                                        │
  │  ready_queue[] = [ TID0, TID1, _, _, _, _, _, _ ]      │
  │                     ↑head              ↑tail           │
  │  rq_head = 0                                           │
  │  rq_tail = 2                                           │
  │  rq_size = 2                                           │
  ├────────────────────────────────────────────────────────┤
  │  current_tid = 0  (ODD is on CPU)                      │
  ├────────────────────────────────────────────────────────┤
  │  sched_lock       — global mutex                       │
  │  sched_cond[0]    — condvar: wakes ODD_THREAD          │
  │  sched_cond[1]    — condvar: wakes EVEN_THREAD         │
  │  sched_wake       — condvar: wakes scheduler loop      │
  ├────────────────────────────────────────────────────────┤
  │  total_context_switches = N                            │
  │  scheduler_cycles       = N                            │
  │  start_time             = (struct timeval)             │
  └────────────────────────────────────────────────────────┘
```

---

## 5. Thread State Machine

```
                         scheduler_register_thread()
                                    │
                                    ▼
                               ┌─────────┐
                               │   NEW   │
                               └────┬────┘
                                    │ pthread spawned + enqueued
                                    ▼
                  ┌─────────────────────────────────────┐
          yield() │                                     │ tcb_load()
         ─────────►         R E A D Y                  ◄─────────────
                  │         (in ready_queue)            │
                  └─────────────────────────────────────┘
                                    │ tcb_load() → signal sched_cond
                                    ▼
                  ┌─────────────────────────────────────┐
          block() │                                     │
         ─────────►         R U N N I N G               │ yield()
        terminate │         (on CPU)                    ├──────────────►  READY
                  └──────────────┬──────────────────────┘
                                 │ block()
                                 ▼
                  ┌─────────────────────────────────────┐
                  │         B L O C K E D               │
                  │         (not in ready_queue)        │
                  │         invisible to scheduler      │
                  └──────────────┬──────────────────────┘
                                 │ scheduler_unblock(tid)
                                 ▼
                              READY  (re-enqueued)

                         terminate()
                             │
                             ▼
                  ┌─────────────────────────────────────┐
                  │       T E R M I N A T E D           │
                  │       (never enqueued again)        │
                  └─────────────────────────────────────┘
```

---

## 6. Ready Queue — Circular FIFO Array

```
  Initial state after registering 2 threads:

  Index :  [0]     [1]     [2]  [3]  [4]  [5]  [6]  [7]
           TID=0   TID=1    _    _    _    _    _    _
            ↑head           ↑tail
  rq_size = 2

  After scheduler dequeues TID=0 (dispatches ODD):
  Index :  [0]     [1]     [2]  ...
           TID=0   TID=1    _
                    ↑head   ↑tail
  rq_size = 1

  After ODD yields (re-enqueues itself at tail):
  Index :  [0]     [1]     [2]  ...
           TID=0   TID=1   TID=0
                    ↑head        ↑tail
  rq_size = 2    → next pick: TID=1 (EVEN)  ← round-robin achieved
```

---

## 7. Execution Timeline — Full Flow from Start to Finish

Numbers in parentheses show `g_counter` value at that moment.

```
  ══════════════════════════════════════════════════════════════════════
  STARTUP
  ══════════════════════════════════════════════════════════════════════
  main()
    scheduler_init()          → g_sched initialised, no threads yet
    register_thread(ODD)      → TCB[0] alloc'd, pthread[0] spawned
                                 pthread[0] parks on sched_cond[0]
                                 TID=0 enqueued  → queue=[0]
    register_thread(EVEN)     → TCB[1] alloc'd, pthread[1] spawned
                                 pthread[1] parks on sched_cond[1]
                                 TID=1 enqueued  → queue=[0,1]
    scheduler_run()           → dispatch loop begins

  ══════════════════════════════════════════════════════════════════════
  SCHEDULER CYCLE #1  (counter=1, odd_turn=true)
  ══════════════════════════════════════════════════════════════════════
  Scheduler:
    rq_dequeue()  → TID=0 (ODD)    queue=[1]
    tcb_load(TCB[0])
      │  TCB[0].state: READY → RUNNING
      │  Print TCB LOAD box (sim_PC/SP/LR/R0/R1 all zero — first run)
      └─ signal sched_cond[0]       ← wakes ODD pthread
    pthread_cond_wait(sched_wake)   ← scheduler releases lock, parks

  ODD_THREAD wakes:
    exits thread_entry_wrapper condvar loop
    releases sched_lock
    enters odd_thread_func()
    acquires sched_lock
    g_counter=1 ≤ 20                ← not done
    g_odd_turn=true                 ← IT IS MY TURN
    prints "1"
    g_counter=2, g_odd_turn=false
    EVEN state=READY (already in queue) → no unblock needed
    releases sched_lock
    scheduler_yield()
      │  acquires sched_lock
      │  TCB UNLOAD: saves sim_PC/SP/LR/R0(=2)/R1(=0) into TCB[0]
      │  TCB[0].state: RUNNING → READY
      │  TID=0 re-enqueued         → queue=[1,0]
      │  signal sched_wake         ← wakes scheduler
      └─ parks on sched_cond[0]

  ══════════════════════════════════════════════════════════════════════
  SCHEDULER CYCLE #2  (counter=2, odd_turn=false)
  ══════════════════════════════════════════════════════════════════════
  Scheduler wakes from sched_wake:
    total_context_switches=1
    rq_dequeue()  → TID=1 (EVEN)   queue=[0]
    tcb_load(TCB[1])
      │  TCB[1].state: READY → RUNNING
      │  Print TCB LOAD box (all-zero context — first run for EVEN)
      └─ signal sched_cond[1]       ← wakes EVEN pthread
    pthread_cond_wait(sched_wake)

  EVEN_THREAD wakes:
    exits thread_entry_wrapper condvar loop
    enters even_thread_func()
    g_counter=2, g_odd_turn=false   ← IT IS MY TURN
    prints "2"
    g_counter=3, g_odd_turn=true
    ODD state=READY → no unblock needed
    scheduler_yield()
      │  TCB UNLOAD: saves context into TCB[1]
      │  TID=1 re-enqueued         → queue=[0,1]
      │  signal sched_wake
      └─ parks on sched_cond[1]

  ══════════════════════════════════════════════════════════════════════
  SCHEDULER CYCLE #3  (counter=3, odd_turn=true)
  ══════════════════════════════════════════════════════════════════════
  Scheduler:
    rq_dequeue()  → TID=0 (ODD)    queue=[1]
    tcb_load(TCB[0])
      │  TCB[0].state: READY → RUNNING
      │  Print TCB LOAD box — sim_PC/SP/LR now show REAL saved addresses
      │  sim_R0=2 (counter at last save), sim_R1=0 (odd_turn at save)
      └─ signal sched_cond[0]
    pthread_cond_wait(sched_wake)

  ODD_THREAD wakes from scheduler_yield's condvar:
    exits yield() condvar loop, releases sched_lock
    re-enters while(1) in odd_thread_func()
    acquires sched_lock
    g_counter=3, g_odd_turn=true    ← my turn again
    prints "3"
    ... same flow ...

  ══════════════════════════════════════════════════════════════════════
  BLOCKING SCENARIO  (when round-robin picks the wrong thread)
  ══════════════════════════════════════════════════════════════════════

  Suppose queue=[1,0] and counter=5, odd_turn=true:

  Scheduler:
    rq_dequeue()  → TID=1 (EVEN)   ← round-robin picks EVEN, but it's ODD's turn
    tcb_load(TCB[1])  → signal sched_cond[1]

  EVEN_THREAD:
    acquires sched_lock
    g_odd_turn=true                 ← NOT my turn
    prints "[EVEN] not my turn → BLOCKING"
    releases sched_lock
    scheduler_block()
      │  acquires sched_lock
      │  TCB UNLOAD: saves context into TCB[1]
      │  TCB[1].state: RUNNING → BLOCKED
      │  NOT re-enqueued           → queue=[0]   (EVEN invisible to scheduler)
      │  signal sched_wake
      └─ parks on sched_cond[1]

  Scheduler:
    rq_dequeue()  → TID=0 (ODD)    ← only READY thread
    tcb_load(TCB[0])  → signal sched_cond[0]

  ODD_THREAD:
    g_odd_turn=true                 ← my turn
    prints "5"
    g_counter=6, g_odd_turn=false
    TCB[1].state == BLOCKED         ← unblock EVEN
    scheduler_unblock(TID=1): TCB[1].state=READY, TID=1 enqueued → queue=[0,1]
    releases sched_lock
    scheduler_yield()
      │  TCB UNLOAD: saves context
      │  TID=0 enqueued            → queue=[1,0]
      │  signal sched_wake
      └─ parks on sched_cond[0]

  Scheduler → picks TID=1 (EVEN), which now has correct turn

  ══════════════════════════════════════════════════════════════════════
  TERMINATION  (after printing MAX_NUMBER=20)
  ══════════════════════════════════════════════════════════════════════

  After EVEN prints 20: g_counter=21, g_odd_turn=true
    unblock ODD if BLOCKED
    scheduler_yield()  → TID=1 re-enqueued

  Scheduler:
    dequeues TID=0 (ODD)
    tcb_load(TCB[0])

  ODD_THREAD:
    g_counter=21 > 20               ← DONE
    releases sched_lock, break
    scheduler_terminate()
      │  unblock any BLOCKED peers
      │  TCB UNLOAD (state → TERMINATED)
      │  signal sched_wake
      └─ returns (pthread exits)

  Scheduler:
    dequeues TID=1 (EVEN)
    tcb_load(TCB[1])

  EVEN_THREAD:
    g_counter=21 > 20               ← DONE
    scheduler_terminate()

  Scheduler:
    alive=0 → break → scheduler_run() returns

  main():
    pthread_join(t0), pthread_join(t1)
    print_final_stats()
    cleanup
    return 0
```

---

## 8. TCB LOAD / TCB UNLOAD — Internal Mechanics

### TCB UNLOAD (context save)

```
  Called from: scheduler_yield()     → new state = READY
               scheduler_block()     → new state = BLOCKED
               scheduler_terminate() → new state = TERMINATED

  Actions:
    1.  gettimeofday(&now)
        slice_us = now − last_scheduled         ← CPU time this slice
        total_cpu_time_us += slice_us

    2.  tcb->sim_pc = __builtin_return_address(0)   ← resume address
        tcb->sim_sp = &local_variable               ← stack approximation
        tcb->sim_lr = sim_pc − 8                    ← link register
        tcb->sim_r0 = g_counter                     ← R0: task variable
        tcb->sim_r1 = (int)g_odd_turn               ← R1: task variable

    3.  prev_state = tcb->state
        tcb->state = new_state
        tcb->n_context_saves++

    4.  Print TCB UNLOAD box showing all fields

  Real kernel equivalent (ARM64 cpu_switch_to):
    stp  x19, x20, [sp, #-16]!    ← push callee-saved regs
    stp  x21, x22, [sp, #-16]!
    ...
    str  sp,  [prev_tcb, #thread_sp_off]   ← save SP into TCB
    str  x30, [prev_tcb, #thread_pc_off]   ← save LR (=PC) into TCB
```

### TCB LOAD (context restore)

```
  Called from: scheduler_run() dispatch loop only

  Actions:
    1.  gettimeofday(&last_scheduled)              ← start CPU timer
    2.  prev = tcb->state
        tcb->state = STATE_RUNNING
        tcb->n_context_loads++
        s->current_tid = tcb->tid

    3.  Print TCB LOAD box — reads back all sim_* fields

    4.  pthread_cond_signal(&s->sched_cond[tid])   ← hands CPU to thread
        (thread wakes, re-acquires sched_lock, exits condvar loop)

  Real kernel equivalent (ARM64 cpu_switch_to):
    ldr  sp,  [next_tcb, #thread_sp_off]   ← restore SP from TCB
    ldr  x30, [next_tcb, #thread_pc_off]   ← restore PC (LR) from TCB
    ldp  x19, x20, [sp], #16               ← pop callee-saved regs
    ...
    ret                                     ← jump to restored PC
```

---

## 9. Call Graph

```
  main()
  ├── scheduler_init()
  │     └── pthread_mutex_init / pthread_cond_init × (MAX_THREADS+1)
  │
  ├── scheduler_register_thread() × 2
  │     ├── calloc(TCB)
  │     ├── pthread_create()  ──► thread_entry_wrapper()
  │     │                              └── waits on sched_cond[tid]
  │     └── rq_enqueue(tid)
  │
  └── scheduler_run()                          ← main dispatch loop
        │
        ├── rq_dequeue()  → next_tid
        ├── tcb_load(next)
        │     └── pthread_cond_signal(sched_cond[tid])
        │                     │
        │                     ▼ (thread wakes)
        │              thread_entry_wrapper exits → odd/even_thread_func()
        │                     │
        │              pthread_mutex_lock(sched_lock)
        │              check g_counter / g_odd_turn
        │                     │
        │              ┌──────┴──────────────────────────┐
        │              │ my turn                         │ not my turn
        │              │                                 │
        │              │ print number                    └── scheduler_block()
        │              │ scheduler_unblock(peer)               ├── tcb_unload(BLOCKED)
        │              │ pthread_mutex_unlock                   ├── signal sched_wake
        │              │ scheduler_yield()                      └── wait sched_cond
        │              │   ├── tcb_unload(READY)
        │              │   ├── rq_enqueue(self)
        │              │   ├── signal sched_wake  ──────────► scheduler wakes
        │              │   └── wait sched_cond               total_cs++, next cycle
        │              │
        │              └── (counter > MAX) → break
        │                        scheduler_terminate()
        │                          ├── scheduler_unblock(peers)
        │                          ├── tcb_unload(TERMINATED)
        │                          └── signal sched_wake  ──► scheduler checks alive=0
        │
        └── (all alive==0) → break → scheduler_run returns

  main() continues:
    pthread_join() × n_threads
    print_final_stats()
    free(TCB) × n_threads
    pthread_mutex_destroy / pthread_cond_destroy
```

---

## 10. Synchronisation Model

```
  ┌─────────────────────────────────────────────────────────────────┐
  │  sched_lock  (pthread_mutex_t)                                  │
  │                                                                 │
  │  Protects:  ready_queue, current_tid, all TCB states,           │
  │             g_counter, g_odd_turn                               │
  │                                                                 │
  │  Invariant: exactly ONE entity holds sched_lock at any time.    │
  │    • When scheduler is running:  sched_lock held                │
  │    • When thread is on CPU:      sched_lock held (or briefly    │
  │                                   released then re-acquired)    │
  │    • During condvar wait:        sched_lock atomically released │
  └─────────────────────────────────────────────────────────────────┘

  sched_cond[tid]  — per-thread condvar
    Signaled by:  tcb_load()         (scheduler → thread)
    Waited on by: thread in yield(), block(), entry_wrapper

  sched_wake  — scheduler condvar
    Signaled by:  scheduler_yield()      (thread → scheduler)
                  scheduler_block()
                  scheduler_terminate()
    Waited on by: scheduler_run() dispatch loop

  No lost-wakeup possible:
    tcb_load() signals sched_cond[tid] while holding sched_lock.
    The woken thread must re-acquire sched_lock before it can
    signal sched_wake.  The scheduler calls pthread_cond_wait
    (releasing sched_lock) AFTER signaling the thread — so by the
    time the thread can call signal sched_wake, the scheduler is
    already in its wait. ✓
```

---

## 11. Expected Output (MAX_NUMBER = 20)

```
  [SCHEDULER INIT]  ...
  [REGISTER]  TID=0  ODD_THREAD    ...
  [REGISTER]  TID=1  EVEN_THREAD   ...
  [SCHEDULER RUN]  Dispatch loop started

  [SCHEDULER CYCLE #1]   Dispatching TID=0  ODD_THREAD
  ╔══ TCB LOAD   : ODD_THREAD    ═══...═╗
  ║  TID = 0  State: READY → RUNNING    ║
  ║  sim_PC = 0x0000000000000000        ║   ← zero: first run
  ...
  ╚═════════════════════════════════════╝

  >>>  [ODD_THREAD ] prints:   1

  [YIELD]  ODD_THREAD   giving up CPU  (yield #1)
  ╔══ TCB UNLOAD : ODD_THREAD    ═══...═╗
  ║  State: RUNNING → READY             ║
  ║  sim_PC = 0x00007f4a1b2c3d4e        ║   ← real address
  ║  sim_R0 = 2   (counter at save)     ║
  ...
  ╚═════════════════════════════════════╝

  [SCHEDULER CYCLE #2]   Dispatching TID=1  EVEN_THREAD
  ...
  >>>  [EVEN_THREAD] prints:   2
  ...
  >>>  [ODD_THREAD ] prints:   3
  >>>  [EVEN_THREAD] prints:   4
  ...
  >>>  [ODD_THREAD ] prints:  19
  >>>  [EVEN_THREAD] prints:  20

  [TERMINATE] ODD_THREAD   Saves=11  Loads=11  Yields=10  Blocks=0
  [TERMINATE] EVEN_THREAD  Saves=11  Loads=11  Yields=10  Blocks=0

  [FINAL STATS]
  Scheduler cycles       : 22
  Total context switches : 22
  Thread ODD_THREAD   Context saves: 11  loads: 11  yields: 10  blocks: 0
  Thread EVEN_THREAD  Context saves: 11  loads: 11  yields: 10  blocks: 0
```

> Set `MAX_NUMBER 100` to see 100 numbers and 102 context switches.

---

## 12. Build & Run

```bash
# Compile
gcc -Wall -Wextra -pthread 01_tcb_scheduler.c -o tcb_sched

# Run (default MAX_NUMBER=20)
./tcb_sched

# Run with full output (pipe to less)
./tcb_sched | less

# Verify all numbers in order
./tcb_sched | grep "prints:" | awk '{print $NF}'

# Count total context switches
./tcb_sched | grep -c "TCB LOAD"
```

---

## 13. Comparison with `00_multithreading.c`

| Aspect                  | `00_multithreading.c`        | `01_tcb_scheduler.c`              |
|-------------------------|------------------------------|-----------------------------------|
| Scheduling control      | OS kernel                    | Our custom scheduler              |
| Context switch trigger  | `pthread_cond_wait` (OS)     | `scheduler_yield()` (explicit)    |
| Context save/restore    | Kernel stack (invisible)     | TCB context frame (visible, logged) |
| Ready queue             | OS run queue (invisible)     | `ready_queue[]` circular array    |
| Thread state tracking   | OS internal                  | `TCB_t.state` (5 states)          |
| Per-switch logging      | None                         | Full TCB LOAD/UNLOAD box          |
| Statistics              | None                         | Saves, loads, yields, CPU time    |
| Blocking mechanism      | `pthread_cond_wait`          | `scheduler_block()` / `_unblock()`|

# TCB Scheduler — Output Log Explained

This document walks through **every section of the program output**, line by line,
and explains what it means in terms of scheduler internals.

> Run: `gcc -Wall -Wextra -pthread 01_tcb_scheduler.c -o tcb_sched.exe && .\tcb_sched.exe`
> Unicode box characters render correctly in UTF-8 terminals (MSYS2 bash, Linux).
> In Windows PowerShell run `chcp 65001` first for correct display.

---

## 1. Banner

```
===================================================================
  TCB-Based Cooperative Scheduler — System Design Implementation
  Task : Print 1 … 20 alternating ODD / EVEN threads
===================================================================
```

**What it means:** Program entry point. Printed once from `main()` before any
thread or scheduler object exists.

---

## 2. Scheduler Initialisation

```
  [SCHEDULER INIT]
  Algorithm   : Cooperative Round-Robin
  Max Threads : 8
  Quantum     : 1000 µs
  Queue       : Circular FIFO array  (head → dequeue, tail → enqueue)
  Context     : sim_PC / sim_SP / sim_LR / sim_R0 / sim_R1
```

| Field | Meaning |
|---|---|
| `Algorithm` | Threads give up the CPU voluntarily (`scheduler_yield` / `scheduler_block`). No timer interrupt. |
| `Max Threads` | `MAX_THREADS` compile-time constant — size of `tcb_pool[]` and `ready_queue[]` arrays. |
| `Quantum` | Advisory time slice stored in `TCB_t.time_quantum_us`. Not enforced in this cooperative model. |
| `Queue` | `ready_queue[MAX_THREADS]` circular integer array. `rq_head` = next to dequeue; `rq_tail` = next write slot. |
| `Context` | Five simulated CPU registers saved in each TCB on every context switch. |

Called from: `scheduler_init(&g_sched)` inside `main()`.

---

## 3. Thread Registration

```
  [REGISTER]  TID=0  ODD_THREAD    Priority=5  Quantum=1000 µs
              TCB allocated at 00000274f8bb1c40  (size=136 bytes)
  [THREAD BORN]  TID=0  ODD_THREAD    pthread=0x1  State: NEW → READY
              State: NEW → READY  |  pthread=0x1
  [READY QUEUE] size=1 [ TID0:ODD_THREAD ]
```

**Sequence inside `scheduler_register_thread()`:**

1. `[REGISTER]` — A new `TCB_t` is heap-allocated (`malloc`). Its address and
   size are printed. The thread is assigned the next available TID (0, then 1, …).

2. `TCB allocated at <addr>` — The raw pointer value of `tcb_pool[tid]`. You can
   use this to correlate with a debugger watch.

3. `[THREAD BORN]` — `pthread_create` succeeded. The new thread immediately parks
   itself on `sched_cond[tid]` inside `thread_entry_wrapper()`, waiting for its
   first dispatch signal. State changes `NEW → READY`.

4. `[READY QUEUE]` — The TID is appended at `rq_tail`. The queue contents are
   printed as `TID<n>:<name>`.

After both threads are registered:

```
  [MAIN]  Both threads registered — handing control to scheduler
```

`main()` calls `scheduler_run(&g_sched)` and blocks there for the lifetime of
the program.

---

## 4. Scheduler Dispatch Loop Start

```
  [SCHEDULER RUN]  Dispatch loop started
```

Printed once at the top of the `while` loop in `scheduler_run()`. The loop
continues until all threads have reached `TERMINATED` state.

---

## 5. Scheduler Cycle Header

```
  [SCHEDULER CYCLE #1  ]  Dispatching TID=0  ODD_THREAD
```

| Part | Meaning |
|---|---|
| `#1` | `scheduler_cycles` counter, incremented each loop iteration. |
| `Dispatching TID=0` | TID dequeued from `rq_head`. This is the thread about to receive the CPU. |
| `ODD_THREAD` | Human-readable name from `TCB_t.name`. |

---

## 6. TCB LOAD Box

```
  +-- TCB LOAD   : ODD_THREAD   ------------------------------------+
  |  TID        = 0      State : READY     --> RUNNING             |
  |  sim_PC     = 0x0000000000000000   (resume here)               |
  |  sim_SP     = 0x0000000000000000   (stack restored)            |
  |  sim_LR     = 0x0000000000000000   (return address)            |
  |  sim_R0     = 0          (counter at last save)                |
  |  sim_R1     = 0          (odd_turn at last save)               |
  |  Priority   = 5       Quantum = 1000   µs                      |
  |  Loads=1    Saves=0    CPU total = 0      µs                   |
  +----------------------------------------------------------------+
```

Printed inside `tcb_load()`. This is the **context restore** step — the
equivalent of a real kernel writing saved registers back to CPU hardware.

| Field | Source in code | Meaning |
|---|---|---|
| `State : READY --> RUNNING` | `tcb->state = RUNNING` | State machine transition. |
| `sim_PC` | `tcb->sim_pc` | **Program Counter** — address where this thread will resume execution. Zero on first load (thread hasn't run yet). On subsequent loads it holds the address captured at the last `tcb_unload`. |
| `sim_SP` | `tcb->sim_sp` | **Stack Pointer** — address of a local variable captured at save time, approximating the thread's stack top. |
| `sim_LR` | `tcb->sim_lr` | **Link Register** — `__builtin_return_address(0)` at save time, i.e. the return address of the function that called `scheduler_yield`. |
| `sim_R0` | `tcb->sim_r0` | General register — holds `g_counter` value snapshotted at last save. |
| `sim_R1` | `tcb->sim_r1` | General register — holds `g_odd_turn` snapshotted at last save. |
| `Priority` | `tcb->priority` | Scheduling priority (not used for ordering in this round-robin model). |
| `Quantum` | `tcb->time_quantum_us` | Configured time slice in microseconds. |
| `Loads` | `tcb->n_context_loads` | How many times this TCB has been loaded (dispatched) since creation. |
| `Saves` | `tcb->n_context_saves` | How many times this TCB has been saved (unloaded) since creation. |
| `CPU total` | `tcb->total_cpu_time_us` | Cumulative CPU time used across all slices. |

After printing the LOAD box, `tcb_load()` signals `sched_cond[tid]` — the thread
wakes from `pthread_cond_wait` and resumes user code.

---

## 7. Thread Print Line

```
  >>>  [ODD_THREAD ] prints:   1
```

Printed by `odd_thread_func()` / `even_thread_func()` when the thread has the
CPU and it is its turn. The `>>>` prefix visually distinguishes application output
from scheduler log lines.

---

## 8. Yield Line

```
  [YIELD]  ODD_THREAD    giving up CPU voluntarily  (yield #1)
```

| Part | Meaning |
|---|---|
| `giving up CPU voluntarily` | Thread called `scheduler_yield(&g_sched)`. |
| `yield #1` | `tcb->n_yields` counter — how many times this thread has yielded so far. |

After printing this, `scheduler_yield()` calls `tcb_unload()` then waits on
`sched_cond[tid]` until the scheduler dispatches it again.

---

## 9. TCB UNLOAD Box

```
  +-- TCB UNLOAD : ODD_THREAD   ------------------------------------+
  |  TID        = 0      State : RUNNING   --> READY               |
  |  sim_PC     = 0x00007ff75baa16f1   (resume address)            |
  |  sim_SP     = 0x000000c4293ffc68   (stack top)                 |
  |  sim_LR     = 0x00007ff75baa16e9   (return address)            |
  |  sim_R0     = 2          (counter at save)                     |
  |  sim_R1     = 0          (odd_turn at save)                    |
  |  CPU slice  = 152    µs  Saves=1    Loads=1    Yields=1        |
  +----------------------------------------------------------------+
```

Printed inside `tcb_unload()`. This is the **context save** step.

| Field | How it is captured | Meaning |
|---|---|---|
| `State : RUNNING --> READY` | New state passed by caller | `READY` on yield, `BLOCKED` on block, `TERMINATED` on exit. |
| `sim_PC` | `(uintptr_t)__builtin_return_address(0)` | GCC built-in: the real return address of the current call frame — approximates the thread's PC at the point it called yield/block. |
| `sim_SP` | `(uintptr_t)&local_var` inside `tcb_unload` | Address of a stack-local variable — approximates current stack pointer. |
| `sim_LR` | `(uintptr_t)__builtin_return_address(0)` | Same as PC in this simulation; in a real ARM/RISC-V kernel these are distinct registers. |
| `sim_R0` | `g_counter` | Counter value at the moment the thread saves context. Incremented by 1 each print, so the next ODD load will see `sim_R0=2` meaning the counter is now 2. |
| `sim_R1` | `g_odd_turn` | Turn flag snapshot. `0` = EVEN's turn next, `1` = ODD's turn next. |
| `CPU slice` | `gettimeofday` delta | Wall-clock time the thread spent running in this single dispatch (load → unload). |

After `tcb_unload()` returns the TID is re-enqueued (if state is `READY`) and
`sched_wake` is signalled so the scheduler loop wakes.

---

## 10. Ready Queue Print (after each yield)

```
  [READY QUEUE] size=2 [ TID1:EVEN_THREAD TID0:ODD_THREAD ]
```

Printed after the yielding thread is re-enqueued. The order shows the true
FIFO queue state — the **leftmost** TID is `rq_head` (next to run).

---

## 11. Block / Unblock Lines

When a thread checks the turn flag and it is not its turn:

```
  [BLOCK]  EVEN_THREAD   waiting for turn  (block #1)
```

When the peer finishes its print and calls `scheduler_unblock`:

```
  [UNBLOCK]  ODD_THREAD  woke up  EVEN_THREAD
  [READY QUEUE] size=1 [ TID1:EVEN_THREAD ]
```

| Event | State transition | Queue change |
|---|---|---|
| `[BLOCK]` | `RUNNING → BLOCKED` | TID **not** added to ready queue |
| `[UNBLOCK]` | `BLOCKED → READY` | TID added at `rq_tail` |

The TCB UNLOAD box printed at block time shows `State : RUNNING --> BLOCKED`.

---

## 12. Terminate Lines

When a thread detects `g_counter > MAX_NUMBER` it calls `scheduler_terminate()`:

```
  [TERMINATE] ODD_THREAD   counter=21 >= MAX=20, exiting

  +-- TCB UNLOAD : ODD_THREAD   ------------------------------------+
  |  TID        = 0      State : RUNNING   --> TERMINATED           |
  |  ...                                                            |
  +----------------------------------------------------------------+

  [TERMINATE] ODD_THREAD   Final stats:
    Saves=11   Loads=11   Yields=10   Blocks=0    CPU=2127 µs
```

`scheduler_terminate()` calls `tcb_unload(..., TERMINATED)` — same save box as
yield/block — then the `pthread` function returns, ending the OS thread.
The thread is **not** re-enqueued; `scheduler_run`'s alive-count decrements.

---

## 13. Final Stats Block

```
  [FINAL STATS]
  ===================================================================
  Scheduler cycles       : 22
  Total context switches : 22
  Wall-clock time        : 39919 µs

  Thread ODD_THREAD    TID=0
    Final state      : TERMINATED
    Context saves    : 11  (TCB UNLOAD count)
    Context loads    : 11  (TCB LOAD count)
    Yields           : 10
    Blocks           : 0
    CPU time         : 2127 µs

  Thread EVEN_THREAD   TID=1
    Final state      : TERMINATED
    Context saves    : 11  (TCB UNLOAD count)
    Context loads    : 11  (TCB LOAD count)
    Yields           : 10
    Blocks           : 0
    CPU time         : 2598 µs
  ===================================================================
```

| Metric | Expected value | Why |
|---|---|---|
| Scheduler cycles | 22 | 20 numbers (10 ODD + 10 EVEN) + 2 termination dispatches |
| Context switches | 22 | One LOAD+UNLOAD pair per cycle |
| Saves per thread | 11 | 10 yields + 1 terminate |
| Loads per thread | 11 | 10 resumes after yield + 1 first dispatch |
| Yields per thread | 10 | One yield per number printed |
| Blocks per thread | 0 | Threads never miss their turn when `MAX_NUMBER=20` and turns are symmetric |

---

## 14. Full Log Flow Summary

```
main()
  |
  +--[SCHEDULER INIT]
  |
  +--[REGISTER TID=0] --> [THREAD BORN] --> [READY QUEUE: 0]
  +--[REGISTER TID=1] --> [THREAD BORN] --> [READY QUEUE: 0,1]
  |
  +--[SCHEDULER RUN]
       |
       +--[CYCLE #1] Dispatch TID=0
       |    TCB LOAD  (READY  -> RUNNING)
       |    >>> prints 1
       |    [YIELD #1]
       |    TCB UNLOAD (RUNNING -> READY)   sim_R0=2
       |    [READY QUEUE: 1, 0]
       |
       +--[CYCLE #2] Dispatch TID=1
       |    TCB LOAD  (READY  -> RUNNING)
       |    >>> prints 2
       |    [YIELD #1]
       |    TCB UNLOAD (RUNNING -> READY)   sim_R0=3
       |    [READY QUEUE: 0, 1]
       |
       +-- ... (cycles 3–20, same pattern) ...
       |
       +--[CYCLE #21] Dispatch TID=0
       |    TCB LOAD  (READY  -> RUNNING)
       |    counter=21 >= MAX=20
       |    [TERMINATE] ODD_THREAD
       |    TCB UNLOAD (RUNNING -> TERMINATED)
       |
       +--[CYCLE #22] Dispatch TID=1
            TCB LOAD  (READY  -> RUNNING)
            counter=21 >= MAX=20
            [TERMINATE] EVEN_THREAD
            TCB UNLOAD (RUNNING -> TERMINATED)

  [FINAL STATS]
  All pthreads joined. main() returns 0.
```

---

## 15. Key Symbol Reference

| Log token | Function that emits it |
|---|---|
| `[SCHEDULER INIT]` | `scheduler_init()` |
| `[REGISTER]` | `scheduler_register_thread()` |
| `[THREAD BORN]` | `thread_entry_wrapper()` |
| `[READY QUEUE]` | `print_ready_queue()` |
| `[SCHEDULER RUN]` | `scheduler_run()` |
| `[SCHEDULER CYCLE #n]` | `scheduler_run()` dispatch loop |
| `TCB LOAD` box | `tcb_load()` |
| `>>> prints N` | `odd_thread_func()` / `even_thread_func()` |
| `[YIELD]` | `scheduler_yield()` |
| `TCB UNLOAD` box | `tcb_unload()` |
| `[BLOCK]` | `scheduler_block()` |
| `[UNBLOCK]` | `scheduler_unblock()` |
| `[TERMINATE]` | `scheduler_terminate()` |
| `[FINAL STATS]` | `scheduler_run()` after dispatch loop exits |

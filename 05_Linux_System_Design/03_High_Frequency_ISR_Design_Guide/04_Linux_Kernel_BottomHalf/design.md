# Approach 04: Linux Kernel Bottom Half
### Top Half / Bottom Half Architecture — Kernel Driver Interview Depth

---

## 1. Linux Interrupt Architecture Overview

```
Hardware IRQ Line asserted
         │
         ▼
   GIC / APIC (interrupt controller)
         │
         ▼
   CPU exception entry
   SAVE registers, switch to IRQ stack
         │
         ▼
   do_IRQ() / generic_handle_irq()
   [kernel/irq/handle.c]
         │
         ▼
   handle_irq_event() → iterate IRQ action list
         │
         ▼
┌────────────────────────────────────────────┐
│   TOP HALF: hf_isr(irq, dev_id)           │  ← this file
│   Local IRQs disabled (non-threaded)      │
│   MUST be fast: < 2 µs at 100 kHz         │
│   1. Check if our device                  │
│   2. Read hardware data                   │
│   3. Clear interrupt pending bit          │
│   4. Push to kfifo                        │
│   5. Schedule bottom half                 │
│   Return: IRQ_HANDLED or IRQ_NONE         │
└───────────────┬────────────────────────────┘
                │
        ┌───────┴────────────┐
        │                    │
        ▼                    ▼
 tasklet_schedule()    queue_work()
        │                    │
        ▼                    ▼
┌──────────────┐    ┌──────────────────┐
│  TASKLET BH  │    │  WORKQUEUE BH    │
│ softirq ctx  │    │  process context │
│ cannot sleep │    │  can sleep       │
│ < 100 µs     │    │  any duration    │
└──────────────┘    └──────────────────┘
```

---

## 2. Linux Softirq Priority Table

Softirqs run in a defined priority order:

```
Priority  Softirq           Use
─────────────────────────────────────────────────────
0         HI_SOFTIRQ        High-priority tasklets
1         TIMER_SOFTIRQ     Timer callbacks (hrtimer)
2         NET_TX_SOFTIRQ    Network transmit
3         NET_RX_SOFTIRQ    Network receive (NAPI)
4         BLOCK_SOFTIRQ     Block device completions
5         IRQ_POLL_SOFTIRQ  Block IRQ polling
6         TASKLET_SOFTIRQ   Regular tasklets ← our tasklet runs here
7         SCHED_SOFTIRQ     Scheduler load balancing
8         HRTIMER_SOFTIRQ   High-resolution timers
9         RCU_SOFTIRQ       RCU callbacks
```

**Implication**: Our tasklet (TASKLET_SOFTIRQ, priority 6) can be delayed by
NET_RX processing (priority 3) on a busy network host. For strict latency,
consider:
- `HI_SOFTIRQ` via `tasklet_hi_schedule()` (priority 0)
- Threaded IRQ with `SCHED_FIFO` priority
- `WQ_HIGHPRI` workqueue (runs before normal work items)

---

## 3. kfifo: Linux's Built-In Lock-Free FIFO

```c
DECLARE_KFIFO(data_fifo, u32, 4096);
```

kfifo is Linux's production SPSC ring buffer. Key properties:
- Power-of-2 size enforced at compile time
- Lock-free for SPSC (single producer ISR, single consumer bottom half)
- Supports multi-producer/consumer with external spinlock (`kfifo_in_spinlocked`)
- Implements the same release-acquire memory model as approach 01

For multi-producer (multiple ISRs to same kfifo):
```c
kfifo_in_spinlocked(&dev->data_fifo, &data, 1, &dev->lock);
// Uses spin_lock internally — ISR-safe (no sleep)
```

---

## 4. The Three Bottom Half Mechanisms — Decision Matrix

| Criterion | Tasklet | Workqueue | Threaded IRQ |
|---|---|---|---|
| Context | Softirq | Process (kworker) | Process (irq thread) |
| Can sleep? | No | Yes | Yes |
| kmalloc(GFP_KERNEL)? | No (GFP_ATOMIC only) | Yes | Yes |
| Latency | < 1 µs | ~10-100 µs | ~10-100 µs |
| CPU affinity | Same as IRQ | Any (WQ_UNBOUND) | Any |
| PREEMPT_RT safe? | No | Partial | Yes |
| Concurrency | Serialized | Parallel (max_active) | Per-IRQ thread |
| Code complexity | Low | Medium | Low |
| Status (kernel 6.x) | Deprecated | Recommended | Recommended |

### Selection guide

```
IRQ processing takes < 100 µs AND cannot sleep?
  → Tasklet (but consider migration to workqueue for new code)

IRQ processing needs sleep/allocation/I/O?
  → Workqueue (use WQ_HIGHPRI + dedicated wq for latency sensitivity)

Need per-IRQ isolation AND PREEMPT_RT compatibility?
  → Threaded IRQ (request_threaded_irq + IRQF_ONESHOT)

PREEMPT_RT (real-time kernel patch)?
  → MUST use threaded IRQ: softirqs are threaded on RT kernel
    (tasklets converted to threads automatically, but deprecated)
```

---

## 5. Memory Ordering in Linux Kernel

Linux uses its own memory ordering primitives (wrapping architecture-specific):

```c
/* Barrier types */
smp_mb()    // full memory barrier (both directions)
smp_rmb()   // read (load) barrier
smp_wmb()   // write (store) barrier
smp_store_release(ptr, val)   // store with release semantics
smp_load_acquire(ptr)         // load with acquire semantics (kernel 3.18+)

/* For ISR/kfifo: already handled internally by kfifo implementation */
/* For manual volatile-style access: */
READ_ONCE(var)    // prevents compiler from optimizing away the read
WRITE_ONCE(var, val) // prevents compiler from optimizing away the write
```

`READ_ONCE` / `WRITE_ONCE` are the Linux equivalents of C11's `volatile` qualifier
but more explicit about intent. They're used throughout the kernel for lockless
data structure access (e.g., RCU, seqlocks).

---

## 6. Interview Q&A — Google / Linux Foundation / Qualcomm Kernel Level

---

### Q1: Explain the Linux top-half / bottom-half model and why it's needed.

**A**: Hardware interrupts preempt whatever the CPU is running. To keep the
system responsive, the hardware ISR (top half) must be as short as possible —
it runs with local interrupts disabled, meaning NO other interrupts can be
serviced on this CPU while it runs.

If complex processing (protocol decode, memory allocation, I/O) happened in
the top half, all other interrupts on this CPU would be blocked for the
duration. At 100 kHz input: 1 µs heavy processing × 100,000/sec = 100 ms/sec
spent in non-interruptible context → 10% of the CPU completely unresponsive
to any other interrupt (network, timer, other hardware).

The bottom half defers heavy processing to a context where:
- Local IRQs are RE-ENABLED (other interrupts can run)
- Processing can yield to other work (scheduler runs)
- Sleeping operations are allowed (workqueue/threaded IRQ)

The top half says: "Data arrived, stored it, go." The bottom half says: "Here's
what to do with that data, take your time."

---

### Q2: What does `IRQ_NONE` vs `IRQ_HANDLED` mean? Why does it matter for IRQF_SHARED?

**A**: On a shared IRQ line (IRQF_SHARED), multiple devices connect to the same
physical interrupt line. When the IRQ fires, Linux iterates ALL registered
handlers for that line.

Each handler:
1. Checks if ITS device triggered the IRQ (reads hardware status register)
2. Returns `IRQ_HANDLED` if yes (this handler processed the IRQ)
3. Returns `IRQ_NONE` if no (not our device, keep checking others)

If ALL handlers return `IRQ_NONE` for a given IRQ: Linux detects spurious IRQ.
After a threshold (100 spurious IRQs), Linux disables the IRQ entirely with
a "nobody cared" message in dmesg.

This is why `status & IRQ_MASK` check is CRITICAL — incorrectly returning
`IRQ_HANDLED` for another device's IRQ means that device's driver never
processes its own interrupt (silent failure).

---

### Q3: What is `devm_request_irq()` and why is it preferred?

**A**: `devm_request_irq()` is a managed resource variant of `request_irq()`.
The "devm" prefix means the kernel will automatically call `free_irq()` when
the device is removed (via `platform_driver.remove()` or module unload).

Without devm:
```c
// probe():
request_irq(irq, handler, flags, name, dev);

// remove():  MUST manually call — if forgotten → handler runs after device freed!
free_irq(irq, dev);  // easy to forget or get ordering wrong
```

With devm:
```c
// probe():
devm_request_irq(&pdev->dev, irq, handler, flags, name, dev);

// remove(): nothing needed — kernel cleans up automatically
```

Resource leak prevention: `free_irq()` forgotten → ISR runs after driver is
unloaded → NULL pointer dereference → kernel oops. `devm_*` makes this class
of bugs impossible.

---

### Q4: How does `kfifo` guarantee safety without locks for SPSC?

**A**: kfifo uses the same SPSC ring buffer invariant as approach 01:

1. `kfifo->in` (head): written ONLY by producer (ISR), read by consumer
2. `kfifo->out` (tail): written ONLY by consumer (bottom half), read by producer
3. No variable is written by two parties simultaneously → no RMW needed

Linux-specific implementation uses `smp_store_release()` for head update
(producer) and `smp_load_acquire()` for head read (consumer) — equivalent
to our DMB + volatile approach but using Linux's portable barrier API.

`WRITE_ONCE(fifo->in, ...)` prevents the compiler from splitting the 32-bit
write into two 16-bit writes (word tearing) on unusual architectures.

---

### Q5: Describe the tasklet execution path from `tasklet_schedule()` to handler.

**A**:
```
1. tasklet_schedule(&dev->tasklet) [in ISR]
   → Sets TASKLET_STATE_SCHED bit on tasklet struct (atomic)
   → Adds tasklet to this CPU's tasklet_vec (per-CPU list, no locking)
   → Raises TASKLET_SOFTIRQ on this CPU: __raise_softirq_irqoff(TASKLET_SOFTIRQ)

2. ISR returns (top half done)
   → CPU exits interrupt context
   → Checks pending softirqs: if_softirq_pending() → invoke_softirq()

3. __do_softirq() [kernel/softirq.c]
   → Processes up to __softirq_pending for up to 2 ms or 10 iterations
   → Calls action[TASKLET_SOFTIRQ]() = tasklet_action()

4. tasklet_action() iterates tasklet_vec for this CPU
   → For each tasklet: clears TASKLET_STATE_SCHED
   → Calls tasklet->func(tasklet)  ← our hf_tasklet_handler()

5. If softirq budget exhausted (>2 ms or >10 rounds):
   → Remaining softirqs handed to ksoftirqd/N kernel thread
   → ksoftirqd runs as SCHED_NORMAL task (can be preempted)
```

Key insight: steps 1-4 happen on the SAME CPU, in order, within microseconds
of the ISR returning. That's why tasklet latency is typically < 1 µs.

---

### Q6: What changes if the system uses `PREEMPT_RT` (real-time Linux patch)?

**A**: PREEMPT_RT fundamentally changes softirq behavior:

**Without PREEMPT_RT (mainline Linux)**:
- Softirqs run in hardirq context (with preemption disabled)
- Tasklets run in softirq context
- High latency jitter possible: softirq can delay real-time tasks

**With PREEMPT_RT**:
- Softirqs are threaded: each softirq type runs in a dedicated thread
  (`ksoftirqd` for TASKLET_SOFTIRQ, etc.)
- Tasklets are automatically converted to workqueue-like items
- `spin_lock()` → sleeping lock (mutex) in process context
- Hardware IRQ threads: all IRQs run in threads (even top half!)
  unless `IRQF_NO_THREAD` is specified
- All threads can be given RT scheduling policy (SCHED_FIFO/RR)

**Impact on driver code**:
- Tasklets: deprecated, will break (no longer run at softirq time)
- `spin_lock_irqsave()` in process context: becomes mutex on RT
- Solution: use threaded IRQ + workqueue instead of tasklets
- For guaranteed latency: `request_threaded_irq() + SCHED_FIFO priority`

**Interview key point**: Any driver intended for PREEMPT_RT systems must avoid
tasklets and avoid holding spinlocks for extended periods.

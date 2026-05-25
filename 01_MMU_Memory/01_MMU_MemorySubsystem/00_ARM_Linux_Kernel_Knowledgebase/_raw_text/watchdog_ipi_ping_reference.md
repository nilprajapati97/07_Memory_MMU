Linux Kernel Watchdog IPI Ping Mechanism
Technical Reference: Line-by-Line Code Explanation
Source: Qualcomm MSM Watchdog Driver  —  msm_watchdog_v2.c

|  |


# 1. Overview
This document provides a comprehensive, line-by-line technical explanation of the watchdog IPI (Inter-Processor Interrupt) ping mechanism found in the Qualcomm MSM Linux kernel watchdog driver. The mechanism is responsible for detecting CPU lockups by sending health-check interrupts to all online CPU cores and verifying their responses.
The analysis covers two cooperating kernel functions: ping_other_cpus() — the orchestrating routine that issues IPIs and evaluates responses — and wdog_ping_cb() — the lightweight callback that executes on each remote CPU to signal liveness.

## 1.1  Key Concepts at a Glance
| Concept | Description |
| IPI | Inter-Processor Interrupt — a signal sent from one CPU core to another via the hardware interrupt controller (e.g., ARM GIC). Used here to probe CPU liveness. |
| cpumask | A kernel bitmask where each bit represents one CPU core. Used to track which CPUs are online and which have responded to the ping. |
| watchdog bark | First-stage watchdog expiry event; triggers a kernel panic with CPU state dump (registers, call stacks). Fires when the watchdog is not "petted" in time. |
| watchdog bite | Second-stage watchdog expiry; causes a hard hardware reset of the SoC. Non-maskable and unrecoverable. |
| SMP | Symmetric Multi-Processing — the Linux kernel subsystem managing multiple CPU cores, providing APIs like smp_call_function_single(). |
| atomic_t | A kernel type for lock-free, race-safe integer operations. Prevents data races when multiple CPUs write concurrently. |
| hard lockup | A CPU stuck in a tight loop with interrupts disabled, unable to service any interrupt — the exact condition this mechanism detects. |


# 2. Full Source Code Listing
The complete source code of both functions is reproduced below for reference. Line numbers are provided to correlate with the detailed explanation in Section 3.
| msm_watchdog_v2.c — IPI Watchdog Ping Mechanism |

/* ──────────────────────────────────────────────────────────── */
/*  FUNCTION 1: ping_other_cpus()  –  Orchestrator              */
/* ──────────────────────────────────────────────────────────── */

static void ping_other_cpus(struct msm_watchdog_data *wdog_dd)
{
    int cpu;

    cpumask_clear(&wdog_dd->alive_mask);
    atomic_set(&wdog_dd->alive_count, 0);

    /* Send IPI to all online CPUs */
    for_each_cpu(cpu, cpu_online_mask) {
        smp_call_function_single(cpu, wdog_ping_cb, wdog_dd, 0);
    }

    /* Wait for all CPUs to respond */
    msleep(IPI_WAIT_TIME);

    /* If any CPU didn't respond → it's stuck */
    if (!cpumask_equal(&wdog_dd->alive_mask, cpu_online_mask)) {
        /* Don't pet → let bark fire → dump state → bite resets */
        pr_err("Watchdog: CPU(s) failed IPI ping: %*pbl\n",
               cpumask_pr_args(cpu_online_mask));
    }
}

/* ──────────────────────────────────────────────────────────── */
/*  FUNCTION 2: wdog_ping_cb()  –  Remote CPU Callback          */
/* ──────────────────────────────────────────────────────────── */

static void wdog_ping_cb(void *info)
{
    struct msm_watchdog_data *wdog_dd = info;
    cpumask_set_cpu(smp_processor_id(), &wdog_dd->alive_mask);
}

Note: Highlighted lines are executable statements (non-comment, non-blank). Grey lines are comments, braces, or blank lines.

# 3. Line-by-Line Explanation
## 3.1  ping_other_cpus() — Main IPI Ping Orchestrator
This function is the watchdog thread's heartbeat probe. It is called periodically by the watchdog kernel thread to verify that every online CPU core is still functional and able to service interrupts.
### Line 1 — Function Signature
static void ping_other_cpus(struct msm_watchdog_data *wdog_dd)

| Token | Meaning |
| static | Limits the function's linkage to the current translation unit (file). Not exported; cannot be called from outside this .c file. A kernel convention for internal driver helpers. |
| void | No return value. The function communicates failure implicitly — by NOT petting the watchdog hardware, rather than returning an error code. |
| ping_other_cpus | Descriptive name: "ping" (send health probe), "other" (CPUs other than the watchdog thread's own), "cpus" (all online cores). |
| struct msm_watchdog_data *wdog_dd | Pointer to the driver's private data structure. Contains the alive_mask bitmask, alive_count atomic counter, timer configuration, and other watchdog state. The "dd" suffix is a Linux convention for "device data". |


Design note: Because all CPUs share the same wdog_dd instance, the alive_mask written by remote CPUs in wdog_ping_cb() is visible to the watchdog thread reading it after msleep(). This works because cpumask_set_cpu() is SMP-safe via internal atomic operations.

### Line 2 — Local Variable Declaration
    int cpu;

Declares the loop iteration variable cpu, which will hold each CPU's numeric identifier (0, 1, 2, …, NR_CPUS-1) as the loop progresses. In the Linux kernel, CPU IDs are small non-negative integers. Declared at the top of the function per C89 style common in older kernel code.

### Line 3 — Clearing the Alive Mask
    cpumask_clear(&wdog_dd->alive_mask);

Resets the alive_mask bitmask to all-zeros, meaning "no CPU has reported as alive yet." This must be done at the start of every probe cycle; otherwise, CPUs that responded in a previous cycle would still appear as alive even if they have since locked up.
| WHY | A cpumask_t is a fixed-width bitmask (one bit per possible CPU). Bit N = 1 means CPU N is alive. cpumask_clear() sets all bits to 0 using memset() under the hood. The address-of operator (&) is required because cpumask_clear() takes a pointer to the mask. |


### Line 4 — Resetting the Atomic Counter
    atomic_set(&wdog_dd->alive_count, 0);

Atomically sets the alive_count integer to zero. atomic_t and its accompanying operations (atomic_set, atomic_inc, etc.) guarantee that reads and writes are race-free even when multiple CPU cores are involved, without requiring spinlocks.
In the current implementation, the primary liveness check uses the alive_mask (cpumask_equal() at the end), not the counter. The counter may be used for statistics or legacy compatibility.

### Line 5 — Iterating Over All Online CPUs
    for_each_cpu(cpu, cpu_online_mask) {

A Linux kernel macro defined in <linux/cpumask.h>. It expands to a for loop that iterates over every CPU bit set in the provided mask. Here the mask is cpu_online_mask — a global cpumask_t maintained by the kernel's CPU hotplug framework that reflects which CPUs are currently online and available to run tasks.
| Parameter | Description |
| cpu | Output variable: receives the CPU number (integer) on each iteration. |
| cpu_online_mask | Global read-only mask: set of currently online (schedulable) CPUs. Protected by the hotplug lock; safe to read here. |


Why not include offline CPUs? Offline CPUs are not executing any code, so they cannot service an IPI. Sending an IPI to an offline CPU would either be silently dropped or cause a kernel warning, depending on the architecture.

### Line 6 — Sending the IPI via smp_call_function_single()
        smp_call_function_single(cpu, wdog_ping_cb, wdog_dd, 0);

This is the heart of the health-check mechanism. It sends an IPI to the specified cpu, requesting it to execute wdog_ping_cb with wdog_dd as the argument.
| Argument | Role |
| cpu | The target CPU number (from the for_each_cpu loop). This CPU will execute the callback. |
| wdog_ping_cb | Function pointer to the callback. The remote CPU will call this function. |
| wdog_dd | Passed as the void* info argument to the callback. Allows the callback to access the shared watchdog state. |
| 0 (wait flag) | 0 = asynchronous: do NOT block waiting for the remote CPU to complete. Returns immediately. Using 1 would cause a deadlock if the remote CPU is stuck. |


| KEY | Asynchronous dispatch (wait=0) is critical here. If wait=1 were used and a CPU were stuck, this function would block forever, hanging the watchdog thread itself and preventing the bark/bite reset. The non-blocking call fires and forgets; the msleep() afterward provides the wait window. |


### Line 7 — Sleeping for IPI Response Window
    msleep(IPI_WAIT_TIME);

After dispatching all IPIs, the watchdog thread yields the CPU for IPI_WAIT_TIME milliseconds (typically 10–200 ms, defined as a module parameter or compile-time constant). This gives every online CPU sufficient time to:
- Receive the IPI hardware interrupt
- Complete any brief critical section it may be in
- Enter the IPI handler and call wdog_ping_cb()
- Set its bit in alive_mask

msleep() is a non-busy sleep: it puts the calling thread into TASK_INTERRUPTIBLE state and yields the CPU to other tasks. This is different from udelay() (busy-wait spin) which would waste CPU cycles and could interfere with other scheduling activity.

### Line 8 — Checking for Missed Responses
    if (!cpumask_equal(&wdog_dd->alive_mask, cpu_online_mask)) {

After the sleep window expires, this line compares the alive_mask (CPUs that actually responded) against cpu_online_mask (CPUs that should have responded). The logical NOT (!) makes the condition true when they are not equal — i.e., at least one CPU failed to set its bit.
cpumask_equal() performs a bitwise comparison of all words in the two masks. On a 64-bit system with NR_CPUS=64, this is a single 64-bit integer comparison — extremely fast.

### Line 9 — Kernel Error Log and Implicit Watchdog Failure
        pr_err("Watchdog: CPU(s) failed IPI ping: %*pbl\n",
               cpumask_pr_args(cpu_online_mask));

Emits a KERN_ERR level message to the kernel log (dmesg / serial console / persistent storage). Logging is the only explicit action here — the failure response is actually implicit: by not petting the watchdog, the hardware timer is allowed to expire.
| Format Element | Description |
| %*pbl | Kernel-specific format specifier for cpumask_t. Prints CPU IDs as a human-readable bit list, e.g., "0-3" or "0,2-5". The * width comes from cpumask_pr_args(). |
| cpumask_pr_args() | Macro that expands to the two arguments required by %*pbl: the number of bits (nr_cpu_ids) and the mask pointer. Defined in <linux/cpumask.h>. |
| pr_err() | printk() wrapper at KERN_ERR level (3). Visible in dmesg and typically written to the persistent (ramoops/pstore) crash log, which survives a reboot. |


| FAIL | Critical Design: The failure path intentionally omits any call to pet (reset) the hardware watchdog timer. By not petting it, the timer is allowed to expire. This triggers: (1) BARK — watchdog interrupt fires, kernel panic with full CPU register dumps and call stacks is captured to persistent storage. (2) BITE — hardware timer expires, SoC is hard-reset. This is a last-resort recovery mechanism. |


## 3.2  wdog_ping_cb() — Remote CPU Liveness Callback
This function executes on each remote CPU when it receives the IPI triggered by smp_call_function_single(). It is the "response" side of the health check — a minimal function that simply records that this CPU is alive and capable of servicing interrupts.

### Line 1 — Callback Function Signature
static void wdog_ping_cb(void *info)

| Token | Meaning |
| static | File-local scope. This function is an internal callback and is never called directly — it is passed as a function pointer. |
| void | No return value. The SMP IPI callback mechanism does not use return values. |
| void *info | Generic pointer argument. The SMP framework passes the third argument of smp_call_function_single() — in this case, wdog_dd — as info. Must be cast to the correct type inside the function. |


### Line 2 — Recovering the Typed Pointer
    struct msm_watchdog_data *wdog_dd = info;

Casts the generic void *info parameter back to the correct concrete type struct msm_watchdog_data *. This is a standard C idiom for passing typed data through a generic callback interface. The assignment is safe because:
- The caller (ping_other_cpus) passed wdog_dd, which is a valid, non-null pointer to a live object.
- The wdog_dd object's lifetime is controlled by the driver module — it is allocated at probe time and freed at remove time, so it is valid for the entire duration the watchdog is running.
- The implicit void* to typed-pointer conversion in C is well-defined and generates no cast warning.

### Line 3 — Recording CPU Liveness
    cpumask_set_cpu(smp_processor_id(), &wdog_dd->alive_mask);

This single line is the entire "I am alive" response. It sets the bit corresponding to the currently executing CPU in the shared alive_mask.
| Sub-expression | Explanation |
| smp_processor_id() | Returns the ID of the CPU executing this code right now. In an IPI callback, this is the target CPU that received the interrupt — the one we want to mark as alive. |
| &wdog_dd->alive_mask | Address of the shared cpumask_t. Since multiple CPUs may be setting different bits concurrently, the implementation uses atomic bitops (set_bit()) internally to avoid races. |
| cpumask_set_cpu(n, m) | Sets bit n in cpumask m. Equivalent to set_bit(n, cpumask_bits(m)). This is SMP-safe: concurrent calls from different CPUs setting different bits will not corrupt the mask. |


Interrupt context: IPI callbacks execute in hardirq context (interrupt handler). This means no sleeping, no mutexes, and no complex locking is allowed — which is why the callback is deliberately minimal. cpumask_set_cpu() is safe in this context because it uses atomic bit operations.

# 4. Execution Flow and Timing Analysis
## 4.1  Step-by-Step Execution Timeline
| Step | Action | Detail |
| 1 | Clear state | cpumask_clear() zeroes alive_mask; atomic_set() zeroes alive_count. This establishes a clean baseline for this probe cycle. |
| 2 | Dispatch IPIs | smp_call_function_single() is called once per online CPU in the for_each_cpu loop. Each call enqueues an IPI on the target CPU's IPI queue and returns immediately (async). |
| 3 | Sleep | msleep(IPI_WAIT_TIME) yields the watchdog thread. CPUs receiving IPIs execute wdog_ping_cb() and set their bits in alive_mask during this window. |
| 4 | Evaluate | cpumask_equal() compares alive_mask to cpu_online_mask. If equal, all CPUs are healthy; the watchdog is petted elsewhere in the driver. If not equal, at least one CPU is stuck. |
| 5a | PASS ✔ | All CPUs responded. Watchdog is petted (hardware timer reset). System continues normal operation. |
| 5b | FAIL ✘ | One or more CPUs did not respond. pr_err() logs the failure. No pet → bark timer fires → kernel panic + register dump → bite → SoC reset. |


## 4.2  Bark → Dump → Bite Reset Sequence
When a CPU fails the IPI ping test, the watchdog intentionally does not pet the hardware watchdog timer. The following cascade is triggered automatically:
| Phase | Trigger | Effect |
| Bark | Bark timer expires (hardware) | ARM WDT fires NMI-equivalent interrupt. Kernel panic handler runs: captures CPU registers, call stacks, memory maps to pstore/ramoops persistent storage. |
| Dump | Panic handler | Full system state is logged: all CPU register banks (PC, LR, SP, PSR), kernel stacks, IRQ states, memory controller registers. Critical for post-mortem analysis. |
| Bite | Bite timer expires | Hardware reset line asserted. SoC performs a full cold reset. On next boot, ramoops/pstore provides the crash dump for engineering analysis. |


# 5. Design Decisions and Engineering Trade-offs
## 5.1  Why Asynchronous IPI Dispatch?
Using wait=0 (asynchronous) in smp_call_function_single() is a deliberate and critical design choice:
- Avoids deadlock: If a target CPU is stuck, a synchronous call (wait=1) would block the watchdog thread indefinitely, preventing any bark/bite response.
- Enables parallelism: All IPIs are dispatched in rapid succession before any CPU has finished responding. This means the total wait time is IPI_WAIT_TIME, not N × IPI_WAIT_TIME.
- Matches hardware reality: IPI delivery is handled by the hardware interrupt controller (ARM GIC) and is effectively instantaneous (<1μs). The IPI_WAIT_TIME budget is for software execution latency, not IPI delivery.

## 5.2  Why cpumask vs. Atomic Counter?
The driver maintains both an alive_mask (cpumask_t) and an alive_count (atomic_t). The mask provides richer information:
| Approach | Capability | Limitation |
| cpumask (used for check) | Identifies exactly which CPU(s) failed; enables targeted error logging with %*pbl. | Slightly more memory (one word per 64 CPUs). |
| atomic count (secondary) | Cheap counter; useful for statistics or rate-limiting logic. | Only tells you HOW MANY failed, not WHICH ones. Insufficient for the error message. |


## 5.3  What This Mechanism Detects vs. Does Not Detect
| Scenario | Detected? | Reason |
| CPU stuck in IRQ-disabled spin loop (hard lockup) | ✔ Yes | IPI not serviced; bit never set in alive_mask. |
| CPU in long non-preemptible kernel section (soft lockup) | ✔ Likely | If IRQs enabled, IPI will interrupt it; if > IPI_WAIT_TIME, detected. |
| CPU fully offline (hotplugged out) | ✘ No | Not in cpu_online_mask; not included in the probe loop. |
| CPU stuck in user space (easy to preempt) | ✘ No | Kernel scheduler preempts it at next timer tick; IPI will be serviced. |
| Memory corruption in wdog_dd struct | ✘ No | Structural / data integrity issues not detected by IPI liveness check. |


# 6. Relevant Kernel Data Structures and APIs
## 6.1  struct msm_watchdog_data (Partial)
The msm_watchdog_data structure encapsulates all state for one watchdog device instance. The fields relevant to the IPI ping mechanism are:
struct msm_watchdog_data {
    /* ... other fields ... */
    cpumask_t    alive_mask;   /* set bit = CPU responded */
    atomic_t     alive_count;  /* count of responding CPUs */
    unsigned long bark_time;   /* bark timer period (ms)  */
    /* ... */
};

## 6.2  Key Kernel API Reference
| API / Macro | Header | Purpose |
| cpumask_clear(m) | <linux/cpumask.h> | Zero all bits in mask m. |
| cpumask_set_cpu(n, m) | <linux/cpumask.h> | Set bit n in mask m (atomic). |
| cpumask_equal(m1, m2) | <linux/cpumask.h> | Return true if m1 and m2 have identical bits. |
| for_each_cpu(cpu, mask) | <linux/cpumask.h> | Loop over each set bit in mask. |
| cpu_online_mask | <linux/cpumask.h> | Global read-only mask of online CPUs. |
| smp_call_function_single() | <linux/smp.h> | Send IPI to target CPU; execute callback there. |
| smp_processor_id() | <linux/smp.h> | Return current CPU's numeric ID. |
| atomic_set(a, v) | <linux/atomic.h> | Atomically assign value v to atomic_t a. |
| msleep(ms) | <linux/delay.h> | Non-busy sleep for ms milliseconds. |
| pr_err(fmt, ...) | <linux/printk.h> | printk() at KERN_ERR level (3). |
| cpumask_pr_args(m) | <linux/cpumask.h> | Expand to (nr_cpu_ids, m) for %*pbl format. |


# 7. Complete Line-by-Line Reference Summary
A consolidated reference mapping every significant code line to its purpose, context, and potential failure mode.

| Line | Code | Purpose | Failure Impact |
| F1-L1 | static void ping_other_cpus(...) | Declare orchestrator function | N/A (declaration) |
| F1-L2 | int cpu; | Loop counter for CPU IDs | N/A |
| F1-L3 | cpumask_clear(&wdog_dd->alive_mask) | Reset alive tracking state | Stale bits → false healthy result |
| F1-L4 | atomic_set(&wdog_dd->alive_count, 0) | Reset alive count atomically | Incorrect statistics |
| F1-L5 | for_each_cpu(cpu, cpu_online_mask) | Iterate all online CPU IDs | Skipped offline CPUs (correct) |
| F1-L6 | smp_call_function_single(..., 0) | Send async IPI to target CPU | If sync: deadlock on stuck CPU |
| F1-L7 | msleep(IPI_WAIT_TIME) | Give CPUs time to respond | Too short → false positives |
| F1-L8 | if (!cpumask_equal(...)) | Detect non-responsive CPUs | Bug here → missed lockups |
| F1-L9 | pr_err(...) + no pet | Log + trigger bark/bite reset | Expected: SoC reset |
| F2-L1 | static void wdog_ping_cb(void *) | Declare IPI callback | N/A (declaration) |
| F2-L2 | struct msm_watchdog_data *wdog_dd = info | Recover typed pointer from void* | NULL deref if caller passes NULL |
| F2-L3 | cpumask_set_cpu(smp_processor_id(), ...) | Mark this CPU as alive | If skipped: CPU appears stuck |


# 8. Glossary of Terms
| Term | Definition |
| IPI | Inter-Processor Interrupt. A software-triggered interrupt delivered from one CPU core to another via the interrupt controller hardware (e.g., ARM GIC, x86 APIC). |
| cpumask_t | A kernel type that is a bitmask with one bit per possible CPU. Used extensively to represent sets of CPUs (online, offline, isolated, etc.). |
| SMP | Symmetric Multi-Processing. A system architecture where multiple CPU cores share the same memory and I/O, each running the same kernel code. |
| atomic_t | A kernel integer type whose operations are guaranteed to be indivisible on all supported architectures. Prevents data races without spinlocks. |
| hardirq context | Execution context inside a hardware interrupt handler. Sleeping, acquiring mutexes, or calling schedule() is forbidden. Only spinlocks and atomic operations are safe. |
| Hard lockup | A CPU stuck in a tight loop with local interrupts disabled (irqs_disabled()), preventing it from responding to any software or hardware interrupt. |
| Soft lockup | A CPU stuck in a loop in kernel mode for a long time (default 20s) without calling schedule(). Unlike hard lockup, IRQs are still enabled. |
| Watchdog bark | First hardware watchdog timer expiry. Generates an NMI or FIQ that triggers a kernel panic, capturing full CPU state to persistent storage. |
| Watchdog bite | Second (final) hardware watchdog timer expiry. Directly resets the SoC. No kernel code runs; recovery is unconditional. |
| pstore / ramoops | Linux persistent storage subsystem. Saves crash dumps to a reserved RAM region that persists across resets, readable on next boot via /sys/fs/pstore/. |
| SoC | System on a Chip. An integrated circuit containing CPU cores, memory controllers, I/O peripherals, and hardware watchdog timers — e.g., a Qualcomm Snapdragon processor. |
| msm_watchdog_v2.c | The Qualcomm MSM (Mobile Station Modem) watchdog driver source file in the Linux kernel, located at drivers/soc/qcom/msm_watchdog_v2.c. |


|  |

Linux Kernel Watchdog IPI Ping Mechanism — Technical Reference
Source: Qualcomm MSM Watchdog Driver (msm_watchdog_v2.c)  |  Linux Kernel Documentation

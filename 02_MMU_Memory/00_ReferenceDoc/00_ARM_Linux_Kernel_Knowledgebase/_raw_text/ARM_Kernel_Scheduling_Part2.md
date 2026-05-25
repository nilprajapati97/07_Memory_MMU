ARM Linux Kernel Scheduling
Part 2: SMP Load Balancing &
Energy-Aware Scheduling
Level 5 — SMP, Load Balancing & ARM Multi-Core | Level 6 — big.LITTLE & EAS

| Series | ARM Linux Kernel Scheduling — Staff Engineer Interview Guide |
| Part | Part 2 of 3 |
| Topics | Topics 25–38: SMP Boot, CPU Affinity, Scheduling Domains, Load Balancing, Migration, IPI, big.LITTLE, CPU Capacity, EAS, uclamp, Thermal Pressure |
| Prerequisites | Part 1 (Levels 1–4): Foundations, Core Scheduling, CFS, Real-Time Scheduling |
| Target | Staff Engineer — Linux Kernel (Qualcomm Compute Devices) |


LEVEL 5 — SMP, LOAD BALANCING & ARM MULTI-CORE
Prerequisites: Level 4 (Real-Time Scheduling). Topics 25–31 cover how the Linux scheduler works across multiple ARM cores, including the big.LITTLE topology.

# Level 5, Topic 25: SMP Boot on ARM
On ARM systems, only CPU 0 (the primary/boot CPU) is running at power-on. All other CPUs are in WFI (Wait For Interrupt) or powered-off state. The kernel must explicitly bring each secondary CPU online.

## Boot Timeline
  Power-on: Only CPU 0 (primary/boot CPU) is running.
  All other CPUs are in WFI/powered-off state.

  Boot Timeline:
  =========================================================
  CPU 0 (Boot CPU):
  |
  |  Firmware (ATF/TF-A at EL3)
  |  -> U-Boot / UEFI (EL2 or EL1)
  |  -> Linux kernel start (head.S)
  |  -> start_kernel()
  |  -> ... init subsystems ...
  |  -> smp_init()
  |       |
  |       |-- cpu_up(1)  -->  Wakes CPU 1
  |       |-- cpu_up(2)  -->  Wakes CPU 2
  |       |-- cpu_up(3)  -->  Wakes CPU 3
  |       +-- ...

## PSCI (Power State Coordination Interface)
PSCI is the standard ARM mechanism used on ALL modern ARM64 SoCs (Qualcomm, i.MX8, etc.) to control secondary CPU power states.
  CPU 0 (kernel)                  Firmware (EL3/ATF)
    |                                 |
    |  SMC #0 (PSCI_CPU_ON)          |
    |  args: target_cpu,             |
    |        entry_point,            |
    |        context_id              |
    |------------------------------> |
    |                                |
    |                          Powers on target CPU
    |                          Sets entry point
    |                          Returns to CPU 0
    |<------------------------------ |
    |
  Target CPU (e.g., CPU 1):
    |
    |  Wakes up at entry_point (secondary_entry)
    |  -> Initializes MMU, caches
    |  -> Calls secondary_start_kernel()
    |  -> Sets up per-CPU runqueue
    |  -> Calls cpu_startup_entry(CPUHP_AP_ONLINE_IDLE)
    |  -> Enters idle loop (schedule_idle())
    |  -> Ready to receive tasks!

## Device Tree for SMP (Qualcomm big.LITTLE Example)
/* Qualcomm Snapdragon -- big.LITTLE example */
cpus {
    #address-cells = <2>;
    #size-cells = <0>;

    /* LITTLE cluster (Cortex-A55) */
    cpu0: cpu@0 {
        device_type = "cpu";
        compatible = "arm,cortex-a55";
        reg = <0x0 0x0>;
        enable-method = "psci";        /* <-- PSCI boot */
        capacity-dmips-mhz = <1024>;   /* <-- Relative capacity */
        dynamic-power-coefficient = <100>;
    };

    /* big cluster (Cortex-A78) */
    cpu4: cpu@400 {
        compatible = "arm,cortex-a78";
        reg = <0x0 0x400>;
        enable-method = "psci";
        capacity-dmips-mhz = <3200>;   /* <-- 3.1x more capable */
        dynamic-power-coefficient = <520>;
    };

    /* CPU topology -- critical for scheduler domains */
    cpu-map {
        cluster0 {  /* LITTLE cluster */
            core0 { cpu = <&cpu0>; };
            core1 { cpu = <&cpu1>; };
        };
        cluster1 {  /* big cluster */
            core0 { cpu = <&cpu4>; };
            core1 { cpu = <&cpu5>; };
        };
    };
};

psci {
    compatible = "arm,psci-1.0";
    method = "smc";
};

## Secondary CPU Initialization Code
void secondary_start_kernel(void)
{
    struct rq *rq = this_rq();
    notify_cpu_starting(cpu);
    /* Set CPU online in cpu_online_mask */
    set_cpu_online(smp_processor_id(), true);
    /* Enter idle loop -- scheduler will assign tasks */
    cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
    /* ^^^ Calls do_idle() in a loop forever */
}


# Level 5, Topic 26: Per-CPU Runqueues & CPU Affinity
## CPU Masks
cpumask_t is a bitmask representing a set of CPUs. Multiple system-wide masks control which CPUs are available for scheduling.
  8-CPU system (4 LITTLE + 4 big):

  Bit:    7   6   5   4   3   2   1   0
        +---+---+---+---+---+---+---+---+
        | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |  cpu_online_mask (all online)
        +---+---+---+---+---+---+---+---+
        |<-- big cluster -->|<-- LITTLE->|

  Key system-wide masks:
  +------------------------------------------------------+
  | cpu_possible_mask  -- CPUs that COULD exist          |
  | cpu_present_mask   -- CPUs physically present        |
  | cpu_online_mask    -- CPUs currently online & usable |
  | cpu_active_mask    -- CPUs accepting new tasks       |
  +------------------------------------------------------+

  Per-task affinity:
  +------------------------------------------------------+
  | task->cpus_mask  -- CPUs this task is allowed on    |
  | Default: all online CPUs (can run anywhere)          |
  | Modified by: sched_setaffinity()                    |
  | CLI: taskset -c 0-3 ./app  (restrict to LITTLE)     |
  |      taskset -c 4-7 ./app  (restrict to big)        |
  +------------------------------------------------------+

## How Affinity Affects Scheduling
  Task A: cpus_mask = 0xFF (all CPUs)
  Task B: cpus_mask = 0x0F (CPUs 0-3 only, LITTLE cluster)
  Task C: cpus_mask = 0xF0 (CPUs 4-7 only, big cluster)

  select_task_rq_fair(Task A):
    -> Can choose ANY CPU -> EAS picks most energy-efficient

  select_task_rq_fair(Task B):
    -> Can ONLY choose CPU 0-3 -> Restricted to LITTLE cluster
    -> Even if big CPUs are idle, Task B stays on LITTLE

  select_task_rq_fair(Task C):
    -> Can ONLY choose CPU 4-7 -> Forced onto big cluster
    -> Even for light work, wastes power on big cores
## Affinity API
#include <sched.h>

cpu_set_t mask;
CPU_ZERO(&mask);
CPU_SET(0, &mask);  /* Allow CPU 0 */
CPU_SET(1, &mask);  /* Allow CPU 1 */
sched_setaffinity(0, sizeof(mask), &mask);  /* 0 = current */

/* Query current affinity */
sched_getaffinity(0, sizeof(mask), &mask);
for (int i = 0; i < CPU_SETSIZE; i++) {
    if (CPU_ISSET(i, &mask))
        printf("Task can run on CPU %d\n", i);
}

/* Kernel side -- checking affinity during task placement */
static int select_task_rq_fair(struct task_struct *p, int prev_cpu,
                                int wake_flags)
{
    /* Only consider CPUs in task's affinity mask */
    cpumask_and(&allowed, p->cpus_ptr, cpu_active_mask);
    /* If EAS is enabled, find energy-efficient CPU within allowed */
    new_cpu = find_energy_efficient_cpu(p, prev_cpu);
    if (new_cpu >= 0 && cpumask_test_cpu(new_cpu, &allowed))
        return new_cpu;
    /* Fallback to load-balance based selection */
}


# Level 5, Topic 27: Scheduling Domains & Groups
Scheduling domains define how load balancing is organized across CPUs. They form a hierarchy that mirrors the hardware topology.
## The Domain Hierarchy
  Qualcomm Snapdragon 8-core big.LITTLE example:

  +-------------------------------------------------------------+
  |                    DIE Domain (sd_die)                      |
  |              Balances across ALL clusters                   |
  |              Balance interval: ~64ms                        |
  |                                                             |
  |  +--------------------------+  +--------------------------+ |
  |  |    MC Domain (sd_mc)     |  |    MC Domain (sd_mc)     | |
  |  |  Multi-Core (cluster)    |  |  Multi-Core (cluster)    | |
  |  |  Balance interval: ~4ms  |  |  Balance interval: ~4ms  | |
  |  |                          |  |                          | |
  |  |  +----++----++----++----+|  |+----++----++----++----+  | |
  |  |  |CPU0||CPU1||CPU2||CPU3||  ||CPU4||CPU5||CPU6||CPU7|  | |
  |  |  | A55|| A55|| A55|| A55||  || A78|| A78|| A78|| A78|  | |
  |  |  +----++----++----++----+|  |+----++----++----++----+  | |
  |  |     LITTLE cluster       |  |      big cluster         | |
  |  +--------------------------+  +--------------------------+ |
  +-------------------------------------------------------------+

  DynamIQ (e.g., 1+3+4 config like Snapdragon 8 Gen 2):
  +-------------------------------------------------------------+
  |  +----------+  +------------------+  +------------------+  |
  |  | MC Domain|  |   MC Domain      |  |   MC Domain      |  |
  |  | +------+ |  |+----++----++----+|  |+----++----++----++----+
  |  | | CPU7 | |  ||CPU4||CPU5||CPU6||  ||CPU0||CPU1||CPU2||CPU3|
  |  | |X3@3.2| |  ||A715||A715||A715||  ||A510||A510||A510||A510|
  |  | | prime| |  || mid| mid | mid ||  ||LITTLE         LITTLE|
  |  | +------+ |  |+----++----++----+|  |+----++----++----++----+
  |  +----------+  +------------------+  +------------------+  |
  +-------------------------------------------------------------+

## sched_domain and sched_group Structures
/* kernel/sched/sched.h */
struct sched_domain {
    struct sched_domain *parent;    /* Next higher level domain */
    struct sched_domain *child;     /* Next lower level domain */
    struct sched_group  *groups;    /* Circular list of groups */
    unsigned long min_interval;     /* Min balance interval (ms) */
    unsigned long max_interval;     /* Max balance interval (ms) */
    unsigned int imbalance_pct;     /* Threshold for migration */
    unsigned long flags;            /* SD_LOAD_BALANCE, SD_BALANCE_WAKE, etc. */
    enum sched_domain_level level;  /* SD_LV_NONE, SD_LV_MC, SD_LV_DIE, etc. */
};

struct sched_group {
    struct sched_group *next;       /* Circular list */
    unsigned int group_weight;      /* Number of CPUs in group */
    struct sched_group_capacity *sgc; /* Capacity info */
    unsigned long cpumask[];        /* CPUs in this group */
};
## Viewing Scheduling Domains
# See the domain hierarchy
cat /proc/sys/kernel/sched_domain/cpu0/domain0/name
# MC (Multi-Core / cluster level)
cat /proc/sys/kernel/sched_domain/cpu0/domain1/name
# DIE (chip level)

# See balance parameters
cat /proc/sys/kernel/sched_domain/cpu0/domain0/min_interval  # 4 (ms)
cat /proc/sys/kernel/sched_domain/cpu0/domain0/max_interval  # 8 (ms)
cat /proc/sys/kernel/sched_domain/cpu0/domain0/imbalance_pct # 117

# See flags
cat /proc/sys/kernel/sched_domain/cpu0/domain0/flags
# SD_LOAD_BALANCE SD_BALANCE_WAKE SD_BALANCE_FORK SD_SHARE_PKG_RESOURCES


# Level 5, Topic 28: Load Balancing
## When Load Balancing Happens
| Trigger | Mechanism | Description |
| PERIODIC | scheduler_tick() -> SCHED_SOFTIRQ | Interval: domain-specific (4ms-64ms) |
| IDLE BALANCE | newidle_balance() on CPU going idle | Most effective: "I'm idle, let me steal work" |
| FORK BALANCE | wake_up_new_task() -> select_task_rq() | Place new task on least loaded CPU (SD_BALANCE_FORK) |
| EXEC BALANCE | sched_exec() -> select_task_rq() | Opportunity to rebalance on exec() (SD_BALANCE_EXEC) |
| WAKE BALANCE | try_to_wake_up() -> select_task_rq() | Place waking task on optimal CPU (SD_BALANCE_WAKE) |


## The Periodic Load Balance Algorithm
  load_balance() -- called per scheduling domain, bottom-up

  Step 1: Find busiest group
    For each sched_group in this domain:
      Calculate group load, capacity, nr_running
      Classify group type:
        * group_has_spare    -- has idle CPUs
        * group_fully_busy   -- all CPUs busy, balanced
        * group_misfit_task  -- task too big for CPU (★)
        * group_asym_packing -- asymmetric (big.LITTLE)
        * group_imbalanced   -- uneven load
        * group_overloaded   -- more tasks than capacity
      Select busiest group (worst type, highest load)

  Step 2: Find busiest CPU in that group
    find_busiest_queue():
      Pick CPU with highest load in busiest group

  Step 3: Migrate tasks
    detach_tasks():
      Lock busiest rq
      Pick suitable tasks (check affinity, cache-hot)
      Detach from busiest rq
    attach_tasks():
      Lock this rq
      Enqueue migrated tasks

## Visual Example of Periodic Load Balance
  BEFORE balance (imbalanced):
  +--------+  +--------+  +--------+  +--------+
  | CPU 0  |  | CPU 1  |  | CPU 2  |  | CPU 3  |
  | 5 tasks|  | 1 task |  | 0 tasks|  | 6 tasks|
  |========|  |==      |  |        |  |========|
  |========|  |        |  |        |  |========|
  |==      |  |        |  |        |  |========|
  +--------+  +--------+  +--------+  +--------+

  Load balance runs on CPU 2 (idle):
  1. Find busiest group -> group containing CPU 3 (6 tasks)
  2. Find busiest CPU -> CPU 3
  3. Migrate 2 tasks from CPU 3 -> CPU 2
  4. Also: CPU 0 migrates 1 task to CPU 1

  AFTER balance:
  +--------+  +--------+  +--------+  +--------+
  | CPU 0  |  | CPU 1  |  | CPU 2  |  | CPU 3  |
  | 4 tasks|  | 2 tasks|  | 2 tasks|  | 4 tasks|
  |========|  |====    |  |====    |  |========|
  |========|  |        |  |        |  |========|
  +--------+  +--------+  +--------+  +--------+

## Idle Balance -- The Most Important Balancer
newidle_balance() is called when a CPU is about to go idle. It proactively steals tasks from busy CPUs to keep the system balanced.
static int newidle_balance(struct rq *this_rq, struct rq_flags *rf)
{
    int pulled_task = 0;
    /* Don't bother if we just went idle very recently */
    if (this_rq->avg_idle < sysctl_sched_migration_cost)
        return 0;  /* CPU will be busy again soon, skip */
    /* Walk scheduling domains bottom-up */
    for_each_domain(this_cpu, sd) {
        if (!(sd->flags & SD_LOAD_BALANCE))
            continue;
        pulled_task = load_balance(this_cpu, this_rq, sd,
                                    CPU_NEWLY_IDLE, &continue_balancing);
        if (pulled_task > 0)
            break;  /* Got work! */
    }
    return pulled_task;
}

## Cache-Hot Detection
★ Key Interview Point: The scheduler avoids migrating "cache-hot" tasks. A task is cache-hot if it ran recently (within sysctl_sched_migration_cost = 500µs) on its current CPU. Cross-cluster migration is much more expensive than within-cluster due to separate L2 caches.
  Task ran < 0.5ms ago  -> cache-hot -> DON'T migrate
  Task ran > 0.5ms ago  -> cache-cold -> OK to migrate

  Within cluster: shared L2, migration cost is LOW
  Cross cluster:  separate L2, migration cost is HIGH
    -> MC domain: SD_SHARE_PKG_RESOURCES (shared cache)
    -> DIE domain: no SD_SHARE_PKG_RESOURCES


# Level 5, Topic 29: Migration Threads
Each CPU has a dedicated migration kernel thread (migration/0, migration/1, ...) that runs in stop_sched_class -- the highest scheduling priority. These handle active migration of running tasks, CPU hotplug, and stop_machine() operations.
## Active Migration Flow
  Problem: Task A is running on CPU 0.
  Load balancer on CPU 2 decides A should move to CPU 2.
  But A is CURRENTLY RUNNING on CPU 0! Can't just dequeue it!

  Solution:
  1. CPU 2 sets rq[0]->active_balance = 1
  2. CPU 2 sends IPI to CPU 0
  3. migration/0 wakes up on CPU 0 (stop class!)
  4. migration/0 preempts Task A
  5. migration/0 calls stop_one_cpu():
     -> Dequeues Task A from rq[0]
     -> Enqueues Task A on rq[2]
  6. Task A now runs on CPU 2

## CPU Hotplug -- Migration Thread Drains All Tasks
echo 0 > /sys/devices/system/cpu/cpu3/online

cpu_down(3):
  1. Stop scheduling new tasks to CPU 3 (clear from cpu_active_mask)
  2. migration/3 wakes up (stop_sched_class)
     -> For each task on rq: find new CPU (respecting affinity)
     -> Migrate all tasks off CPU 3
     -> Drain all timers, hrtimers, softirqs
  3. Clear CPU 3 from cpu_online_mask
  4. CPU 3 calls:
     arch_cpu_idle_dead() -> WFI (ARM)
     or PSCI CPU_OFF -> firmware powers down core
  5. CPU 3 is now offline

Bringing it back: echo 1 > /sys/devices/system/cpu/cpu3/online
  1. PSCI CPU_ON -> firmware wakes CPU 3
  2. CPU 3 re-initializes (secondary_start_kernel)
  3. Set cpu_online_mask, cpu_active_mask
  4. Rebuild scheduling domains
  5. CPU 3 enters idle loop, ready for tasks


# Level 5, Topic 30: Push/Pull Migration (RT)
CFS load balancing is based on "fairness" -- spread load evenly. RT migration is based on PRIORITY -- the highest priority RT task must ALWAYS be running on SOME CPU.
## The Problem
  Both CPUs busy with RT tasks:
  +--------+  +--------+
  | CPU 0  |  | CPU 1  |
  | RT:99  |  | RT:50  |
  |(running)|  |(running)|
  +--------+  +--------+

  Now RT:80 wakes up on CPU 0's runqueue:
  +--------+  +--------+
  | CPU 0  |  | CPU 1  |
  | RT:99  |  | RT:50  |
  | RT:80  |  |        |  <- RT:80 is waiting! But RT:50 runs
  |(queued)|  |        |     on CPU 1 with LOWER priority!
  +--------+  +--------+

  Solution: PUSH RT:80 to CPU 1 (preempts RT:50)
  +--------+  +--------+
  | CPU 0  |  | CPU 1  |
  | RT:99  |  | RT:80  |  <- RT:80 pushed to CPU 1
  |        |  | RT:50  |     RT:50 preempted
  |        |  |(queued)|
  +--------+  +--------+

## PUSH Migration Algorithm
  Triggered by: enqueue_task_rt() when new RT task queued
  but can't preempt current (lower prio)

  push_rt_task():
  1. Find highest-priority pushable RT task on this rq
  2. Find a CPU where this task CAN preempt current:
     -> Scan rq->rd->cpupri (priority-indexed CPU map)
     -> Find CPU running lowest-priority task
     -> Check: pushable_task->prio < target_rq->curr
  3. If found: migrate task to target CPU
  4. Repeat until no more pushable tasks
## PULL Migration Algorithm
  Triggered by: dequeue_task_rt() when current RT task
  blocks/finishes and next task has lower priority

  pull_rt_task():
  1. This CPU's highest RT task just left
  2. Check other CPUs: do they have RT tasks that are
     queued (not running) with higher priority than
     what we're about to run?
  3. If yes: pull that task to this CPU

  Example:
  CPU 0: RT:99 finishes -> next is RT:30
  CPU 1: RT:80 running, RT:60 queued
  -> Pull RT:60 from CPU 1 to CPU 0
     (RT:60 > RT:30, so it should run on CPU 0)

## cpupri -- Priority-Indexed CPU Map
  cpupri is a data structure that allows O(1) lookup of
  "which CPU is running the lowest priority task?"

  Priority levels:
  CPUPRI_IDLE   = 0  (CPU is idle)
  CPUPRI_NORMAL = 1  (running CFS/normal task)
  CPUPRI_RT_2   = 2  (RT prio 0, lowest RT)
  ...                 ...
  CPUPRI_RT_99  = 99 (RT prio 98)
  CPUPRI_HIGHER = 100 (running stop/DL task)

  Example state:
  CPU 0: running RT:99 -> cpupri level 99
  CPU 1: running RT:50 -> cpupri level 50
  CPU 2: running CFS   -> cpupri level 1 (NORMAL)
  CPU 3: idle           -> cpupri level 0 (IDLE)

  Push RT:80 -> scan from level 0:
    Level 0 (IDLE): CPU 3 available! -> Push to CPU 3 OK


# Level 5, Topic 31: IPI for Rescheduling
When CPU 0 needs CPU 1 to reschedule (e.g., after waking a task that should run on CPU 1), it sends an Inter-Processor Interrupt (IPI) via the GIC.
## IPI Flow on ARM64
  CPU 0                              CPU 1
  |                                  |
  | try_to_wake_up(task_B)           | Running task_C
  |   -> task_B should run on CPU 1  |
  |   -> enqueue task_B on rq        |
  |   -> resched_curr(rq)            |
  |     -> set TIF_NEED_RESCHED      |
  |       on CPU 1's current task    |
  |     -> smp_send_reschedule(1)    |
  |       |                          |
  |       |  SGI (IPI) via GIC       |
  |       +------------------------->|  IRQ!
  |                                  |  |
  |                                  |  scheduler_ipi()
  |                                  |  (mostly a no-op,
  |                                  |   real work on IRQ
  |                                  |   return path)
  |                                  |  |
  |                                  |  IRQ return:
  |                                  |  check NEED_RESCHED
  |                                  |  -> SET!
  |                                  |  -> schedule()
  |                                  |  -> switch to task_B

## ARM64 IPI Implementation
/* arch/arm64/kernel/smp.c */
void smp_send_reschedule(int cpu)
{
    smp_cross_call(cpumask_of(cpu), IPI_RESCHEDULE);
    /* Sends SGI (Software Generated Interrupt) via GIC */
}

/* GIC SGI numbers used by Linux on ARM: */
enum ipi_msg_type {
    IPI_RESCHEDULE,   /* 0 -- Reschedule request */
    IPI_CALL_FUNC,    /* 1 -- Call function on target CPU */
    IPI_CPU_STOP,     /* 2 -- Stop CPU (panic/kexec) */
    IPI_CPU_CRASH_STOP,
    IPI_TIMER,        /* 4 -- Timer broadcast */
    IPI_IRQ_WORK,     /* 5 -- IRQ work */
    NR_IPI
};

/* Count IPIs per CPU */
cat /proc/interrupts | grep IPI
# IPI0: Rescheduling interrupts (high count = many cross-CPU wakeups)
# IPI1: Function call interrupts


LEVEL 6 — ARM-SPECIFIC: big.LITTLE & ENERGY-AWARE SCHEDULING
Prerequisites: Level 5 (SMP & Load Balancing). Topics 32–38 cover heterogeneous ARM scheduling -- big.LITTLE, DynamIQ, CPU capacity, EAS, uclamp, and thermal pressure.

# Level 6, Topic 32: big.LITTLE / DynamIQ Architecture
## The Heterogeneous Computing Problem
  Traditional SMP: All CPUs are identical
  +----++----++----++----+
  | A78|| A78|| A78|| A78|  All same performance & power
  +----++----++----++----+

  big.LITTLE: CPUs have DIFFERENT capabilities
  +----++----++----++----+  +----++----++----++----+
  | A55|| A55|| A55|| A55|  | A78|| A78|| A78|| A78|
  | LOW|| LOW|| LOW|| LOW|  |HIGH||HIGH||HIGH||HIGH|
  |power         power  |  | perf          perf  |
  +----++----++----++----+  +----++----++----++----+
       LITTLE cluster              big cluster

  DynamIQ (e.g., Snapdragon 8 Gen 2: 1+3+4 config):
  +------+  +------------------+  +--------------------+
  | CPU7 |  |CPU4 | CPU5 | CPU6|  |CPU0|CPU1|CPU2|CPU3|
  | X3   |  | A715| A715 | A715|  |A510|A510|A510|A510|
  |@3.2G |  |    @ 2.8 GHz    |  |        @ 2.0 GHz  |
  | prime|  |      mid        |  |       LITTLE       |
  +------+  +------------------+  +--------------------+
   cap:1024      cap: ~700              cap: ~300
★ The scheduler must decide: which task goes on which CPU? Wrong placement wastes power (light task on big core) or hurts performance (heavy task on LITTLE core).

## CPU Capacity
| CPU Type | Max Freq | IPC | Capacity | Normalized |
| A510 (LITTLE) | 2.0 GHz | 1.0x | ~300 | ~29% |
| A715 (mid) | 2.8 GHz | 1.5x | ~700 | ~68% |
| X3 (prime) | 3.2 GHz | 2.0x | 1024 | 100% (reference) |


## How Capacity is Computed
  Capacity comes from:
  1. capacity-dmips-mhz in device tree (raw compute per MHz)
  2. Scaled by current frequency:
     capacity = raw_capacity x (curr_freq / max_freq)
  3. Reduced by thermal pressure:
     effective_capacity = capacity - thermal_pressure

  arch_scale_cpu_capacity(cpu):
    Returns ORIGINAL capacity (no freq scaling)
    Used for task placement decisions

  capacity_of(cpu):
    Returns capacity scaled by current frequency
    AND reduced by RT/DL/IRQ time
    = arch_capacity x (curr_freq/max_freq)
      - capacity used by RT tasks
      - capacity used by IRQ handling

  capacity_orig_of(cpu):
    Returns capacity at max frequency (no thermal)

## Misfit Task Detection
  A "misfit" task needs MORE capacity than its current CPU.

  Task utilization: 800 (out of 1024)
  Running on: A510 (capacity 300)

  Task util (800) > CPU capacity (300) -> MISFIT! WARNING

  The scheduler flags this:
  rq->misfit_task_load = task_util

  Load balancer sees misfit flag:
  -> group_misfit_task classification
  -> Migrates task to a bigger CPU

  After migration:
  Task util (800) < A78 capacity (1024) -> Fits! OK

  Code path:
  task_tick_fair() -> check_for_misfit():
    if (task_util > capacity_of(cpu))
        rq->misfit_task_load = task_util;
    else
        rq->misfit_task_load = 0;


# Level 6, Topic 33: CPU Capacity & Frequency Scaling
## Frequency-Capacity Relationship
| Frequency | Capacity | Power (mW) | Perf/Watt | Notes |
| 500 MHz | 160 | 50 | 3.2 cap/mW (BEST) | Most efficient |
| 1000 MHz | 320 | 150 | 2.1 cap/mW | Good balance |
| 1500 MHz | 480 | 350 | 1.4 cap/mW | Moderate |
| 2000 MHz | 640 | 700 | 0.9 cap/mW | Diminishing returns |
| 3200 MHz | 1024 | 2500 | 0.4 cap/mW (WORST) | Power-hungry |

★ Key Insight: Power grows SUPER-LINEARLY with frequency (P ∝ V² × f, and V increases with f). This is WHY energy-aware scheduling matters -- running a light task on a big core at high frequency wastes enormous power!

# Level 6, Topic 34: schedutil CPUFreq Governor
## Scheduler-Driven Frequency Selection
  Traditional governors (ondemand, conservative):
  +------------------------------------------------------+
  | Periodic sampling (every 10-80ms):                   |
  |   Read CPU utilization from /proc/stat               |
  |   If util > up_threshold -> increase freq            |
  |   If util < down_threshold -> decrease freq          |
  |                                                      |
  | Problems:                                            |
  |   * Sampling delay (react 10-80ms late!)             |
  |   * No per-task information                          |
  +------------------------------------------------------+

  schedutil governor (scheduler-integrated):
  +------------------------------------------------------+
  | Called DIRECTLY from scheduler on every:             |
  |   * scheduler_tick()                                 |
  |   * task wakeup (try_to_wake_up)                     |
  |   * task migration                                   |
  |                                                      |
  | Formula:                                             |
  |   next_freq = 1.25 x (util / max) x max_freq        |
  |   (1.25 = headroom factor, slightly over-provision)  |
  |                                                      |
  | Advantages:                                          |
  |   * Zero sampling delay (reacts immediately!)        |
  |   * Per-task awareness (via PELT)                    |
  |   * Integrated with EAS decisions                    |
  +------------------------------------------------------+

## PELT (Per-Entity Load Tracking)
PELT tracks utilization as a decaying average. Each 1ms period contributes to the running average. Older periods decay exponentially with half-life ~32ms.
  util_avg = Sum(contribution_i x decay_factor^i)

  Example: Task runs for 5ms, sleeps for 5ms:
  Time: |========__| |========__| |========__|
         0   5  10   15  20  25

  util_avg ~= 512 (50% of 1024 max)
  -> schedutil sets freq to ~50% of max

  Tracked per scheduling entity (se):
  * se.avg.util_avg      -- task utilization
  * se.avg.load_avg      -- task load (weighted by prio)
  * cfs_rq.avg.util_avg  -- CPU utilization (sum of tasks)
## schedutil Flow
  scheduler_tick() or try_to_wake_up()
       |
       v
  update_load_avg()          <- Update PELT signals
       |
       v
  cpufreq_update_util()      <- Hook into cpufreq
       |
       v
  sugov_update_shared()      <- schedutil governor callback
       |
       |-- Calculate desired frequency:
       |   util = cpu_util_cfs(cpu) + cpu_util_rt(cpu)
       |   freq = 1.25 x util / max_capacity x max_freq
       |
       |-- Apply uclamp limits:
       |   freq = clamp(freq, uclamp_min_freq, uclamp_max_freq)
       |
       +-- Request frequency change:
           cpufreq_driver->target(freq)
           -> On Qualcomm: qcom-cpufreq-hw driver
           -> Writes to DCVS hardware registers
           -> Frequency changes in ~10-50 microseconds


# Level 6, Topic 35: Energy Model Framework
## How the Kernel Knows Energy Costs
The Energy Model (EM) provides a table of "At frequency X, this CPU consumes Y power." This data is registered by the cpufreq driver from device tree information.
/* Key data structures */
struct em_perf_domain {
    struct em_perf_state *table;  /* OPP table */
    int nr_perf_states;           /* # of OPPs */
    unsigned long cpus[];         /* CPUs in domain */
};

struct em_perf_state {
    unsigned long frequency;  /* kHz */
    unsigned long power;      /* mW (or abstract) */
    unsigned long cost;       /* energy per cycle = power / frequency */
    unsigned long performance;/* capacity at this freq */
};

## Example Energy Model Tables
### LITTLE Cluster (A510)
| Freq (MHz) | Capacity | Power (mW) | Cost (higher = less efficient) | Comment |
| 500 | 150 | 30 | 200 | Most efficient |
| 1000 | 300 | 100 | 333 |  |
| 1500 | 450 | 250 | 556 |  |
| 2000 | 600 | 500 | 833 | Least efficient |


### big Cluster (A78)
| Freq (MHz) | Capacity | Power (mW) | Cost | Comment |
| 500 | 160 | 80 | 500 | Less efficient than LITTLE@500 |
| 1500 | 480 | 350 | 729 |  |
| 2500 | 800 | 1200 | 1500 |  |
| 3200 | 1024 | 2500 | 2441 | Most power hungry |

★ Key Observation: LITTLE@1000MHz: 300 capacity for 100mW -> cost 333. big@500MHz: 160 capacity for 80mW -> cost 500. LITTLE is MORE energy-efficient for the same work! EAS will prefer LITTLE for light tasks.

## Energy Model Registration
/* Registered by cpufreq driver or from device tree */
/* On Qualcomm: qcom-cpufreq-hw driver registers EM */

/* From device tree (dynamic-power-coefficient): */
cpu0: cpu@0 {
    compatible = "arm,cortex-a55";
    dynamic-power-coefficient = <100>;
    /* Power = coefficient x voltage^2 x frequency */
};

/* Or registered programmatically: */
em_dev_register_perf_domain(cpu_dev, nr_states, &em_cb, cpus, true);


# Level 6, Topic 36: Energy-Aware Scheduling (EAS)
EAS is the heart of task placement on big.LITTLE/DynamIQ. It is called during try_to_wake_up() -> select_task_rq_fair() and finds the CPU that minimizes total system energy.
## The Core Algorithm: find_energy_efficient_cpu()
  find_energy_efficient_cpu(task, prev_cpu):

  Goal: Find the CPU that results in the LOWEST
  total SYSTEM energy if we place this task there.

  Algorithm:
  1. For each performance domain (cluster):
     a. Find the CPU with max spare capacity
        (best fit within the cluster)
     b. Compute system energy IF task placed there

  2. Compare energy of all candidate placements

  3. Pick the CPU with LOWEST total energy

  4. But only migrate if energy savings > 6%
     (avoids ping-pong migration for tiny gains)

## Step-by-Step Walkthrough
Example: Task T wakes up (util=200), was on CPU 1 (LITTLE). System: 4 LITTLE (CPU 0-3, cap=600) + 4 big (CPU 4-7, cap=1024).
  Step 1: For LITTLE cluster (perf domain 0):
  +------------------------------------------------------+
  | CPU 0: util=400, spare=200 -> can fit T (200) OK     |
  | CPU 1: util=300, spare=300 -> can fit T (200) OK     |
  |        (prev_cpu -- preferred if energy is same)     |
  | CPU 2: util=550, spare=50  -> can't fit T (200) FAIL |
  | CPU 3: util=100, spare=500 -> can fit T (200) OK     |
  |                                                      |
  | Best candidate: CPU 3 (max spare capacity)           |
  | Also consider: CPU 1 (prev_cpu, cache warm)          |
  +------------------------------------------------------+

  Step 2: For big cluster (perf domain 1):
  +------------------------------------------------------+
  | CPU 4: util=800, spare=224 -> can fit T (200) OK     |
  | CPU 5: util=200, spare=824 -> can fit T (200) OK     |
  | CPU 6: util=0,   spare=1024-> can fit T (200) OK     |
  | CPU 7: util=900, spare=124 -> can't fit T (200) FAIL |
  |                                                      |
  | Best candidate: CPU 6 (max spare capacity)           |
  +------------------------------------------------------+

  Step 3: Compute energy for each candidate:
  +----------------+---------------+-------------+
  | Placement      | LITTLE energy | big energy  |
  +----------------+---------------+-------------+
  | CPU 1 (prev)   | 450 mW        | 1800 mW     |
  | Total: 2250    |               |             |
  +----------------+---------------+-------------+
  | CPU 3 (LITTLE) | 480 mW        | 1800 mW     |
  | Total: 2280    |               |             |
  +----------------+---------------+-------------+
  | CPU 6 (big)    | 380 mW        | 1900 mW     |
  | Total: 2280    |               |             |
  +----------------+---------------+-------------+

  Winner: CPU 1 (prev_cpu) with 2250 mW
  -> Stay on LITTLE, no migration needed
  -> Saves power vs moving to big cluster

## The 6% Margin (Hysteresis)
  To avoid unnecessary migrations:

  energy_diff = prev_energy - best_energy
  margin = best_energy x 6 / 100

  if (energy_diff < margin)
      -> Stay on prev_cpu (not worth migrating)
  else
      -> Migrate to best_cpu

  Example:
  prev_energy = 2250 mW
  best_energy = 2200 mW (on some other CPU)
  margin = 2200 x 0.06 = 132 mW
  diff = 2250 - 2200 = 50 mW
  50 < 132 -> DON'T migrate (savings too small)

  This prevents:
  * Cache thrashing from frequent migrations
  * IPI overhead
  * Ping-pong between clusters

## EAS Prerequisites (When EAS is Active)
| Requirement | Why It Matters |
| Energy Model registered for ALL perf domains | em_dev_register_perf_domain() called for each cluster |
| Asymmetric CPU capacities detected | SD_ASYM_CPUCAPACITY flag in sched_domain -- signals big.LITTLE |
| schedutil governor active | EAS needs scheduler-driven DVFS. Won't work with ondemand/performance |
| System NOT overutilized | If any CPU > 80% util: EAS disabled, falls back to load balancing |
| CPU count <= em_max_pd_cpus | EAS computation is O(CPUs x OPPs). Too expensive for large NUMA servers |


## The Overutilized Check
  Normal load (EAS active):
  +----++----++----++----+  +----++----++----++----+
  | 30%|| 45%|| 20%|| 60%|  | 50%|| 70%|| 40%|| 55%|
  | A55|| A55|| A55|| A55|  | A78|| A78|| A78|| A78|
  +----++----++----++----+  +----++----++----++----+
  All < 80% -> EAS active -> energy-optimal placement OK

  Heavy load (overutilized, EAS disabled):
  +----++----++----++----+  +----++----++----++----+
  | 95%|| 88%|| 92%|| 85%|  | 90%|| 95%|| 88%|| 92%|
  | A55|| A55|| A55|| A55|  | A78|| A78|| A78|| A78|
  +----++----++----++----+  +----++----++----++----+
  Multiple > 80% -> overutilized -> load balance mode
  (Performance matters more than energy when overloaded)

## Complete EAS Code Flow
/* kernel/sched/fair.c -- simplified */
static int find_energy_efficient_cpu(struct task_struct *p, int prev_cpu)
{
    struct root_domain *rd = cpu_rq(prev_cpu)->rd;
    unsigned long prev_delta = ULONG_MAX;
    unsigned long best_delta = ULONG_MAX;
    int best_energy_cpu = prev_cpu;
    struct perf_domain *pd;

    /* Iterate over all performance domains (clusters) */
    for_each_pd(pd, rd) {
        unsigned long max_spare_cap = 0;
        int max_spare_cap_cpu = -1;

        /* Find CPU with max spare capacity in this domain */
        for_each_cpu(cpu, perf_domain_span(pd)) {
            if (!cpumask_test_cpu(cpu, p->cpus_ptr))
                continue;  /* Task can't run here (affinity) */

            util = cpu_util_next(cpu, p, cpu);
            spare_cap = capacity_of(cpu) - util;

            if (spare_cap > max_spare_cap) {
                max_spare_cap = spare_cap;
                max_spare_cap_cpu = cpu;
            }
        }

        if (max_spare_cap_cpu < 0)
            continue;  /* No CPU in this domain can fit task */

        /* Compute energy delta for placing task on this CPU */
        cur_delta = compute_energy(p, max_spare_cap_cpu, pd)
                  - base_energy(pd);

        if (cur_delta < best_delta) {
            best_delta = cur_delta;
            best_energy_cpu = max_spare_cap_cpu;
        }
        if (max_spare_cap_cpu == prev_cpu)
            prev_delta = cur_delta;
    }

    /* Apply 6% margin -- only migrate if significant savings */
    if (prev_delta != ULONG_MAX) {
        if ((prev_delta - best_delta) * 100 < prev_delta * 6)
            return prev_cpu;  /* Not worth migrating */
    }

    return best_energy_cpu;
}


# Level 6, Topic 37: Utilization Clamping (uclamp)
uclamp allows setting bounds on a task's utilization signal, overriding what PELT computes. This directly controls which CPU and which frequency the task gets.
## What is uclamp?
  Without uclamp:
  Task util = 200 (PELT measured)
  -> schedutil sets freq for util=200
  -> EAS places on LITTLE (low util, save energy)

  With uclamp_min = 600 (BOOST):
  Task util = max(200, 600) = 600 (boosted!)
  -> schedutil sets freq for util=600 (higher freq)
  -> EAS may place on big (needs more capacity)

  With uclamp_max = 300 (CAP):
  If PELT was 500: Task util = min(500, 300) = 300 (capped!)
  -> Prevents task from boosting freq too high
  -> Saves power for background tasks

  uclamp_min: "This task needs AT LEAST this much performance"
              (floor / boost)
  uclamp_max: "This task should use AT MOST this much performance"
              (ceiling / cap)
  Range: 0 to 1024 (maps to 0% to 100% of capacity)

## Use Cases on ARM/Android
| Use Case | Problem | uclamp Solution |
| UI Thread Boost | Touch handler has low PELT util but needs LOW LATENCY | uclamp_min=512 -> Forces big core + high frequency |
| Background Cap | Background sync spikes occasionally, steals big core from foreground | uclamp_max=300 -> Task stays on LITTLE cores |
| RT Audio Pipeline | Audio thread runs 2ms/20ms, PELT util~100, needs guaranteed perf | uclamp_min=400 -> Ensures mid/big core placement, no audio glitches |
| Game Rendering | Variable frame time needs consistent high performance | uclamp_min=600, uclamp_max=1024 -> Prime core, max freq |


## Setting uclamp
/* Per-task uclamp via sched_setattr() */
struct sched_attr attr = {
    .size = sizeof(attr),
    .sched_flags = SCHED_FLAG_UTIL_CLAMP_MIN | SCHED_FLAG_UTIL_CLAMP_MAX,
    .sched_util_min = 512,   /* Boost: at least 50% capacity */
    .sched_util_max = 800,   /* Cap: at most ~78% capacity */
};
syscall(SYS_sched_setattr, pid, &attr, 0);

/* Per-cgroup uclamp (cgroups v2) */
echo 512 > /sys/fs/cgroup/foreground/cpu.uclamp.min
echo 1024 > /sys/fs/cgroup/foreground/cpu.uclamp.max
echo 0   > /sys/fs/cgroup/background/cpu.uclamp.min
echo 300 > /sys/fs/cgroup/background/cpu.uclamp.max

/* Android cgroup hierarchy: */
echo 600 > /dev/cpuctl/top-app/cpu.uclamp.min
echo 400 > /dev/cpuctl/foreground/cpu.uclamp.min
echo 200 > /dev/cpuctl/background/cpu.uclamp.max

## How uclamp Affects the Scheduler
  1. Task placement (EAS):
  effective_util = clamp(pelt_util, uclamp_min, uclamp_max)
  Used in: find_energy_efficient_cpu(), fits_capacity()
           misfit detection

  2. Frequency selection (schedutil):
  cpu_util = Sum(clamp(task_util, uclamp_min, uclamp_max))
             for all tasks on this CPU
  freq = 1.25 x cpu_util / max_capacity x max_freq

  3. CPU-level aggregation:
  cpu_uclamp_min = max(task_A.uclamp_min, task_B.uclamp_min, ...)
  cpu_uclamp_max = max(task_A.uclamp_max, task_B.uclamp_max, ...)

  IMPORTANT: If ANY task on the CPU has high uclamp_min,
  the entire CPU runs at higher frequency!
  -> This is why task placement matters!
  -> Don't put boosted tasks on same CPU as capped ones


# Level 6, Topic 38: Thermal Pressure
## How Thermal Throttling Affects Scheduling
  Normal operation:
  big core: max_freq=3.2GHz, capacity=1024

  Thermal throttling kicks in:
  big core: max_freq=2.0GHz, capacity=640
  (firmware limits freq via thermal zone / DCVS)

  Without thermal pressure awareness:
  Scheduler thinks big core has capacity 1024
  -> Places heavy tasks there
  -> Tasks can't get enough CPU -> performance drops
  -> Misfit detection doesn't trigger (1024 > util)

  With thermal pressure:
  Scheduler knows effective capacity = 640
  -> Adjusts task placement accordingly
  -> May move tasks to LITTLE cores if they fit
  -> Misfit detection works correctly

## Thermal Pressure Mechanism
  Thermal Zone Driver
       |
       | Temperature exceeds threshold
       v
  Thermal Governor (step_wise / power_allocator)
       |
       | Decides to limit cooling device
       v
  CPU Cooling Device (cpu_cooling)
       |
       | Caps max frequency
       | cpufreq_cooling_set_cur_state()
       v
  arch_set_thermal_pressure(cpus, capped_capacity)
       |
       | Updates per-CPU thermal_pressure variable
       v
  Scheduler reads thermal pressure:
  effective_capacity = arch_scale_cpu_capacity(cpu)
                     - arch_scale_thermal_pressure(cpu)

  Example:
  Original capacity: 1024
  Thermal pressure:   384 (throttled from 3.2->2.0GHz)
  Effective capacity:  640

## Thermal Pressure in Code
/* arch/arm64/kernel/topology.c */
void arch_set_thermal_pressure(const struct cpumask *cpus,
                                unsigned long th_pressure)
{
    int cpu;
    for_each_cpu(cpu, cpus)
        WRITE_ONCE(per_cpu(thermal_pressure, cpu), th_pressure);
}

/* Used in scheduler: */
static unsigned long capacity_of(int cpu)
{
    unsigned long cap = arch_scale_cpu_capacity(cpu);
    /* Subtract thermal pressure */
    cap -= arch_scale_thermal_pressure(cpu);
    /* Subtract capacity used by RT/DL/IRQ */
    cap -= cpu_util_rt(cpu_rq(cpu));
    cap -= cpu_util_dl(cpu_rq(cpu));
    cap -= cpu_util_irq(cpu_rq(cpu));
    return cap;
}

## Qualcomm Thermal Management Stack
  Hardware: On-die thermal sensors (tsens)
  * Multiple sensors per cluster
  * Read via MMIO / SCM calls
  * Interrupt on threshold crossing

  Firmware: DCVS (Dynamic Clock and Voltage Scaling)
  * Can autonomously limit frequency
  * Communicates limits to kernel
  * LMh (Limits Management hardware)

  Kernel:
  * qcom-tsens driver -> thermal zone
  * thermal governor -> cooling device
  * cpu_cooling -> cpufreq max limit
  * arch_set_thermal_pressure() -> scheduler
## Viewing Thermal State
# Check thermal zones and temperatures
cat /sys/class/thermal/thermal_zone*/type
cat /sys/class/thermal/thermal_zone*/temp
# Output: 45000 (= 45.0 degrees C)

# Check effective CPU capacity (reflects thermal pressure)
cat /sys/devices/system/cpu/cpu*/cpu_capacity
# LITTLE: 277 (normally ~300, slightly throttled)
# big:    640 (normally 1024, heavily throttled!)

# Check cooling device state
cat /sys/class/thermal/cooling_device*/cur_state
cat /sys/class/thermal/cooling_device*/max_state


INTERVIEW READY SUMMARY

# Level 5: SMP & Load Balancing -- Interview Questions
| Interview Question | Key Answer Points |
| What is a scheduling domain? | Hierarchical grouping of CPUs for load balancing. Each domain covers a set of CPUs at a certain topology level (SMT, MC, DIE, NUMA). Load balancing runs bottom-up through domains. |
| How does load balancing work? | Periodic: SCHED_SOFTIRQ calls load_balance() per domain. Idle: newidle_balance() steals tasks when CPU goes idle. Steps: find busiest group -> find busiest CPU -> migrate tasks. |
| What is a misfit task? | A task whose utilization exceeds its current CPU's capacity. The load balancer detects this (group_misfit_task) and migrates the task to a bigger CPU. |
| How does RT task migration work? | PUSH: RT task wakes but can't preempt current -> find CPU running lower-prio task via cpupri -> push there. PULL: RT task finishes -> check if other CPUs have queued high-prio RT tasks -> pull them. |
| How does rescheduling work across CPUs? | CPU 0 sets TIF_NEED_RESCHED on CPU 1's current task, then calls smp_send_reschedule(1) which sends SGI via GIC. CPU 1 takes IRQ, returns, checks TIF_NEED_RESCHED, calls schedule(). |
| What is IPI_RESCHEDULE? | SGI (Software Generated Interrupt) number 0. Sent via GIC when a CPU needs to reschedule. The handler is scheduler_ipi() which triggers schedule() on the IRQ return path. |


# Level 6: EAS & big.LITTLE -- Interview Questions
| Interview Question | Key Answer Points |
| What is EAS? | Energy-Aware Scheduling -- places tasks on the CPU that minimizes total system energy. Uses Energy Model + PELT utilization. Active only when: EM registered, schedutil governor, asymmetric capacities, system not overutilized. |
| How does find_energy_efficient_cpu() work? | For each perf domain: find CPU with max spare capacity. Compute total system energy for each candidate. Pick lowest energy. Apply 6% margin before migrating to prevent thrashing. |
| When is EAS disabled? | When system is overutilized (any CPU > 80% util), no Energy Model registered, non-schedutil governor, or too many CPUs (large NUMA servers). |
| What is uclamp? | Utilization clamping. uclamp_min = floor/boost (task needs at least this performance). uclamp_max = ceiling/cap (task uses at most this). Range 0-1024. Affects both task placement and frequency selection via schedutil. |
| How does thermal pressure work? | Thermal throttling reduces max freq -> arch_set_thermal_pressure() updates per-CPU value -> scheduler subtracts from capacity -> tasks placed according to reduced effective capacity. Prevents misfit tasks being placed on throttled big cores. |
| How does schedutil differ from ondemand? | schedutil: called directly from scheduler (zero delay), uses PELT signals, integrates with uclamp. ondemand: samples periodically (10-80ms delay), reads /proc/stat. schedutil is required for EAS to work. |
| What is PELT? | Per-Entity Load Tracking. Exponentially decaying average of task/CPU utilization. Half-life ~32ms. Tracked per scheduling entity. Used by schedutil for frequency selection and EAS for task placement. |
| What is the 6% EAS margin? | Hysteresis to prevent excessive migrations. Only migrate if energy savings > 6% of best_energy. Without this, tiny energy differences would cause constant task ping-pong between clusters. |


# Key Concepts Quick Reference
## EAS Decision Flow
  Task wakes up
       |
       v
  Is system overutilized? (any CPU > 80%)
  YES -> Standard load balancing (performance mode)
  NO  -> EAS path:
          |
          v
          For each perf_domain (LITTLE, mid, big, prime):
            Find CPU with most spare capacity
            Compute system energy delta
          Pick CPU with lowest energy delta
          Is saving > 6%? YES -> migrate  NO -> stay

## uclamp Impact Summary
  PELT util = 200
  uclamp_min = 0    -> effective = 200  -> LITTLE @ low freq
  uclamp_min = 512  -> effective = 512  -> big @ medium freq
  uclamp_min = 1024 -> effective = 1024 -> prime @ max freq

  PELT util = 800
  uclamp_max = 1024 -> effective = 800  -> big core
  uclamp_max = 300  -> effective = 300  -> LITTLE core
  uclamp_max = 0    -> effective = 0    -> almost no CPU needed

## Capacity Chain
  DT: capacity-dmips-mhz                    (raw compute per MHz)
       |
       v per-CPU normalized value
  arch_scale_cpu_capacity()                 (at max freq)
       |
       | x (curr_freq / max_freq)
       v
  capacity_orig_of()                        (freq-scaled)
       |
       | - thermal_pressure
       | - RT/DL/IRQ usage
       v
  capacity_of()                             (available for CFS)
       |
       | used by EAS, misfit detection,
       | load balancing decisions
       v
  Task fits? (task_util <= capacity_of)

## One-Liner Key Points for Interview
- "Direct reclaim is the enemy of latency -- pre-allocate everything."
- "EAS only works if: Energy Model + schedutil + asymmetric capacities + not overutilized."
- "uclamp_min boosts performance floor; uclamp_max caps power ceiling."
- "Thermal pressure reduces capacity_of() -- scheduler automatically places tasks on cooler cores."
- "The 6% EAS margin prevents ping-pong migration for tiny energy gains."
- "PELT half-life is ~32ms -- it tracks utilization trend, not instantaneous values."
- "misfit task: util > capacity_of(current_cpu) -> load balancer migrates to bigger CPU."
- "schedutil has ZERO sampling delay -- reacts to utilization change at next scheduler tick."
- "Push migration = RT task can't preempt here, find where it can. Pull migration = I'm free, find a queued RT task."
- "cpupri is an O(1) data structure mapping priority levels to CPU masks -- used by RT push/pull."
- "Qualcomm DCVS hardware handles OPP transitions in 10-50 microseconds without kernel involvement."
- "ICC bandwidth voting: icc_set_bw(path, avg, peak) -- vote 0 when idle for DDR power savings."


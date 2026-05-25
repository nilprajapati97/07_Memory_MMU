Linux Scheduling Internals
Linux Posts  •  Tuesday, 8 March 2016
Linux scheduling is at the heart of how the kernel decides which task to run next. This document covers the internal mechanisms of the Linux CPU scheduler — from scheduling classes and task initialization, through runqueues and preemption flags, to the scheduler entry points and how sleeping tasks are woken up.
# 1. Linux Scheduling Classes
In Linux, scheduling is determined by the scheduling class to which the process belongs. The sched_class data structure can be found in include/linux/sched.h. All existing scheduling classes in the kernel are arranged in a linked list ordered by priority:

| stop_sched_class → rt_sched_class → fair_sched_class → idle_sched_class → NULL |


Stop and Idle are special scheduling classes:
•  Stop: Used to schedule the per-cpu stop task. It pre-empts everything and can be pre-empted by nothing.
•  Idle: Used to schedule the per-cpu idle task (also called the swapper task), which is run if no other task is runnable.
•  fair_sched_class: kernel\sched\fair.c — implements the CFS (Completely Fair Scheduler) scheduler.
•  rt_sched_class: kernel\sched\rt.c — implements SCHED_FIFO and SCHED_RR semantics.
# 2. Initialisation of task_struct
The scheduling-related elements of the task_struct are shown below. Each process has its own sched_class pointer, sched_entity (for CFS), sched_rt_entity (for RT), and sched_dl_entity (for deadline scheduling).

| struct task_struct { .. int prio, static_prio, normal_prio; unsigned int rt_priority; const struct sched_class *sched_class; struct sched_entity se; struct sched_rt_entity rt;#ifdef CONFIG_CGROUP_SCHED struct task_group *sched_task_group;#endif struct sched_dl_entity dl; ...}; |


The task_struct is initialized with the appropriate sched_class when a process is forked via sched_fork(). The kernel selects the class based on priority:

| /* sched_fork() — assigning sched_class on process fork */} else if (rt_prio(p->prio)) { p->sched_class = &rt_sched_class;} else { p->sched_class = &fair_sched_class;} |


# 3. Scheduler Policies
The POSIX standard specifies three scheduling policies. One is the "usual" or normal policy and is always the default. The other two are (soft) real-time scheduling policies:
○  SCHED_NORMAL (or SCHED_OTHER)  ←  Default scheduling policy
○  SCHED_RR  —  Round-robin real-time scheduling
○  SCHED_FIFO  —  First-in, first-out real-time scheduling
# 4. SCHED_NORMAL / CFS and Process Vruntime
In CFS (Completely Fair Scheduler), the virtual runtime is expressed and tracked via the per-task value p->se.vruntime (in nanosecond units). This allows the kernel to accurately timestamp and measure the "expected CPU time" a task should have gotten.
CFS's task picking logic is based on the p->se.vruntime value. The algorithm is simple: it always tries to run the task with the smallest p->se.vruntime value.
CFS maintains a time-ordered red-black tree (rbtree), where all runnable tasks are sorted by the p->se.vruntime key. This key is updated in the function call chain:

| entity_tick -> update_curr |


# 5. Runqueues
The basic data structure in the scheduler is the runqueue. The runqueue is defined in kernel/sched.c as struct rq. The runqueue holds the list of runnable processes on a given processor. There is one runqueue per processor.
The runqueue struct embeds dedicated sub-runqueue data structures for both fair (CFS) and real-time scheduling classes:

| /* Runqueue sub-structures for scheduling classes (inside struct rq) */struct cfs_rq cfs; /* CFS runqueue for SCHED_NORMAL tasks */struct rt_rq rt; /* RT runqueue for SCHED_FIFO / SCHED_RR tasks */ |


# 6. TIF_NEED_RESCHED
The timer interrupt sets the need_resched flag in the task_struct, indicating that the schedule() function should be called.
## 6.1  How TIF_NEED_RESCHED is Set (Timer Tick Path)
The following call chain shows how the periodic timer tick eventually leads to setting TIF_NEED_RESCHED:

| tick_setup_device tick_setup_periodic tick_set_periodic_handler dev->event_handler = tick_handle_periodictick_handle_periodic(struct clock_event_device *dev) tick_periodic update_process_times(int user_tick) scheduler_tick() |


Inside scheduler_tick(), the task_tick method of the current task's scheduling class is invoked:

| /* scheduler_tick() calls task_tick via the sched_class vtable */..curr->sched_class->task_tick(rq, curr, 0);.. |


For CFS, the task_tick function is task_tick_fair, which calls entity_tick:

| entity_tick(cfs_rq, se, queued); update_curr(cfs_rq); /* Update current task runtime statistics; calls resched_task if preemption needed */ if (queued) { resched_task(rq_of(cfs_rq)->curr); return; } |


The TIF_NEED_RESCHED flag is ultimately set inside resched_task():

| void resched_task(struct task_struct *p){ .. set_tsk_need_resched(p); /* sets TIF_NEED_RESCHED in p->thread_info.flags */ ..} |


## 6.2  hrtimer Path for Updating Process Time
An alternative path for updating process time and setting TIF_NEED_RESCHED uses the high-resolution timer (hrtimer):

| run_timer_softirq hrtimer_run_pending hrtimer_switch_to_hres tick_setup_sched_timer ts->sched_timer.function = tick_sched_timertick_sched_timer tick_sched_handle update_process_times(user_mode(regs)) |


The TIF_NEED_RESCHED flag is checked on interrupt return and userspace return. If the flag is set, the current process is scheduled out and a call to __schedule() is made.
# 7. Scheduler Entry Points
There are three primary entry points through which the scheduler is invoked:
## 7.1  Entry Point 1: TIF_NEED_RESCHED Flag
Based on the TIF_NEED_RESCHED flag, the schedule() function is called from two sub-paths:
A) System Call Return Path (returning to user-space)
Upon returning to user-space (system call return path), if TIF_NEED_RESCHED is set, the kernel invokes the scheduler before continuing. The following snippet is from entry_64.S:

| /* entry_64.S — system call return path */ret_from_sys_call: ..sysret_careful: bt $TIF_NEED_RESCHED, %edx jnc sysret_signal TRACE_IRQS_ON ENABLE_INTERRUPTS(CLBR_NONE) pushq_cfi %rdi SCHEDULE_USER#ifdef CONFIG_CONTEXT_TRACKING# define SCHEDULE_USER call schedule_user#else# define SCHEDULE_USER call schedule#endif |


B) Hardware Interrupt Return Path
Upon returning from a hardware interrupt, the need_resched flag is checked. If it is set and preempt_count is zero (meaning the kernel is in a preemptible region and no locks are held), the kernel invokes the scheduler before continuing.

| /* entry_64.S — interrupt return path */ENTRY(retint_kernel) cmpl $0, PER_CPU_VAR(__preempt_count) jnz retint_restore_args bt $9, EFLAGS-ARGOFFSET(%rsp) /* interrupts off? */ jnc retint_restore_args call preempt_schedule_irq jmp exit_intrpreempt_schedule_irq: void __sched preempt_schedule_irq(void) { ... local_irq_enable(); __schedule(); local_irq_disable(); ... } |


## 7.2  Entry Point 2: Running Task Goes to Sleep
The scheduler is called when the currently running task voluntarily goes to sleep on a wait queue. The canonical pattern used by kernel code is:

| /* 'q' is the wait queue we wish to sleep on */DEFINE_WAIT(wait);add_wait_queue(q, &wait);while (!condition) { /* condition is the event we are waiting for */ prepare_to_wait(&q, &wait, TASK_INTERRUPTIBLE); if (signal_pending(current)) /* handle signal */ schedule(); /* voluntarily yield CPU */}finish_wait(&q, &wait); |


## 7.3  Entry Point 3: Sleeping Task Wakes Up
The code that causes the event a sleeping task is waiting for typically calls wake_up() on the corresponding wait queue. This eventually ends up in the scheduler function try_to_wake_up().
The full call chain from wake_up() to enqueue_task() is:

| try_to_wake_up() ttwu_queue() ttwu_do_activate() ttwu_activate() activate_task() enqueue_task() p->sched_class->enqueue_task(rq, p, flags); |


At the bottom of the call chain, enqueue_task() calls the scheduling class's own enqueue_task method (e.g., enqueue_task_fair for CFS), which inserts the task into the appropriate runqueue (the CFS rbtree or the RT runqueue).
Source: Linux Posts — Linux Scheduling Internals (8 March 2016) by sk

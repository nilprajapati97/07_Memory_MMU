/* ===========================================================================
 * FILE : 01_tcb_scheduler.c
 * DESC : TCB-Based Cooperative Round-Robin Scheduler — System Design
 *
 *  Demonstrates the complete scheduler lifecycle for 2 threads:
 *    ┌─ TCB allocation & initialisation per thread
 *    ├─ TCB LOAD   : restore simulated CPU context → dispatch thread
 *    ├─ TCB UNLOAD : save simulated CPU context   → remove from CPU
 *    ├─ Ready Queue (circular FIFO array, round-robin selection)
 *    ├─ State machine: NEW → READY → RUNNING → BLOCKED → TERMINATED
 *    └─ Cooperative preemption via scheduler_yield() / scheduler_block()
 *
 *  Build : gcc -Wall -Wextra -pthread 01_tcb_scheduler.c -o tcb_sched
 *  Run   : ./tcb_sched
 *
 *  Change MAX_NUMBER to 100 to match 00_multithreading.c behaviour.
 * ===========================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

/* ─────────────────────────────  CONSTANTS  ──────────────────────────────── */

#define MAX_THREADS     8
#define MAX_NUMBER      20       /* print 1 … MAX_NUMBER  (set 100 for full run) */
#define TIME_QUANTUM_US 1000     /* cooperative time quantum in microseconds     */

/* ─────────────────────────────  ENUMS  ──────────────────────────────────── */

typedef enum {
    STATE_NEW        = 0,
    STATE_READY      = 1,
    STATE_RUNNING    = 2,
    STATE_BLOCKED    = 3,
    STATE_TERMINATED = 4
} thread_state_t;

static const char *const STATE_STR[] = {
    "NEW", "READY", "RUNNING", "BLOCKED", "TERMINATED"
};

/* ─────────────────────────────  TCB  ────────────────────────────────────── */
/*
 * Task Control Block — one per thread, the scheduler's entire knowledge
 * about a thread.  In the Linux kernel this is 'struct task_struct'.
 *
 * Layout (conceptual):
 *
 *   ┌─────────────────────────────────────────────────────┐
 *   │  Identity         tid / name / state / pthread_id   │
 *   ├─────────────────────────────────────────────────────┤
 *   │  Context Frame    sim_PC / sim_SP / sim_LR           │  ← saved on
 *   │  (register save)  sim_R0 / sim_R1                   │    UNLOAD
 *   ├─────────────────────────────────────────────────────┤  ← restored on
 *   │  Sched Metadata   priority / time_quantum            │    LOAD
 *   ├─────────────────────────────────────────────────────┤
 *   │  Statistics       saves / loads / yields / blocks   │
 *   └─────────────────────────────────────────────────────┘
 *
 * In a real kernel, cpu_switch_to() saves the physical registers r4-r15,
 * PC, SP, LR into this area via assembly.  Here we capture real host
 * process addresses to show the concept authentically.
 */
typedef struct {

    /* ── Identity ───────────────────────────────────────────────────── */
    int             tid;              /* scheduler-assigned thread ID       */
    char            name[32];         /* human-readable label               */
    thread_state_t  state;            /* current FSM state                  */
    pthread_t       pthread_id;       /* underlying OS thread handle        */
    void           *(*func)(void *);  /* entry function pointer             */
    void           *arg;              /* argument for entry function        */

    /* ── Simulated CPU Register Save Area (Context Frame) ───────────── */
    /*    Populated by tcb_unload(), read by tcb_load().                  */
    /*    In a real OS these come from/go to assembly cpu_switch_to().    */
    uintptr_t       sim_pc;   /* Program Counter  — address to resume at  */
    uintptr_t       sim_sp;   /* Stack Pointer    — top of stack frame    */
    uintptr_t       sim_lr;   /* Link Register    — return address        */
    int             sim_r0;   /* General reg R0   — holds 'counter'       */
    int             sim_r1;   /* General reg R1   — holds 'odd_turn' flag */

    /* ── Scheduling Metadata ────────────────────────────────────────── */
    int             priority;         /* 1 (low) – 10 (high)              */
    int             time_quantum_us;  /* allotted time slice (µs)         */

    /* ── Statistics ─────────────────────────────────────────────────── */
    int             n_context_saves;   /* times TCB was unloaded           */
    int             n_context_loads;   /* times TCB was loaded             */
    int             n_yields;          /* times thread called yield()      */
    int             n_blocks;          /* times thread called block()      */
    long            total_cpu_time_us; /* cumulative CPU time (µs)         */
    struct timeval  last_scheduled;    /* timestamp of last TCB load       */

} TCB_t;

/* ─────────────────────────────  SCHEDULER  ──────────────────────────────── */
/*
 * The Scheduler — owns the ready queue and orchestrates every
 * context switch.  Runs in the main() thread as a dispatch loop.
 *
 *   tcb_pool[]          — all registered TCBs
 *   ready_queue[]       — circular FIFO of TIDs eligible to run
 *   current_tid         — TID presently on the CPU
 *   sched_lock          — single global lock guarding all state
 *   sched_cond[tid]     — per-thread condvar: wakes exactly one thread
 *   sched_wake          — wakes the scheduler loop (yield/block/terminate)
 */
typedef struct {

    TCB_t          *tcb_pool[MAX_THREADS];
    int             n_threads;

    /* Ready Queue — circular array, round-robin */
    int             ready_queue[MAX_THREADS];
    int             rq_head;
    int             rq_tail;
    int             rq_size;

    /* Dispatcher state */
    int             current_tid;       /* TID on CPU; -1 = idle             */

    /* Synchronisation */
    pthread_mutex_t sched_lock;
    pthread_cond_t  sched_cond[MAX_THREADS]; /* per-thread wake             */
    pthread_cond_t  sched_wake;              /* scheduler loop wake         */

    /* Statistics */
    int             total_context_switches;
    int             scheduler_cycles;
    struct timeval  start_time;

} Scheduler_t;

/* ─────────────────────────────  GLOBAL STATE  ───────────────────────────── */

static Scheduler_t   g_sched;
static volatile int  g_counter  = 1;
static volatile bool g_odd_turn = true;
static int           g_odd_tid  = -1;
static int           g_even_tid = -1;

/* ─────────────────────────────  DISPLAY HELPERS  ────────────────────────── */

#define SEP  "  ─────────────────────────────────────────────────────────────"
#define DSEP "═══════════════════════════════════════════════════════════════"

static void print_sep  (void) { puts(SEP);  }
static void print_dsep (void) { puts(DSEP); }

/* ─────────────────────────────  TIMING  ─────────────────────────────────── */

static long elapsed_us(const struct timeval *a, const struct timeval *b)
{
    return (b->tv_sec  - a->tv_sec)  * 1000000L
         + (b->tv_usec - a->tv_usec);
}

/* ─────────────────────────────  SIMULATED REGISTER CAPTURE  ─────────────── */
/*
 * Capture real host-process addresses to populate the TCB context frame.
 *
 *   sim_pc  : __builtin_return_address(0) — address of the instruction
 *             that called tcb_unload(); in a real kernel this would be the
 *             saved PC of the preempted thread (set by cpu_switch_to asm).
 *
 *   sim_sp  : address of a local variable — approximates the stack top
 *             of the calling frame at save time.
 *
 * These values are real, visible in /proc/<pid>/maps, and illustrate the
 * concept accurately even though they come from the simulation frame rather
 * than the thread's "logical" stack.
 */
static uintptr_t capture_pc(void)
{
    return (uintptr_t)__builtin_return_address(0);
}

static uintptr_t capture_sp(void)
{
    volatile uintptr_t local = 0;
    return (uintptr_t)&local;
}

/* ─────────────────────────────  READY QUEUE  ────────────────────────────── */

static void rq_enqueue(Scheduler_t *s, int tid)
{
    s->ready_queue[s->rq_tail] = tid;
    s->rq_tail = (s->rq_tail + 1) % MAX_THREADS;
    s->rq_size++;
}

static int rq_dequeue(Scheduler_t *s)
{
    if (s->rq_size == 0) return -1;
    int tid    = s->ready_queue[s->rq_head];
    s->rq_head = (s->rq_head + 1) % MAX_THREADS;
    s->rq_size--;
    return tid;
}

static void rq_print(const Scheduler_t *s)
{
    printf("  [READY QUEUE] size=%d [", s->rq_size);
    for (int i = 0; i < s->rq_size; i++) {
        int idx = (s->rq_head + i) % MAX_THREADS;
        printf(" TID%d:%s", s->ready_queue[idx],
               s->tcb_pool[s->ready_queue[idx]]->name);
    }
    printf(" ]\n");
}

/* ===========================================================================
 * TCB UNLOAD  —  Save context, remove thread from CPU
 * ===========================================================================
 *
 * Called when a thread yields, blocks, or terminates.
 *
 * Real kernel analogue  →  cpu_switch_to() stores callee-saved registers
 *                           (r4–r15 / rbx–r15) plus PC and SP into the TCB
 *                           thread_struct via assembly.
 *
 * What this function does:
 *   1. Capture sim_PC / sim_SP / sim_LR into the TCB context frame
 *   2. Snapshot 'counter' and 'odd_turn' into sim_R0 / sim_R1
 *   3. Record CPU time consumed this time slice
 *   4. Transition state: RUNNING → {READY | BLOCKED | TERMINATED}
 *   5. Increment n_context_saves counter
 *   6. Print the full TCB register frame so we can see every field
 *
 * ===========================================================================
 */
static void tcb_unload(Scheduler_t *s, TCB_t *tcb, thread_state_t new_state)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    long slice_us = elapsed_us(&tcb->last_scheduled, &now);
    tcb->total_cpu_time_us += slice_us;

    /* Capture simulated CPU register state into TCB context frame */
    tcb->sim_pc = capture_pc();
    tcb->sim_sp = capture_sp();
    tcb->sim_lr = tcb->sim_pc - 8;   /* LR: 2 instructions before PC       */
    tcb->sim_r0 = g_counter;         /* R0: current counter value          */
    tcb->sim_r1 = (int)g_odd_turn;   /* R1: current odd_turn flag          */

    thread_state_t prev = tcb->state;
    tcb->state = new_state;
    tcb->n_context_saves++;

    print_sep();
    printf("  ╔══ TCB UNLOAD : %-12s ══════════════════════════════════╗\n",
           tcb->name);
    printf("  ║  TID        = %-2d     State : %-9s ──► %-9s      ║\n",
           tcb->tid, STATE_STR[prev], STATE_STR[new_state]);
    printf("  ║  sim_PC     = 0x%016" PRIxPTR "   (resume address)    ║\n",
           tcb->sim_pc);
    printf("  ║  sim_SP     = 0x%016" PRIxPTR "   (stack top)         ║\n",
           tcb->sim_sp);
    printf("  ║  sim_LR     = 0x%016" PRIxPTR "   (return address)    ║\n",
           tcb->sim_lr);
    printf("  ║  sim_R0     = %-6d     (counter at save)                ║\n",
           tcb->sim_r0);
    printf("  ║  sim_R1     = %-6d     (odd_turn at save)               ║\n",
           tcb->sim_r1);
    printf("  ║  CPU slice  = %-6ld µs  Saves=%-3d  Loads=%-3d  Yields=%-3d ║\n",
           slice_us, tcb->n_context_saves, tcb->n_context_loads, tcb->n_yields);
    printf("  ╚════════════════════════════════════════════════════════════╝\n");
    print_sep();

    (void)s;
}

/* ===========================================================================
 * TCB LOAD  —  Restore context, put thread on CPU
 * ===========================================================================
 *
 * Called by the scheduler dispatcher when it selects the next thread.
 *
 * Real kernel analogue  →  cpu_switch_to() reads back callee-saved
 *                           registers from the new thread's TCB via assembly,
 *                           restoring PC, SP and all general registers.
 *
 * What this function does:
 *   1. Print the TCB context frame (what we are "restoring" into the CPU)
 *   2. Transition state: READY/BLOCKED → RUNNING
 *   3. Record timestamp (for CPU time accounting)
 *   4. Increment n_context_loads counter
 *   5. Signal sched_cond[tid] → hands execution to the thread
 *
 * ===========================================================================
 */
static void tcb_load(Scheduler_t *s, TCB_t *tcb)
{
    gettimeofday(&tcb->last_scheduled, NULL);
    thread_state_t prev = tcb->state;
    tcb->state = STATE_RUNNING;
    tcb->n_context_loads++;
    s->current_tid = tcb->tid;

    print_sep();
    printf("  ╔══ TCB LOAD   : %-12s ══════════════════════════════════╗\n",
           tcb->name);
    printf("  ║  TID        = %-2d     State : %-9s ──► RUNNING         ║\n",
           tcb->tid, STATE_STR[prev]);
    printf("  ║  sim_PC     = 0x%016" PRIxPTR "   (resume here)       ║\n",
           tcb->sim_pc);
    printf("  ║  sim_SP     = 0x%016" PRIxPTR "   (stack restored)    ║\n",
           tcb->sim_sp);
    printf("  ║  sim_LR     = 0x%016" PRIxPTR "   (return address)    ║\n",
           tcb->sim_lr);
    printf("  ║  sim_R0     = %-6d     (counter at last save)           ║\n",
           tcb->sim_r0);
    printf("  ║  sim_R1     = %-6d     (odd_turn at last save)          ║\n",
           tcb->sim_r1);
    printf("  ║  Priority   = %-3d     Quantum = %-6d µs                ║\n",
           tcb->priority, tcb->time_quantum_us);
    printf("  ║  Loads=%-3d  Saves=%-3d  CPU total = %-6ld µs            ║\n",
           tcb->n_context_loads, tcb->n_context_saves, tcb->total_cpu_time_us);
    printf("  ╚════════════════════════════════════════════════════════════╝\n");
    print_sep();

    /* Signal this thread's dedicated condvar → it was blocked waiting for   */
    /* the scheduler to dispatch it (in yield, block, or the entry wrapper). */
    pthread_cond_signal(&s->sched_cond[tcb->tid]);
}

/* ===========================================================================
 * SCHEDULER INIT
 * ===========================================================================
 */
void scheduler_init(Scheduler_t *s)
{
    memset(s, 0, sizeof(*s));
    s->current_tid = -1;
    pthread_mutex_init(&s->sched_lock, NULL);
    pthread_cond_init(&s->sched_wake, NULL);
    for (int i = 0; i < MAX_THREADS; i++)
        pthread_cond_init(&s->sched_cond[i], NULL);
    gettimeofday(&s->start_time, NULL);

    print_dsep();
    printf("  [SCHEDULER INIT]\n");
    printf("  Algorithm   : Cooperative Round-Robin\n");
    printf("  Max Threads : %d\n", MAX_THREADS);
    printf("  Quantum     : %d µs\n", TIME_QUANTUM_US);
    printf("  Queue       : Circular FIFO array  (head → dequeue, tail → enqueue)\n");
    printf("  Context     : sim_PC / sim_SP / sim_LR / sim_R0 / sim_R1\n");
    print_dsep();
}

/* ─────────────────────────────  THREAD ENTRY WRAPPER  ───────────────────── */
/*
 * Every registered thread starts here.  It immediately parks on its
 * per-thread condvar (sched_cond[tid]) and waits until the scheduler
 * first dispatches it via tcb_load(), which signals that condvar.
 * This mimics how a newly created task in the kernel sits on the run queue
 * waiting for the scheduler to pick it.
 */
typedef struct { Scheduler_t *sched; int tid; } ThreadArg_t;

static void *thread_entry_wrapper(void *raw)
{
    ThreadArg_t *ta  = (ThreadArg_t *)raw;
    Scheduler_t *s   = ta->sched;
    int          tid = ta->tid;
    free(ta);

    pthread_mutex_lock(&s->sched_lock);
    TCB_t *tcb = s->tcb_pool[tid];

    printf("  [THREAD BORN]  TID=%-2d %-12s  pthread=0x%lx  State: NEW → READY\n",
           tid, tcb->name, (unsigned long)tcb->pthread_id);

    /* Wait until scheduler dispatches us (tcb_load signals sched_cond[tid]) */
    while (tcb->state != STATE_RUNNING)
        pthread_cond_wait(&s->sched_cond[tid], &s->sched_lock);

    pthread_mutex_unlock(&s->sched_lock);

    /* Execute actual thread logic */
    tcb->func(tcb->arg);
    return NULL;
}

/* ===========================================================================
 * SCHEDULER REGISTER THREAD
 * ===========================================================================
 * Steps:
 *   1. Allocate and initialise a TCB
 *   2. Spawn underlying pthread (it will park on sched_cond[tid])
 *   3. Set state NEW → READY, enqueue TID in ready queue
 */
int scheduler_register_thread(Scheduler_t *s,
                               const char   *name,
                               void *(*func)(void *),
                               void         *arg,
                               int           priority)
{
    if (s->n_threads >= MAX_THREADS) {
        fprintf(stderr, "[SCHEDULER] ERROR: max threads reached\n");
        return -1;
    }

    /* ── Allocate TCB ──────────────────────────────────────────────── */
    TCB_t *tcb = (TCB_t *)calloc(1, sizeof(TCB_t));
    if (!tcb) { perror("calloc TCB"); return -1; }

    int tid          = s->n_threads++;
    tcb->tid         = tid;
    strncpy(tcb->name, name, sizeof(tcb->name) - 1);
    tcb->state       = STATE_NEW;
    tcb->func        = func;
    tcb->arg         = arg;
    tcb->priority    = priority;
    tcb->time_quantum_us = TIME_QUANTUM_US;
    /* Context frame is all-zero on first load (thread hasn't run yet) */
    gettimeofday(&tcb->last_scheduled, NULL);
    s->tcb_pool[tid] = tcb;

    printf("\n  [REGISTER]  TID=%d  %-12s  Priority=%d  Quantum=%d µs\n",
           tid, name, priority, TIME_QUANTUM_US);
    printf("              TCB allocated at %p  (size=%zu bytes)\n",
           (void *)tcb, sizeof(TCB_t));

    /* ── Spawn pthread ─────────────────────────────────────────────── */
    ThreadArg_t *ta = (ThreadArg_t *)malloc(sizeof(ThreadArg_t));
    if (!ta) { free(tcb); perror("malloc ThreadArg"); return -1; }
    ta->sched = s;
    ta->tid   = tid;
    pthread_create(&tcb->pthread_id, NULL, thread_entry_wrapper, ta);

    /* Give new thread time to reach its condvar wait before we proceed */
    usleep(2000);

    /* ── NEW → READY, enqueue ──────────────────────────────────────── */
    pthread_mutex_lock(&s->sched_lock);
    tcb->state = STATE_READY;
    rq_enqueue(s, tid);
    printf("              State: NEW → READY  |  pthread=0x%lx\n",
           (unsigned long)tcb->pthread_id);
    rq_print(s);
    pthread_mutex_unlock(&s->sched_lock);

    return tid;
}

/* ===========================================================================
 * SCHEDULER YIELD
 * ===========================================================================
 * Called by a running thread to cooperatively surrender the CPU.
 *
 * Flow:
 *   thread calls yield()
 *     │
 *     ├─ acquire sched_lock
 *     ├─ tcb_unload(RUNNING → READY)    ← saves sim_PC/SP/LR/R0/R1
 *     ├─ enqueue self in ready_queue    ← back to round-robin rotation
 *     ├─ signal sched_wake              ← wake scheduler loop
 *     ├─ pthread_cond_wait(sched_cond)  ← release lock, park here
 *     │
 *     └─ (scheduler eventually calls tcb_load → signals sched_cond)
 *        thread wakes, re-acquires lock, exits wait, returns from yield()
 */
void scheduler_yield(Scheduler_t *s)
{
    pthread_mutex_lock(&s->sched_lock);
    int    tid = s->current_tid;
    TCB_t *tcb = s->tcb_pool[tid];
    tcb->n_yields++;

    printf("\n  [YIELD]  %-12s  giving up CPU voluntarily  (yield #%d)\n",
           tcb->name, tcb->n_yields);

    tcb_unload(s, tcb, STATE_READY);
    rq_enqueue(s, tid);
    rq_print(s);

    pthread_cond_signal(&s->sched_wake);
    while (tcb->state != STATE_RUNNING)
        pthread_cond_wait(&s->sched_cond[tid], &s->sched_lock);

    pthread_mutex_unlock(&s->sched_lock);
}

/* ===========================================================================
 * SCHEDULER BLOCK
 * ===========================================================================
 * Called when a thread cannot proceed (condition not satisfied).
 * Unlike yield(), the thread is NOT re-enqueued — it becomes invisible
 * to the scheduler until another thread calls scheduler_unblock(tid).
 *
 * Flow:
 *   thread calls block()
 *     │
 *     ├─ acquire sched_lock
 *     ├─ tcb_unload(RUNNING → BLOCKED)  ← saves context
 *     │  (NOT enqueued — scheduler cannot pick this thread)
 *     ├─ signal sched_wake              ← wake scheduler
 *     ├─ pthread_cond_wait(sched_cond)  ← park here
 *     │
 *     └─ (some thread calls scheduler_unblock(tid))
 *        (scheduler then calls tcb_load → signals sched_cond)
 *        thread wakes, re-acquires lock, returns from block()
 */
void scheduler_block(Scheduler_t *s)
{
    pthread_mutex_lock(&s->sched_lock);
    int    tid = s->current_tid;
    TCB_t *tcb = s->tcb_pool[tid];
    tcb->n_blocks++;

    printf("\n  [BLOCK]  %-12s  condition not met, going BLOCKED  (block #%d)\n",
           tcb->name, tcb->n_blocks);

    tcb_unload(s, tcb, STATE_BLOCKED);
    /* NOT re-enqueued: invisible to scheduler until unblocked */
    rq_print(s);

    pthread_cond_signal(&s->sched_wake);
    while (tcb->state != STATE_RUNNING)
        pthread_cond_wait(&s->sched_cond[tid], &s->sched_lock);

    pthread_mutex_unlock(&s->sched_lock);
}

/* ===========================================================================
 * SCHEDULER UNBLOCK
 * ===========================================================================
 * Move a BLOCKED thread to READY and re-enqueue it.
 * MUST be called with sched_lock HELD by the caller.
 */
void scheduler_unblock(Scheduler_t *s, int target_tid)
{
    /* Caller already holds sched_lock */
    TCB_t *tcb = s->tcb_pool[target_tid];
    if (tcb->state != STATE_BLOCKED) return;

    tcb->state = STATE_READY;
    rq_enqueue(s, target_tid);
    printf("  [UNBLOCK] %-12s  BLOCKED → READY  re-enqueued\n", tcb->name);
    rq_print(s);
}

/* ===========================================================================
 * SCHEDULER TERMINATE
 * ===========================================================================
 * Called by a thread that has finished all work.
 *
 * Unblocks any other BLOCKED threads first so they can observe
 * counter > MAX_NUMBER and also exit cleanly.
 * Then marks self TERMINATED and signals the scheduler loop.
 */
void scheduler_terminate(Scheduler_t *s)
{
    pthread_mutex_lock(&s->sched_lock);
    int    tid = s->current_tid;
    TCB_t *tcb = s->tcb_pool[tid];

    /* Wake any blocked peer so it can detect termination condition */
    for (int i = 0; i < s->n_threads; i++) {
        if (i != tid && s->tcb_pool[i]->state == STATE_BLOCKED)
            scheduler_unblock(s, i);
    }

    tcb_unload(s, tcb, STATE_TERMINATED);

    printf("  [TERMINATE] %-12s  Final stats:\n", tcb->name);
    printf("    Saves=%-3d  Loads=%-3d  Yields=%-3d  Blocks=%-3d  CPU=%ld µs\n",
           tcb->n_context_saves, tcb->n_context_loads,
           tcb->n_yields, tcb->n_blocks, tcb->total_cpu_time_us);

    pthread_cond_signal(&s->sched_wake);
    pthread_mutex_unlock(&s->sched_lock);
    /* Thread function returns after this — pthread exits naturally */
}

/* ===========================================================================
 * SCHEDULER RUN  —  main dispatch loop
 * ===========================================================================
 *
 * Runs in the main() thread.  Embodies the scheduler itself.
 *
 *   ┌─ loop ─────────────────────────────────────────────────────────────┐
 *   │  1. Check: all threads TERMINATED?  → break                        │
 *   │  2. rq_dequeue() — pick next READY thread (round-robin FIFO)       │
 *   │     If queue empty (all alive threads BLOCKED) → idle-wait         │
 *   │  3. tcb_load(next)  — restore context frame, wake thread via       │
 *   │                        pthread_cond_signal(sched_cond[tid])        │
 *   │  4. pthread_cond_wait(sched_wake)  — release lock, park here       │
 *   │     (thread runs until it calls yield / block / terminate,         │
 *   │      which signals sched_wake and wakes us)                        │
 *   │  5. total_context_switches++  → repeat                             │
 *   └─────────────────────────────────────────────────────────────────── ┘
 *
 * ===========================================================================
 */
void scheduler_run(Scheduler_t *s)
{
    print_dsep();
    printf("  [SCHEDULER RUN]  Dispatch loop started\n");
    print_dsep();

    while (1) {
        pthread_mutex_lock(&s->sched_lock);

        /* ── Termination check ─────────────────────────────────────── */
        int alive = 0;
        for (int i = 0; i < s->n_threads; i++)
            if (s->tcb_pool[i]->state != STATE_TERMINATED) alive++;

        if (alive == 0) {
            pthread_mutex_unlock(&s->sched_lock);
            break;
        }

        /* ── Pick next READY thread (round-robin) ──────────────────── */
        int next_tid = rq_dequeue(s);

        if (next_tid < 0) {
            /* All live threads are BLOCKED — idle-wait */
            printf("  [SCHEDULER] IDLE — no READY thread, waiting\n");
            pthread_cond_wait(&s->sched_wake, &s->sched_lock);
            pthread_mutex_unlock(&s->sched_lock);
            continue;
        }

        s->scheduler_cycles++;
        TCB_t *next = s->tcb_pool[next_tid];

        print_dsep();
        printf("  [SCHEDULER CYCLE #%-3d]  Dispatching TID=%d  %s\n",
               s->scheduler_cycles, next_tid, next->name);
        print_dsep();

        /* ── TCB LOAD: restore context frame, hand CPU to thread ───── */
        tcb_load(s, next);

        /* ── Wait for thread to yield / block / terminate ──────────── */
        /*    Thread signals sched_wake when it gives up the CPU.        */
        pthread_cond_wait(&s->sched_wake, &s->sched_lock);

        s->total_context_switches++;
        pthread_mutex_unlock(&s->sched_lock);
    }
}

/* ─────────────────────────────  FINAL STATISTICS  ───────────────────────── */

static void print_final_stats(Scheduler_t *s)
{
    struct timeval end;
    gettimeofday(&end, NULL);
    long wall_us = elapsed_us(&s->start_time, &end);

    print_dsep();
    printf("  [FINAL STATS]\n");
    print_dsep();
    printf("  Scheduler cycles       : %d\n",   s->scheduler_cycles);
    printf("  Total context switches : %d\n",   s->total_context_switches);
    printf("  Wall-clock time        : %ld µs\n\n", wall_us);

    for (int i = 0; i < s->n_threads; i++) {
        TCB_t *t = s->tcb_pool[i];
        printf("  Thread %-12s  TID=%d\n", t->name, t->tid);
        printf("    Final state      : %s\n",  STATE_STR[t->state]);
        printf("    Context saves    : %d  (TCB UNLOAD count)\n", t->n_context_saves);
        printf("    Context loads    : %d  (TCB LOAD count)\n",   t->n_context_loads);
        printf("    Yields           : %d\n",  t->n_yields);
        printf("    Blocks           : %d\n",  t->n_blocks);
        printf("    CPU time         : %ld µs\n\n", t->total_cpu_time_us);
    }
    print_dsep();
}

/* ===========================================================================
 * THREAD FUNCTIONS
 * ===========================================================================
 *
 * Both threads share a single sched_lock that protects g_counter /
 * g_odd_turn.  The blocking/unblocking mechanism ensures that even if the
 * round-robin scheduler dispatches the "wrong" thread (one whose turn it
 * isn't), that thread will block itself and the scheduler will pick the
 * correct one.
 *
 * Per-iteration flow (ODD as example):
 *
 *   acquire sched_lock
 *   ├─ counter > MAX?  → release lock, break, scheduler_terminate()
 *   ├─ not my turn?    → release lock, scheduler_block()  [BLOCKED, not enqueued]
 *   │                     continue  (re-enter loop after being unblocked + dispatched)
 *   └─ my turn:
 *        print number
 *        counter++, odd_turn = false
 *        unblock EVEN if it was BLOCKED
 *        release sched_lock
 *        scheduler_yield()   ← cooperative: give EVEN a chance to run
 *
 * ===========================================================================
 */
void *odd_thread_func(void *arg)
{
    Scheduler_t *s = (Scheduler_t *)arg;

    while (1) {
        pthread_mutex_lock(&s->sched_lock);

        /* ── Termination check (always first) ──────────────────────── */
        if (g_counter > MAX_NUMBER) {
            pthread_mutex_unlock(&s->sched_lock);
            break;
        }

        /* ── Turn check ─────────────────────────────────────────────── */
        if (!g_odd_turn) {
            printf("  [ODD]  counter=%-3d  not my turn → BLOCKING\n", g_counter);
            pthread_mutex_unlock(&s->sched_lock);
            scheduler_block(s);   /* saves TCB, parks; re-dispatched later */
            continue;             /* re-check counter & turn on next run   */
        }

        /* ── Critical section: print odd number ─────────────────────── */
        printf("\n  >>>  [ODD_THREAD ] prints: %3d\n", g_counter);
        g_counter++;
        g_odd_turn = false;

        /* Unblock EVEN if it was blocked waiting for its turn */
        if (g_even_tid >= 0 &&
            s->tcb_pool[g_even_tid]->state == STATE_BLOCKED)
            scheduler_unblock(s, g_even_tid);

        pthread_mutex_unlock(&s->sched_lock);

        /* Cooperative yield: let EVEN run */
        scheduler_yield(s);
    }

    scheduler_terminate(s);
    return NULL;
}

void *even_thread_func(void *arg)
{
    Scheduler_t *s = (Scheduler_t *)arg;

    while (1) {
        pthread_mutex_lock(&s->sched_lock);

        /* ── Termination check (always first) ──────────────────────── */
        if (g_counter > MAX_NUMBER) {
            pthread_mutex_unlock(&s->sched_lock);
            break;
        }

        /* ── Turn check ─────────────────────────────────────────────── */
        if (g_odd_turn) {
            printf("  [EVEN] counter=%-3d  not my turn → BLOCKING\n", g_counter);
            pthread_mutex_unlock(&s->sched_lock);
            scheduler_block(s);
            continue;
        }

        /* ── Critical section: print even number ────────────────────── */
        printf("\n  >>>  [EVEN_THREAD] prints: %3d\n", g_counter);
        g_counter++;
        g_odd_turn = true;

        /* Unblock ODD if it was blocked waiting for its turn */
        if (g_odd_tid >= 0 &&
            s->tcb_pool[g_odd_tid]->state == STATE_BLOCKED)
            scheduler_unblock(s, g_odd_tid);

        pthread_mutex_unlock(&s->sched_lock);

        /* Cooperative yield: let ODD run */
        scheduler_yield(s);
    }

    scheduler_terminate(s);
    return NULL;
}

/* ===========================================================================
 * MAIN
 * ===========================================================================
 */
int main(void)
{
    print_dsep();
    printf("  TCB-Based Cooperative Scheduler — System Design Implementation\n");
    printf("  Task : Print 1 … %d alternating ODD / EVEN threads\n", MAX_NUMBER);
    print_dsep();

    /* ── Step 1: Initialise scheduler ──────────────────────────────── */
    scheduler_init(&g_sched);

    /* ── Step 2: Register threads ───────────────────────────────────── */
    /*    Each call: allocates TCB, spawns pthread, enqueues TID          */
    printf("\n  [MAIN]  Registering threads...\n");
    g_odd_tid  = scheduler_register_thread(&g_sched, "ODD_THREAD",
                                           odd_thread_func,  &g_sched, 5);
    g_even_tid = scheduler_register_thread(&g_sched, "EVEN_THREAD",
                                           even_thread_func, &g_sched, 5);

    printf("\n  [MAIN]  Both threads registered — handing control to scheduler\n\n");

    /* ── Step 3: Run dispatch loop ──────────────────────────────────── */
    /*    Blocks here until all threads are TERMINATED                    */
    scheduler_run(&g_sched);

    /* ── Step 4: Join underlying pthreads ───────────────────────────── */
    for (int i = 0; i < g_sched.n_threads; i++)
        pthread_join(g_sched.tcb_pool[i]->pthread_id, NULL);

    /* ── Step 5: Final statistics ───────────────────────────────────── */
    print_final_stats(&g_sched);

    /* ── Step 6: Cleanup ────────────────────────────────────────────── */
    for (int i = 0; i < g_sched.n_threads; i++)
        free(g_sched.tcb_pool[i]);
    pthread_mutex_destroy(&g_sched.sched_lock);
    pthread_cond_destroy(&g_sched.sched_wake);
    for (int i = 0; i < MAX_THREADS; i++)
        pthread_cond_destroy(&g_sched.sched_cond[i]);

    return 0;
}

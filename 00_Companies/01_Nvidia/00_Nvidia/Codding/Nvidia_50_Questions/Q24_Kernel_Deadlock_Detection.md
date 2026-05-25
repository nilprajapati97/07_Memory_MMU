# Q24: Kernel Deadlock Detection and Prevention

**Section:** Concurrency & Synchronization | **Difficulty:** Hard | **Topics:** deadlock, lock ordering, `lockdep`, `mutex_trylock`, `cond_resched`, ABBA deadlock, lock dependency graph

---

## Question

Describe deadlock detection in the Linux kernel and implement a fix for a classic ABBA deadlock.

---

## Answer

```c
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/lockdep.h>

/* ─── Deadlock scenario: GPU context + GPU memory manager ────────────────
 *
 * Thread A (GPU command submit):    lock(ctx_lock) → lock(mm_lock)
 * Thread B (GPU memory eviction):   lock(mm_lock)  → lock(ctx_lock)
 *
 * If A holds ctx_lock and B holds mm_lock simultaneously:
 *   A waits for mm_lock (held by B)
 *   B waits for ctx_lock (held by A)
 *   → DEADLOCK
 */

static DEFINE_MUTEX(ctx_lock);
static DEFINE_MUTEX(mm_lock);

/* ─── WRONG: opposite lock order in different code paths ─────────────────*/
void gpu_command_submit_BAD(void)
{
    mutex_lock(&ctx_lock);   /* Thread A: lock 1 */
    mutex_lock(&mm_lock);    /* Thread A: lock 2 */
    /* submit command */
    mutex_unlock(&mm_lock);
    mutex_unlock(&ctx_lock);
}

void gpu_evict_memory_BAD(void)
{
    mutex_lock(&mm_lock);    /* Thread B: lock 1 (opposite order!) */
    mutex_lock(&ctx_lock);   /* Thread B: lock 2 → POTENTIAL DEADLOCK */
    /* evict pages */
    mutex_unlock(&ctx_lock);
    mutex_unlock(&mm_lock);
}

/* ─── FIX 1: Consistent lock ordering ────────────────────────────────────
 * Rule: always acquire locks in a globally consistent order.
 * Convention: ctx_lock is ALWAYS acquired before mm_lock.
 */
void gpu_command_submit_FIXED(void)
{
    mutex_lock(&ctx_lock);   /* always first */
    mutex_lock(&mm_lock);    /* always second */
    /* submit command */
    mutex_unlock(&mm_lock);
    mutex_unlock(&ctx_lock);
}

void gpu_evict_memory_FIXED(void)
{
    mutex_lock(&ctx_lock);   /* same order as above: ctx first */
    mutex_lock(&mm_lock);
    /* evict pages */
    mutex_unlock(&mm_lock);
    mutex_unlock(&ctx_lock);
}

/* ─── FIX 2: trylock with backoff (when ordering is impractical) ──────────
 * Use when one path cannot easily reorder — e.g., already holds mm_lock
 * and needs ctx_lock (can't release mm_lock first without logic redesign).
 */
int gpu_trylock_both(void)
{
    int retries = 0;

retry:
    mutex_lock(&mm_lock);

    if (!mutex_trylock(&ctx_lock)) {
        /*
         * Could not acquire ctx_lock — release mm_lock to avoid deadlock.
         * Back off, yield CPU (cond_resched), then retry.
         */
        mutex_unlock(&mm_lock);

        if (++retries > 100) {
            pr_err("GPU: failed to acquire locks after 100 retries\n");
            return -EDEADLK;
        }

        /*
         * cond_resched: yield CPU if another task is runnable.
         * Prevents busy-looping and allows the competing thread to progress,
         * breaking the livelock.
         */
        cond_resched();
        goto retry;
    }

    /* Both locks held */
    /* ... critical section ... */
    mutex_unlock(&ctx_lock);
    mutex_unlock(&mm_lock);
    return 0;
}

/* ─── FIX 3: Lock address ordering for dynamically allocated objects ──────
 * When two locks are of the same class (e.g., two gpu_context mutexes),
 * always lock the one with the lower address first.
 */
struct gpu_context {
    struct mutex lock;
    u64          id;
    /* ... */
};

void gpu_ctx_merge(struct gpu_context *a, struct gpu_context *b)
{
    struct gpu_context *first, *second;

    /* Canonical ordering: lower address → first lock */
    if ((unsigned long)a < (unsigned long)b) {
        first  = a;
        second = b;
    } else {
        first  = b;
        second = a;
    }

    mutex_lock(&first->lock);
    mutex_lock(&second->lock);

    /* merge contexts */

    mutex_unlock(&second->lock);
    mutex_unlock(&first->lock);
}

/* ─── CONFIG_LOCKDEP annotation example ──────────────────────────────────
 * lockdep detects lock ordering violations at runtime during development.
 * Annotate locks with classes to group locks of the same "type".
 */
static struct lock_class_key gpu_ctx_lock_class;
static struct lock_class_key gpu_mm_lock_class;

void gpu_annotate_locks(void)
{
    lockdep_set_class(&ctx_lock, &gpu_ctx_lock_class);
    lockdep_set_class(&mm_lock, &gpu_mm_lock_class);
}

/*
 * With lockdep enabled (CONFIG_LOCKDEP=y):
 * If any code path acquires mm_lock before ctx_lock after we've established
 * ctx_lock → mm_lock ordering, lockdep prints:
 *
 *   WARNING: possible circular locking dependency detected
 *   ... gpu_evict_memory -> mm_lock -> ctx_lock
 *   ... gpu_command_submit -> ctx_lock -> mm_lock
 *   Possible deadlock!
 */
```

---

## Explanation

### Core Concept

A deadlock requires all four **Coffman conditions** simultaneously:
1. **Mutual exclusion**: only one thread can hold a lock at a time
2. **Hold and wait**: a thread holds one lock while waiting for another
3. **No preemption**: locks cannot be forcibly taken from a thread
4. **Circular wait**: thread A waits for B, B waits for A (cycle in wait-for graph)

**Prevention strategies** (break one condition):
- **Consistent lock ordering** → breaks circular wait
- **`trylock` + backoff** → breaks hold-and-wait (release and retry)
- **Single lock redesign** → breaks mutual exclusion need (coarser lock)
- **Lock-free algorithms** → eliminates mutual exclusion entirely

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `mutex_lock(m)` | Acquire mutex (sleeps if unavailable) |
| `mutex_trylock(m)` | Try to acquire without sleeping; returns 1=acquired, 0=busy |
| `mutex_unlock(m)` | Release mutex |
| `cond_resched()` | Yield CPU if scheduler wants to run another task |
| `lockdep_set_class(lock, key)` | Annotate lock with a lockdep class |
| `CONFIG_LOCKDEP` | Enable runtime lock dependency tracking |
| `CONFIG_DEBUG_MUTEXES` | Enable mutex debug info (owner tracking, stack traces) |
| `CONFIG_DEBUG_ATOMIC_SLEEP` | Detect sleeping in atomic context |
| `DEFINE_MUTEX(name)` | Statically define and initialize a mutex |
| `mutex_is_locked(m)` | Non-atomic check if mutex is held (debug only) |

### Trade-offs & Pitfalls

- **Trylock livelock.** If both threads use trylock and both always back off simultaneously, they may livelock — repeatedly acquiring and releasing without making progress. The `cond_resched()` + retry count limit prevents this.
- **Lock ordering documentation.** Consistent ordering only works if ALL developers know the convention. Document lock ordering in a comment at the top of the file or in a `LOCKING` section in the driver documentation.
- **Lockdep performance.** `CONFIG_LOCKDEP` adds significant overhead (tracking all lock acquisitions). Use only in development/test kernels. Production NVIDIA GPU driver builds disable lockdep.

### NVIDIA / GPU Context

Typical NVIDIA GPU driver lock hierarchy (must be acquired in this order):
```
1. gpu_list_lock    (global list of GPU devices)
2. gpu_lock         (per-GPU device lock)
3. ctx_lock         (per-context lock)
4. mm_lock          (GPU memory manager lock)
5. ring_lock        (command ring spinlock)
```

Any code path that needs multiple locks must follow this order. Violations are caught by lockdep during driver testing on debug kernels before release.

---

## Cross Questions & Answers

**CQ1: How does `CONFIG_LOCKDEP` detect deadlocks at runtime?**
> Lockdep builds a directed graph of lock dependencies: if thread A acquires lock B while holding lock A, lockdep adds edge A→B. On every new lock acquisition, it checks whether adding the new edge creates a cycle in the dependency graph (cycle = potential deadlock). If a cycle is detected, lockdep prints a warning with full stack traces of all the code paths forming the cycle — before an actual deadlock occurs, enabling pre-production detection.

**CQ2: What is a priority inversion and how is it different from a deadlock?**
> Priority inversion: a high-priority thread waits for a low-priority thread that holds a mutex. If a medium-priority thread preempts the low-priority thread, the high-priority thread is effectively blocked by a medium-priority one — "inversion" of priority. Unlike deadlock, the system eventually makes progress (the low-priority thread runs eventually). Linux `RT_MUTEX` solves this with priority inheritance: temporarily boosts the lock-holder's priority to the waiter's priority.

**CQ3: What is the difference between a deadlock and a livelock?**
> Deadlock: all threads are blocked, waiting for each other — zero progress. Livelock: threads are running (not blocked) but continuously retry and back off in a pattern that prevents any thread from making forward progress — CPU cycles are consumed but no work is done. The trylock + backoff without randomization can cause livelock. Fix: add random jitter to backoff delays, or use a global arbitration mechanism (e.g., assign lock ownership by thread ID comparison).

**CQ4: Can `mutex_lock_interruptible` help with deadlock recovery?**
> `mutex_lock_interruptible` returns `-EINTR` if a signal arrives while waiting. This allows user-initiated recovery: if a userspace CUDA process sends `SIGKILL`, the waiting kernel thread returns from `mutex_lock_interruptible` with `-EINTR` and can clean up. However, it doesn't detect or prevent deadlock automatically — it only provides an escape hatch via signal delivery. True deadlock detection requires lockdep or a timeout-based watchdog.

**CQ5: How does the GPU driver watchdog detect and recover from a deadlock in production?**
> NVIDIA's GPU driver uses a watchdog workqueue that runs every N seconds. It records the current lock state (which mutexes are held, by which CPU) and the GPU fence seqno progress. If the same lock has been held for > T seconds without the holding thread making progress, the watchdog triggers: (1) logs the stack trace of the stuck thread, (2) if the lock is a GPU ring lock, issues a GPU reset, (3) signals all pending fences with an error, (4) allows CUDA processes to receive `-ENODEV` and gracefully terminate rather than hanging forever.

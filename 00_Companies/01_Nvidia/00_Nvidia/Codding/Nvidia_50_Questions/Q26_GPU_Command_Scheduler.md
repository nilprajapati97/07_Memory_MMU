# Q26: GPU Command Scheduler with Priority Queues

**Section:** System Design | **Difficulty:** Hard | **Topics:** Red-Black Tree, priority scheduling, `rb_node`, GPU context scheduling, runlist, fairness

---

## Question

Design a GPU command scheduler using a priority-ordered red-black tree of GPU contexts.

---

## Answer

```c
#include <linux/rbtree.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/ktime.h>

/* ─── GPU Context (one per CUDA stream group / process) ──────────────────*/
struct gpu_context {
    u32              ctx_id;
    int              priority;        /* higher = runs first (nice-like, inverted) */
    u64              submitted_seqno; /* last submitted command seqno             */
    u64              completed_seqno; /* last GPU-confirmed complete seqno        */
    u64              vruntime;        /* virtual runtime for fair scheduling       */
    ktime_t          last_scheduled;  /* when we last picked this ctx             */
    struct list_head cmd_queue;       /* pending command buffers for this ctx      */
    struct rb_node   node;            /* position in scheduler's RB tree          */
    spinlock_t       lock;            /* protects cmd_queue                        */
};

/* ─── GPU Scheduler ───────────────────────────────────────────────────────*/
struct gpu_scheduler {
    struct rb_root   ctx_tree;   /* RB tree ordered by (priority, vruntime) */
    spinlock_t       lock;       /* protects ctx_tree                        */
    u32              nr_ctxs;    /* number of contexts in tree               */
    u64              min_vruntime; /* minimum vruntime across all contexts   */
};

/* ─── RB Tree insertion: ordered by (-priority, vruntime) ────────────────
 * Higher priority contexts appear at rb_first() (leftmost).
 * Among equal priority, lower vruntime runs first (CFS-like fairness).
 */
static void sched_insert_context(struct gpu_scheduler *sched,
                                   struct gpu_context *ctx)
{
    struct rb_node **new = &sched->ctx_tree.rb_node;
    struct rb_node  *parent = NULL;

    while (*new) {
        struct gpu_context *this =
            rb_entry(*new, struct gpu_context, node);

        parent = *new;

        /* Primary sort: higher priority first (negative for rb_first order) */
        if (ctx->priority > this->priority)
            new = &(*new)->rb_left;
        else if (ctx->priority < this->priority)
            new = &(*new)->rb_right;
        else {
            /* Same priority: lower vruntime first (CFS fairness) */
            if (ctx->vruntime < this->vruntime)
                new = &(*new)->rb_left;
            else
                new = &(*new)->rb_right;
        }
    }

    rb_link_node(&ctx->node, parent, new);
    rb_insert_color(&ctx->node, &sched->ctx_tree);
    sched->nr_ctxs++;
}

/* ─── Pick next context to run ────────────────────────────────────────────
 * Returns highest-priority context with pending work.
 * O(log n) in the worst case, but usually O(1) with cached leftmost node.
 */
static struct gpu_context *sched_pick_next(struct gpu_scheduler *sched)
{
    struct rb_node *node;
    struct gpu_context *ctx;

    spin_lock(&sched->lock);

    node = rb_first(&sched->ctx_tree);
    while (node) {
        ctx = rb_entry(node, struct gpu_context, node);

        spin_lock(&ctx->lock);
        if (!list_empty(&ctx->cmd_queue)) {
            /* This context has work — pick it */
            /* Remove from tree before updating vruntime (key change) */
            rb_erase(&ctx->node, &sched->ctx_tree);
            sched->nr_ctxs--;
            spin_unlock(&ctx->lock);
            spin_unlock(&sched->lock);
            return ctx;
        }
        spin_unlock(&ctx->lock);
        node = rb_next(node);
    }

    spin_unlock(&sched->lock);
    return NULL; /* no runnable context */
}

/* ─── Update vruntime after GPU execution and re-insert ──────────────────
 * Called after a context's commands are submitted to GPU hardware.
 * vruntime increases proportionally to time used — prevents starvation.
 */
static void sched_context_ran(struct gpu_scheduler *sched,
                                struct gpu_context *ctx,
                                u64 exec_time_ns)
{
    unsigned long flags;

    /* Scale exec_time by priority weight (higher priority = slower vruntime growth) */
    u64 delta_vruntime = exec_time_ns / max(ctx->priority, 1);

    ctx->vruntime += delta_vruntime;

    /* Normalize: keep vruntime relative to min across all contexts */
    sched->min_vruntime = max(sched->min_vruntime,
                               ctx->vruntime - delta_vruntime);

    /* Re-insert with updated vruntime */
    spin_lock_irqsave(&sched->lock, flags);
    sched_insert_context(sched, ctx);
    spin_unlock_irqrestore(&sched->lock, flags);
}

/* ─── Scheduler tick: called from GPU completion interrupt ────────────────*/
void gpu_sched_tick(struct gpu_scheduler *sched)
{
    struct gpu_context *next = sched_pick_next(sched);
    if (!next)
        return; /* GPU goes idle */

    /* Submit next context's commands to GPU hardware */
    gpu_hw_submit_context(next);

    /* vruntime updated after execution time measured */
}
```

### Scheduler Architecture

```
gpu_scheduler.ctx_tree (Red-Black Tree)
         ├── Priority=10, vruntime=100 ← leftmost = runs first
         ├── Priority=10, vruntime=200
         ├── Priority=5,  vruntime=50
         └── Priority=1,  vruntime=10

pick_next → rb_first() → highest priority, lowest vruntime
Each context has:
  cmd_queue: [buf0] → [buf1] → [buf2] → (list of pending GPU commands)
  submitted_seqno: 42  (last submitted to GPU)
  completed_seqno: 40  (last confirmed done by GPU)
```

---

## Explanation

### Core Concept

The GPU command scheduler mirrors Linux CFS (Completely Fair Scheduler):
- **Red-Black Tree** ordered by priority + virtual runtime
- **vruntime**: fair-share measure; contexts that used more GPU time have higher vruntime and are deprioritized
- **Priority**: static bias; CUDA streams with higher priority (via `cudaStreamCreateWithPriority`) grow vruntime slower
- **`rb_first()` = O(1)**: always returns the most deserving context without a linear scan

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `struct rb_root` | RB tree root (initialize with `RB_ROOT`) |
| `struct rb_node` | Node embedded in data structure |
| `rb_link_node(node, parent, link)` | Link new node into tree position |
| `rb_insert_color(node, root)` | Rebalance tree after insertion |
| `rb_erase(node, root)` | Remove node and rebalance |
| `rb_first(root)` | Leftmost (smallest) node — O(log n) first call, O(1) cached |
| `rb_next(node)` | In-order successor |
| `rb_entry(node, type, member)` | `container_of` for RB nodes |

### Trade-offs & Pitfalls

- **Never modify the key while in the tree.** `vruntime` is the sort key — erase before updating, then re-insert. Modifying in-place corrupts the tree invariants.
- **Lock ordering.** `sched->lock` (tree lock) must always be acquired before `ctx->lock` (per-context lock). Reverse order causes deadlock.
- **Starvation prevention.** Low-priority contexts never starve because their vruntime only grows slowly — they eventually catch up to high-priority contexts. The `min_vruntime` clamp prevents a new context from monopolizing by starting with vruntime = 0.

### NVIDIA / GPU Context

NVIDIA GPU scheduler (GPC Scheduler):
- Maintains per-context runlists (equivalent to `cmd_queue`)
- Priority scheduling via TSG (Time Slice Group) priority in hardware registers
- Software-side scheduler decides which TSG to program into the GPU runlist next
- vruntime-like accounting via GPU hardware cycle counters

---

## Cross Questions & Answers

**CQ1: How does the runlist differ from the cmd_queue in this design?**
> The `cmd_queue` is the software queue of pending GPU command buffers. The **runlist** is what gets programmed into the GPU hardware — a ring buffer of context descriptors that the GPU's hardware scheduler cycles through. The software scheduler picks from `cmd_queue`, packages commands into the runlist, and writes the runlist to GPU MMIO registers. The GPU hardware then executes contexts in runlist order, signaling completion via interrupt.

**CQ2: How would you implement gang scheduling for multi-GPU CUDA kernels?**
> Gang scheduling requires all N GPUs to start executing a kernel simultaneously. Extend the scheduler with a `gang_barrier`: collect contexts on all N GPU schedulers, then atomically release all of them at once using `atomic_cmpxchg` on a bitmask. `if (atomic_or_return(1 << gpu_id, &gang_mask) == ALL_GPUS_MASK)` — the last GPU to arrive triggers `wake_up_all` on all waiting contexts. All GPUs start simultaneously, ensuring synchronized execution across NVLink fabric.

**CQ3: What happens when a GPU context is killed mid-flight while in the scheduler tree?**
> The cleanup path must: (1) acquire `sched->lock`, (2) `rb_erase` the context from the tree, (3) flush pending commands from `cmd_queue` (signal all fences with error), (4) wait for in-flight commands to complete (or GPU reset if context is hung), (5) free the context. If the context is currently executing on GPU hardware, a GPU preemption must be triggered via MMIO (write to GPU PREEMPT register) before the context can be safely removed.

**CQ4: How would you add time-slicing to prevent a single context from monopolizing the GPU?**
> Program a GPU preemption timer: after each context runs for `TIME_SLICE_NS` (e.g., 5ms), the GPU's hardware timer fires an interrupt. In the interrupt handler, save the GPU context state (register file snapshot) and call `gpu_sched_tick` to pick the next context. This requires the GPU to support preemptive context switching (all modern NVIDIA GPUs with compute preemption support this). Time slice duration is a trade-off: shorter = better fairness, longer = lower context-switch overhead.

**CQ5: How does NVIDIA's MIG (Multi-Instance GPU) affect the scheduler design?**
> With MIG, each GPU partition (slice) gets dedicated hardware resources (SMs, DRAM, PCIe bandwidth). Each MIG instance runs its own scheduler independently. The system-level scheduler becomes a multi-level hierarchy: (1) MIG partition assignment — which process gets which partition, (2) per-partition command scheduler — handles contexts within that partition. The per-partition scheduler is identical to the design above; MIG just bounds the hardware resources available to it.

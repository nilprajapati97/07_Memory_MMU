# Q30: Distributed GPU Job Scheduler

**Section:** System Design | **Difficulty:** Hard | **Topics:** Gang scheduling, distributed systems, gRPC, `atomic_cmpxchg`, bitmask, NVSwitch, cluster-wide GPU allocation

---

## Question

Design a distributed GPU job scheduler for a multi-node GPU cluster with gang scheduling support.

---

## Answer

```c
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/completion.h>

#define MAX_GPUS_PER_NODE  8
#define MAX_NODES          64
#define ALL_GPUS_MASK      ((1UL << MAX_GPUS_PER_NODE) - 1)

/* ─── Per-node GPU scheduler state ───────────────────────────────────────*/
struct node_gpu_scheduler {
    atomic_t    free_gpus;      /* bitmask: bit N = GPU N is free         */
    atomic_t    reserved_gpus;  /* bitmask: reserved but not yet active   */
    u32         node_id;
    u32         num_gpus;
    spinlock_t  lock;
};

/* ─── Gang Job: requires N GPUs to start simultaneously ──────────────────*/
struct gang_job {
    u32                  required_gpus;  /* number of GPUs needed (e.g., 8) */
    u32                  node_id;        /* which node to schedule on       */
    u64                  job_id;
    struct completion    all_ready;      /* signaled when all GPUs acquired */
    atomic_t             gpus_reserved;  /* count of GPUs successfully reserved */
    unsigned long        gpu_mask;       /* which GPUs are reserved          */
};

/* ─── Reserve GPUs atomically using bitmask CAS ──────────────────────────
 * Attempt to atomically allocate `n` GPUs from the free bitmask.
 * Uses compare-and-swap loop to handle concurrent reservation attempts.
 * Returns the GPU bitmask allocated, or 0 on failure.
 */
static unsigned long try_reserve_gpus(struct node_gpu_scheduler *ns, int n)
{
    unsigned long free_mask, alloc_mask, old, new_mask;
    int bit, count;

retry:
    free_mask = (unsigned long)atomic_read(&ns->free_gpus);

    /* Find n consecutive or non-consecutive free GPUs */
    alloc_mask = 0;
    count = 0;
    for_each_set_bit(bit, &free_mask, ns->num_gpus) {
        set_bit(bit, &alloc_mask);
        if (++count == n)
            break;
    }

    if (count < n)
        return 0; /* not enough free GPUs */

    /* CAS: atomically clear the selected bits in free_gpus */
    old = (unsigned long)atomic_cmpxchg(&ns->free_gpus,
                                          (int)free_mask,
                                          (int)(free_mask & ~alloc_mask));
    if (old != free_mask)
        goto retry; /* another CPU modified free_gpus — retry */

    return alloc_mask;
}

/* ─── Release GPUs back to free pool ─────────────────────────────────────*/
static void release_gpus(struct node_gpu_scheduler *ns,
                           unsigned long gpu_mask)
{
    /*
     * atomic_or: atomically OR the bits back into free_gpus.
     * No CAS needed — setting bits is idempotent and safe.
     */
    atomic_or((int)gpu_mask, &ns->free_gpus);
}

/* ─── Gang job submission ─────────────────────────────────────────────────
 * All required GPUs must start simultaneously (gang scheduling).
 * If we can't get all GPUs at once, we don't reserve any — no partial holds.
 */
int gang_job_submit(struct node_gpu_scheduler *ns, struct gang_job *job)
{
    unsigned long gpu_mask;

    init_completion(&job->all_ready);
    atomic_set(&job->gpus_reserved, 0);

    /* All-or-nothing: get all required GPUs atomically */
    gpu_mask = try_reserve_gpus(ns, job->required_gpus);
    if (!gpu_mask) {
        pr_debug("Gang job %llu: insufficient GPUs on node %u\n",
                 job->job_id, ns->node_id);
        return -EAGAIN; /* caller should retry or queue the job */
    }

    job->gpu_mask = gpu_mask;
    atomic_set(&job->gpus_reserved, job->required_gpus);

    /* Signal all GPUs to start simultaneously */
    complete_all(&job->all_ready);

    pr_info("Gang job %llu: reserved GPUs 0x%lx on node %u\n",
            job->job_id, gpu_mask, ns->node_id);
    return 0;
}

/* ─── Per-GPU execution thread (one per GPU in the gang) ─────────────────*/
int gpu_gang_worker(void *data)
{
    struct gang_job *job = data;
    int gpu_id = current->pid % job->required_gpus; /* example: assigned GPU */
    long ret;

    /* Wait for all gang members to be ready before starting */
    ret = wait_for_completion_timeout(&job->all_ready,
                                       msecs_to_jiffies(5000));
    if (!ret) {
        pr_err("GPU %d: gang job %llu timeout waiting for all members\n",
               gpu_id, job->job_id);
        return -ETIMEDOUT;
    }

    /* All GPUs start executing simultaneously */
    gpu_execute_kernel(gpu_id, job);

    return 0;
}

/* ─── Cluster-level scheduler (runs on headnode) ─────────────────────────
 * Selects which node to run a job on based on:
 *   - available GPU count
 *   - NVSwitch topology (prefer all-NVSwitch-connected nodes)
 *   - locality (prefer node with least IB hops from storage)
 */
struct cluster_scheduler {
    struct node_gpu_scheduler nodes[MAX_NODES];
    u32                        num_nodes;
    spinlock_t                 lock;
};

int cluster_schedule_gang_job(struct cluster_scheduler *cs,
                                struct gang_job *job)
{
    int i;

    for (i = 0; i < cs->num_nodes; i++) {
        struct node_gpu_scheduler *ns = &cs->nodes[i];
        int free_count = hweight32(atomic_read(&ns->free_gpus));

        if (free_count >= (int)job->required_gpus) {
            job->node_id = ns->node_id;
            return gang_job_submit(ns, job);
        }
    }

    return -ENOSPC; /* no node has enough free GPUs */
}
```

### Cluster Architecture

```
                    ┌─────────────────────────┐
                    │   Job Broker (etcd/k8s)  │
                    │  cluster_scheduler       │
                    └────────┬────────────────┘
                    gRPC     │    gRPC
          ┌─────────┘                └─────────┐
          ▼                                    ▼
  ┌───────────────┐                  ┌───────────────┐
  │  Node 0 Sched │                  │  Node 1 Sched │
  │  free_gpus=FF │                  │  free_gpus=FF │
  └──┬──┬──┬──┬──┘                  └──┬──┬──┬──┬──┘
     │  │  │  │                        │  │  │  │
    G0 G1 G2 G3                       G4 G5 G6 G7
     └──┴──┴──┴──── NVSwitch ──────────┴──┴──┴──┘
```

---

## Explanation

### Core Concept

**Gang scheduling**: all N GPUs in a job start at exactly the same time. This is required for:
- **Collective operations** (AllReduce, AllGather): all GPUs must be ready to send/receive simultaneously
- **NVLink synchronization**: NVLink barriers stall waiting for all peers — if one GPU hasn't started, all others wait forever

**CAS bitmask allocation** ensures atomicity: either all N GPUs are reserved or none — prevents partial allocation (which would stall other jobs waiting for those GPUs).

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `atomic_cmpxchg(v, old, new)` | Atomically swap bitmask (all-or-nothing reservation) |
| `atomic_or(bits, v)` | Release bits back to free pool |
| `atomic_read(v)` | Read current bitmask |
| `for_each_set_bit(bit, addr, size)` | Iterate set bits in bitmask |
| `hweight32(mask)` | Count set bits (number of free GPUs) |
| `complete_all(comp)` | Wake all gang members simultaneously |
| `wait_for_completion_timeout(comp, jiffies)` | Per-GPU wait for gang start |

### Trade-offs & Pitfalls

- **Bitmask CAS retry storm.** If many jobs compete for GPUs simultaneously, the CAS retry loop becomes a spinlock equivalent. For production: use a single scheduler lock per node with an event queue — more predictable under high contention.
- **Deadlock with partial multi-node gangs.** If a job needs 4 GPUs from node 0 and 4 from node 1, reserving node 0 first then node 1 can deadlock with another job doing the opposite. Use consistent ordering (always reserve by ascending node ID) or a global transaction manager.
- **Stale bitmask read.** `atomic_read` is relaxed. Another CPU may have changed `free_gpus` between `atomic_read` and `atomic_cmpxchg`. The CAS handles this correctly — if `old != free_mask`, we retry with the freshest value.

### NVIDIA / GPU Context

NVIDIA DGX/SuperPOD GPU scheduling:
- **Slurm + DCGM** manages cluster-level GPU allocation using similar bitmask accounting
- **NCCL (NVIDIA Collective Communications Library)** requires gang scheduling — `ncclCommInitAll` must be called on all ranks simultaneously
- **NVSwitch** provides non-blocking all-to-all bandwidth between 8/16 GPUs on a node; cross-node uses InfiniBand

---

## Cross Questions & Answers

**CQ1: How does NCCL use gang scheduling for AllReduce?**
> NCCL `AllReduce` requires all N GPUs (ranks) to participate simultaneously. The NCCL communicator initialization (`ncclCommInitRank`) is a barrier — all ranks must call it before any proceeds. The cluster scheduler ensures all ranks are assigned GPUs at the same time (gang scheduling). If rank 2 is scheduled 10 seconds after rank 0, rank 0 blocks in `ncclCommInitRank` for 10 seconds — wasting GPU compute time. Gang scheduling guarantees all ranks start within milliseconds.

**CQ2: What is backfill scheduling and how can it improve cluster utilization?**
> Backfill scheduling: while a large gang job waits for enough GPUs, allow smaller jobs to run on currently-free GPUs, as long as they finish before the large job's resources become available. The scheduler estimates the small job's runtime; if it completes before the deadline, the large job is not delayed. This improves utilization from ~60% (pure gang scheduling) to ~80-90%. Risk: small job runtime estimation errors can delay large jobs.

**CQ3: How would you handle a GPU failure mid-job in a gang-scheduled workload?**
> A GPU failure causes its gang member to stop responding. Options: (1) **Checkpoint and restart**: periodically checkpoint model weights to distributed storage (NEMO/DeepSpeed checkpointing), restart the job on healthy GPUs. (2) **Spare GPUs**: keep N+1 GPUs allocated; if one fails, remap the failed rank's work to the spare. (3) **Elastic training**: NCCL supports `ncclCommAbort`; training frameworks like PyTorch FSDP support elastic reinitialization without full job restart. NVIDIA DGX systems use option 1 with 5-minute checkpoints.

**CQ4: What is the "thundering herd" problem in GPU scheduling and how do you mitigate it?**
> When a large gang of GPUs becomes free (e.g., a 256-GPU job finishes), many waiting jobs simultaneously attempt to reserve GPUs — all retry their CAS loops. Most fail and retry again — O(n²) CAS contention. Mitigation: (1) use exponential random backoff in the retry loop, (2) implement a priority queue of waiting jobs with a single "scheduler thread" that wakes exactly one job, (3) use a hardware queue (NVSwitch's built-in priority arbiter for link bandwidth, extended to scheduling).

**CQ5: How does Kubernetes GPU scheduling with the NVIDIA device plugin differ from this approach?**
> The NVIDIA device plugin reports GPU resources to kubelet as `nvidia.com/gpu: 8`. Kubernetes allocates pods to nodes based on resource requests. Limitations vs this design: (1) no gang scheduling — Kubernetes schedules pods independently (gang scheduling requires the `volcano` or `yunikorn` scheduler), (2) no topology awareness (NVSwitch vs IB) without NVIDIA's GPU topology-aware scheduler extension, (3) binary allocation (full GPU or none) without MIG fractions. The `nvidia-device-plugin` is production-grade but requires topology-aware extensions for distributed DL training.

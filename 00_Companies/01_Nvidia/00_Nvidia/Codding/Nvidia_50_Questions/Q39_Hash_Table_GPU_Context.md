# Q39: Hash Table for GPU Context Management

**Section:** Performance & Algorithms | **Difficulty:** Hard | **Topics:** `DEFINE_HASHTABLE`, `hash_add`, `hash_for_each_possible_rcu`, `hlist_node`, RCU, spinlock, context lookup

---

## Question

Implement a hash table-based GPU context manager supporting fast O(1) context lookup by ID.

---

## Answer

```c
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

/* ─── GPU Context ─────────────────────────────────────────────────────────*/
struct gpu_context {
    u64                 ctx_id;         /* unique context ID              */
    pid_t               owner_pid;      /* owning process PID             */
    atomic_t            ref_count;      /* reference count (kref alternative) */
    struct gpu_va_space va_space;       /* GPU virtual address space       */
    struct list_head    channel_list;   /* GPU channels in this context   */

    /* Hash table linkage: embedded node avoids separate allocation */
    struct hlist_node   hash_node;
    /* RCU head for safe deletion */
    struct rcu_head     rcu;
};

/* ─── GPU Context Table ───────────────────────────────────────────────────
 * CTX_HASH_BITS=10: 2^10 = 1024 buckets.
 * Each bucket is an hlist_head (singly-linked list with a back-pointer).
 * Expected load: 10–100 contexts → excellent average case.
 */
#define CTX_HASH_BITS 10

struct gpu_ctx_table {
    /*
     * DEFINE_HASHTABLE(name, bits): declares an array of hlist_head.
     * Equivalent to: struct hlist_head ctx_ht[1 << CTX_HASH_BITS];
     */
    DEFINE_HASHTABLE(ctx_ht, CTX_HASH_BITS);

    spinlock_t      write_lock;  /* protects hash_add / hash_del          */
    /* Reads use RCU (read side: rcu_read_lock / rcu_read_unlock)          */
    atomic64_t      next_ctx_id; /* monotonically increasing context IDs  */
};

/* ─── Create a new GPU context ────────────────────────────────────────────*/
struct gpu_context *gpu_ctx_create(struct gpu_ctx_table *table, pid_t pid)
{
    struct gpu_context *ctx;
    u64 id;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return ERR_PTR(-ENOMEM);

    id              = atomic64_inc_return(&table->next_ctx_id);
    ctx->ctx_id     = id;
    ctx->owner_pid  = pid;
    atomic_set(&ctx->ref_count, 1);
    INIT_LIST_HEAD(&ctx->channel_list);

    /*
     * hash_add_rcu: insert into the hash table bucket for ctx_id.
     * Uses RCU-safe list insertion (rcu_assign_pointer internally).
     * hash_32(key, bits): Jenkins hash of the key to select bucket.
     * After insertion, readers using rcu_read_lock will see the context.
     *
     * Write side always holds write_lock before insertion.
     */
    spin_lock(&table->write_lock);
    hash_add_rcu(table->ctx_ht, &ctx->hash_node, ctx->ctx_id);
    spin_unlock(&table->write_lock);

    pr_debug("GPU: created ctx %llu for pid %d\n", id, pid);
    return ctx;
}

/* ─── Look up a context by ID (lock-free RCU read) ─────────────────────*/
struct gpu_context *gpu_ctx_find(struct gpu_ctx_table *table, u64 ctx_id)
{
    struct gpu_context *ctx = NULL;

    /*
     * rcu_read_lock: marks the beginning of an RCU read-side critical section.
     * No locks taken. Prevents the current CPU from being preempted in a way
     * that would defer RCU grace periods indefinitely.
     */
    rcu_read_lock();

    /*
     * hash_for_each_possible_rcu: iterates all entries in the bucket
     * that ctx_id hashes to. Typically 1 entry (no collision) in practice.
     * RCU-safe: uses rcu_dereference to read next pointers.
     */
    hash_for_each_possible_rcu(table->ctx_ht, ctx, hash_node, ctx_id) {
        if (ctx->ctx_id == ctx_id) {
            /* Found — take a reference before dropping RCU read lock */
            if (!atomic_inc_not_zero(&ctx->ref_count))
                ctx = NULL; /* concurrent deletion — object being freed */
            break;
        }
        ctx = NULL;
    }

    rcu_read_unlock();
    return ctx; /* NULL if not found */
}

/* ─── Iterate all contexts (RCU read-side) ───────────────────────────────*/
void gpu_ctx_for_each(struct gpu_ctx_table *table,
                       void (*fn)(struct gpu_context *ctx, void *priv),
                       void *priv)
{
    struct gpu_context *ctx;
    int bkt;

    rcu_read_lock();

    /*
     * hash_for_each_rcu: iterates ALL entries in ALL buckets.
     * bkt: current bucket index (managed by macro).
     * O(n) over all contexts — use only for administrative operations.
     */
    hash_for_each_rcu(table->ctx_ht, bkt, ctx, hash_node)
        fn(ctx, priv);

    rcu_read_unlock();
}

/* ─── RCU callback: free context after grace period ─────────────────────*/
static void gpu_ctx_rcu_free(struct rcu_head *rcu)
{
    struct gpu_context *ctx = container_of(rcu, struct gpu_context, rcu);
    kfree(ctx);
}

/* ─── Release a context reference ────────────────────────────────────────*/
void gpu_ctx_put(struct gpu_ctx_table *table, struct gpu_context *ctx)
{
    if (!atomic_dec_and_test(&ctx->ref_count))
        return; /* other references still held */

    /* Last reference dropped — remove from hash table */
    spin_lock(&table->write_lock);
    /*
     * hash_del_rcu: remove from hash table in RCU-safe manner.
     * Existing RCU readers can still traverse to this entry until
     * the current RCU grace period ends.
     */
    hash_del_rcu(&ctx->hash_node);
    spin_unlock(&table->write_lock);

    /*
     * call_rcu: schedule ctx free AFTER all current RCU readers finish.
     * This ensures no reader has a stale pointer to ctx when kfree runs.
     */
    call_rcu(&ctx->rcu, gpu_ctx_rcu_free);

    pr_debug("GPU: ctx %llu freed (RCU)\n", ctx->ctx_id);
}

/* ─── Initialize context table ───────────────────────────────────────────*/
void gpu_ctx_table_init(struct gpu_ctx_table *table)
{
    hash_init(table->ctx_ht); /* zero all bucket heads */
    spin_lock_init(&table->write_lock);
    atomic64_set(&table->next_ctx_id, 1);
}
```

---

## Explanation

### Core Concept

```
GPU Context Table:
ctx_ht[0]:   ctx_id=1024 → ctx_id=2048 → NULL
ctx_ht[1]:   ctx_id=1    → NULL
ctx_ht[2]:   NULL
...
ctx_ht[1023]: ctx_id=9000 → NULL

Lookup ctx_id=1: hash(1, 10) = 1 → bucket[1] → found in O(1)
Reads: lock-free (RCU)    Writes: spinlock protected
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `DEFINE_HASHTABLE(name, bits)` | Declare hash table array |
| `hash_init(ht)` | Initialize all bucket heads to empty |
| `hash_add_rcu(ht, node, key)` | RCU-safe insert |
| `hash_del_rcu(node)` | RCU-safe removal |
| `hash_for_each_possible_rcu(ht, obj, node, key)` | Iterate bucket for key |
| `hash_for_each_rcu(ht, bkt, obj, node)` | Iterate all entries |
| `rcu_read_lock()` / `rcu_read_unlock()` | RCU read-side critical section |
| `call_rcu(head, fn)` | Free after RCU grace period |
| `atomic_inc_not_zero(v)` | Safe ref-count increment (returns 0 if already 0) |

### Trade-offs & Pitfalls

- **Hash collisions.** `CTX_HASH_BITS=10` gives 1024 buckets. With 1000 contexts, expected bucket depth ≈ 1 (good). But context IDs are sequential — hash function must distribute them well. `hash_32`/`hash_64` use multiplication-based hash which handles sequential keys well.
- **RCU deletion and stale pointers.** After `hash_del_rcu`, existing RCU readers may still have a pointer to the deleted context. `call_rcu` ensures `kfree` only runs after all readers have exited their critical sections. Never `kfree` immediately after `hash_del_rcu`.

### NVIDIA / GPU Context

NVIDIA's kernel driver (nvidia.ko) maintains a hash table of all active CUDA contexts (one per `cuCtxCreate` call). Context IDs are allocated monotonically and stored in the hash table for O(1) lookup by context ID during GPU fault handling (Q29), which requires fast context identification from a GPU virtual address.

---

## Cross Questions & Answers

**CQ1: Why use RCU for reads and spinlock for writes instead of a single rwlock?**
> RCU advantage: `rcu_read_lock()` is a NOP on non-preemptible kernels (just disables preemption). Readers never block or spin. Multiple readers on different CPUs proceed simultaneously with zero synchronization overhead. `rwlock_t` readers must acquire the read lock (cache line contention on the rwlock itself). For a context table with 1000 lookups/second per CPU across 64 CPUs: RCU eliminates 64,000 cache-line acquisitions/second vs rwlock.

**CQ2: What is the ABA problem and how does `atomic_inc_not_zero` prevent it for reference counting?**
> ABA problem: thread A reads `ref_count = 1`, thread B decrements to 0 and frees the object, thread C allocates a new object at the same address with `ref_count = 1`. Thread A now increments a freed/reused object. `atomic_inc_not_zero` prevents this: if the ref_count has reached 0 (object being freed), the atomic CAS fails and returns 0. Thread A knows the object is being freed and discards its pointer. Only succeeds if ref_count was non-zero (object is alive).

**CQ3: How would you resize the hash table dynamically when load factor becomes too high?**
> Linux's kernel hash table (`DEFINE_HASHTABLE`) is static-size. For dynamic resizing: allocate a new larger table, iterate all entries from the old table (O(n)), re-insert into new table (O(n log n) total), then use RCU grace period to swap old → new (`rcu_assign_pointer`), wait for grace period, free old table. During the resize operation, writes are blocked by the write_lock. This is expensive — NVIDIA's driver pre-allocates a large enough table (2^16 buckets for 65536 contexts) to avoid runtime resize.

**CQ4: What is the hash function used by `hash_add` and is it collision-resistant?**
> Linux uses a Knuth multiplicative hash: `hash_32(val, bits) = (val * GOLDEN_RATIO_32) >> (32 - bits)` where `GOLDEN_RATIO_32 = 0x61C88647`. This distributes keys uniformly even for sequential inputs. For 64-bit keys: `hash_64(val, bits)` uses `val * GOLDEN_RATIO_64`. These are NOT cryptographic hashes — collisions are easy to construct deliberately. For GPU context IDs (sequential, generated by the driver): multiplicative hash provides good distribution without adversarial inputs.

**CQ5: How does the GPU driver handle context lookup during a GPU fault when holding an IRQ spinlock?**
> GPU fault IRQ handler runs with interrupts disabled. `rcu_read_lock()` is safe in IRQ context (just disables preemption, which is already disabled). `hash_for_each_possible_rcu` is safe in IRQ context. The issue: `atomic_inc_not_zero` succeeds, giving us a reference. We cannot call `gpu_ctx_put` (which calls `call_rcu`) from IRQ context — `call_rcu` requires a softirq-safe context. Solution: defer processing to a workqueue (Q09 bottom-half pattern): save the ctx pointer, schedule a work item, return from IRQ. The work item does the actual fault handling and calls `gpu_ctx_put` safely.

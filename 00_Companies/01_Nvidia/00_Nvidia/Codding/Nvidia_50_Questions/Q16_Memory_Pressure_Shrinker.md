# Q16: Memory Pressure Shrinker for GPU Memory Reclaim

**Section:** Memory Management | **Difficulty:** Hard | **Topics:** `shrinker`, memory reclaim, OOM, `register_shrinker`, LRU eviction, GPU cache

---

## Question

Implement a memory pressure notifier to reclaim GPU memory under OOM.

---

## Answer

```c
#include <linux/shrinker.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/module.h>

/* GPU buffer eligible for reclaim */
struct gpu_buffer {
    void            *cpu_addr;
    dma_addr_t       dma_addr;
    size_t           size;
    atomic_t         refcount;
    struct list_head lru;       /* position in GPU LRU list */
    bool             pinned;    /* pinned = not reclaimable */
};

static LIST_HEAD(gpu_lru_list);
static DEFINE_SPINLOCK(gpu_lru_lock);
static atomic_long_t gpu_cached_pages; /* total reclaimable pages */

/* ─── Shrinker count callback ─────────────────────────────────────────────
 * Called by VM to estimate how many objects the driver CAN free.
 * Should be fast and non-blocking — no locks.
 */
static unsigned long gpu_shrink_count(struct shrinker *s,
                                       struct shrink_control *sc)
{
    return atomic_long_read(&gpu_cached_pages);
}

/* ─── Shrinker scan callback ──────────────────────────────────────────────
 * Called by VM to actually free up to sc->nr_to_scan objects.
 * Returns the number of objects freed.
 * May be called from direct reclaim (process context) or kswapd.
 */
static unsigned long gpu_shrink_scan(struct shrinker *s,
                                      struct shrink_control *sc)
{
    struct gpu_buffer *buf, *tmp;
    unsigned long freed = 0;
    LIST_HEAD(to_free);

    spin_lock(&gpu_lru_lock);
    list_for_each_entry_safe(buf, tmp, &gpu_lru_list, lru) {
        if (freed >= sc->nr_to_scan)
            break;
        /* Only reclaim buffers with no active users and not pinned */
        if (atomic_read(&buf->refcount) == 0 && !buf->pinned) {
            list_move(&buf->lru, &to_free);
            freed++;
        }
    }
    spin_unlock(&gpu_lru_lock);

    /* Free outside the lock — dma_free_coherent may sleep */
    list_for_each_entry_safe(buf, tmp, &to_free, lru) {
        list_del(&buf->lru);
        gpu_free_buffer_internal(buf);
        atomic_long_sub(buf->size >> PAGE_SHIFT, &gpu_cached_pages);
    }

    pr_debug("GPU shrinker: freed %lu pages (requested %lu)\n",
             freed * (PAGE_SIZE), sc->nr_to_scan * PAGE_SIZE);
    return freed;
}

/* ─── Shrinker registration ───────────────────────────────────────────────*/
static struct shrinker gpu_shrinker = {
    .count_objects  = gpu_shrink_count,
    .scan_objects   = gpu_shrink_scan,
    .seeks          = DEFAULT_SEEKS, /* relative cost: 1 = default */
    .batch          = 0,             /* 0 = use default batch size */
};

int gpu_shrinker_init(void)
{
    atomic_long_set(&gpu_cached_pages, 0);

    /*
     * register_shrinker: registers with the kernel VM.
     * When memory is low (kswapd wakes, GFP_KERNEL fails, OOM approaches),
     * the kernel calls our scan_objects to free cached GPU memory.
     */
    return register_shrinker(&gpu_shrinker, "nvidia-gpu-cache");
}

void gpu_shrinker_exit(void)
{
    unregister_shrinker(&gpu_shrinker);
}

/* ─── Buffer lifecycle: track in LRU for potential reclaim ────────────────*/
void gpu_buffer_cache(struct gpu_buffer *buf)
{
    spin_lock(&gpu_lru_lock);
    list_add(&buf->lru, &gpu_lru_list); /* add as MRU */
    spin_unlock(&gpu_lru_lock);

    atomic_long_add(buf->size >> PAGE_SHIFT, &gpu_cached_pages);
}

/* Promote to MRU when accessed (prevents premature eviction) */
void gpu_buffer_touch(struct gpu_buffer *buf)
{
    spin_lock(&gpu_lru_lock);
    list_move(&buf->lru, &gpu_lru_list);
    spin_unlock(&gpu_lru_lock);
}

/* Remove from LRU when explicitly freed by user */
void gpu_buffer_uncache(struct gpu_buffer *buf)
{
    spin_lock(&gpu_lru_lock);
    list_del_init(&buf->lru);
    spin_unlock(&gpu_lru_lock);

    atomic_long_sub(buf->size >> PAGE_SHIFT, &gpu_cached_pages);
}

/* ─── OOM notifier (alternative: receive OOM killer events) ───────────────*/
static int gpu_oom_notify(struct notifier_block *nb,
                           unsigned long action, void *data)
{
    struct shrink_control sc = {
        .gfp_mask    = GFP_KERNEL,
        .nr_to_scan  = atomic_long_read(&gpu_cached_pages),
    };

    pr_warn("GPU: OOM detected, aggressively reclaiming all cached GPU memory\n");
    gpu_shrink_scan(&gpu_shrinker, &sc);
    return NOTIFY_OK;
}

static struct notifier_block gpu_oom_nb = {
    .notifier_call = gpu_oom_notify,
};

/* register with: register_oom_notifier(&gpu_oom_nb); */
```

---

## Explanation

### Core Concept

The Linux kernel's VM (Virtual Memory) subsystem calls registered **shrinkers** when memory pressure builds:

```
Low memory event triggers:
  kswapd wakes (low watermark crossed)
  Direct reclaim (GFP_KERNEL allocation fails)
  OOM killer about to fire

    │
    ▼
kernel calls: shrinker->count_objects()   ← how much can be freed?
kernel calls: shrinker->scan_objects()    ← free up to nr_to_scan
    │
    ▼
Driver: free LRU GPU cached buffers
    │
    ▼
System memory available again
```

**Priority:** `seeks` controls relative priority. Low `seeks` = high priority (freed first). Default `DEFAULT_SEEKS = 2`.

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `register_shrinker(shrinker, name)` | Register with VM reclaim path |
| `unregister_shrinker(shrinker)` | Unregister (must be called on module exit) |
| `struct shrinker.count_objects` | Callback: estimate reclaimable objects |
| `struct shrinker.scan_objects` | Callback: actually free up to `nr_to_scan` |
| `struct shrink_control.nr_to_scan` | How many objects the VM wants freed |
| `struct shrink_control.gfp_mask` | GFP flags of the caller — if `GFP_ATOMIC`, cannot sleep |
| `struct shrink_control.nid` | NUMA node — prefer freeing from this node |
| `DEFAULT_SEEKS` | Default shrinker seeks value (= 2) |
| `register_oom_notifier(nb)` | Register callback called just before OOM kill |

### Trade-offs & Pitfalls

- **No sleeping under `gpu_lru_lock` spinlock.** The `dma_free_coherent` call may sleep. Use the collect-under-lock, free-outside-lock pattern (splice to local list first).
- **`count_objects` must be O(1) or at least cheap.** It's called frequently. Use an `atomic_long_t` counter instead of traversing the list.
- **Return SHRINK_STOP if nothing can be freed.** If `scan_objects` returns 0 when `nr_to_scan > 0`, the VM considers the shrinker exhausted and won't call it again until `count_objects` returns non-zero.
- **NUMA awareness.** `sc->nid` specifies which NUMA node's memory is under pressure. Prefer reclaiming buffers allocated on that node to maximize effectiveness. Use `page_to_nid(virt_to_page(buf->cpu_addr))` to check.

### NVIDIA / GPU Context

NVIDIA GPU drivers register shrinkers for:
- **GPU page table cache:** GMMU page tables cached for reuse — freed under pressure
- **GPU firmware staging buffers:** large buffers kept resident for faster firmware updates — first to be reclaimed
- **CUDA memory pool:** `cudaMemPoolCreate` buffers are lazily freed — reclaimed by shrinker before OOM
- Without a shrinker, GPU drivers would "hoard" memory during CUDA workloads, leaving no room for the rest of the system — leading to unnecessary OOM kills of unrelated processes

---

## Cross Questions & Answers

**CQ1: What is the difference between the shrinker and the OOM notifier?**
> The **shrinker** is called proactively when memory is low but before OOM — the goal is to prevent OOM by freeing caches. It is called repeatedly with increasing pressure. The **OOM notifier** is called as a last resort, just before the OOM killer selects a process to kill. For GPU drivers, shrinkers are preferred because they allow graceful incremental reclaim. OOM notifiers are a safety net for emergency bulk reclaim.

**CQ2: What happens if a shrinker's `scan_objects` returns a smaller number than `nr_to_scan`?**
> The VM receives the count of freed objects. If it's less than requested, the VM continues calling other shrinkers to make up the difference. If all shrinkers are exhausted (total freed < total needed), the VM escalates to the OOM killer. A good shrinker should always report its true reclaimable count in `count_objects` to give the VM accurate information for pressure decisions.

**CQ3: How do you ensure the shrinker doesn't reclaim memory that is actively being used?**
> Use a reference count (`atomic_t refcount`). The shrinker only reclaims objects with `refcount == 0`. When a GPU operation needs the buffer, it increments `refcount`. When the operation completes, it decrements. Only idle, unreferenced buffers appear in the shrinker's reclaimable set. Additionally, use a `pinned` flag for buffers that must survive for the lifetime of a GPU context (e.g., page tables, channel rings).

**CQ4: How does `seeks` influence which shrinker is called first?**
> `seeks` represents the relative I/O cost of recreating the freed objects. A low `seeks` value means the objects are cheap to recreate (e.g., CPU page cache) — freed first. A high `seeks` value means expensive-to-recreate objects (e.g., GPU page tables requiring GMMU flush) — freed later. By setting a high `seeks` for GPU page tables (expensive to rebuild) and low `seeks` for GPU data caches (cheap to regenerate), drivers influence the reclaim order to minimize GPU performance impact.

**CQ5: What is `memcg` (memory cgroup) and how does it interact with the GPU shrinker?**
> Memory cgroups limit memory usage per process group. When a cgroup hits its memory limit, the kernel calls shrinkers with `sc->memcg` set to the offending cgroup. A NUMA-aware, memcg-aware shrinker should only reclaim memory belonging to that cgroup's GPU contexts. NVIDIA's driver supports memcg-aware reclaim by associating GPU buffer allocations with the allocating task's memcg, enabling proper per-container GPU memory accounting in Kubernetes pods.

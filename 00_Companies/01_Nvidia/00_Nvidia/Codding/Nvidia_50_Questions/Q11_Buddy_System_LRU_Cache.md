# Q11: Buddy System Page Allocator + Page Cache with LRU Eviction

**Section:** Memory Management | **Difficulty:** Hard | **Topics:** buddy allocator, page cache, LRU, `list_head`, hashtable, `__free_page`, memory reclaim

---

## Question

Explain the Linux page allocator — Buddy system. Implement a page cache with LRU eviction.

---

## Answer

```c
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mutex.h>

#define CACHE_BITS   8       /* 256 hash buckets */
#define MAX_ENTRIES  128     /* max cached pages */

struct cache_entry {
    u64               key;   /* cache lookup key (e.g., page offset) */
    struct page      *page;  /* the cached physical page */
    struct list_head  lru;   /* node in LRU list (MRU at head, LRU at tail) */
    struct hlist_node hash;  /* node in hash table */
};

static DEFINE_HASHTABLE(cache_ht, CACHE_BITS);
static LIST_HEAD(lru_list);       /* head = MRU, tail = LRU */
static int          cache_count;
static DEFINE_MUTEX(cache_lock);

/* ─── Evict the LRU (tail) entry ──────────────────────────────────────────*/
static void evict_lru(void)
{
    struct cache_entry *victim;

    if (list_empty(&lru_list))
        return;

    /* Least-recently-used entry is at the tail */
    victim = list_last_entry(&lru_list, struct cache_entry, lru);

    hash_del(&victim->hash);
    list_del(&victim->lru);
    __free_page(victim->page); /* return page to buddy allocator */
    kfree(victim);
    cache_count--;
}

/* ─── Lookup (cache hit → promote to MRU) ────────────────────────────────*/
struct page *cache_get(u64 key)
{
    struct cache_entry *e;
    struct page        *page = NULL;

    mutex_lock(&cache_lock);
    hash_for_each_possible(cache_ht, e, hash, key) {
        if (e->key == key) {
            /* Promote to MRU position (move to head of LRU list) */
            list_move(&e->lru, &lru_list);
            page = e->page;
            break;
        }
    }
    mutex_unlock(&cache_lock);
    return page;
}

/* ─── Insert a new page ───────────────────────────────────────────────────*/
int cache_insert(u64 key, struct page *page)
{
    struct cache_entry *e;

    e = kmalloc(sizeof(*e), GFP_KERNEL);
    if (!e)
        return -ENOMEM;

    e->key  = key;
    e->page = page;

    mutex_lock(&cache_lock);
    if (cache_count >= MAX_ENTRIES)
        evict_lru();             /* make room */

    hash_add(cache_ht, &e->hash, key);
    list_add(&e->lru, &lru_list); /* add as MRU (head) */
    cache_count++;
    mutex_unlock(&cache_lock);
    return 0;
}

/* ─── Invalidate a specific page ──────────────────────────────────────────*/
void cache_invalidate(u64 key)
{
    struct cache_entry *e;

    mutex_lock(&cache_lock);
    hash_for_each_possible(cache_ht, e, hash, key) {
        if (e->key == key) {
            hash_del(&e->hash);
            list_del(&e->lru);
            __free_page(e->page);
            kfree(e);
            cache_count--;
            break;
        }
    }
    mutex_unlock(&cache_lock);
}

/* ─── Drain entire cache ──────────────────────────────────────────────────*/
void cache_flush_all(void)
{
    struct cache_entry *e;
    struct hlist_node  *tmp;
    int bkt;

    mutex_lock(&cache_lock);
    hash_for_each_safe(cache_ht, bkt, tmp, e, hash) {
        hash_del(&e->hash);
        list_del(&e->lru);
        __free_page(e->page);
        kfree(e);
    }
    cache_count = 0;
    mutex_unlock(&cache_lock);
}
```

---

## Explanation

### Core Concept — Buddy Allocator

The Linux **buddy system** manages physical memory in blocks of size $2^{order}$ pages:

```
Order 0:  4 KB  (1 page)
Order 1:  8 KB  (2 pages)
Order 2:  16 KB (4 pages)
...
Order 10: 4 MB  (1024 pages)
```

**Allocation:** Find smallest order block that satisfies the request. If not available, split a larger block ("buddy" splitting). **Free:** Merge adjacent buddies back into larger blocks ("buddy coalescing"). This gives O(log n) alloc/free and excellent fragmentation resistance.

**LRU Cache Design:**

```
LRU List:    [MRU] → [e3] → [e7] → [e1] → [e5] → [LRU]
                                                    ↑ evict this
Hash Table:  key → entry (O(1) lookup)
```

- On **hit**: `list_move` entry to head (O(1)) — promotes to MRU
- On **miss + full**: evict tail (O(1)), insert new at head

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `alloc_page(gfp)` | Allocate a single page from buddy allocator |
| `alloc_pages(gfp, order)` | Allocate `2^order` contiguous pages |
| `__free_page(page)` | Free a single page back to buddy |
| `__free_pages(page, order)` | Free a multi-page allocation |
| `DEFINE_HASHTABLE(name, bits)` | Declare a hash table with `2^bits` buckets |
| `hash_add(ht, node, key)` | Insert into hash table |
| `hash_del(node)` | Remove from hash table |
| `hash_for_each_possible(ht, obj, member, key)` | Iterate over a hash bucket |
| `hash_for_each_safe(ht, bkt, tmp, obj, member)` | Safe full iteration |
| `list_move(&entry, &head)` | O(1) move entry to head of another list |
| `list_last_entry(head, type, member)` | Get last entry (LRU tail) |

### Trade-offs & Pitfalls

- **Mutex vs spinlock for cache:** This cache uses `mutex_lock` because `__free_page` and `kmalloc` may sleep. If the cache is accessed from softirq context, use a spinlock with `GFP_ATOMIC` allocations.
- **Hash collision:** With a 256-bucket hash and 128 entries, load factor is 0.5 — acceptable. A heavily loaded cache should increase `CACHE_BITS`.
- **`page` reference counting:** In a production cache, always call `get_page(page)` on cache insert and `put_page(page)` on eviction/removal to integrate with the kernel's page reference counting.

### NVIDIA / GPU Context

NVIDIA GPU drivers maintain similar caches for:
- **GMMU Page Table Cache:** caches recently-used GPU page table entries (avoid redundant GMMU walks)
- **DMA Buffer Cache:** caches recently-used pinned user pages to avoid repeated `pin_user_pages` calls
- **Descriptor Cache:** LRU pool of pre-allocated GPU command descriptors (avoids per-submission `kmalloc`)

---

## Cross Questions & Answers

**CQ1: What is internal vs external fragmentation in the buddy allocator?**
> **External fragmentation:** Free memory exists, but not as a single contiguous block of the required size. The buddy allocator minimizes this by coalescing buddies. **Internal fragmentation:** Allocating a `2^order` block when you need slightly less (e.g., 5KB wastes 3KB from an 8KB order-1 block). The slab allocator on top of the buddy system reduces internal fragmentation for small kernel objects by subdividing pages.

**CQ2: How does the kernel's `shrink_slab` interact with a custom page cache?**
> The kernel memory reclaim path calls registered `shrinker` callbacks when memory is low. A driver with a custom page cache should register a `struct shrinker` with `register_shrinker()`. The `count_objects` callback returns the number of reclaimable pages, and `scan_objects` evicts up to `nr_to_scan` pages. Without this, the kernel cannot reclaim the driver's cached pages under memory pressure — leading to OOM when the driver is the largest memory consumer.

**CQ3: What is the difference between `page_to_phys(page)` and `page_to_virt(page)`?**
> `page_to_phys(page)` returns the physical address of the page (type `phys_addr_t`) — used for programming DMA addresses into hardware. `page_to_virt(page)` returns the kernel virtual address (in the linear mapping region, `PAGE_OFFSET + phys`) — used for CPU access to the page content. For high memory pages (on 32-bit kernels), `page_to_virt` is invalid; use `kmap` instead.

**CQ4: How would you make this LRU cache thread-safe for concurrent `get` and `insert` calls from multiple CPUs without using a global mutex?**
> Use a **sharded approach**: divide the hash table into N shards, each with its own lock. Lookups and inserts only lock one shard. This scales to N CPUs with N× throughput. A more advanced approach: use RCU for reads (no lock on hit path) and per-shard spinlocks only for writes. Linux's page cache uses exactly this design — `struct address_space` has a per-inode spinlock and RCU for page lookups.

**CQ5: What is the difference between `alloc_page(GFP_KERNEL)` and `alloc_page(GFP_HIGHUSER_MOVABLE)`?**
> `GFP_KERNEL` allocates from the kernel's lowmem zone — the page gets a permanent kernel virtual address and should not be migrated. `GFP_HIGHUSER_MOVABLE` allocates a user-space page that is marked movable — the kernel's memory compactor can migrate it to create contiguous free regions for huge page allocation. GPU drivers use `GFP_HIGHUSER_MOVABLE` for user-accessible GPU buffers to allow the system to maintain memory compaction. Kernel-internal data must use `GFP_KERNEL` (not movable).

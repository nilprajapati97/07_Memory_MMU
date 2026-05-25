# Q38: Red-Black Tree for GPU Virtual Address Space

**Section:** Performance & Algorithms | **Difficulty:** Hard | **Topics:** `rb_root`, `rb_node`, `rb_insert_color`, `rb_first`, `rb_erase`, VA range management, O(log n)

---

## Question

Implement a red-black tree-based virtual address range manager for GPU address space allocation.

---

## Answer

```c
#include <linux/rbtree.h>
#include <linux/slab.h>

/* ─── VA Range Node ───────────────────────────────────────────────────────*/
struct va_range {
    u64              start;     /* inclusive start of VA range          */
    u64              end;       /* exclusive end of VA range            */
    u32              flags;     /* PROT_READ | PROT_WRITE | cache type  */
    struct rb_node   node;      /* embedded RB node — no separate alloc */
};

/* ─── GPU VA space ────────────────────────────────────────────────────────*/
struct gpu_va_space {
    struct rb_root  root;          /* root of RB tree; rb_root = {NULL}  */
    u64             va_base;
    u64             va_end;
    spinlock_t      lock;
};

/* ─── Insert a VA range into the RB tree ─────────────────────────────────
 * Maintains tree sorted by va_range.start.
 */
static int rb_insert_va(struct gpu_va_space *space, struct va_range *new_range)
{
    struct rb_node **link  = &space->root.rb_node;
    struct rb_node  *parent = NULL;
    struct va_range *cur;

    /* Standard RB tree insertion walk */
    while (*link) {
        parent = *link;
        cur    = container_of(parent, struct va_range, node);

        if (new_range->start < cur->start) {
            link = &parent->rb_left;
        } else if (new_range->start >= cur->end) {
            link = &parent->rb_right;
        } else {
            /* Overlap detected — VA ranges must not overlap */
            pr_err("VA: overlap: [0x%llx, 0x%llx) conflicts with [0x%llx, 0x%llx)\n",
                   new_range->start, new_range->end,
                   cur->start, cur->end);
            return -EEXIST;
        }
    }

    /*
     * rb_link_node: attach new node at the found position.
     * rb_insert_color: rebalance the tree (rotations/recoloring) to
     * maintain red-black invariants after insertion.
     */
    rb_link_node(&new_range->node, parent, link);
    rb_insert_color(&new_range->node, &space->root);
    return 0;
}

/* ─── Find VA range containing addr ─────────────────────────────────────*/
struct va_range *rb_find_va(struct gpu_va_space *space, u64 addr)
{
    struct rb_node *node = space->root.rb_node;

    while (node) {
        struct va_range *cur = container_of(node, struct va_range, node);

        if (addr < cur->start)
            node = node->rb_left;
        else if (addr >= cur->end)
            node = node->rb_right;
        else
            return cur; /* found */
    }
    return NULL; /* not found */
}

/* ─── Find best-fit free VA range ────────────────────────────────────────
 * Walk the tree to find the first gap large enough for 'size'.
 * O(n) worst case — acceptable for allocation (vs O(log n) for lookup).
 */
static u64 rb_find_free_va(struct gpu_va_space *space, u64 size, u64 align)
{
    struct rb_node *node;
    u64 candidate;

    candidate = ALIGN(space->va_base, align);

    /*
     * rb_first: get the leftmost (smallest VA) node — O(log n).
     * Then iterate in-order (ascending VA) to find the first gap.
     */
    for (node = rb_first(&space->root); node; node = rb_next(node)) {
        struct va_range *cur = container_of(node, struct va_range, node);

        /* Is there a gap between 'candidate' and this range's start? */
        if (candidate + size <= cur->start)
            return candidate; /* gap found before this range */

        /* Skip past this range */
        candidate = ALIGN(cur->end, align);
    }

    /* Check gap after the last range */
    if (candidate + size <= space->va_end)
        return candidate;

    return 0; /* no free VA found */
}

/* ─── Allocate a VA range ─────────────────────────────────────────────────*/
struct va_range *gpu_va_alloc(struct gpu_va_space *space,
                               u64 size, u64 align, u32 flags)
{
    struct va_range *range;
    u64 va;
    int ret;

    range = kzalloc(sizeof(*range), GFP_KERNEL);
    if (!range)
        return ERR_PTR(-ENOMEM);

    spin_lock(&space->lock);

    va = rb_find_free_va(space, size, align);
    if (!va) {
        spin_unlock(&space->lock);
        kfree(range);
        return ERR_PTR(-ENOMEM);
    }

    range->start = va;
    range->end   = va + size;
    range->flags = flags;

    ret = rb_insert_va(space, range);
    spin_unlock(&space->lock);

    if (ret) {
        kfree(range);
        return ERR_PTR(ret);
    }

    return range;
}

/* ─── Free a VA range ─────────────────────────────────────────────────────*/
void gpu_va_free(struct gpu_va_space *space, struct va_range *range)
{
    spin_lock(&space->lock);
    /*
     * rb_erase: remove node from RB tree and rebalance.
     * Does NOT free the node — caller owns the va_range memory.
     */
    rb_erase(&range->node, &space->root);
    spin_unlock(&space->lock);

    kfree(range);
}

/* ─── Print all VA ranges (for debugging) ────────────────────────────────*/
void gpu_va_dump(struct gpu_va_space *space)
{
    struct rb_node *node;

    pr_info("GPU VA space [0x%llx – 0x%llx]:\n",
            space->va_base, space->va_end);

    /* rb_for_each walks the tree in sorted order */
    for (node = rb_first(&space->root); node; node = rb_next(node)) {
        struct va_range *r = container_of(node, struct va_range, node);
        pr_info("  [0x%llx – 0x%llx) size=%lluMB flags=0x%x\n",
                r->start, r->end, (r->end - r->start) >> 20, r->flags);
    }
}
```

---

## Explanation

### Core Concept

```
GPU VA Space (48-bit: 256TB)
  rb_root
      │
     [0x1000–0x5000)
      /              \
[0x0000–0x1000)   [0x5000–0x9000)
                        \
                   [0xA000–0xC000)

In-order traversal: 0x0000, 0x1000, 0x5000, 0xA000  (sorted by start address)
Lookup: O(log n)   Insert/Delete: O(log n) amortized
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `struct rb_root` | RB tree root (`= RB_ROOT` to initialize) |
| `struct rb_node` | Embedded node (no separate allocation) |
| `rb_link_node(node, parent, link)` | Place node at BST position |
| `rb_insert_color(node, root)` | Rebalance after BST insertion |
| `rb_erase(node, root)` | Remove node and rebalance |
| `rb_first(root)` | Leftmost (min-key) node — O(log n) |
| `rb_next(node)` | In-order successor — O(1) amortized |
| `container_of(ptr, type, member)` | Get enclosing struct from embedded node |

### Trade-offs & Pitfalls

- **Free-space tracking.** This implementation scans in-order to find free gaps — O(n). For high-performance allocators, maintain an interval tree (augmented RB tree where each node tracks `max_free` in its subtree) — O(log n) free space lookup. Linux's `struct vma_area_struct` uses exactly this approach.
- **No merging of adjacent free ranges.** After `gpu_va_free`, the freed range could be merged with adjacent free ranges. This implementation doesn't merge — can lead to fragmentation. Add merge logic: check left/right neighbors on free and merge if they're both free.

### NVIDIA / GPU Context

NVIDIA's GMMU (GPU Memory Management Unit) manages a 48-bit GPU VA space per context. The nvidia.ko driver maintains this exact RB tree structure for each CUDA context's GPU VA allocations. `cuMemAlloc` → driver calls `gpu_va_alloc` → inserts into RB tree → programs GMMU PTE.

---

## Cross Questions & Answers

**CQ1: What is the time complexity of RB tree operations and why is it preferred over AVL trees?**
> RB tree: O(log n) for search, insert, delete. AVL tree: also O(log n) but with more rotations on insert/delete (strictly balanced). Linux kernel uses RB trees because they have fewer rotations per insert (max 2) vs AVL (max log n rotations). For GPU VA management where allocations/frees are frequent, fewer rotations = better performance. AVL trees are better for read-heavy workloads (slightly more balanced = fewer comparisons per lookup).

**CQ2: How does the Linux kernel's augmented RB tree work for interval trees?**
> An augmented RB tree adds a cached value to each node derived from its subtree. For an interval tree: each node stores `subtree_max_end` = max of all `end` values in its subtree. On insert/delete/rotate, the `subtree_max_end` is propagated up. Searching for an interval overlapping `[lo, hi]`: if `node->subtree_max_end < lo`, the entire subtree has no candidates — prune. This reduces average case from O(n) to O(log n + k) where k = number of results. Linux provides `INTERVAL_TREE_DEFINE` macro.

**CQ3: What is the `container_of` macro and how does it enable zero-allocation RB tree nodes?**
> `container_of(ptr, type, member)` computes the address of the enclosing struct by subtracting the offset of `member` from `ptr`. For RB trees: `struct va_range` embeds `struct rb_node node` directly. When the RB tree traversal gives us a `struct rb_node *`, we recover `struct va_range *` via `container_of(rbnode, struct va_range, node)`. This avoids a separate allocation for the tree node — the `va_range` IS the tree node. One allocation, no pointer chasing.

**CQ4: How would you make the GPU VA allocator thread-safe for concurrent CUDA contexts?**
> Current implementation uses a single `spinlock_t` protecting the entire RB tree. For high-concurrency (thousands of CUDA contexts): use a reader-writer lock (`rwlock_t`) — multiple concurrent `rb_find_va` lookups, exclusive writes for alloc/free. For even higher concurrency: RCU-protected RB tree (`rbtree_latch` or `rcu_assign_pointer` on root pointer updates) — reads are lock-free. NVIDIA uses RCU for GPU VA lookups in the GPU fault handler (NMI-like context where spinlocks may not be taken).

**CQ5: How does the GPU virtual-to-physical address translation work after VA allocation?**
> After `gpu_va_alloc` returns a VA range `[va, va+size)`, the driver must create GPU page table entries (PTEs) mapping each GPU VA page to a physical address. NVIDIA's GMMU is a multi-level page table (similar to x86 4-level paging, but for GPU VAs): PGD → PUD → PMD → PTE. Each PTE maps one GPU page (4KB or 2MB for large pages) to a physical address in VRAM or system memory. The driver writes PTEs via MMIO or a GPU CE (Copy Engine) command. After writing PTEs, the GMMU's TLB must be invalidated (TLB shootdown via GPU interrupt).

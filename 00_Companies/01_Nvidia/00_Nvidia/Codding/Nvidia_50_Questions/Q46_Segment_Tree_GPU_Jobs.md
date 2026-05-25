# Q46: Segment Tree for GPU Job Interval Queries

**Section:** Performance & Algorithms | **Difficulty:** Hard | **Topics:** segment tree, range sum query, range update, GPU job scheduling, O(log n), lazy propagation

---

## Question

Implement a segment tree for range sum queries on GPU job intervals, with O(log n) query and update.

---

## Answer

```c
#include <linux/slab.h>
#include <linux/string.h>

/* ─── Segment Tree (Sum) ──────────────────────────────────────────────────
 *
 * Use case: GPU job scheduler tracks execution time for N time slots.
 * Range query: total execution time in slots [l, r].
 * Point update: slot i completed — update its execution time.
 *
 * Tree stored as 1-indexed array (parent = i, children = 2i, 2i+1).
 * Size: 4*N to handle all tree levels safely.
 */

#define SEG_MAX_N  1024   /* max number of GPU time slots */

struct seg_tree {
    long long tree[4 * SEG_MAX_N]; /* segment tree array (1-indexed)      */
    long long lazy[4 * SEG_MAX_N]; /* lazy propagation values             */
    int       n;                   /* number of leaves                     */
};

/* ─── Build segment tree from array ──────────────────────────────────────
 * Recursively builds the tree bottom-up.
 * node: current node index (root = 1)
 * start, end: range this node covers
 * arr: input array
 */
static void seg_build(struct seg_tree *st, int node,
                       int start, int end, const long long *arr)
{
    if (start == end) {
        /* Leaf node: store the single element */
        st->tree[node] = arr[start];
        return;
    }

    int mid = start + (end - start) / 2;

    /* Build left and right subtrees */
    seg_build(st, 2 * node,     start, mid,   arr);
    seg_build(st, 2 * node + 1, mid + 1, end, arr);

    /* Internal node: sum of children */
    st->tree[node] = st->tree[2 * node] + st->tree[2 * node + 1];
}

/* ─── Range sum query: sum of arr[l..r] ─────────────────────────────────
 * Returns the sum of elements in range [l, r] (1-indexed, inclusive).
 * Time: O(log n)
 */
long long seg_query(struct seg_tree *st, int node,
                     int start, int end,
                     int l, int r)
{
    /* Query range [l,r] doesn't overlap this node's range [start,end] */
    if (r < start || end < l)
        return 0;

    /* Query range fully covers this node's range */
    if (l <= start && end <= r)
        return st->tree[node];

    /* Partial overlap: recurse on both children */
    int mid = start + (end - start) / 2;
    long long left  = seg_query(st, 2 * node,     start, mid,   l, r);
    long long right = seg_query(st, 2 * node + 1, mid + 1, end, l, r);
    return left + right;
}

/* ─── Point update: set arr[idx] = val ──────────────────────────────────
 * Updates a single element and propagates up to root.
 * Time: O(log n)
 */
void seg_update(struct seg_tree *st, int node,
                 int start, int end,
                 int idx, long long val)
{
    if (start == end) {
        /* Leaf: update the value */
        st->tree[node] = val;
        return;
    }

    int mid = start + (end - start) / 2;

    if (idx <= mid)
        seg_update(st, 2 * node,     start, mid,   idx, val);
    else
        seg_update(st, 2 * node + 1, mid + 1, end, idx, val);

    /* Update parent with new sum */
    st->tree[node] = st->tree[2 * node] + st->tree[2 * node + 1];
}

/* ─── Range update with lazy propagation: add val to arr[l..r] ──────────
 * Defers propagation to avoid O(n) updates for wide ranges.
 * Time: O(log n) per update
 */
static void push_down(struct seg_tree *st, int node, int start, int end)
{
    if (st->lazy[node] != 0) {
        int mid = start + (end - start) / 2;

        /* Propagate lazy value to children */
        st->tree[2 * node]     += st->lazy[node] * (mid - start + 1);
        st->lazy[2 * node]     += st->lazy[node];

        st->tree[2 * node + 1] += st->lazy[node] * (end - mid);
        st->lazy[2 * node + 1] += st->lazy[node];

        st->lazy[node] = 0; /* clear lazy for this node */
    }
}

void seg_range_add(struct seg_tree *st, int node,
                    int start, int end,
                    int l, int r, long long val)
{
    if (r < start || end < l)
        return;

    if (l <= start && end <= r) {
        /* Fully covered: update this node and mark lazy */
        st->tree[node] += val * (end - start + 1);
        st->lazy[node] += val;
        return;
    }

    push_down(st, node, start, end);

    int mid = start + (end - start) / 2;
    seg_range_add(st, 2 * node,     start, mid,   l, r, val);
    seg_range_add(st, 2 * node + 1, mid + 1, end, l, r, val);
    st->tree[node] = st->tree[2 * node] + st->tree[2 * node + 1];
}

/* ─── GPU Job Scheduling Application ────────────────────────────────────
 *
 * Problem: GPU time is divided into N slots. Each GPU job occupies a
 * contiguous range of slots. Query: how much GPU time is occupied in
 * the next K slots?
 *
 * Use: segment tree for O(log n) range sum queries.
 */

struct gpu_time_tracker {
    struct seg_tree seg;
    int             num_slots;       /* total time slots                   */
    long long       slot_duration_ns; /* duration of each time slot        */
};

int gpu_time_tracker_init(struct gpu_time_tracker *tracker,
                           int num_slots, long long slot_ns)
{
    long long *init_arr;
    int i;

    if (num_slots > SEG_MAX_N)
        return -EINVAL;

    init_arr = kzalloc(sizeof(long long) * num_slots, GFP_KERNEL);
    if (!init_arr)
        return -ENOMEM;

    tracker->num_slots        = num_slots;
    tracker->slot_duration_ns = slot_ns;

    /* Initialize: all slots empty (0 GPU time used) */
    for (i = 0; i < num_slots; i++)
        init_arr[i] = 0;

    memset(&tracker->seg, 0, sizeof(tracker->seg));
    tracker->seg.n = num_slots;
    seg_build(&tracker->seg, 1, 1, num_slots, init_arr);

    kfree(init_arr);
    return 0;
}

/* Mark slots [l, r] as occupied by a GPU job */
void gpu_job_schedule(struct gpu_time_tracker *tracker,
                       int l, int r, long long weight)
{
    seg_range_add(&tracker->seg, 1, 1, tracker->num_slots, l, r, weight);
}

/* Query: total GPU work in slots [l, r] */
long long gpu_query_load(struct gpu_time_tracker *tracker, int l, int r)
{
    return seg_query(&tracker->seg, 1, 1, tracker->num_slots, l, r);
}
```

---

## Explanation

### Core Concept

```
Segment tree for N=8 time slots:

                 tree[1]  (slots 1..8)
                /                   \
        tree[2] (1..4)         tree[3] (5..8)
        /       \              /       \
   tree[4]  tree[5]      tree[6]  tree[7]
   (1..2)   (3..4)       (5..6)   (7..8)
   /   \    /   \        /   \    /   \
 [1]  [2] [3] [4]      [5] [6] [7]  [8]   ← leaves

Query [2..6]: visits 4 nodes = O(log n)
Update slot 3: visits 3 nodes (leaf→root) = O(log n)
```

### Key APIs / Macros Used

| Concept | Purpose |
|---------|---------|
| `tree[2*node]` | Left child index |
| `tree[2*node+1]` | Right child index |
| `mid = start + (end-start)/2` | Safe midpoint (avoids overflow) |
| `if (r < start || end < l) return 0` | Range mismatch: contribute 0 |
| `if (l <= start && end <= r)` | Range fully covered: return node sum |
| `lazy[node]` | Deferred range-add value |
| `push_down()` | Propagate lazy to children before recursing |

### Trade-offs & Pitfalls

- **Static N at compile time.** `SEG_MAX_N=1024` wastes memory for small trees. For variable N: allocate `4*n` dynamically. Ensure `4*n` — the `4×` factor covers all levels of an unbalanced tree.
- **1-indexed vs 0-indexed.** This implementation is 1-indexed (leaf `i` corresponds to index 1..N). Adjust `seg_build`, `seg_query`, `seg_update` if using 0-indexed arrays.

### NVIDIA / GPU Context

GPU job schedulers use segment trees for: (1) tracking which time slots are occupied across multiple GPU engines, (2) finding the earliest slot window where a job fits (binary search + range query), (3) computing utilization statistics over arbitrary time windows in O(log n) instead of O(n).

---

## Cross Questions & Answers

**CQ1: What is the time complexity of segment tree build, query, and update?**
> Build: O(n) — each node computed once from children. Query: O(log n) — visits at most 4 nodes per tree level (2 partial overlaps per level). Update: O(log n) — path from leaf to root (log n nodes). Range update with lazy: O(log n) — lazy stops propagation at covered nodes. Space: O(n) — the 4×N array stores all internal nodes and leaves.

**CQ2: What is lazy propagation and when is it necessary?**
> Without lazy: adding value `v` to range `[l,r]` updates every leaf in `[l,r]` individually — O(n) per update. With lazy: when a range `[l,r]` fully covers a node's range, store `v` in `lazy[node]` instead of propagating immediately. Only push down the lazy value when the node's children are needed (during a query or update that requires partial coverage). This amortizes updates: range updates cost O(log n) instead of O(n).

**CQ3: How would you adapt the segment tree for min/max queries instead of sum?**
> Replace the merge operation: `tree[node] = min(tree[2*node], tree[2*node+1])` (for min-tree). For queries: instead of summing children, return `min(left, right)`. Lazy propagation for range-min-update: `tree[node] = min(tree[node], val)`. Range-min updates are harder than range-add because min is not easily decomposed. For GPU job scheduling: a min-tree finds the minimum load slot in O(log n) — useful for finding the least-loaded GPU engine.

**CQ4: What is a Fenwick tree (Binary Indexed Tree) and when is it preferred over a segment tree?**
> Fenwick tree (BIT): simpler implementation, lower constant factor. Supports: (1) point update O(log n), (2) prefix sum query O(log n). Does NOT natively support range queries with range updates or non-invertible operations (min/max). Segment tree: supports all operations including range updates, range min/max queries, non-invertible operations. Use Fenwick when only prefix sums + point updates are needed (simpler, faster in practice). Use segment tree for range updates, range min/max, or 2D queries.

**CQ5: How would you extend the segment tree to 2D for GPU grid job scheduling?**
> 2D segment tree: outer tree over rows, inner tree over columns. Build: O(n×m) space and time. Query [r1,r2]×[c1,c2]: O(log n × log m). Used for GPU 2D resource allocation: rows = GPU SMs, columns = time slots. Query: how many SMs are occupied in time slots [t1,t2] and SM rows [s1,s2]. Update: allocate job to SM range × time range. Alternative: use a 1D segment tree on flattened index `row × M + col` — simpler but only for contiguous 2D ranges.

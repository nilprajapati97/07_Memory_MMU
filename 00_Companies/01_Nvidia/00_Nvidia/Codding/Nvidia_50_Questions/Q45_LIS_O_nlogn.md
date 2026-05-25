# Q45: Longest Increasing Subsequence O(n log n)

**Section:** Performance & Algorithms | **Difficulty:** Hard | **Topics:** LIS, binary search, patience sorting, O(n log n), GPU fence dependency ordering

---

## Question

Implement the Longest Increasing Subsequence algorithm in O(n log n) time, applied to GPU fence dependency resolution.

---

## Answer

```c
#include <linux/slab.h>
#include <linux/string.h>

/* ─── O(n²) LIS (naive, for understanding) ───────────────────────────────
 *
 * dp[i] = length of LIS ending at index i.
 * Time: O(n²)  Space: O(n)
 */
static int lis_naive(const int *arr, int n, int *result, int *result_len)
{
    int *dp    = kmalloc_array(n, sizeof(int), GFP_KERNEL);
    int *prev  = kmalloc_array(n, sizeof(int), GFP_KERNEL);
    int max_len = 1, max_idx = 0;
    int i, j;

    if (!dp || !prev) {
        kfree(dp); kfree(prev);
        return -ENOMEM;
    }

    for (i = 0; i < n; i++) { dp[i] = 1; prev[i] = -1; }

    for (i = 1; i < n; i++) {
        for (j = 0; j < i; j++) {
            if (arr[j] < arr[i] && dp[j] + 1 > dp[i]) {
                dp[i]   = dp[j] + 1;
                prev[i] = j;
            }
        }
        if (dp[i] > max_len) { max_len = dp[i]; max_idx = i; }
    }

    /* Reconstruct the subsequence */
    *result_len = max_len;
    for (i = max_len - 1; i >= 0; i--) {
        result[i] = arr[max_idx];
        max_idx   = prev[max_idx];
    }

    kfree(dp); kfree(prev);
    return 0;
}

/* ─── O(n log n) LIS using patience sorting ─────────────────────────────
 *
 * Key insight: maintain `tails` array where tails[i] is the smallest
 * tail element of all increasing subsequences of length i+1.
 * This array is always sorted → binary search for O(log n) per element.
 *
 * Time: O(n log n)   Space: O(n)
 */
int lis_fast(const int *arr, int n)
{
    int *tails; /* tails[i] = smallest tail of LIS of length i+1 */
    int  len = 0; /* current LIS length */
    int  i, lo, hi, mid;

    if (n == 0) return 0;

    tails = kmalloc_array(n, sizeof(int), GFP_KERNEL);
    if (!tails)
        return -ENOMEM;

    for (i = 0; i < n; i++) {
        int x = arr[i];

        /* Binary search: find leftmost position in tails where tails[pos] >= x */
        lo = 0; hi = len;
        while (lo < hi) {
            mid = lo + (hi - lo) / 2;
            if (tails[mid] < x)
                lo = mid + 1;
            else
                hi = mid;
        }

        /*
         * lo is the position to place arr[i]:
         * - if lo == len: x is larger than all tails → extend LIS by 1
         * - else: x replaces tails[lo] (better tail for LIS of length lo+1)
         */
        tails[lo] = x;
        if (lo == len)
            len++;
    }

    kfree(tails);
    return len;
}

/* ─── GPU Fence Dependency Application ───────────────────────────────────
 *
 * Problem: Given N GPU commands with fence seqnos, find the longest
 * chain of commands where each command depends only on the previous one
 * in the chain (fence seqno strictly increasing).
 *
 * This represents the critical path of the GPU workload dependency graph.
 * The LIS length = minimum number of parallel waves needed to execute all
 * commands respecting their dependencies.
 */

struct gpu_cmd_node {
    u64  seqno;         /* GPU fence seqno                         */
    u64  wait_seqno;    /* must wait for this seqno before running  */
    u32  cmd_id;
};

/*
 * Find the critical path length in a GPU command dependency graph.
 * Each command has a seqno. A command can depend on at most one prior command.
 * The dependency creates a total order: seqno[dep] < seqno[cmd].
 *
 * LIS of seqnos gives the longest dependency chain = critical path length.
 */
int gpu_critical_path_len(struct gpu_cmd_node *cmds, int n)
{
    int *seqnos;
    int  lis_len;
    int  i;

    seqnos = kmalloc_array(n, sizeof(int), GFP_KERNEL);
    if (!seqnos)
        return -ENOMEM;

    /* Extract seqnos for LIS computation */
    for (i = 0; i < n; i++)
        seqnos[i] = (int)cmds[i].seqno;

    lis_len = lis_fast(seqnos, n);
    kfree(seqnos);

    pr_debug("GPU: command chain of %d cmds has critical path of %d\n",
             n, lis_len);
    return lis_len;
}

/* ─── Reconstruct the actual LIS sequence ────────────────────────────────
 * O(n log n) with path reconstruction.
 * Requires storing the index from which each element was placed.
 */
int lis_reconstruct(const int *arr, int n, int *result, int *result_len)
{
    int *tails;      /* tails[i]: smallest tail value for LIS of length i+1 */
    int *tails_idx;  /* tails_idx[i]: index in arr of tails[i]              */
    int *parent;     /* parent[i]: index of predecessor of arr[i] in LIS    */
    int  len = 0;
    int  i, lo, hi, mid;

    if (n == 0) { *result_len = 0; return 0; }

    tails     = kmalloc_array(n, sizeof(int), GFP_KERNEL);
    tails_idx = kmalloc_array(n, sizeof(int), GFP_KERNEL);
    parent    = kmalloc_array(n, sizeof(int), GFP_KERNEL);
    if (!tails || !tails_idx || !parent) {
        kfree(tails); kfree(tails_idx); kfree(parent);
        return -ENOMEM;
    }

    for (i = 0; i < n; i++) parent[i] = -1;

    for (i = 0; i < n; i++) {
        int x = arr[i];

        lo = 0; hi = len;
        while (lo < hi) {
            mid = lo + (hi - lo) / 2;
            if (tails[mid] < x) lo = mid + 1;
            else                hi = mid;
        }

        tails[lo]     = x;
        tails_idx[lo] = i;
        parent[i]     = (lo > 0) ? tails_idx[lo - 1] : -1;

        if (lo == len) len++;
    }

    /* Reconstruct: trace back from the tail of the longest subsequence */
    *result_len = len;
    int idx = tails_idx[len - 1];
    for (i = len - 1; i >= 0; i--) {
        result[i] = arr[idx];
        idx       = parent[idx];
    }

    kfree(tails); kfree(tails_idx); kfree(parent);
    return 0;
}
```

---

## Explanation

### Core Concept

```
arr = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5]

Patience sort (tails array evolution):
i=0: x=3  → tails=[3]             len=1
i=1: x=1  → replace tails[0]:     tails=[1]             len=1
i=2: x=4  → append:               tails=[1,4]           len=2
i=3: x=1  → replace tails[0]:     tails=[1,4]           len=2
i=4: x=5  → append:               tails=[1,4,5]         len=3
i=5: x=9  → append:               tails=[1,4,5,9]       len=4
i=6: x=2  → replace tails[1]:     tails=[1,2,5,9]       len=4
i=7: x=6  → replace tails[3]:     tails=[1,2,5,6]       len=4
...
Final LIS length = 4  (e.g., [1,4,5,9] or [1,2,5,6])
```

### Key APIs / Macros Used

| Concept | Purpose |
|---------|---------|
| `tails[i]` | Smallest tail of all LIS of length `i+1` |
| Binary search `lo/hi/mid` | Find insertion position in O(log n) |
| `tails[lo] = x` | Replace or extend tails |
| `if (lo == len) len++` | Extend LIS when new element is largest |
| `parent[i]` | Predecessor index for path reconstruction |

### Trade-offs & Pitfalls

- **Reconstruction requires extra O(n) arrays.** If only the LIS length is needed (not the actual sequence), skip `tails_idx` and `parent` arrays — saves memory and removes bookkeeping overhead.
- **Strictly increasing vs non-decreasing.** The binary search uses `tails[mid] < x` (strict) → strict LIS. For non-decreasing: use `tails[mid] <= x`.

### NVIDIA / GPU Context

GPU fence dependency resolution: CUDA stream dependencies form a DAG. Finding the critical path (longest dependency chain) determines the minimum parallel execution time. The LIS algorithm is used in NVIDIA's Multi-Process Service (MPS) to schedule GPU time slices: find the longest serialized command chain to determine the minimum required time slice.

---

## Cross Questions & Answers

**CQ1: Why does the `tails` array always remain sorted?**
> Proof by induction: Initially empty (trivially sorted). When we place `x` at `tails[lo]` (the leftmost position where `tails[lo] >= x`): (1) if `lo > 0`: `tails[lo-1] < x` (binary search guarantee), (2) if `lo < len`: `tails[lo+1]` was previously `>= old_tails[lo] >= x` (since we replaced with a smaller or equal value). So `tails[lo-1] < x <= tails[lo+1]` — sorted order maintained.

**CQ2: What is the time complexity of the O(n log n) LIS and how is binary search justified?**
> For each of n elements, we perform one binary search over `tails` (max size n). Binary search is O(log n). Total: O(n log n). The binary search is correct because `tails` is always sorted (see CQ1). We search for the leftmost position where `tails[pos] >= arr[i]` — this is the standard "lower bound" binary search pattern. Lower bound guarantees we find the correct position to maintain the "smallest possible tail" invariant.

**CQ3: How does LIS relate to the minimum number of decreasing subsequences (Dilworth's theorem)?**
> Dilworth's theorem: the minimum number of chains needed to cover a partially ordered set equals the size of the largest antichain. For sequences: (1) **LIS length** = minimum number of strictly decreasing subsequences needed to partition the array (Dilworth), (2) conversely, the minimum number of increasing subsequences = the length of the longest strictly decreasing subsequence. Application: GPU fence scheduling — if LIS=4, you need at least 4 sequential pipeline stages. Parallelizing to reduce to fewer stages requires breaking the dependencies.

**CQ4: How would you extend LIS to find the longest increasing subsequence with a given target value as the end element?**
> After `lis_reconstruct`, the `parent` array encodes the full sequence. To find the LIS ending at a specific element with value `v`: search for `v` in `arr`, get its index `i`, then trace `parent[i]` backwards. Alternatively, for a target seqno in GPU fence scheduling: run the full reconstruction, filter the result to sequences ending at the desired seqno. For real-time use in GPU scheduling: maintain a separate `dp[]` array (O(n²)) and query `dp[target_idx]` in O(1).

**CQ5: What is the "patience sorting" algorithm and how does it relate to LIS?**
> Patience sorting: deal cards into piles where each card can only go on a face-up card with an equal or higher value. Number of piles = LIS length. The top card of each pile forms the `tails` array. To find where to place a new card: binary search for the leftmost pile whose top >= new card, place on it (reducing the pile's "cost"). The insight: each pile corresponds to a "slot" in the tails array. The algorithm is named after the solitaire card game "Patience" — the same process produces both optimal piles and the LIS.

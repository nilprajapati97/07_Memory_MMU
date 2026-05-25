# Q47: Dijkstra for PCIe Topology Pathfinding

**Section:** Performance & Algorithms | **Difficulty:** Hard | **Topics:** Dijkstra's algorithm, PCIe topology, GPU-to-GPU DMA routing, shortest path, priority queue, min-heap

---

## Question

Implement Dijkstra's algorithm to find the lowest-latency path in a PCIe topology for GPU-to-GPU P2P DMA routing.

---

## Answer

```c
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/limits.h>

/* ─── PCIe Topology Graph ─────────────────────────────────────────────────
 *
 *  Node types: GPU, PCIe Switch, Root Complex, CPU NUMA node
 *  Edge weights: latency in nanoseconds
 *
 *  Example topology (8 nodes):
 *  Node 0: CPU NUMA0 (Root Complex)
 *  Node 1: CPU NUMA1 (Root Complex)
 *  Node 2: PCIe Switch 0 (under NUMA0)
 *  Node 3: PCIe Switch 1 (under NUMA0)
 *  Node 4: GPU0 (under Switch 0)
 *  Node 5: GPU1 (under Switch 0)
 *  Node 6: GPU2 (under Switch 1)
 *  Node 7: GPU3 (under Switch 1)
 *
 *  Edges (latencies in ns):
 *  NUMA0 ↔ Switch0: 50ns  NUMA0 ↔ Switch1: 50ns
 *  Switch0 ↔ GPU0: 100ns  Switch0 ↔ GPU1: 100ns
 *  Switch1 ↔ GPU2: 100ns  Switch1 ↔ GPU3: 100ns
 *  Switch0 ↔ Switch1: 30ns (same root complex)
 *  NUMA0 ↔ NUMA1: 200ns (cross-socket QPI/UPI)
 */

#define PCIE_MAX_NODES  64
#define INF             U64_MAX

struct pcie_topology {
    u64  dist[PCIE_MAX_NODES][PCIE_MAX_NODES]; /* adjacency matrix; INF=no edge */
    int  n;                                     /* number of nodes               */
};

/* ─── Simple min-heap for Dijkstra ──────────────────────────────────────
 * Priority queue: (node, distance) pairs, min by distance.
 */
struct heap_entry {
    u64 dist;
    int node;
};

struct min_heap {
    struct heap_entry data[PCIE_MAX_NODES * PCIE_MAX_NODES];
    int size;
};

static void heap_push(struct min_heap *h, u64 d, int node)
{
    int i = h->size++;
    h->data[i].dist = d;
    h->data[i].node = node;

    /* Sift up */
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->data[parent].dist <= h->data[i].dist) break;
        /* Swap */
        struct heap_entry tmp = h->data[parent];
        h->data[parent]       = h->data[i];
        h->data[i]            = tmp;
        i = parent;
    }
}

static struct heap_entry heap_pop(struct min_heap *h)
{
    struct heap_entry top = h->data[0];
    h->data[0] = h->data[--h->size];

    /* Sift down */
    int i = 0;
    while (1) {
        int l = 2 * i + 1, r = 2 * i + 2, smallest = i;
        if (l < h->size && h->data[l].dist < h->data[smallest].dist) smallest = l;
        if (r < h->size && h->data[r].dist < h->data[smallest].dist) smallest = r;
        if (smallest == i) break;
        struct heap_entry tmp = h->data[i];
        h->data[i]            = h->data[smallest];
        h->data[smallest]     = tmp;
        i = smallest;
    }
    return top;
}

/* ─── Dijkstra's Algorithm ───────────────────────────────────────────────
 * Finds shortest (lowest-latency) paths from src to all other nodes.
 *
 * Time:  O((V + E) log V)  where V=nodes, E=edges
 * Space: O(V²) for adjacency matrix + O(V) for dist/prev arrays
 *
 * Returns: dist[] array (shortest distances from src)
 *          prev[] array (predecessor for path reconstruction)
 */
int dijkstra(const struct pcie_topology *topo, int src,
              u64 *dist_out, int *prev)
{
    u64        dist[PCIE_MAX_NODES];
    bool       visited[PCIE_MAX_NODES];
    struct min_heap *heap;
    int n = topo->n;
    int i, v;

    heap = kzalloc(sizeof(*heap), GFP_KERNEL);
    if (!heap)
        return -ENOMEM;

    /* Initialize distances to infinity */
    for (i = 0; i < n; i++) {
        dist[i]    = INF;
        prev[i]    = -1;
        visited[i] = false;
    }
    dist[src] = 0;

    heap_push(heap, 0, src);

    while (heap->size > 0) {
        struct heap_entry e = heap_pop(heap);
        u = e.node;

        if (visited[u]) continue; /* stale heap entry */
        visited[u] = true;

        /* Relax all edges from u */
        for (v = 0; v < n; v++) {
            u64 edge = topo->dist[u][v];
            if (edge == INF)  continue; /* no edge */
            if (visited[v])   continue;

            u64 new_dist = dist[u] + edge;
            if (new_dist < dist[v]) {
                dist[v] = new_dist;
                prev[v] = u;
                heap_push(heap, new_dist, v);
            }
        }
    }

    /* Copy results */
    for (i = 0; i < n; i++)
        dist_out[i] = dist[i];

    kfree(heap);
    return 0;
}

/* ─── Reconstruct path from src to dst ───────────────────────────────────*/
int reconstruct_path(const int *prev, int src, int dst,
                      int *path, int *path_len)
{
    int stack[PCIE_MAX_NODES];
    int top = 0, cur = dst;

    while (cur != -1) {
        stack[top++] = cur;
        cur = prev[cur];
    }

    if (stack[top - 1] != src)
        return -1; /* no path */

    *path_len = top;
    for (int i = 0; i < top; i++)
        path[i] = stack[top - 1 - i];

    return 0;
}

/* ─── GPU P2P Route Selection ────────────────────────────────────────────
 * Find the lowest-latency route between two GPUs for P2P DMA.
 */
void gpu_find_p2p_route(const struct pcie_topology *topo,
                          int gpu_src_node, int gpu_dst_node)
{
    u64 dist[PCIE_MAX_NODES];
    int prev[PCIE_MAX_NODES];
    int path[PCIE_MAX_NODES];
    int path_len;

    if (dijkstra(topo, gpu_src_node, dist, prev)) {
        pr_err("Dijkstra: OOM\n");
        return;
    }

    reconstruct_path(prev, gpu_src_node, gpu_dst_node, path, &path_len);

    pr_info("P2P route: latency=%lluns, path_len=%d hops\n",
            dist[gpu_dst_node], path_len - 1);

    for (int i = 0; i < path_len; i++)
        pr_info("  hop %d: node %d\n", i, path[i]);
}
```

---

## Explanation

### Core Concept

```
PCIe Topology (example 8 nodes):

      NUMA0 ──200ns── NUMA1
       │                │
      50ns             50ns
       │                │
    Switch0 ──30ns── Switch1
    /     \          /     \
100ns    100ns    100ns   100ns
GPU0     GPU1    GPU2    GPU3

GPU0→GPU2 via Dijkstra:
  Path: GPU0 → Switch0 → Switch1 → GPU2
  Latency: 100 + 30 + 100 = 230ns  ✓ (shortest)

vs GPU0 → Switch0 → NUMA0 → Switch1 → GPU2:
  Latency: 100 + 50 + 50 + 100 = 300ns  (longer)
```

### Key APIs / Macros Used

| Concept | Purpose |
|---------|---------|
| `dist[u][v] = INF` | No direct edge between u and v |
| `dist[src] = 0` | Source has zero distance to itself |
| `heap_push(heap, new_dist, v)` | Add node to priority queue |
| `if (visited[u]) continue` | Skip stale heap entries |
| `dist[v] = dist[u] + edge` | Relax edge (update if shorter path found) |
| `prev[v] = u` | Record predecessor for path reconstruction |

### Trade-offs & Pitfalls

- **Adjacency matrix O(V²) space.** For 64 nodes: `64×64×8 bytes = 32KB` — fits in L1 cache. For larger topologies (hundreds of nodes), use adjacency lists to reduce space.
- **Stale heap entries.** This implementation uses a lazy deletion approach — push updated distances into the heap without removing old entries. `if (visited[u]) continue` discards stale entries. This is simpler than a decrease-key heap but may have up to O(E) extra heap entries.

### NVIDIA / GPU Context

NVIDIA's fabric manager (for NVSwitch systems) runs topology discovery to build the PCIe/NVLink graph and then uses Dijkstra to compute optimal routes for all GPU pairs. Routes are stored in routing tables in the NVSwitch hardware. For multi-hop NVLink topologies, the shortest path determines which NVSwitch ports to configure.

---

## Cross Questions & Answers

**CQ1: What is the time complexity of Dijkstra with a binary heap vs Fibonacci heap?**
> Binary heap (this implementation): O((V + E) log V). Each vertex is extracted once (V operations), each edge may update the heap (E operations), each heap operation is O(log V). Fibonacci heap: O(V log V + E) amortized — `decrease-key` is O(1) amortized. For PCIe topology (V=64, E=128, sparse): binary heap is fast enough and simpler to implement. Fibonacci heap advantages only appear for dense graphs (E ≫ V log V) or when decrease-key is the bottleneck.

**CQ2: Why does Dijkstra fail for negative edge weights?**
> Dijkstra's correctness proof relies on: "once a node is visited (finalized), its shortest distance is correct." This holds only if all edge weights are non-negative. With negative edges: a later-discovered path through a negative edge could yield a shorter total distance to an already-finalized node. For PCIe topology: all latencies are positive — Dijkstra is correct. For general graphs with negative edges: use Bellman-Ford (O(V×E)) which relaxes all edges V-1 times.

**CQ3: How would you handle dynamic topology changes (GPU hot-plug) in the PCIe topology?**
> When a GPU or switch is removed: (1) update the adjacency matrix (set removed edges to INF), (2) re-run Dijkstra from all active GPU nodes, (3) update routing tables in NVSwitch hardware. For hot-add: same process. Optimization: instead of recomputing all-pairs shortest paths, recompute only paths affected by the changed edges (incremental Dijkstra). For NVIDIA's fabric manager: topology changes trigger a fabric reconfiguration event which re-runs route computation and reprograms switch routing tables.

**CQ4: What is the all-pairs shortest path problem and how is it solved?**
> All-pairs shortest path: compute shortest paths between ALL pairs of nodes (not just from one source). Floyd-Warshall: O(V³), works for negative weights (no negative cycles), uses DP: `dp[i][j][k] = min path from i to j using intermediate nodes 1..k`. For PCIe topology (64 nodes): Floyd-Warshall requires 64³ = 262,144 operations — fast. Used when we need to pre-compute all GPU-to-GPU route tables at initialization time. Dijkstra run from each node: O(V × (V+E) log V) — may be faster for sparse graphs.

**CQ5: How does NVIDIA NVSwitch compute and update routing tables?**
> NVSwitch contains a routing table with entries for each GPU pair: `route[src_gpu][dst_gpu] = (output_port, output_channel)`. Routes are computed by the fabric manager (software) using Dijkstra/ECMP on the NVLink topology. The fabric manager writes routes to NVSwitch registers via PCIe MMIO. NVSwitch hardware uses source routing (the sender specifies the route in the packet header) — packets carry a routing header that encodes the sequence of NVSwitch ports to traverse. The route header is set by the GPU driver when initiating a P2P DMA.

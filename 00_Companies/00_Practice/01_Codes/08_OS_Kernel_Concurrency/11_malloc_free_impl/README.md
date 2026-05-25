# 11 — Custom `malloc`/`free` Implementation

## Problem
Design a heap allocator. Hand back contiguous memory ranges; reclaim on `free`; minimise fragmentation; meet alignment.

## Why It Matters
Every embedded firmware / RTOS / kernel either uses or implements one. Tests pointer arithmetic, alignment, free-list manipulation, and trade-off thinking.

## Approaches

### Approach 1 — Bump (Linear / Arena) Allocator
A pointer walks forward through a big block; `free` is a no-op (or only frees everything at once).
```text
init: cur = base
malloc(n):
    p = align_up(cur, ALIGN)
    if p + n > end: return NULL
    cur = p + n
    return p
free(p): /* no-op */
reset(): cur = base
```
- Pros: tiny code, near-zero overhead, perfectly fast.
- Cons: no per-allocation free. Excellent for per-frame / per-request arenas.

### Approach 2 — Free List (Implicit / Explicit)
Each block stores a header (`size`, `is_free`, `prev`, `next`). `malloc` scans the list for a fit; `free` flips the flag and coalesces neighbours.
```text
header = { size, free?, prev, next }
malloc(n):
    walk free list; find block with size >= n
    if found: split if remainder > min, mark used, return user pointer
    else: sbrk/mmap more
free(p):
    h = (header*)(p - sizeof(header))
    h.free = true
    coalesce(h, prev); coalesce(h, next)
```
- Fits: first-fit, next-fit, best-fit, worst-fit.
- O(n) malloc in worst case; coalescing reduces external fragmentation.
- Classic K&R allocator uses this.

### Approach 3 — Segregated Free Lists (Size Classes)
N lists, each holding blocks of a power-of-two size class. `malloc(n)` → round up to class → pop. `free` → push onto class list.
- O(1) malloc/free.
- Internal fragmentation (rounded up).
- Used by ptmalloc / tcmalloc / jemalloc in fast path.

### Approach 4 — Buddy Allocator
Maintain free lists for each power-of-two size. To allocate size `2^k`: split larger buddies down to `2^k`. To free: coalesce with sibling if it's free (its address is computed by XOR with size).
```text
free buddy of block B at offset O, size 2^k:
    buddy_addr = O XOR (1 << k)
    if buddy is free and same size: merge, k++, repeat
```
- O(log N) operations.
- Internal fragmentation up to 2× (round up to next power of two).
- Linux kernel page allocator.

### Approach 5 — Slab Allocator
For fixed-size objects (kernel inodes, sockets): pre-allocate a "slab" of N fixed-size slots; `malloc` pops, `free` pushes.
- O(1), zero fragmentation per slab.
- Caches initialised state (constructors).
- Linux SLAB/SLUB/SLOB for kmem_cache.

### Approach 6 — TLAB / Thread-Local Caches
Each thread has a small per-thread cache; refill from central heap. Eliminates lock contention on common path.
- jemalloc / tcmalloc / ptmalloc arenas use this.

### Approach 7 — Region / Pool with Generation
Allocate in regions; bulk-free regions. Used for compilers (per-pass) and games (per-frame).

## Comparison
| Allocator | malloc | free | Fragmentation | Use case |
|---|---|---|---|---|
| Bump / arena | O(1) | n/a | none until reset | per-request / per-frame |
| Free list | O(n) | O(1)+coalesce | external | classic general |
| Segregated lists | O(1) | O(1) | internal (rounding) | high-throughput general |
| Buddy | O(log) | O(log) | up to 2× internal | OS page allocator |
| Slab / pool | O(1) | O(1) | minimal | fixed-size objects |
| TLAB | O(1) | O(1) | per-thread cache | multithreaded general |

## Block Header Layout (Free-List)
```
+--------+--------+--------+--------+----------+
| size   | flags  | prev   | next   | payload..|
+--------+--------+--------+--------+----------+
                  ^ user pointer returned here for malloc(n)
```
- Always round payload start to `alignof(max_align_t)` (commonly 8 or 16 bytes).
- "Boundary tags" duplicate `size` at the **end** of a block too, so coalescing with the **previous** block in memory is O(1).

## Key Insight
- Speed and fragmentation trade off via **size classes** and **per-thread caches**.
- Coalescing is what keeps a free-list allocator alive long-term; without it you fragment to death.
- Real allocators (jemalloc, tcmalloc) layer: thread cache → size-class central heap → page-level allocator (buddy/mmap).

## Pitfalls
- Returning unaligned pointers → `double`/SIMD load crashes
- Double-free → corrupts the free list; often detected only later
- Use-after-free → silent until the block is reallocated
- Off-by-one in header arithmetic — `p - sizeof(header)` vs `p + sizeof(header)`
- Heap metadata adjacent to user buffer → buffer overflow corrupts allocator
- Forgetting to handle `malloc(0)` (return NULL or unique pointer — implementation choice; be consistent)
- Free-list ABA in lock-free allocators — needs tagged pointers
- `mmap`-vs-`sbrk` thresholds — large allocations often go straight to `mmap` so they can be returned to OS on free

## Interview Tips
1. Clarify scope: bump vs general? single-thread vs multi? fragmentation important?
2. Lead with a simple **bump** allocator (always correct, 5 lines).
3. Upgrade to **free list with coalescing**; mention boundary tags for O(1) backward coalesce.
4. Mention segregated lists / buddy / slab and cite which real systems use them (Linux: buddy + slab; glibc: ptmalloc; Google: tcmalloc; Facebook: jemalloc).
5. Volunteer the alignment requirement (`alignof(max_align_t)`).

## Related / Follow-ups
- [01_stack_vs_heap](../../07_Memory_Storage/01_stack_vs_heap/)
- [06_struct_padding](../../07_Memory_Storage/06_struct_padding/) — alignment
- Doug Lea's `dlmalloc` paper; ptmalloc, tcmalloc, jemalloc design docs
- Linux kernel SLAB/SLUB/SLOB, buddy allocator
- Valgrind / AddressSanitizer for catching heap errors

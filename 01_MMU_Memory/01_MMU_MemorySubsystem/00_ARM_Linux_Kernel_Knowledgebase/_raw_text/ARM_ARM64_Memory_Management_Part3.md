ARM/ARM64 Linux Kernel
Memory Management

Part 3: SLUB Allocator & Memory Compaction / CMA
| Complete Technical Reference for Kernel DevelopersARM32 | ARM64 (AArch64) | Qualcomm SoC | Linux Kernel 6.x |


| SLUB Allocator Internals | Per-CPU Fast Path | Freelist Inside Objects | Full Alloc/Free Flows |


| NUMA Support | SLUB Debugging | Memory Compaction | Migration Types |


| CMA Architecture | Device Tree Setup | CMA vs Carveout | Camera ISP Driver Example |


# Table of Contents
	1.	SLUB Allocator Internals	3
	1.1	Why SLUB Exists — The Problem It Solves	3
	1.2	Core Concepts: The kmem_cache Structure	3
	1.3	SLUB Architecture — Three-Level Design	4
	1.4	Per-CPU Slab: The Fast Path (with Pseudocode)	5
	1.5	The Free Pointer — Freelist Inside Objects	5
	1.6	Full Allocation Flow — Step by Step	6
	1.7	Full Free Flow — Step by Step	7
	1.8	SLUB vs SLAB vs SLOB Comparison	7
	1.9	SLUB Debugging: Poison, Red Zones, Hardening	8
	1.10	NUMA Support in SLUB	9
	1.11	Object Constructors / Destructors	9
	1.12	Important kmem_cache Flags	10
	1.13	SLUB Lifecycle Diagram	10
	1.14	Driver Patterns for ARM64 / Qualcomm SoC	11
	2.	Memory Compaction & CMA	12
	2.1	The Fragmentation Problem	12
	2.2	Migration Types	13
	2.3	Two-Scanner Compaction Algorithm	13
	2.4	CMA Architecture	14
	2.5	Device Tree Setup: reusable vs no-map	15
	2.6	CMA APIs: Kernel Configuration & Driver API	15
	2.7	CMA Allocation Flow Internals	16
	2.8	CMA vs Carveout vs Compaction Comparison	17
	2.9	Transparent Huge Pages (THP)	17
	2.10	Debugging CMA and Compaction	18
	2.11	Complete Qualcomm Camera ISP Driver Example	18
# 1. SLUB Allocator Internals
The SLUB allocator is the default slab allocator in the Linux kernel (since ~2.6.23), replacing the original SLAB and the simpler SLOB. It manages sub-page memory allocations efficiently by grouping same-sized objects into slabs (pages or groups of pages), with heavy optimization for multi-core systems.
## 1.1 Why SLUB Exists — The Problem It Solves
The buddy allocator works at page granularity (minimum 4 KB). But the kernel constantly allocates tiny objects:
- struct task_struct (~7 KB on ARM64)
- struct inode (~600 bytes)
- struct sk_buff (~240 bytes)
- kmalloc(64) calls from drivers

Without a sub-page allocator, every 64-byte allocation would waste 4032 bytes of a 4096-byte page. SLUB solves this by packing many objects into a single page.
## 1.2 Core Concepts: The kmem_cache Structure
Every type of object has a kmem_cache (also called a "cache" or "slab cache"). This is the heart of SLUB:
struct kmem_cache {
    struct kmem_cache_cpu __percpu *cpu_slab;  // Per-CPU fast path
    unsigned int size;          // Object size (with metadata)
    unsigned int object_size;   // Actual object size
    unsigned int offset;        // Free pointer offset within object
    unsigned int min_partial;   // Min partial slabs to keep
    int refcount;
    struct list_head list;      // All caches linked here
    struct kmem_cache_node *node[MAX_NUMNODES];  // Per-NUMA-node
    // ...
};

The kernel pre-creates caches for kmalloc sizes (8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 bytes). Drivers create their own with kmem_cache_create().
## 1.3 SLUB Architecture — Three-Level Design
SLUB organizes memory management into three hierarchical levels, each with different locking characteristics:

kmem_cache
|
|-- cpu_slab (per-CPU)            <-- LEVEL 1: FAST PATH (no locking)
|   |-- freelist               <-- pointer to next free object
|   +-- page *                 <-- the active slab page being used
|
+-- node[N] (per-NUMA node)    <-- LEVEL 2/3: SLOW PATH
    |-- partial list           <-- pages: some free + some used
    +-- (full pages tracked implicitly via page flags)

### Level 1: Per-CPU Slab (Fastest Path)
Each CPU has its own active slab page and a freelist — a linked list of free objects within that page. Allocation requires NO locking at all:
+------------------------------------------------------+
| [obj0] [obj1] [obj2] [obj3] ... [obj62]              |
|   ^                                                  |
|  freelist -> obj0 -> obj3 -> obj7 -> obj15 -> NULL   |
|  (free objects form a singly-linked list via         |
|   a pointer stored inside the free object itself)    |
+------------------------------------------------------+
Example: 4 KB page, 64-byte objects → 63 objects per page.
### Level 2: Partial List (Node Level)
When the per-CPU slab page is full (no free objects), SLUB moves the full page off the per-CPU slot and grabs a partial page from the node's partial list:
node->partial list:
  [page: 20/63 free] -> [page: 5/63 free] -> [page: 45/63 free] -> NULL
### Level 3: New Page from Buddy Allocator
When the partial list is also empty, SLUB requests a new page from the buddy allocator (alloc_pages()), initializes all objects as free, and makes it the active per-CPU slab.
## 1.4 Per-CPU Slab: The Fast Path (with Pseudocode)
The per-CPU fast path is the key performance innovation of SLUB. Allocation pseudocode:
// Pseudocode: kmalloc(64, GFP_KERNEL) FAST PATH
object = cpu_slab->freelist;          // grab head of freelist
cpu_slab->freelist = object->next;    // advance freelist pointer
return object;                        // done -- no lock needed!

Free path pseudocode (back to per-CPU slab):
// kfree(ptr) FAST PATH - ptr belongs to this CPU's active slab
object->next = cpu_slab->freelist;    // link into freelist
cpu_slab->freelist = object;          // update freelist head
// Done -- no lock needed!

| ⚡ Performance: The fast path is essentially two pointer operations with zero locking. |
| kmalloc/kfree in the fast path takes ~10-20 ns vs ~100-200 ns for the slow path. |
| This makes SLUB allocations nearly as fast as stack allocations for small objects. |

## 1.5 The Free Pointer — Freelist Inside Objects
SLUB stores the next free object pointer INSIDE the free object itself (at a fixed offset). This is elegant — no separate metadata array needed:

Free object layout (object_size = 64 bytes, offset = 0):

Bytes 0-7:   [ ptr to next free object ]  <- used by SLUB when FREE
Bytes 8-63:  [ ... unused ...            ]

Allocated object layout:
Bytes 0-63:  [ user data                 ]  <- SLUB does NOT touch

When an object is allocated, the user owns all bytes. When freed, SLUB writes the next-free pointer into the first 8 bytes (or at the configured "offset" if the object has a constructor that needs byte 0 preserved).

With CONFIG_SLAB_FREELIST_HARDENED=y, the free pointer is XOR-encoded:
// Stored as: ptr XOR secret XOR address_of_slot
// Prevents heap grooming attacks that overwrite freelist pointers
## 1.6 Full Allocation Flow — Step by Step
Complete path for kmalloc(64, GFP_KERNEL):

kmalloc(64, GFP_KERNEL)
|
|-- [FAST PATH] Check per-CPU freelist
|   |-- freelist != NULL -> pop object, return  [~ns, no lock]
|   +-- freelist == NULL -> slow path
|
|-- [SLOW PATH] Refill per-CPU slab
|   |-- Lock node's partial list
|   |-- Partial page available?
|   |   |-- YES -> move to per-CPU, unlock, retry fast path
|   |   +-- NO  -> allocate new page from buddy allocator
|   |             initialize all objects as free (obj->next chain)
|   |             set as per-CPU active slab
|   |             unlock, retry fast path
|   +-- Buddy allocator failed -> return NULL (OOM)
|
+-- Return allocated object pointer to caller
## 1.7 Full Free Flow — Step by Step

kfree(ptr)
|
|-- Find slab page: page = virt_to_head_page(ptr)
|
|-- [FAST PATH] ptr belongs to THIS CPU's active slab?
|   |-- YES -> push ptr onto per-CPU freelist  [no lock]
|   +-- NO  -> slow path
|
|-- [SLOW PATH] ptr belongs to different CPU or partial slab
|   |-- Lock the page's freelist (cmpxchg on ARM64)
|   |-- Add ptr to page's freelist
|   |-- Was page FULL? -> move to node partial list
|   |-- Is page now COMPLETELY EMPTY?
|   |   |-- partial list > min_partial -> return to buddy allocator
|   |   +-- else -> keep in partial list (avoid thrashing)
|   +-- Unlock

| ⚠️ min_partial: SLUB keeps a minimum number of empty slabs in the partial list |
| to avoid allocating/freeing pages repeatedly when workload oscillates. |
| Default: 5. Configurable via /sys/kernel/slab/<cache>/min_partial |

## 1.8 SLUB vs SLAB vs SLOB Comparison
Linux has three slab allocator implementations. SLUB is the default since kernel 2.6.23:

| Feature | SLOB | SLAB (legacy) | SLUB (default) |
| Target system | Tiny embedded | General purpose | General purpose (modern) |
| Complexity | Minimal | High | Medium |
| Per-CPU optimization | No | Yes (complex queues) | Yes (simpler, better) |
| NUMA awareness | No | Yes | Yes |
| Debugging support | Minimal | Good | Excellent |
| Memory overhead | Lowest | High | Low |
| Fragmentation | High | Low | Low |
| Default since | Never (opt-in) | Pre-2.6.23 | 2.6.23+ (current) |
| Config option | CONFIG_SLOB | CONFIG_SLAB | CONFIG_SLUB (default) |


## 1.9 SLUB Debugging: Poison, Red Zones, Hardening
SLUB has excellent built-in debugging capabilities enabled with CONFIG_SLUB_DEBUG=y:

### Poison Patterns
Free objects are filled with 0x6b ("kk" pattern). If you see 0x6b6b6b6b in a crash dump, you are reading a freed object — use-after-free bug.
// Poison patterns:
Free object:      [6b 6b 6b 6b 6b 6b 6b 6b ...]  // "free poison"
Allocated:        [5a 5a 5a 5a 5a 5a 5a 5a ...]  // "alloc poison"
Padding / end:    [5a] bytes beyond object_size

With SLAB_POISON flag on your cache:
// After kmem_cache_free() -- memory filled with 0x6b
// Before kmem_cache_alloc() returns -- memory filled with 0x5a
// If user reads 0x6b6b... -> use-after-free!
// If user reads 0xa5a5... -> uninitialized memory!

### Red Zones (Buffer Overflow Detection)
SLUB adds red zone bytes before and after each object (with SLAB_RED_ZONE). If these are overwritten, SLUB detects a buffer overflow:
[RED_ZONE 0xbb...][  object data (user bytes)  ][RED_ZONE 0xbb...][free_ptr]

When the object is freed, SLUB validates red zones. Corruption triggers a kernel warning with the exact cache name, object address, and corrupted bytes.

### Freelist Pointer Hardening
With CONFIG_SLAB_FREELIST_HARDENED=y (default on modern kernels), free pointers are XOR-encoded to prevent heap manipulation attacks:
// Stored free pointer:
encoded = ptr XOR secret XOR address_of_freelist_slot
// Attacker cannot predict the encoded value without knowing "secret"

### Runtime SLUB Validation Commands
# Validate all slab caches
echo 1 > /sys/kernel/slab/<cache_name>/validate

# Check for memory leaks in a cache
cat /sys/kernel/slab/<cache_name>/alloc_calls
cat /sys/kernel/slab/<cache_name>/free_calls

# Summary: slabtop shows real-time slab usage
slabtop
cat /proc/slabinfo
## 1.10 NUMA Support in SLUB
On NUMA systems (multiple memory nodes), SLUB maintains per-node partial lists to minimize cross-node memory access latency:

kmem_cache
|-- cpu_slab[CPU0] -> active page on Node 0 (fast, local)
|-- cpu_slab[CPU1] -> active page on Node 0 (fast, local)
|-- cpu_slab[CPU2] -> active page on Node 1 (fast, local)
|-- node[0]->partial -> pages from Node 0 memory
+-- node[1]->partial -> pages from Node 1 memory

Allocations prefer memory local to the current CPU's NUMA node. The SLUB allocator calls alloc_pages_node() to ensure the new slab page comes from the correct node.

NUMA-aware kmalloc variant:
// Allocate from specific NUMA node
void *ptr = kmalloc_node(size, GFP_KERNEL, node_id);
void *ptr = vmalloc_node(size, node_id);
## 1.11 Object Constructors / Destructors
kmem_cache_create() accepts an optional constructor function that is called once per object when the slab page is first populated — NOT on every kmem_cache_alloc():

void my_ctor(void *obj) {
    struct my_obj *p = obj;
    mutex_init(&p->lock);       // Init ONCE, reused across alloc/free
    INIT_LIST_HEAD(&p->list);   // List head stays valid
    atomic_set(&p->refcount, 0);
}

struct kmem_cache *cache = kmem_cache_create(
    "my_cache",
    sizeof(struct my_obj),
    0,                  // alignment (0 = natural)
    SLAB_HWCACHE_ALIGN, // flags
    my_ctor             // constructor
);

| ⚡ Constructor Called ONCE: The constructor runs when the slab page is first populated. |
| Objects retain initialized state between alloc/free cycles. |
| Mutexes stay initialized, list heads stay valid -- no re-initialization overhead. |
| This is a major performance win for objects with expensive initialization (e.g., inodes). |

## 1.12 Important kmem_cache Flags

| Flag | Effect |
| SLAB_HWCACHE_ALIGN | Align objects to CPU cache line (prevents false sharing, improves performance) |
| SLAB_POISON | Fill with 0x6b on free, 0x5a on alloc (use-after-free and uninit detection) |
| SLAB_RED_ZONE | Add red zones around objects (buffer overflow detection) |
| SLAB_PANIC | Panic if cache creation fails (for critical caches like task_struct) |
| SLAB_ACCOUNT | Account memory to kmemcg (cgroup memory controller tracking) |
| SLAB_TYPESAFE_BY_RCU | Safe for RCU-protected lookups; page not freed while RCU readers exist |
| SLAB_RECLAIM_ACCOUNT | Objects are reclaimable (e.g., inode cache); counted in /proc/meminfo reclaimable |
| SLAB_CONSISTENCY_CHECKS | Enable expensive consistency checking (for debugging, debug kernels only) |


## 1.13 SLUB Lifecycle Diagram
Complete lifecycle of a kmem_cache from creation to destruction:

kmem_cache_create("my_cache", 64, ...)
|
|  Creates kmem_cache struct
|  Registers with /sys/kernel/slab/my_cache
|  Sets up per-CPU and per-node structures
|
v
kmem_cache_alloc(cache, GFP_KERNEL)  <-- First call
|
|  Per-CPU freelist empty, node partial list empty
|  -> alloc_pages(GFP_KERNEL, order) from buddy allocator
|  -> [ctor called if provided] Initialize all N objects as free
|  -> Link objects as free chain: obj0->obj1->...->objN->NULL
|  -> Set page as per-CPU active slab
|  -> Pop obj0 from freelist
|
v
[Object returned to caller -- caller uses it]
|
v
kmem_cache_free(cache, obj)
|
|  obj belongs to this CPU's active slab?
|  YES -> push onto per-CPU freelist (no lock)
|
v
kmem_cache_alloc(cache, GFP_KERNEL)  <-- Second call
|
|  Per-CPU freelist has obj -> pop and return IMMEDIATELY
|  Object reused -- constructor NOT called again!
|
v
kmem_cache_destroy(cache)  <-- Module exit
|
|  All objects must be freed first (warns if any are still allocated)
|  Returns all slab pages to buddy allocator
+- Unregisters from /sys/kernel/slab/

### Runtime Information Commands
# All slab caches: name, active objects, total, object size
cat /proc/slabinfo

# Per-cache detailed stats
cat /sys/kernel/slab/<cache_name>/objects
cat /sys/kernel/slab/<cache_name>/slabs
cat /sys/kernel/slab/<cache_name>/partial
cat /sys/kernel/slab/<cache_name>/object_size

# Real-time slab memory usage
slabtop  # top-like display of slab caches

## 1.14 Driver Patterns for ARM64 / Qualcomm SoC
Two common patterns from real-world kernel driver development:

### Pattern 1: Per-Device Private Data Cache
static struct kmem_cache *my_dev_cache;

static int __init my_driver_init(void) {
    my_dev_cache = kmem_cache_create("my_dev_priv",
        sizeof(struct my_device_priv),
        0, SLAB_HWCACHE_ALIGN, NULL);
    return my_dev_cache ? 0 : -ENOMEM;
}

static int my_probe(struct platform_device *pdev) {
    struct my_device_priv *priv;
    priv = kmem_cache_alloc(my_dev_cache, GFP_KERNEL);
    if (!priv)
        return -ENOMEM;
    platform_set_drvdata(pdev, priv);
    // ... initialize priv fields ...
    return 0;
}

static int my_remove(struct platform_device *pdev) {
    struct my_device_priv *priv = platform_get_drvdata(pdev);
    kmem_cache_free(my_dev_cache, priv);
    return 0;
}

static void __exit my_driver_exit(void) {
    kmem_cache_destroy(my_dev_cache);
}

### Pattern 2: High-Frequency Transfer Descriptor Cache
For frequently allocated structs in hot paths (e.g., I2C/SPI message descriptors, USB URBs):
// Instead of kmalloc() in the interrupt/transfer hot path:
struct kmem_cache *msg_cache;

// Module init:
msg_cache = kmem_cache_create("i2c_msg",
    sizeof(struct i2c_msg), 0,
    SLAB_HWCACHE_ALIGN | SLAB_TYPESAFE_BY_RCU, NULL);

// In interrupt/transfer path (GFP_ATOMIC since atomic context):
struct i2c_msg *msg = kmem_cache_alloc(msg_cache, GFP_ATOMIC);
if (!msg) { /* handle allocation failure */ return -ENOMEM; }
// ... fill and submit msg ...
kmem_cache_free(msg_cache, msg);
# 2. Memory Compaction & CMA
Memory compaction and the Contiguous Memory Allocator (CMA) address one of the fundamental challenges in long-running Linux systems: the inability to satisfy large physically contiguous allocation requests due to memory fragmentation. This is critically important for Qualcomm SoC drivers that need large DMA buffers.
## 2.1 The Fragmentation Problem
Even with the buddy allocator's coalescing, long-running systems suffer from external fragmentation:

Physical Memory after hours of operation:

[USED][FREE][USED][FREE][USED][FREE][USED][FREE][USED][FREE]
  P0    P1    P2    P3    P4    P5    P6    P7    P8    P9

Free pages: P1, P3, P5, P7, P9  (5 pages free = 20 KB)
Largest contiguous free block: 1 page (4 KB)

-> CANNOT satisfy a 2-page (8 KB) contiguous allocation!
-> Even though 20 KB is free in total

Real-world impact on Qualcomm SoC drivers:
- Camera ISP needs 30+ MB contiguous frame buffer for 4K video
- GPU command buffer allocations fail at runtime
- Video codec (venus) cannot decode because DMA buffer unavailable
- System logs show: "page allocation failure: order:7, mode:0x40cc0"

Two mechanisms address this:
- Memory Compaction: Moves movable pages around to consolidate free space into large contiguous blocks.
- CMA: Reserves a region at boot time, guaranteed to be contiguous when needed.
## 2.2 Migration Types
The kernel classifies pages into migration types. The buddy allocator maintains separate free lists per migration type within each order (anti-fragmentation):

| Migration Type | Description | Examples |
| MIGRATE_UNMOVABLE | Cannot be moved | Kernel code/data, kmalloc, DMA buffers |
| MIGRATE_MOVABLE | Can be moved (page tables updated) | User process pages, file page cache |
| MIGRATE_RECLAIMABLE | Can be dropped and reloaded | File-backed pages, inode/dentry cache |
| MIGRATE_CMA | Reserved for CMA allocations | CMA region pages (reused as MOVABLE normally) |
| MIGRATE_ISOLATE | Temporarily isolated during migration | Pages being compacted/migrated right now |


| ℹ️ Anti-Fragmentation Strategy: The buddy allocator tries to keep UNMOVABLE pages |
| clustered together so MOVABLE pages can be compacted freely without being blocked |
| by immovable "anchor" pages. Check /proc/pagetypeinfo for per-zone type distribution. |

## 2.3 Two-Scanner Compaction Algorithm
Linux compaction uses two concurrent scanners that meet in the middle of the zone being compacted:

Low address                                    High address
|                                                         |
v                                                         v
[migration scanner] ---------> <---------- [free scanner]
  finds MOVABLE pages              finds FREE page frames
  to migrate AWAY                  to migrate INTO

When they meet -> compaction of this zone is complete

Step-by-step compaction example:

BEFORE compaction:
[UNMOV][MOV][UNMOV][MOV][FREE][MOV][UNMOV][FREE][MOV][FREE]
  P0    P1    P2    P3    P4    P5    P6    P7    P8    P9

Free scanner finds: P4, P7, P9
Migration scanner finds: P1, P3, P5, P8

Migrate P1 -> P9 (copy content, update PTEs, flush TLB)
Migrate P3 -> P7 (copy content, update PTEs, flush TLB)

AFTER compaction:
[UNMOV][FREE][UNMOV][FREE][FREE][MOV][UNMOV][FREE][MOV][MOV]
  P0    P1    P2    P3    P4    P5    P6    P7    P8    P9

Contiguous free block: P1, P3, P4 (3 pages) -- much better!

### Compaction Triggers
- Automatically: high-order allocation fails and memory pressure is moderate
- Proactively: kcompactd daemon runs in background (one per NUMA node)
- Manually: echo 1 > /proc/sys/vm/compact_memory
- Via khugepaged: when trying to collapse 4KB pages into 2MB huge page (THP)

### Compaction Tuning
# Proactive compaction aggressiveness (0=disabled, 100=aggressive)
/proc/sys/vm/compaction_proactiveness   # default: 20

# Statistics
cat /proc/vmstat | grep compact
# compact_migrate_scanned  -- pages scanned by migration scanner
# compact_free_scanned     -- pages scanned by free scanner
# compact_isolated         -- pages isolated for migration
# compact_stall            -- allocs that stalled for compaction
# compact_success          -- successful compaction events
# compact_fail             -- failed compaction events

| ⚠️ Compaction Cost: Can cause allocation stalls of tens to hundreds of milliseconds. |
| UNMOVABLE pages act as "anchors" that block compaction. |
| For latency-critical drivers (audio, real-time camera), use CMA instead of relying on compaction. |

## 2.4 CMA Architecture
The Contiguous Memory Allocator (CMA) solves the large contiguous buffer problem with a different strategy: reserve at boot time, reuse normally, evict on demand.

Physical Memory Layout with CMA:

+--------------------------------------------------------------+
|                                                              |
|  Normal Memory (kernel + user)     CMA Region (reserved)   |
|                                                              |
|  [kernel][user][user][user]..   [movable][movable][movable] |
|                                    ^                         |
|                                    Normally used for         |
|                                    movable user pages        |
|                                    (NO WASTE!)               |
|                                                              |
|                                    When driver needs it:     |
|                                    [movable pages EVICTED]   |
|                                    [contiguous DMA buffer]   |
|                                                              |
+--------------------------------------------------------------+

Key CMA design principles:
- Zero waste: CMA region is filled with MIGRATE_MOVABLE pages during normal operation. No memory wasted.
- Guaranteed contiguity: The entire CMA region is guaranteed physically contiguous since it was reserved at boot.
- Bitmap tracking: CMA tracks allocated pages via a bitmap. Each bit represents one page (or alignment unit).
- Migration on demand: When driver needs the space, MIGRATE_MOVABLE pages are migrated elsewhere. This takes time (milliseconds).

### CMA Internal Data Structures
struct cma {
    unsigned long   base_pfn;    // First page frame number of CMA region
    unsigned long   count;       // Total number of pages
    unsigned long   *bitmap;     // Allocation bitmap (1 bit per page)
    unsigned int    order_per_bit; // Pages per bitmap bit
    struct mutex    lock;        // Protects bitmap operations
    char            name[CMA_MAX_NAME];
};

### Multiple CMA Regions
A system can have multiple CMA regions, each dedicated to different subsystems:
System CMA regions:
  [default]  : 256 MB  -- used by dma_alloc_coherent() default
  [camera]   : 512 MB  -- dedicated to camera ISP pipeline
  [gpu]      : 256 MB  -- dedicated to GPU command buffers
  [video]    : 128 MB  -- dedicated to video codec (venus)
## 2.5 Device Tree Setup: reusable vs no-map
On ARM/ARM64 SoCs, CMA regions are defined in the Device Tree reserved-memory node. Understanding the key properties is essential for driver development:

/ {
    reserved-memory {
        #address-cells = <2>;
        #size-cells = <2>;
        ranges;

        /* Default CMA -- used by dma_alloc_coherent() */
        linux,cma {
            compatible = "shared-dma-pool";
            reusable;                        /* CMA: reusable! */
            size = <0x0 0x10000000>;         /* 256 MB */
            alignment = <0x0 0x400000>;      /* 4 MB aligned */
            linux,cma-default;               /* default pool */
        };

        /* Dedicated CMA for camera ISP */
        camera_cma: camera_region {
            compatible = "shared-dma-pool";
            reusable;
            reg = <0x0 0x90000000 0x0 0x20000000>;
        };

        /* Carveout -- hard-reserved, no-map */
        secure_region: secure@0xA0000000 {
            compatible = "shared-dma-pool";
            no-map;
            reg = <0x0 0xA0000000 0x0 0x4000000>;
        };
    };

    camera@1234000 {
        memory-region = <&camera_cma>;
    };
};

| Property | Type | Behavior |
| reusable | CMA | Kernel CAN use pages normally; evicts when driver needs contiguous block |
| no-map | Carveout | NEVER used by kernel; completely reserved; instant alloc but memory wasted |
| linux,cma-default | CMA hint | This pool used by dma_alloc_coherent() when no device-specific region bound |
| memory-region | Device bind | Binds device to specific CMA/carveout region (use of_reserved_mem_device_init()) |


## 2.6 CMA APIs: Kernel Configuration & Driver API
### Kernel Configuration
CONFIG_CMA=y
CONFIG_CMA_SIZE_MBYTES=256  # Default if not set in DT
CONFIG_CMA_ALIGNMENT=8      # Min alignment: 2^8 = 256 pages
CONFIG_DMA_CMA=y            # Enable CMA for DMA allocations
CONFIG_CMA_DEBUGFS=y        # /sys/kernel/debug/cma/ statistics

### High-Level Driver API (Preferred)
void *cpu_addr = dma_alloc_coherent(
    dev,          // struct device * (platform/PCI device)
    size,         // bytes requested
    &dma_handle,  // output: DMA bus address for hardware registers
    GFP_KERNEL    // allocation flags
);
// cpu_addr  -> CPU virtual address (kernel reads/writes)
// dma_handle -> physical/bus address (write to DMA ctrl regs)
dma_free_coherent(dev, size, cpu_addr, dma_handle);

### Low-Level CMA API (For Custom Allocators)
#include <linux/cma.h>
struct page *pages = cma_alloc(cma, count, align, no_warn);
if (!pages) return -ENOMEM;
phys_addr_t phys = page_to_phys(pages);
cma_release(cma, pages, count);  // return to CMA
## 2.7 CMA Allocation Flow Internals
When dma_alloc_coherent() triggers a CMA allocation, here is what happens internally:

dma_alloc_coherent(dev, 256MB, &dma_handle, GFP_KERNEL)
|
|-- Size > threshold -> try CMA
|
|-- cma_alloc(cma_area, 65536_pages, align, false)
|   |
|   |-- Search CMA bitmap for free contiguous range
|   |   (bitmap_find_next_zero_area)
|   |
|   |-- Range found but occupied by movable user pages
|   |   |
|   |   |-- isolate_migratepages_range()
|   |   |   -> Mark pages as MIGRATE_ISOLATE
|   |   |   -> Remove from buddy allocator free lists
|   |   |
|   |   |-- migrate_pages()
|   |   |   |-- Allocate replacement pages outside CMA region
|   |   |   |-- Copy page content (memcpy of 4KB per page)
|   |   |   |-- Update page tables (reverse map / rmap walk)
|   |   |   +-- TLB flush on all CPUs (IPI for remote CPUs)
|   |   |
|   |   +-- CMA pages are now FREE
|   |
|   |-- Mark pages as allocated in CMA bitmap
|   +-- Return struct page * (physically contiguous block)
|
|-- Map pages into kernel virtual space
|   (non-cached if non-HW-coherent SoC, cached if HW-coherent)
|
+-- Return cpu_addr (virtual) + dma_handle (physical) to driver

### Binding Device to CMA Region in Driver Probe
static int my_probe(struct platform_device *pdev) {
    // Step 1: Bind to DT memory-region
    int ret = of_reserved_mem_device_init(&pdev->dev);
    if (ret) {
        dev_err(&pdev->dev, "No reserved memory region\n");
        return ret;
    }

    // Step 2: Allocate from bound CMA region
    priv->buf = dma_alloc_coherent(&pdev->dev,
                                   BUF_SIZE,
                                   &priv->dma_addr,
                                   GFP_KERNEL);
    if (!priv->buf) {
        of_reserved_mem_device_release(&pdev->dev);
        return -ENOMEM;
    }
    return 0;
}

static int my_remove(struct platform_device *pdev) {
    dma_free_coherent(&pdev->dev, BUF_SIZE,
                      priv->buf, priv->dma_addr);
    of_reserved_mem_device_release(&pdev->dev);
    return 0;
}
## 2.8 CMA vs Carveout vs Compaction Comparison

| Aspect | Compaction | CMA (reusable) | Carveout (no-map) |
| Reserved at boot | No | Yes | Yes |
| Normal kernel use | N/A | Yes (movable pages) | NO (completely reserved) |
| Memory waste | None | None (reused) | Full reservation |
| Allocation guarantee | Not guaranteed | High (may fail if unmovable) | Always succeeds |
| Allocation latency | High (100s of ms) | Medium (ms, migrate pages) | Instant |
| Security isolation | No | No | Yes (TrustZone use) |
| DT property | N/A | reusable | no-map |
| Use case | General defrag | Camera, GPU, codec | Secure mem, firmware |


## 2.9 Transparent Huge Pages (THP)
Transparent Huge Pages (THP) use compaction to create 2 MB pages (on ARM64) instead of 4 KB pages, reducing TLB pressure for large memory workloads:

Normal 4KB pages:   [4K][4K][4K]...[4K]  (512 pages = 2 MB)
                       v khugepaged collapses v
Huge page:          [          2 MB block         ]

Benefits:
  - 512x fewer TLB entries needed (1 entry covers 2 MB vs 4 KB)
  - Better performance for databases, JVM, large memory apps
  - ARM64: uses 2 MB block entries at L2 in page tables

THP relies on compaction to find 512 contiguous free 4 KB pages to form a 2 MB block. Settings:
# THP mode
cat /sys/kernel/mm/transparent_hugepage/enabled
# [always] madvise never

# For latency-sensitive apps (use madvise, not always)
echo madvise > /sys/kernel/mm/transparent_hugepage/enabled

# Use in application code:
madvise(ptr, len, MADV_HUGEPAGE);  // request THP for this range
## 2.10 Debugging CMA and Compaction

### CMA Statistics via DebugFS
# CMA per-region statistics
cat /sys/kernel/debug/cma/<cma_name>/alloc_count   # allocations
cat /sys/kernel/debug/cma/<cma_name>/release_count # releases
cat /sys/kernel/debug/cma/<cma_name>/fail_count    # failures

# CMA bitmap: shows which pages are allocated (1=allocated)
cat /sys/kernel/debug/cma/<cma_name>/bitmap

# Overall CMA memory status
cat /proc/meminfo | grep Cma
# CmaTotal:    262144 kB   (256 MB reserved)
# CmaFree:     245760 kB   (free in CMA)

### Compaction Statistics
# Compaction events
cat /proc/vmstat | grep -E "compact|migrate"

# Key metrics to watch:
# compact_stall   -> allocation stalled waiting for compaction (BAD)
# compact_success -> compaction helped                         (GOOD)
# compact_fail    -> compaction could not satisfy request

# Memory fragmentation per zone (free pages per order)
cat /proc/buddyinfo
cat /proc/pagetypeinfo  # per migration type

# Force compaction now
echo 1 > /proc/sys/vm/compact_memory

### Kernel Config for CMA Debugging
CONFIG_CMA_DEBUG=y           # verbose CMA allocation messages
CONFIG_CMA_DEBUGFS=y         # /sys/kernel/debug/cma/
CONFIG_MIGRATION=y           # page migration support (required)
CONFIG_COMPACTION=y          # memory compaction support

## 2.11 Complete Qualcomm Camera ISP Driver Example
This complete example demonstrates a real-world Qualcomm camera ISP driver that uses CMA for large frame buffers, integrating all the concepts covered in Part 3:

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/of_reserved_mem.h>
#include <linux/slab.h>
#include <linux/io.h>

#define ISP_FRAME_WIDTH   3840  /* 4K UHD */
#define ISP_FRAME_HEIGHT  2160
#define ISP_BYTES_PP      4     /* RGBA: 4 bytes per pixel */
#define ISP_FRAME_BUF_CNT 3     /* triple buffering */

#define ISP_FRAME_ADDR_LO 0x00  /* hardware register offsets */
#define ISP_FRAME_ADDR_HI 0x04
#define ISP_CTRL_REG      0x08
#define ISP_STATUS_REG    0x0C
#define ISP_IRQ_MASK      0x10

struct cam_isp_frame {
    void       *cpu_addr;    /* kernel virtual address */
    dma_addr_t  dma_addr;    /* hardware DMA address   */
    size_t      size;        /* buffer size in bytes   */
};

struct cam_isp_dev {
    struct device           *dev;
    void __iomem            *base;       /* ISP MMIO registers    */
    struct cam_isp_frame     frames[ISP_FRAME_BUF_CNT]; /* CMA */
    int                      irq;
    struct kmem_cache       *cmd_cache;  /* SLUB cache for cmds  */
    spinlock_t               lock;
    int                      active_frame;
};

/* ----- probe: allocate CMA frame buffers ----- */
static int cam_isp_probe(struct platform_device *pdev)
{
    struct cam_isp_dev *isp;
    struct resource *res;
    size_t frame_size;
    int i, ret;

    /* 1. Allocate private data using kmalloc (small struct) */
    isp = devm_kzalloc(&pdev->dev, sizeof(*isp), GFP_KERNEL);
    if (!isp)
        return -ENOMEM;

    isp->dev = &pdev->dev;
    spin_lock_init(&isp->lock);

    /* 2. Map ISP hardware registers (MMIO) */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    isp->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(isp->base))
        return PTR_ERR(isp->base);

    /* 3. Bind to camera_cma DT memory-region */
    ret = of_reserved_mem_device_init(&pdev->dev);
    if (ret) {
        dev_err(&pdev->dev, "No CMA region in DT\n");
        return ret;
    }

    /* 4. Allocate frame buffers from CMA (large contiguous) */
    frame_size = ISP_FRAME_WIDTH * ISP_FRAME_HEIGHT * ISP_BYTES_PP;
    /* = 3840 * 2160 * 4 = 33,177,600 bytes (~31.6 MB) */

    for (i = 0; i < ISP_FRAME_BUF_CNT; i++) {
        isp->frames[i].size = frame_size;
        isp->frames[i].cpu_addr = dma_alloc_coherent(
                &pdev->dev, frame_size,
                &isp->frames[i].dma_addr, GFP_KERNEL);
        if (!isp->frames[i].cpu_addr) {
            dev_err(&pdev->dev,
                "Failed to alloc frame buf %d\n", i);
            ret = -ENOMEM;
            goto err_free_frames;
        }
        dev_info(&pdev->dev,
            "Frame[%d]: CPU=%p DMA=%pad size=%zu\n",
            i, isp->frames[i].cpu_addr,
            &isp->frames[i].dma_addr, frame_size);
    }

    /* 5. Create SLUB cache for ISP command objects */
    isp->cmd_cache = kmem_cache_create("cam_isp_cmd",
        sizeof(struct cam_isp_command), 0,
        SLAB_HWCACHE_ALIGN, NULL);
    if (!isp->cmd_cache) {
        ret = -ENOMEM;
        goto err_free_frames;
    }

    /* 6. Get IRQ and register handler */
    isp->irq = platform_get_irq(pdev, 0);
    ret = devm_request_irq(&pdev->dev, isp->irq,
                            cam_isp_irq_handler, 0,
                            "cam-isp", isp);
    if (ret) goto err_destroy_cache;

    /* 7. Program first frame buffer DMA address into hardware */
    writel(lower_32_bits(isp->frames[0].dma_addr),
           isp->base + ISP_FRAME_ADDR_LO);
    writel(upper_32_bits(isp->frames[0].dma_addr),
           isp->base + ISP_FRAME_ADDR_HI);
    isp->active_frame = 0;

    platform_set_drvdata(pdev, isp);
    return 0;

err_destroy_cache:
    kmem_cache_destroy(isp->cmd_cache);
err_free_frames:
    for (i--; i >= 0; i--)
        dma_free_coherent(&pdev->dev,
                          isp->frames[i].size,
                          isp->frames[i].cpu_addr,
                          isp->frames[i].dma_addr);
    of_reserved_mem_device_release(&pdev->dev);
    return ret;
}

/* ----- irq handler: triple-buffer swap ----- */
static irqreturn_t cam_isp_irq_handler(int irq, void *dev_id)
{
    struct cam_isp_dev *isp = dev_id;
    struct cam_isp_command *cmd;
    int next_frame;
    unsigned long flags;

    spin_lock_irqsave(&isp->lock, flags);

    /* Allocate command from SLUB cache (GFP_ATOMIC: interrupt ctx) */
    cmd = kmem_cache_alloc(isp->cmd_cache, GFP_ATOMIC);
    if (!cmd) {
        spin_unlock_irqrestore(&isp->lock, flags);
        return IRQ_HANDLED;
    }

    /* Triple-buffer: advance to next frame */
    next_frame = (isp->active_frame + 1) % ISP_FRAME_BUF_CNT;

    /* Program next frame DMA address into hardware (interrupt-safe) */
    writel(lower_32_bits(isp->frames[next_frame].dma_addr),
           isp->base + ISP_FRAME_ADDR_LO);
    writel(upper_32_bits(isp->frames[next_frame].dma_addr),
           isp->base + ISP_FRAME_ADDR_HI);
    isp->active_frame = next_frame;

    /* Free command object back to SLUB cache */
    kmem_cache_free(isp->cmd_cache, cmd);

    spin_unlock_irqrestore(&isp->lock, flags);
    return IRQ_HANDLED;
}

/* ----- remove: clean up in reverse order ----- */
static int cam_isp_remove(struct platform_device *pdev)
{
    struct cam_isp_dev *isp = platform_get_drvdata(pdev);
    int i;

    /* Stop hardware */
    writel(0, isp->base + ISP_CTRL_REG);

    /* Destroy SLUB command cache */
    kmem_cache_destroy(isp->cmd_cache);

    /* Free all CMA frame buffers */
    for (i = 0; i < ISP_FRAME_BUF_CNT; i++)
        dma_free_coherent(&pdev->dev,
                          isp->frames[i].size,
                          isp->frames[i].cpu_addr,
                          isp->frames[i].dma_addr);

    /* Release CMA region binding */
    of_reserved_mem_device_release(&pdev->dev);
    return 0;
}

/* ----- Device Tree match and driver registration ----- */
static const struct of_device_id cam_isp_of_match[] = {
    { .compatible = "qcom,camera-isp" },
    { }
};
MODULE_DEVICE_TABLE(of, cam_isp_of_match);

static struct platform_driver cam_isp_driver = {
    .probe  = cam_isp_probe,
    .remove = cam_isp_remove,
    .driver = {
        .name = "qcom-cam-isp",
        .of_match_table = cam_isp_of_match,
    },
};
module_platform_driver(cam_isp_driver);

MODULE_DESCRIPTION("Qualcomm Camera ISP Driver");
MODULE_LICENSE("GPL v2");

| ✔ This driver demonstrates the key patterns from Part 3: |
| 1. SLUB: kmem_cache for high-frequency ISP command objects (GFP_ATOMIC in IRQ) |
| 2. CMA: dma_alloc_coherent() for large 31.6 MB frame buffers (x3 = ~95 MB) |
| 3. DT: of_reserved_mem_device_init() to bind device to camera_cma region |
| 4. IRQ: per-frame DMA address switch in interrupt handler (triple buffering) |
| 5. Error handling: proper cleanup in reverse order on probe failure |

## Part 3 Quick Reference Summary

| Concept | Key Point |
| SLUB fast path | Per-CPU freelist: two pointer ops, zero locking, ~10-20 ns |
| SLUB slow path | Node partial list -> buddy allocator -> initialize all objects as free chain |
| Freelist storage | Stored INSIDE free objects at offset; XOR-encoded with SLAB_FREELIST_HARDENED |
| SLUB debugging | Poison: 0x6b (free), 0x5a (alloc). Red zones. /sys/kernel/slab/<name>/validate |
| SLUB NUMA | Per-node partial lists; alloc_pages_node() ensures local memory |
| Constructor timing | Called ONCE when slab page first populated, NOT on every alloc. Objects retain init state. |
| Fragmentation | Free pages scattered; cannot satisfy large contiguous requests despite sufficient total free |
| Migration types | UNMOVABLE (kmalloc), MOVABLE (user), RECLAIMABLE (cache), CMA, ISOLATE |
| Compaction scanners | Migration scanner (low->high, finds movable) meets free scanner (high->low, finds free) |
| CMA design | Reserve at boot, use as MOVABLE normally (no waste), evict on demand for DMA |
| reusable vs no-map | reusable = CMA (kernel uses pages). no-map = carveout (hard reserved, instant alloc) |
| CMA driver API | of_reserved_mem_device_init() -> dma_alloc_coherent() -> dma_free_coherent() |
| Key files | mm/slub.c, mm/compaction.c, mm/cma.c, include/linux/slab.h, include/linux/cma.h |



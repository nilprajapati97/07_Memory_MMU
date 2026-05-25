ARM/ARM64 Linux Kernel
Memory Management
Reference Guide

PART 2: Memory Allocation Mechanisms & Buddy Allocator Internals

| All Allocators • GFP Flags • Decision Flowchart • Common MistakesBuddy Allocator • Splitting & Coalescing • XOR Buddy Calculation • Free Area StructureComplete 8-Page State Sequence • Fragmentation Prevention • Debugging Tools |


Companion to Part 1 (Fundamentals) and Part 3 (SMMU, DMA, MMU, Boot)
# 1. Memory Allocation Mechanisms
The Linux kernel provides a layered system of memory allocators because no single allocator fits all requirements. Memory allocations in the kernel have wildly different constraints:
- Physically contiguous: required for DMA to hardware (camera ISP, GPU, network controller)
- Zero latency: interrupt handlers cannot sleep or block
- Large allocations: firmware buffers, kernel module loading
- Tiny and frequent: kernel objects, inodes, sk_buff structures
- Special alignment: hardware register pages, DMA descriptor rings

## 1.1 Allocator Layered Architecture Diagram
Every higher-level allocator ultimately depends on the Buddy Allocator as the source of physical pages. The layered hierarchy below shows how allocators build upon each other:

| Hardware Physical RAM (DRAM) | v+-----------------------------------------------+| Buddy Allocator (alloc_pages) | <- Page-level, base layer| Manages free pages by zone & order (0..11) |+-----------------------------------------------+ | | v v+----------------+ +---------------------+| SLUB Allocator | | vmalloc || (sub-page obj) | | (virtual contig.) |+-------+--------+ +---------------------+ | v+--------------------------------------------------+| kmalloc / kzalloc / kfree <- Most common kernel API || kmem_cache_alloc / kmem_cache_free <- Object caches || dma_alloc_coherent / dma_pool_alloc <- DMA-safe allocation || alloc_percpu / mempool_alloc <- Specialized use |+--------------------------------------------------+ |


## 1.2 Complete Allocator Comparison Table
| Allocator | Phys Contig | DMA Safe | Can Sleep | Max Size | Use Case | Notes |
| kmalloc | Yes | Yes | GFP-dep. | ~4 MB | General kernel objects | Most used API in drivers |
| kzalloc | Yes | Yes | GFP-dep. | ~4 MB | Same, zero-init | Preferred over kmalloc |
| vmalloc | No | No | Yes | VA-space | Large non-DMA buffers | Slower, per-page TLB |
| alloc_pages | Yes | Yes | GFP-dep. | Order-based | Page-level control | Returns struct page* |
| dma_alloc_coherent | Yes | Yes | Yes | Platform | DMA buffers | Always coherent |
| dma_pool_alloc | Yes | Yes | Yes | Pool size | Small DMA objects | Descriptor rings |
| kmem_cache_alloc | Yes | Yes | GFP-dep. | Cache size | Frequent fixed-size | kmem_cache_create first |
| mempool_alloc | Yes | Yes | Yes | Pool size | Must-not-fail paths | Block I/O, SCSI stack |
| alloc_percpu | N/A | No | Yes | Unlimited | Per-CPU data | No locking needed |


# 2. GFP Flags — Complete Reference
GFP stands for "Get Free Pages". GFP flags control which memory zone is used, whether the allocator can sleep/reclaim memory, and special behavioral options. Choosing the wrong GFP flag is one of the most common kernel programming mistakes.

## 2.1 Primary GFP Flags
| Flag | Can Sleep | Zone | Description & When to Use |
| GFP_KERNEL | Yes | NORMAL | Process context. Can sleep, reclaim memory, best effort. Default for most allocations. |
| GFP_ATOMIC | No | NORMAL | Interrupt handler, spinlock held, tasklet. Uses emergency reserves. May return NULL. |
| GFP_NOWAIT | No | NORMAL | Process context. Cannot sleep or reclaim. Fails fast if memory not immediately available. |
| GFP_NOIO | Yes | NORMAL | Block device I/O path. Can sleep but no disk I/O. Prevents recursive I/O deadlocks. |
| GFP_NOFS | Yes | NORMAL | Filesystem code path. Can sleep but no FS operations. Prevents filesystem recursion. |
| GFP_DMA | Yes | DMA | DMA buffers for legacy 16 MB-limited devices. Allocates from ZONE_DMA only. |
| GFP_HIGHUSER | Yes | HIGHMEM | User pages. Allocates from ZONE_HIGHMEM preferred. Requires kmap() for kernel access. |
| GFP_USER | Yes | NORMAL | User page allocations. Like GFP_KERNEL but marks pages as user-space. |


## 2.2 Modifier Flags (Combined with Primary Flags)
| Modifier Flag | Effect |
| __GFP_ZERO | Zero-initialize the allocated memory. Always prefer kzalloc() over kmalloc()+memset(). |
| __GFP_NOFAIL | Loop until allocation succeeds. Use only in critical paths where failure would corrupt system state. |
| __GFP_NOWARN | Suppress OOM warning messages. Use when allocation failure is expected and handled gracefully. |
| __GFP_HIGHMEM | Allow allocation from ZONE_HIGHMEM. Returned page may require kmap() for CPU access. |
| __GFP_RECLAIM | Allow memory reclaim (page writeback, slab shrinkers) to satisfy the allocation. |
| __GFP_IO | Allow disk I/O during reclaim. Part of GFP_KERNEL. Not set in GFP_NOIO. |
| __GFP_FS | Allow filesystem operations during reclaim. Not set in GFP_NOFS. |
| __GFP_COMP | Mark pages as compound (huge page). Used by THP and hugetlb. |


## 2.3 Critical GFP Usage Rules
The single most important rule: NEVER use GFP_KERNEL while holding a spinlock — it can sleep and cause a deadlock. Use GFP_ATOMIC instead.

| /* CORRECT: process context (workqueue, kthread, syscall handler) */ptr = kmalloc(size, GFP_KERNEL); // can sleep, best effortif (!ptr) return -ENOMEM;/* CORRECT: interrupt handler or while spinlock is held */ptr = kmalloc(size, GFP_ATOMIC); // cannot sleep, may failif (!ptr) { /* handle gracefully - do not panic */}/* CORRECT: block device I/O path */ptr = kmalloc(size, GFP_NOIO); // prevents I/O deadlock/* BUG: GFP_KERNEL inside spinlock = DEADLOCK! */spin_lock(&lock);ptr = kmalloc(size, GFP_KERNEL); // BUG! can sleep, holding spinlockspin_unlock(&lock);/* CORRECT FIX: use GFP_ATOMIC */spin_lock(&lock);ptr = kmalloc(size, GFP_ATOMIC); // correctspin_unlock(&lock); |


# 3. Buddy Allocator — alloc_pages API
The Buddy Allocator is the lowest-level page allocator in the Linux kernel. It manages free physical pages organized in power-of-2 blocks called "orders". Most drivers use higher-level APIs, but understanding this layer is essential for page-level operations.

## 3.1 API Reference
| #include <linux/gfp.h>#include <linux/mm.h>/* Allocate 2^order physically contiguous pages */struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);struct page *alloc_page(gfp_t gfp_mask); // order=0, single page/* Get virtual address directly (lowmem pages only) */unsigned long __get_free_pages(gfp_t gfp, unsigned int order);unsigned long __get_free_page(gfp_t gfp);unsigned long get_zeroed_page(gfp_t gfp); // single zeroed page/* Free pages - order MUST match alloc order! */void __free_pages(struct page *page, unsigned int order);void free_pages(unsigned long addr, unsigned int order);void free_page(unsigned long addr);/* Helper: virtual address from struct page * */void *page_address(struct page *page); // NULL if highmem!/* Example: allocate 4 contiguous pages (order=2, 2^2=4 = 16 KB) */struct page *pages = alloc_pages(GFP_KERNEL, 2);if (!pages) return -ENOMEM;void *vaddr = page_address(pages);/* ... use vaddr ... */__free_pages(pages, 2); /* MUST match alloc order */ |


# 4. SLUB/Slab Allocator — kmalloc, kzalloc, kmem_cache
The SLUB allocator (default since Linux 2.6.23) sits on top of the Buddy Allocator and provides efficient allocation of small, fixed-size objects. It solves the internal fragmentation problem: allocating a 64-byte structure without SLUB would waste 4032 bytes of a 4096-byte page.

## 4.1 General-Purpose kmalloc / kzalloc
| /* Allocate memory - not zero-initialized */void *ptr = kmalloc(size, gfp_flags);/* Allocate + zero-initialize (PREFERRED over kmalloc+memset) */void *ptr = kzalloc(size, gfp_flags);/* Allocate array of n elements, zero-initialized */void *ptr = kcalloc(n, size, gfp_flags);/* Resize an existing allocation */void *ptr = krealloc(old_ptr, new_size, gfp_flags);/* Free */kfree(ptr);kfree_sensitive(ptr); /* zero before freeing - for security-sensitive data *//* Check valid kmalloc range (must match a cache size) */size_t ksize(const void *objp); /* actual allocated size */ |


The kernel pre-creates caches for common sizes: 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192 bytes. kmalloc() rounds up to the nearest cache size.

## 4.2 Dedicated Object Caches — kmem_cache API
For frequently allocated fixed-size structures (struct inode, struct task_struct, driver-specific objects), use a dedicated kmem_cache for better CPU cache utilization and debugging visibility.
| /* Create a cache at module/driver init */struct kmem_cache *my_cache = kmem_cache_create( "my_object_cache", /* name in /proc/slabinfo, /sys/kernel/slab/ */ sizeof(struct my_obj), /* object size */ 0, /* alignment (0 = natural alignment) */ SLAB_HWCACHE_ALIGN, /* flags - align to CPU cache line */ NULL /* constructor (optional, called once per object) */);if (!my_cache) return -ENOMEM;/* Allocate an object */struct my_obj *obj = kmem_cache_alloc(my_cache, GFP_KERNEL);if (!obj) return -ENOMEM;/* Initialize and use object */obj->field = value;/* Free object back to cache */kmem_cache_free(my_cache, obj);/* Destroy cache at module exit - ALL objects must be freed first! */kmem_cache_destroy(my_cache); |


## 4.3 kmem_cache Flags
| Flag | Effect |
| SLAB_HWCACHE_ALIGN | Align objects to CPU hardware cache line (prevents false sharing between objects on adjacent cache lines) |
| SLAB_POISON | Fill free objects with 0x6b. If you see 0x6b6b6b6b in crash dump, you have a use-after-free bug. |
| SLAB_RED_ZONE | Add red zone bytes (0xbb...) before/after each object. Detects buffer overflows and underflows. |
| SLAB_PANIC | Panic if cache creation fails. Use for caches that are absolutely critical for system operation. |
| SLAB_TYPESAFE_BY_RCU | Safe for RCU-protected lookups. Kernel will not free the slab page while RCU readers exist. |
| SLAB_ACCOUNT | Account memory to kmemcg (cgroup memory tracking). Required for containers. |
| SLAB_RECLAIM_ACCOUNT | Objects are reclaimable under memory pressure (e.g., inode cache, dentry cache). |


# 5. vmalloc — Large Virtual-Contiguous Allocations
vmalloc allocates memory that is virtually contiguous but not necessarily physically contiguous. It stitches together scattered physical pages into a contiguous virtual range using the vmalloc area of kernel virtual space.

## 5.1 vmalloc API
| #include <linux/vmalloc.h>void *vmalloc(unsigned long size); /* virtually contiguous */void *vzalloc(unsigned long size); /* zero-initialized */void *vmalloc_node(unsigned long size, int node); /* NUMA-aware */void *vmalloc_user(unsigned long size); /* user-space mappable */void vfree(const void *addr); /* free vmalloc allocation *//* Example: large non-DMA firmware buffer */void *fw_buf = vmalloc(firmware_size);if (!fw_buf) return -ENOMEM;memcpy(fw_buf, firmware_data, firmware_size);/* process firmware ... */vfree(fw_buf); |


## 5.2 vmalloc vs kmalloc — Decision Guide
| Physical memory layout: kmalloc(32KB): vmalloc(32KB): +------+------+------+------+ +------+ +------+ | Page | Page | Page | Page | | Page | | Page | | 0 | 1 | 2 | 3 | | 5 | | 17 | +------+------+------+------+ +------+ +------+ Physically contiguous Physically SCATTERED Fast (no TLB setup per page) Slower (per-page TLB entries) DMA capable NOT DMA capable! Max ~4MB Limited by virtual address space Rule: kmalloc for DMA/small/fast. vmalloc for large non-DMA buffers. |


# 6. DMA Allocators — dma_alloc_coherent, dma_pool
DMA allocators are critical for driver development. Hardware DMA engines require physically contiguous memory with a known physical (bus) address and proper cache coherency. These allocators handle all platform-specific details automatically.

## 6.1 dma_alloc_coherent — Standard DMA API
| #include <linux/dma-mapping.h>/* Allocate coherent DMA memory */void *cpu_addr = dma_alloc_coherent( dev, /* struct device * (your platform/PCI device) */ size, /* bytes requested */ &dma_handle, /* OUTPUT: DMA (bus) address for hardware registers */ GFP_KERNEL /* allocation flags */);/* * cpu_addr -> CPU virtual address (for kernel read/write) * dma_handle-> physical/bus address (program into DMA controller) * Coherent means: CPU and DMA always see same data * - On HW-coherent SoCs: cached, coherency handled by interconnect * - On non-coherent SoCs: mapped as non-cacheable (MT_NORMAL_NC) *//* Free - must pass EXACT same size as alloc */dma_free_coherent(dev, size, cpu_addr, dma_handle); |


## 6.2 DMA Pool — For Small Repeated DMA Allocations
| /* Create pool at driver init - for descriptor rings, command buffers */struct dma_pool *pool = dma_pool_create( "my_desc_pool", dev, sizeof(struct my_descriptor), /* object size */ 32, /* alignment in bytes */ 0 /* boundary (0 = no boundary) */);/* Allocate one object from pool */dma_addr_t dma_addr;struct my_descriptor *desc = dma_pool_alloc(pool, GFP_KERNEL, &dma_addr);if (!desc) { /* handle OOM */ }/* Program dma_addr into hardware DMA descriptor registers *//* Free object back to pool */dma_pool_free(pool, desc, dma_addr);/* Destroy pool at driver exit */dma_pool_destroy(pool); |


## 6.3 Streaming DMA — For Existing Buffers
| /* Map existing buffer for DMA (flushes cache for DMA_TO_DEVICE) */dma_addr_t dma_addr = dma_map_single( dev, cpu_ptr, /* kernel virtual address of existing buffer */ size, DMA_TO_DEVICE /* or DMA_FROM_DEVICE, DMA_BIDIRECTIONAL */);/* ALWAYS check for mapping error */if (dma_mapping_error(dev, dma_addr)) return -ENOMEM;/* Program dma_addr into hardware register, start DMA ... *//* Wait for DMA completion ... *//* Unmap after DMA completes (invalidates cache for DMA_FROM_DEVICE) */dma_unmap_single(dev, dma_addr, size, DMA_TO_DEVICE);/* Now safe to access cpu_ptr from CPU again */ |


# 7. Per-CPU Allocations and mempool
## 7.1 Per-CPU Allocations
Per-CPU data eliminates locking overhead for CPU-local data. Each CPU has its own copy, so no synchronization is needed for accesses with preemption disabled.
| /* Static per-CPU variable declaration */DEFINE_PER_CPU(int, my_counter);/* Access with preemption disabled (get_cpu_var disables preemption) */int val = get_cpu_var(my_counter);val++;put_cpu_var(my_counter);/* Access for a specific CPU */per_cpu(my_counter, cpu_id) = 42;/* Dynamic per-CPU allocation */int __percpu *ptr = alloc_percpu(int);if (!ptr) return -ENOMEM;*per_cpu_ptr(ptr, smp_processor_id()) = 42;free_percpu(ptr);/* Use case: performance counters, per-CPU queues, statistics */ |


## 7.2 mempool — Guaranteed Allocation Pool
mempool maintains a pre-allocated reserve pool. When the system is under memory pressure, mempool falls back to the reserve rather than failing. Essential for block device I/O paths that cannot afford allocation failures.
| /* Create pool with 16 pre-allocated objects */mempool_t *pool = mempool_create_kmalloc_pool(16, sizeof(struct my_obj));if (!pool) return -ENOMEM;/* Allocate - blocks if pool empty but will NOT fail if can_wait */struct my_obj *obj = mempool_alloc(pool, GFP_NOIO);/* Free - returns to pool reserve first */mempool_free(obj, pool);mempool_destroy(pool);/* Use case: SCSI command descriptors, block request queues, NVMe I/O */ |


# 8. Allocator Decision Flowchart
Use this flowchart when deciding which allocator to use in your kernel driver:

| Need memory in kernel driver?|+-- Is it for DMA / hardware register programming?| +-- Small, repeated objects (descriptors, command buffers)| | --> dma_pool_alloc()| +-- Large single buffer (video frame, audio, codec)| --> dma_alloc_coherent()|+-- Needs physical contiguity (non-DMA)?| +-- Small to medium (< ~4 MB)| | --> kmalloc() or kzalloc()| +-- Exact page count needed| --> alloc_pages() returns struct page*|+-- Physical contiguity NOT required?| +-- Large buffer (firmware, module data, buffers > 4MB)| --> vmalloc() or vzalloc()|+-- Frequently allocated/freed fixed-size struct?| --> kmem_cache_create() + kmem_cache_alloc()| Benefit: per-CPU cache, better performance, debugging|+-- Per-CPU data (statistics, hot path counters)?| --> DEFINE_PER_CPU() or alloc_percpu()|+-- Must never fail (block I/O critical path)?| --> mempool_alloc() with pre-allocated reserve|+-- Small temporary local variable (< 256 bytes)? --> Stack variable (kernel stack is 8KB/16KB, be careful!) |


# 9. Common Allocation Mistakes — Wrong vs Right Patterns
## 9.1 Using GFP_KERNEL in Atomic Context (DEADLOCK)
| /* WRONG - GFP_KERNEL can sleep, spinlock prevents sleep = DEADLOCK */spin_lock(&my_lock);ptr = kmalloc(size, GFP_KERNEL); // BUG: can sleep while holding spinlock!spin_unlock(&my_lock);/* RIGHT - GFP_ATOMIC in locked context */spin_lock(&my_lock);ptr = kmalloc(size, GFP_ATOMIC); // correctif (!ptr) { spin_unlock(&my_lock); return -ENOMEM;}spin_unlock(&my_lock); |


## 9.2 Not Checking Allocation Return Value (NULL Dereference)
| /* WRONG - NULL dereference if allocation fails */ptr = kmalloc(size, GFP_KERNEL);ptr->field = value; // Oops: NULL pointer dereference!/* RIGHT - always check return value */ptr = kmalloc(size, GFP_KERNEL);if (!ptr) return -ENOMEM;ptr->field = value; // safe |


## 9.3 Using vmalloc for DMA Buffers (Wrong Physical Address)
| /* WRONG - vmalloc pages are not physically contiguous! */buf = vmalloc(size);dma_handle = virt_to_phys(buf); // WRONG! Not valid for vmalloc memory/* DMA engine reads garbage - hardware corruption! *//* RIGHT - use DMA-safe allocator */buf = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);if (!buf) return -ENOMEM;/* dma_handle is valid bus address for hardware */writel(lower_32_bits(dma_handle), hw_reg_dma_lo);writel(upper_32_bits(dma_handle), hw_reg_dma_hi); |


## 9.4 Memory Leaks in Error Paths
| /* WRONG - leak priv->buf if IRQ allocation fails */static int my_probe(struct platform_device *pdev) { priv->buf = kmalloc(BUF_SIZE, GFP_KERNEL); if (!priv->buf) return -ENOMEM; ret = request_irq(irq, my_handler, 0, "my_dev", priv); if (ret < 0) return ret; // BUG: priv->buf is leaked! return 0;}/* RIGHT - always clean up on error */static int my_probe(struct platform_device *pdev) { priv->buf = kmalloc(BUF_SIZE, GFP_KERNEL); if (!priv->buf) return -ENOMEM; ret = request_irq(irq, my_handler, 0, "my_dev", priv); if (ret < 0) { kfree(priv->buf); // free on error! priv->buf = NULL; return ret; } return 0;} |


## 9.5 Double Free (Heap Corruption)
| /* WRONG - double free corrupts heap metadata */kfree(ptr);kfree(ptr); // BUG: double free -> SLUB corruption -> kernel panic/* RIGHT - set to NULL after free */kfree(ptr);ptr = NULL;/* Second call is now safe: kfree(NULL) is a no-op */kfree(ptr); // safe: kfree(NULL) does nothing |


## 9.6 Large Stack Allocations (Stack Overflow)
| /* WRONG - large buffer on kernel stack = stack overflow */void my_func(void) { char buf[4096]; // DANGER: kernel stack is only 8-16 KB! struct large_obj obj; // DANGER: large struct on stack}/* RIGHT - allocate from heap */void my_func(void) { char *buf = kmalloc(4096, GFP_KERNEL); if (!buf) return; /* use buf */ kfree(buf);}/* Rule: Never put more than ~256 bytes on the kernel stack */ |


# 10. Debugging Memory Allocation Issues
## 10.1 Kernel Config Options for Debugging
| # Enable in kernel .config for debugging builds:CONFIG_KASAN=y # Kernel Address Sanitizer - use-after-free, out-of-boundsCONFIG_KMEMLEAK=y # Memory leak detectionCONFIG_SLUB_DEBUG=y # Slab corruption, red zones, poisonCONFIG_DEBUG_PAGEALLOC=y # Page allocator corruption detectionCONFIG_DEBUG_SLAB=y # Slab allocator debug checksCONFIG_SLAB_FREELIST_HARDENED=y # XOR-encode freelist pointersCONFIG_DMA_API_DEBUG=y # Detects DMA mapping errors and leaksCONFIG_DEBUG_VM=y # VM consistency checks |


## 10.2 Runtime Debugging Commands
| Tool / File | Purpose & Usage |
| KASAN output | Prints detailed stack trace for use-after-free, buffer overflows in dmesg. Most powerful debug tool. |
| /proc/slabinfo | Lists all slab caches: name, active objects, total objects, object size. cat /proc/slabinfo |
| /sys/kernel/slab/<name>/ | Per-cache statistics: alloc_calls, free_calls, objects, slabs, shrink. Rich debug info. |
| slabtop | Real-time top-like display of slab cache memory usage. Shows largest caches by memory consumed. |
| /proc/buddyinfo | Shows free page count at each buddy order per zone per NUMA node. Diagnose fragmentation. |
| /proc/pagetypeinfo | Free pages per order grouped by migration type (Unmovable/Movable/Reclaimable/CMA). |
| /proc/vmallocinfo | All vmalloc allocations with caller, size, virtual address range. |
| /proc/meminfo | System memory summary: MemTotal, MemFree, Cached, Slab, VmallocUsed, CmaTotal/CmaFree. |
| KMEMLEAK scan | echo scan > /sys/kernel/debug/kmemleak then cat /sys/kernel/debug/kmemleak to see leaks. |


## 10.3 SLUB Debugging — Poison Values
SLUB uses specific byte patterns to detect memory bugs:
| Free object fill pattern: 0x6b 0x6b 0x6b 0x6b ... ("kk" = "kfree") Allocated object fill pattern: 0x5a 0x5a 0x5a 0x5a ... ("Z" = zeroed) Red zone marker: 0xbb 0xbb 0xbb 0xbb ... Free pointer (Hardened SLUB): ptr XOR secret XOR slot_address If you see 0x6b6b6b6b in a kernel Oops trace: --> Use-after-free bug! Object was freed, then accessed. If you see 0xbbbbbbbb around the crash site: --> Buffer overflow! Red zone was overwritten. Validate a slab cache manually: echo 1 > /sys/kernel/slab/<cache_name>/validate |


# 11. Buddy Allocator Internals
The Buddy Allocator is one of the most elegant data structures in the Linux kernel. Understanding its internals is essential for memory system debugging, driver development, and kernel engineering interviews.

## 11.1 Orders — Power-of-2 Page Blocks
Each "order" represents a block of 2^n contiguous, naturally-aligned physical pages:

| Order | Pages | Size (4KB pages) | Typical Use | API Example |
| 0 | 1 | 4 KB | Single page, kmalloc small objects | alloc_page(GFP_KERNEL) |
| 1 | 2 | 8 KB | Small DMA descriptors | alloc_pages(GFP_KERNEL, 1) |
| 2 | 4 | 16 KB | ARM32 PGD table | alloc_pages(GFP_KERNEL, 2) |
| 3 | 8 | 32 KB | Medium DMA buffers | alloc_pages(GFP_KERNEL, 3) |
| 4 | 16 | 64 KB | ARM64 page table tables | alloc_pages(GFP_KERNEL, 4) |
| 6 | 64 | 256 KB | Large vmalloc backing | alloc_pages(GFP_KERNEL, 6) |
| 8 | 256 | 1 MB | Large DMA ring buffers | alloc_pages(GFP_KERNEL, 8) |
| 10 | 1024 | 4 MB | Max kmalloc size | alloc_pages(GFP_KERNEL, 10) |
| 11 (MAX) | 2048 | 8 MB | Largest single allocation | alloc_pages(GFP_KERNEL, 11) |


## 11.2 The free_area Structure
The kernel maintains one free_area per order per memory zone. Each free_area contains a list of free page blocks of that order, plus a bitmap for tracking which pages have been split:
| /* include/linux/mmzone.h */struct free_area { struct list_head free_list[MIGRATE_TYPES]; /* per migration type */ unsigned long nr_free; /* total free blocks at this order */};struct zone { /* ... other fields ... */ struct free_area free_area[MAX_ORDER + 1]; /* 12 orders: 0..11 */ /* ... */};/* MIGRATE_TYPES within each order: * MIGRATE_UNMOVABLE (kernel allocations, kmalloc) * MIGRATE_MOVABLE (user process pages, page cache) * MIGRATE_RECLAIMABLE (file-backed, inode cache) * MIGRATE_CMA (CMA reserved region) * MIGRATE_ISOLATE (temporarily isolated for migration) *//* View free pages per order in /proc/buddyinfo: * Node 0, zone Normal 4 2 1 0 1 1 1 0 1 0 3 * ^order0 ^1 ^2 ^3 ^4 ^5 ^6 ^7 ^8 ^9 ^10 */ |


## 11.3 Step-by-Step Block Splitting — Allocation
When the requested order is not available, the buddy allocator finds the next larger order and splits it into buddies until the needed size is obtained:

| REQUEST: alloc_pages(GFP_KERNEL, 0) -- Request 1 page (order-0)INITIAL STATE: Only one free order-2 block (4 contiguous pages P0-P3) free_area[0] -> (empty) free_area[1] -> (empty) free_area[2] -> [P0][P1][P2][P3] <- one free 4-page block=========================================================================STEP 1: Search free_area[0] for order-0 block -> NOT FOUND Search free_area[1] for order-1 block -> NOT FOUND Search free_area[2] for order-2 block -> FOUND [P0][P1][P2][P3]=========================================================================STEP 2: Remove [P0..P3] from free_area[2] Split order-2 into two order-1 buddies: Buddy A = [P0][P1] (lower half) Buddy B = [P2][P3] (upper half) free_area[0] -> (empty) free_area[1] -> [P0][P1] [P2][P3] <- two order-1 blocks free_area[2] -> (empty)=========================================================================STEP 3: Take Buddy A [P0][P1] from free_area[1] Split order-1 into two order-0 buddies: Buddy A1 = [P0] (lower) Buddy A2 = [P1] (upper) free_area[0] -> [P0] [P1] <- two order-0 blocks free_area[1] -> [P2][P3] <- one order-1 block free_area[2] -> (empty)=========================================================================STEP 4: Take [P0] from free_area[0] -> ALLOCATE to callerFINAL STATE after allocation: free_area[0] -> [P1] <- P1 remains free free_area[1] -> [P2][P3] <- P2-P3 remain free free_area[2] -> (empty) Allocated: [P0] (1 page, 4 KB) returned to caller |


Visualized as a split tree:
| [P0][P1][P2][P3] (order-2 block, 4 pages) | +-- [P0][P1] (order-1 buddy A) | | | +-- [P0] <- ALLOCATED (order-0) | +-- [P1] <- stays in free_area[0] | +-- [P2][P3] <- stays in free_area[1] |


## 11.4 Step-by-Step Buddy Coalescing — Free Operation
When freeing a page, the allocator checks if its "buddy" block is also free. If yes, the two are merged into a larger block. This process repeats up through the orders until either the buddy is allocated or the maximum order is reached.

| OPERATION: free_pages(P0, 0) -- Free page P0 (order-0)INITIAL STATE (after previous allocation example): free_area[0] -> [P1] free_area[1] -> [P2][P3] P0 is ALLOCATED (not in any free list)=========================================================================STEP 1: Add P0 to order-0 free list Compute buddy of P0 at order-0: buddy_pfn = pfn(P0) XOR (1 << 0) = 0 XOR 1 = 1 = pfn(P1) Is P1 free at order-0? YES -> proceed to merge=========================================================================STEP 2: Remove P1 from free_area[0] Merge P0 + P1 -> form order-1 block [P0][P1] (The lower-addressed page, P0, becomes the representative) free_area[0] -> (empty) free_area[1] -> [P0][P1] [P2][P3] free_area[2] -> (empty)=========================================================================STEP 3: Compute buddy of [P0][P1] at order-1: buddy_pfn = pfn(P0) XOR (1 << 1) = 0 XOR 2 = 2 = pfn(P2) Is [P2][P3] free at order-1? YES -> proceed to merge=========================================================================STEP 4: Remove [P2][P3] from free_area[1] Merge [P0][P1] + [P2][P3] -> form order-2 block [P0][P1][P2][P3] free_area[0] -> (empty) free_area[1] -> (empty) free_area[2] -> [P0][P1][P2][P3] <- FULLY RESTORED! ^=========================================================================STEP 5: Compute buddy of [P0..P3] at order-2: buddy_pfn = pfn(P0) XOR (1 << 2) = 0 XOR 4 = 4 (not in this region) Buddy is not free (or does not exist) -> STOP merging.RESULT: Memory fully restored to initial state. No fragmentation! |


Visualized as a merge tree:
| [P0] freed + buddy [P1] is free at order-0 ==> merge ==> [P0][P1] (order-1) + buddy [P2][P3] is free at order-1 ==> merge ==> [P0][P1][P2][P3] (order-2) DONE! |


## 11.5 XOR Buddy Address Calculation
The buddy address is calculated with a single XOR operation — one of the most elegant tricks in the Linux kernel. Two blocks are buddies if and only if they are the same size, adjacent, and came from the same parent block.

| /* Given page frame number (pfn) and order, compute buddy pfn: */buddy_pfn = pfn ^ (1UL << order);/* This works because buddy blocks always differ in exactly one bit * (the bit at position "order") and all lower bits are zero. * * Examples: * pfn=0, order=0 -> buddy = 0 ^ 1 = 1 (P0 buddies with P1) * pfn=1, order=0 -> buddy = 1 ^ 1 = 0 (P1 buddies with P0) ✓ symmetric * pfn=0, order=1 -> buddy = 0 ^ 2 = 2 ([P0,P1] buddies with [P2,P3]) * pfn=4, order=1 -> buddy = 4 ^ 2 = 6 ([P4,P5] buddies with [P6,P7]) * pfn=0, order=2 -> buddy = 0 ^ 4 = 4 ([P0..P3] buddies with [P4..P7]) * pfn=8, order=2 -> buddy = 8 ^ 4 = 12 ([P8..P11] buddies with [P12..P15]) * * Rule: A block at pfn is a valid block start iff (pfn & ((1<<order)-1)) == 0 * i.e., the pfn must be naturally aligned to 2^order. *//* Linux kernel source (mm/page_alloc.c): */static inline unsigned long__find_buddy_pfn(unsigned long page_pfn, unsigned int order){ return page_pfn ^ (1 << order);} |


## 11.6 Complete 8-Page Allocation/Free State Sequence
This section shows the complete state of free_area lists across a sequence of allocations and frees on an 8-page physical memory region (P0 through P7). Initial state: all pages free in one order-3 block.

| Physical Pages: [P0] [P1] [P2] [P3] [P4] [P5] [P6] [P7] ^ ^ pfn=0 pfn=7=================================================================STATE 0 - Initial: All free free_area[3] -> [P0..P7] (one order-3 block = 8 pages) free_area[2] -> (empty) free_area[1] -> (empty) free_area[0] -> (empty) Physical map: [FREE][FREE][FREE][FREE][FREE][FREE][FREE][FREE] P0 P1 P2 P3 P4 P5 P6 P7=================================================================ACTION 1: alloc_pages(order=0) -- Allocate 1 page -> gets P0 Splits order-3 -> order-2 [P0..P3] + [P4..P7] Splits order-2 [P0..P3] -> order-1 [P0,P1] + [P2,P3] Splits order-1 [P0,P1] -> order-0 [P0] + [P1] Allocate [P0] free_area[0] -> [P1] free_area[1] -> [P2,P3] free_area[2] -> [P4..P7] free_area[3] -> (empty) Physical map: [ALLOC][FREE][FREE][FREE][FREE][FREE][FREE][FREE]=================================================================ACTION 2: alloc_pages(order=1) -- Allocate 2 pages -> gets [P2,P3] free_area[1] has [P2,P3] -> allocate directly free_area[0] -> [P1] free_area[1] -> (empty) free_area[2] -> [P4..P7] Physical map: [A][F][A][A][F][F][F][F] A=Allocated F=Free=================================================================ACTION 3: alloc_pages(order=2) -- Allocate 4 pages -> gets [P4..P7] free_area[2] has [P4..P7] -> allocate directly free_area[0] -> [P1] free_area[1] -> (empty) free_area[2] -> (empty) Physical map: [A][F][A][A][A][A][A][A] Only P1 is free!=================================================================ACTION 4: free_pages(P0, 0) -- Free P0 P0 freed: buddy = P0 pfn(0) XOR 1 = P1 P1 is free -> merge [P0]+[P1] -> [P0,P1] order-1 Buddy of [P0,P1] = pfn(0) XOR 2 = pfn(2) = [P2,P3] -> ALLOCATED -> stop free_area[0] -> (empty) free_area[1] -> [P0,P1] free_area[2] -> (empty) Physical map: [F][F][A][A][A][A][A][A]=================================================================ACTION 5: free_pages(P2, 1) -- Free [P2,P3] (order=1) [P2,P3] freed: buddy = pfn(2) XOR 2 = pfn(0) = [P0,P1] [P0,P1] is free -> merge [P0,P1]+[P2,P3] -> [P0..P3] order-2 Buddy of [P0..P3] = pfn(0) XOR 4 = pfn(4) = [P4..P7] -> ALLOCATED -> stop free_area[0] -> (empty) free_area[1] -> (empty) free_area[2] -> [P0..P3] Physical map: [F][F][F][F][A][A][A][A]=================================================================ACTION 6: free_pages(P4, 2) -- Free [P4..P7] (order=2) [P4..P7] freed: buddy = pfn(4) XOR 4 = pfn(0) = [P0..P3] [P0..P3] is free -> merge [P0..P3]+[P4..P7] -> [P0..P7] order-3 Buddy of [P0..P7] at order-3: pfn(0) XOR 8 = pfn(8) -> not in region -> stop free_area[3] -> [P0..P7] FULLY RESTORED to initial state! Physical map: [F][F][F][F][F][F][F][F] No fragmentation - buddy system fully recovered the 8-page block! |


## 11.7 Fragmentation Prevention — Migration Types
A key insight of the modern buddy allocator is that not all pages are equal. The allocator maintains separate free lists for each migration type within each order. This prevents unmovable kernel allocations from fragmenting the memory used by movable user pages.

| Within EACH order, there are SEPARATE free lists per migration type: free_area[order].free_list[MIGRATE_UNMOVABLE] <- kernel, kmalloc, DMA free_area[order].free_list[MIGRATE_MOVABLE] <- user pages, page cache free_area[order].free_list[MIGRATE_RECLAIMABLE]<- inode/dentry cache free_area[order].free_list[MIGRATE_CMA] <- CMA reserved regionWhy this prevents fragmentation: SCENARIO without migration types: [USER][KERNEL][USER][KERNEL][USER][KERNEL][USER][KERNEL] free used free used free used free used --> 4 free pages but NO contiguous pair available! SCENARIO with migration types: [USER][USER][USER][USER][KERNEL][KERNEL][KERNEL][KERNEL] free free free free used used used used --> 4 contiguous free pages available for large allocation! Kernel tries to cluster UNMOVABLE allocations together so MOVABLE pages remain in large contiguous regions that can be compacted or freed as huge pages. View migration type statistics: cat /proc/pagetypeinfo Example output: Page block order: 9 Pages per block: 512 Free pages count per migrate type at order 0 1 2 3 4... Node 0, zone Normal Unmovable 4 0 0 1 0... Node 0, zone Normal Movable 32 8 4 2 1... Node 0, zone Normal Reclaimable 4 2 0 0 0... |


## 11.8 Buddy Allocator Properties Summary
| Property | Detail |
| Algorithm | Power-of-2 block splitting and merging with XOR buddy address calculation |
| Time complexity | O(log N) for allocation and free where N is MAX_ORDER (typically 12) |
| Fragmentation type | Suffers from external fragmentation over time (blocks of right size not available). Addressed by compaction. |
| Internal fragmentation | Power-of-2 rounding: requesting 5 pages gets 8 pages (order-3). Wasted pages = 3. SLUB reduces this for small allocs. |
| Anti-fragmentation | Separate free lists per migration type (Unmovable/Movable/Reclaimable) cluster similar types together |
| Coalescing | Aggressive: checks and merges buddies on every free operation, up to MAX_ORDER |
| Lock granularity | Per-zone spinlock for free lists. PCP (Per-CPU Pages) cache reduces lock contention for order-0 pages. |
| Key source file | mm/page_alloc.c: __alloc_pages(), expand() (split), __free_one_page() (merge) |
| Debugging | /proc/buddyinfo (free counts), /proc/pagetypeinfo (per migration type), CONFIG_DEBUG_PAGEALLOC |
| ARM64 relevance | Buddy allocator underpins all memory on ARM64: kmalloc, vmalloc, DMA, CMA. Understanding orders is essential for memory debugging on Qualcomm SoCs. |


## 11.9 Key Kernel Source Functions
| /* mm/page_alloc.c - Key functions to study: */__alloc_pages() // Top-level allocation entry pointget_page_from_freelist() // Searches free lists in each zoneexpand() // Splits a found block down to requested order__free_one_page() // Frees a page and coalesces with buddy__find_buddy_pfn() // XOR buddy address calculationmove_freepages_block() // Moves pages between migration type lists/* include/linux/mmzone.h - Key structures: */struct zone -> contains free_area[] array + zone statsstruct free_area -> free_list[MIGRATE_TYPES] + nr_free countstruct pglist_data -> per-NUMA-node data, contains zones[]/* Useful debug: show buddy state */echo m > /proc/sysrq-trigger // shows memory info in dmesg |


# Appendix: Quick Reference Tables
## A.1 kmalloc Size Classes
| Size class Object size Typical users------------ ------------ -----------------------------------------kmalloc-8 8 B Tiny kernel structureskmalloc-16 16 B Small list nodeskmalloc-32 32 B task_struct fieldskmalloc-64 64 B sk_buff headkmalloc-128 128 B inode metadatakmalloc-256 256 B Medium driver objectskmalloc-512 512 B File system objectskmalloc-1k 1 KB Page table scratch areaskmalloc-2k 2 KB Block I/O requestskmalloc-4k 4 KB One full pagekmalloc-8k 8 KB ARM32 PGD tablekmalloc-16k 16 KB Kernel stack (ARM32)kmalloc-32k 32 KB Large DMA descriptors...kmalloc-4M 4 MB Maximum kmalloc size (order-10)/* kmalloc rounds UP to next size class: kmalloc(100) -> uses kmalloc-128 class (28 bytes wasted) Use ksize(ptr) to find actual allocated size */ |


## A.2 GFP Flags Quick Reference Card
| Context Use Notes-------------------------------- -------------------- ---------------------------Process context (kthread, probe) GFP_KERNEL Best choice, can sleepInterrupt handler GFP_ATOMIC Cannot sleep, may failSpinlock held GFP_ATOMIC Cannot sleep, may failBlock I/O path GFP_NOIO No I/O, prevents deadlockFilesystem code GFP_NOFS No FS opsDMA buffer (legacy devices) GFP_DMA | GFP_KERNEL From ZONE_DMA (<16MB)User page allocation GFP_HIGHUSER From ZONE_HIGHMEM (32-bit)Must not fail GFP_KERNEL|__GFP_NOFAIL Use with care!Zero initialize GFP_KERNEL|__GFP_ZERO or kzalloc()Combine modifiers: kzalloc(size, GFP_KERNEL) = kmalloc + zero kmalloc(size, GFP_ATOMIC|__GFP_NOWARN) = atomic, silent on failure alloc_pages(GFP_KERNEL|__GFP_COMP, order) = compound/huge page |


## A.3 /proc/buddyinfo Interpretation
| $ cat /proc/buddyinfoNode 0, zone DMA 1 1 1 0 2 1 1 0 1 1 3Node 0, zone DMA32 2 1 3 0 4 2 1 1 2 0 1Node 0, zone Normal 7 5 2 1 2 4 2 1 0 0 1 ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ Order: 0 1 2 3 4 5 6 7 8 9 10 Pages: 1 2 4 8 16 32 64 128 256 512 1024 Size(4K): 4K 8K 16K 32K 64K 128K 256K 512K 1M 2M 4MReading: "Zone Normal has 7 free 4KB pages, 5 free 8KB blocks, 2 free 16KB blocks, ..., 1 free 4MB block"Fragmentation indicator: Many small blocks, no large = fragmented.Healthy system: Some blocks at all orders, including higher orders.$ cat /proc/vmstat | grep -E "pgalloc|pgfree|compact"pgalloc_dma 12345 <- buddy allocs from DMA zonepgalloc_normal 5678901 <- buddy allocs from Normal zonecompact_success 234 <- compaction freed large blockscompact_fail 12 <- compaction failed (unmovable anchors) |


― End of Part 2 ―
Continue with Part 3: SLUB Internals, Memory Compaction, CMA, DMA Coherency, SMMU, MMU, and Boot Sequence

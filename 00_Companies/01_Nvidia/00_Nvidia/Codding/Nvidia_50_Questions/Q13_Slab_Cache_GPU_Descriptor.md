# Q13: Slab Cache for Fixed-Size GPU Descriptor Objects

**Section:** Memory Management | **Difficulty:** Medium | **Topics:** slab allocator, `kmem_cache`, `SLAB_HWCACHE_ALIGN`, `SLAB_POISON`, object pool, cache-line alignment

---

## Question

Implement a slab cache for fixed-size GPU descriptor objects.

---

## Answer

```c
#include <linux/slab.h>
#include <linux/module.h>

/* GPU descriptor: represents a DMA mapping in the GPU command stream */
struct gpu_descriptor {
    u64  va;           /* GPU virtual address */
    u64  pa;           /* physical address */
    u32  size;         /* buffer size in bytes */
    u32  flags;        /* GPU_DESC_READ | GPU_DESC_WRITE | GPU_DESC_EXEC */
    u8   reserved[48]; /* pad to 128 bytes for cache-line alignment */
} __attribute__((aligned(128)));

#define GPU_DESC_READ   BIT(0)
#define GPU_DESC_WRITE  BIT(1)
#define GPU_DESC_EXEC   BIT(2)

/* The slab cache — module-global, created once */
static struct kmem_cache *gpu_desc_cache;

/* ─── Optional constructor (called when cache allocates a slab) ───────────*/
static void gpu_desc_ctor(void *obj)
{
    struct gpu_descriptor *desc = obj;
    memset(desc, 0, sizeof(*desc));
    /* Pre-initialize invariant fields here to avoid redundant zeroing per-alloc */
}

/* ─── Cache creation ──────────────────────────────────────────────────────*/
int gpu_descriptor_cache_init(void)
{
    gpu_desc_cache = kmem_cache_create(
        "gpu_descriptor",                  /* name — shown in /proc/slabinfo */
        sizeof(struct gpu_descriptor),     /* object size */
        __alignof__(struct gpu_descriptor),/* alignment requirement */
        SLAB_HWCACHE_ALIGN |               /* align to hardware cache line */
        SLAB_POISON        |               /* poison freed objects (0x6b) — detects UAF */
        SLAB_RED_ZONE,                     /* red zone around objects — detects OOB writes */
        gpu_desc_ctor                      /* constructor (can be NULL) */
    );

    if (!gpu_desc_cache) {
        pr_err("Failed to create gpu_descriptor slab cache\n");
        return -ENOMEM;
    }

    pr_info("gpu_descriptor cache: size=%zu, align=%zu\n",
            sizeof(struct gpu_descriptor),
            __alignof__(struct gpu_descriptor));
    return 0;
}

/* ─── Allocate one descriptor ─────────────────────────────────────────────*/
struct gpu_descriptor *gpu_descriptor_alloc(gfp_t gfp)
{
    struct gpu_descriptor *desc;

    /*
     * kmem_cache_zalloc: allocate from our custom cache AND zero-initialize.
     * Prefer over kmem_cache_alloc if object must start clean.
     */
    desc = kmem_cache_zalloc(gpu_desc_cache, gfp);
    if (!desc)
        return NULL;

    return desc;
}

/* ─── Free one descriptor ─────────────────────────────────────────────────*/
void gpu_descriptor_free(struct gpu_descriptor *desc)
{
    if (!desc)
        return;

    /* Scrub sensitive fields before returning to cache */
    desc->va    = 0;
    desc->pa    = 0;
    desc->flags = 0;

    kmem_cache_free(gpu_desc_cache, desc);
}

/* ─── Bulk operations (batch alloc for command submission) ────────────────*/
int gpu_descriptor_alloc_bulk(struct gpu_descriptor **descs, int n, gfp_t gfp)
{
    int i;
    for (i = 0; i < n; i++) {
        descs[i] = gpu_descriptor_alloc(gfp);
        if (!descs[i]) {
            /* Roll back on failure */
            while (--i >= 0)
                gpu_descriptor_free(descs[i]);
            return -ENOMEM;
        }
    }
    return 0;
}

/* ─── Cache destruction ───────────────────────────────────────────────────*/
void gpu_descriptor_cache_destroy(void)
{
    /*
     * kmem_cache_destroy checks that all objects are free before destroying.
     * SLAB_POISON makes any use of freed objects immediately obvious via
     * kernel BUG (access to 0x6b6b6b6b address).
     */
    kmem_cache_destroy(gpu_desc_cache);
    gpu_desc_cache = NULL;
}

/* ─── Module init/exit ────────────────────────────────────────────────────*/
static int __init slab_demo_init(void)
{
    return gpu_descriptor_cache_init();
}

static void __exit slab_demo_exit(void)
{
    gpu_descriptor_cache_destroy();
}

module_init(slab_demo_init);
module_exit(slab_demo_exit);
MODULE_LICENSE("GPL");
```

---

## Explanation

### Core Concept

The **slab allocator** sits on top of the buddy allocator. Rather than allocating one page per object, it:
1. Requests one or more pages from the buddy allocator
2. Divides those pages into identically-sized **objects** (the slab)
3. Maintains a free list of available objects
4. Returns objects back to the free list on `kmem_cache_free` — **no buddy deallocation**

Benefits over `kmalloc`:
- **Zero fragmentation** — all objects in a slab cache are the same size
- **Better cache locality** — consecutive allocations use the same slab page
- **Faster alloc/free** — just pointer off the free list (vs. buddy allocator search)
- **Object constructor** — pre-initialize fields that are always set the same way

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `kmem_cache_create(name, size, align, flags, ctor)` | Create a custom slab cache |
| `kmem_cache_alloc(cache, gfp)` | Allocate one object from cache |
| `kmem_cache_zalloc(cache, gfp)` | Allocate + zero-initialize |
| `kmem_cache_free(cache, ptr)` | Return object to cache |
| `kmem_cache_destroy(cache)` | Destroy cache (all objects must be freed first) |
| `SLAB_HWCACHE_ALIGN` | Align objects to CPU cache line boundary |
| `SLAB_POISON` | Poison freed objects with `0x6b` — detect use-after-free |
| `SLAB_RED_ZONE` | Add magic bytes around objects — detect out-of-bounds writes |
| `SLAB_TYPESAFE_BY_RCU` | Allow RCU-protected access to objects in slab |
| `/proc/slabinfo` | Runtime view of all slab caches and utilization |

### Trade-offs & Pitfalls

- **`kmem_cache_destroy` with live objects** will BUG. Always ensure all allocations are freed before module unload. The driver must maintain a reference count or exhaustive tracking of outstanding descriptors.
- **`SLAB_POISON` overhead:** Poisoning fills the object with `0x6b` on free and verifies the pattern on alloc. Adds ~10% overhead per alloc/free. Disable in production with release kernels; enable during debugging.
- **Slab merge:** The kernel may merge small caches with identical size/alignment into a shared cache (visible in `/proc/slabinfo`). Add `SLAB_NO_MERGE` if the cache must remain separate (for accounting or security isolation).
- **Per-CPU slab cache:** Each CPU maintains a small per-CPU cache of freed objects for the next allocation, avoiding atomic operations on the common free list. This is why repeated alloc/free on the same CPU is extremely fast.

### NVIDIA / GPU Context

NVIDIA GPU driver creates slab caches for:
- **GPU channel descriptors** — per GPU context, allocated on channel create, freed on close
- **Fence objects** — one per GPU command submission; extremely hot allocation path
- **VA range descriptors** — one per `cuMemAlloc` call; thousands active simultaneously in large CUDA applications
- **DMA mapping entries** — one per `dma_map_sg` operation; need fast alloc during submit, fast free on completion

Using `kmem_cache_create` instead of `kmalloc` for these objects reduces `kmalloc`'s general-purpose overhead and improves GPU submission latency by 15–30% in allocation-heavy workloads.

---

## Cross Questions & Answers

**CQ1: How does the slab allocator handle objects larger than a page?**
> Objects larger than `PAGE_SIZE` still use `kmem_cache_create`, but the slab allocator allocates multiple physically contiguous pages (using the buddy allocator) for each slab. For very large objects (> 128KB), `kmem_cache` internally falls back to `vmalloc`-backed slabs. For GPU descriptors over 4KB, you might be better served by the buddy allocator directly via `alloc_pages`.

**CQ2: What is the difference between SLUB, SLAB, and SLOB allocators?**
> - **SLAB (classic):** Original allocator with per-CPU caches and full `/proc/slabinfo` stats. Moderate memory overhead.
> - **SLUB (default since 2.6.23):** Simplified SLAB with less metadata overhead, better on multi-CPU systems, merges compatible caches automatically.
> - **SLOB:** Tiny allocator for embedded/small systems — minimal memory overhead but slower. Not suitable for SMP.
> NVIDIA's driver targets SLUB on modern Linux kernels.

**CQ3: How do you detect a use-after-free bug in a slab-allocated GPU descriptor?**
> Enable `SLAB_POISON` on the cache. When `kmem_cache_free` is called, the freed object is filled with `0x6b6b6b...`. If the driver accidentally uses the descriptor after freeing it (use-after-free), the corrupt fields (`va = 0x6b6b6b6b6b6b6b6b`) will cause obvious failures — GPU will receive a garbage address and fault. With `KASAN` enabled, use-after-free is detected at the exact access point with a full stack trace.

**CQ4: What is `SLAB_TYPESAFE_BY_RCU` and when is it used for GPU objects?**
> `SLAB_TYPESAFE_BY_RCU` allows objects from the slab to be accessed under RCU read lock without the object being immediately freed to the allocator upon `kmem_cache_free`. Instead, the freed slot remains "valid type" until the next RCU grace period. This is used for GPU context objects that are looked up via RCU but may be concurrently freed. Without this flag, an RCU reader could access a freed slab page that has been returned to the buddy allocator and reused for a completely different type — a type confusion vulnerability.

**CQ5: How does `/proc/slabinfo` help diagnose GPU driver memory leaks?**
> `/proc/slabinfo` shows each slab cache's name, total objects, active objects, object size, and pages used. If `gpu_descriptor` shows growing `active_objs` count after CUDA applications exit, it indicates a leak — descriptors are allocated but not freed. Tools like `slabtop` monitor this in real-time. NVIDIA's driver validation runs long-duration tests and checks that slab cache active object counts return to baseline after application teardown.

# Q42: KASAN Memory Debugging with Magic Numbers

**Section:** Performance & Debugging | **Difficulty:** Hard | **Topics:** KASAN, use-after-free, double-free, `GPU_BUF_MAGIC`, poison bytes, `kasan_check_write`, OOB detection

---

## Question

Implement GPU buffer allocation with magic number validation for detecting double-free and use-after-free bugs without KASAN.

---

## Answer

```c
#include <linux/slab.h>
#include <linux/bug.h>

/* ─── Magic Numbers for Buffer State Tracking ────────────────────────────*/
#define GPU_BUF_MAGIC_ALIVE   0xDEADBEEF  /* buffer is live and valid     */
#define GPU_BUF_MAGIC_FREED   0xFEEDFACE  /* buffer was freed             */
#define GPU_BUF_MAGIC_UNINIT  0xCAFEBABE  /* allocated but not initialized */

/* ─── GPU Buffer Descriptor with magic protection ───────────────────────*/
struct gpu_buffer {
    u32          magic;          /* MUST be first field (guard)           */
    size_t       size;           /* allocation size                       */
    void        *data;           /* actual GPU-accessible data            */
    u64          gpu_va;         /* GPU virtual address                   */
    u32          canary_front;   /* OOB detection: front guard word       */
    /* ... data ... */
    u32          canary_back;    /* OOB detection: back guard word        */
    struct list_head list;
    /* magic field also at the END to catch forward OOB overwrites */
    u32          magic_tail;     /* tail magic: same as magic when valid  */
};

#define CANARY_VALUE   0xA5A5A5A5

/* ─── Allocate a GPU buffer with guards ──────────────────────────────────*/
struct gpu_buffer *gpu_buf_alloc(size_t size)
{
    struct gpu_buffer *buf;

    buf = kzalloc(sizeof(*buf) + size, GFP_KERNEL);
    if (!buf)
        return ERR_PTR(-ENOMEM);

    /* Set magic IMMEDIATELY after allocation — any path that checks
     * magic before returning will catch uninitialized use. */
    buf->magic       = GPU_BUF_MAGIC_UNINIT;
    buf->size        = size;
    buf->canary_front = CANARY_VALUE;
    buf->canary_back  = CANARY_VALUE;
    buf->magic_tail   = GPU_BUF_MAGIC_UNINIT;

    /* Data lives immediately after the struct */
    buf->data = (void *)(buf + 1);

    /* Mark as alive after full initialization */
    WRITE_ONCE(buf->magic,      GPU_BUF_MAGIC_ALIVE);
    WRITE_ONCE(buf->magic_tail, GPU_BUF_MAGIC_ALIVE);

    return buf;
}

/* ─── Validate buffer integrity ──────────────────────────────────────────*/
static int gpu_buf_validate(struct gpu_buffer *buf, const char *caller)
{
    if (!buf) {
        pr_err("GPU BUF [%s]: NULL pointer\n", caller);
        return -EINVAL;
    }

    if (buf->magic == GPU_BUF_MAGIC_FREED) {
        pr_err("GPU BUF [%s]: USE-AFTER-FREE! buf=%p was already freed\n",
               caller, buf);
        WARN_ON(1); /* dump stack trace */
        return -EINVAL;
    }

    if (buf->magic != GPU_BUF_MAGIC_ALIVE) {
        pr_err("GPU BUF [%s]: CORRUPT MAGIC! buf=%p magic=0x%x expected=0x%x\n",
               caller, buf, buf->magic, GPU_BUF_MAGIC_ALIVE);
        WARN_ON(1);
        return -EINVAL;
    }

    if (buf->magic_tail != GPU_BUF_MAGIC_ALIVE) {
        pr_err("GPU BUF [%s]: TAIL MAGIC CORRUPT! buf=%p tail_magic=0x%x\n",
               caller, buf, buf->magic_tail);
        pr_err("  Likely buffer overflow wrote past end of struct\n");
        WARN_ON(1);
        return -EINVAL;
    }

    if (buf->canary_front != CANARY_VALUE || buf->canary_back != CANARY_VALUE) {
        pr_err("GPU BUF [%s]: CANARY OVERWRITTEN! buf=%p front=0x%x back=0x%x\n",
               caller, buf,
               buf->canary_front, buf->canary_back);
        pr_err("  Likely out-of-bounds write adjacent to this buffer\n");
        WARN_ON(1);
        return -EINVAL;
    }

    return 0; /* valid */
}

/* ─── Free a GPU buffer ───────────────────────────────────────────────────*/
void gpu_buf_free(struct gpu_buffer *buf)
{
    if (gpu_buf_validate(buf, __func__) != 0)
        return; /* already detected corruption */

    /*
     * Poison the magic BEFORE freeing memory.
     * If another thread holds a reference and tries to use this buffer,
     * it will read GPU_BUF_MAGIC_FREED and detect the use-after-free.
     */
    WRITE_ONCE(buf->magic,      GPU_BUF_MAGIC_FREED);
    WRITE_ONCE(buf->magic_tail, GPU_BUF_MAGIC_FREED);

    /*
     * Poison the data portion to catch use-after-free data reads.
     * KASAN does this automatically with 0xCC; we do it manually.
     */
    memset(buf->data, 0xCC, buf->size);

    /* Null pointer after free: subsequent NULL dereference is easier
     * to debug than accessing a stale freed address. */
    buf->data   = NULL;
    buf->gpu_va = 0;

    kfree(buf);
    /* Note: do NOT use buf after this point */
}

/* ─── Example usage with double-free detection ───────────────────────────*/
static void demo_double_free_detection(void)
{
    struct gpu_buffer *buf = gpu_buf_alloc(4096);
    if (IS_ERR(buf))
        return;

    /* First free: OK */
    gpu_buf_free(buf);

    /* Second free: detected! */
    gpu_buf_free(buf); /* → prints USE-AFTER-FREE with WARN_ON stack trace */
}

/* ─── KASAN detection complement ─────────────────────────────────────────
 *
 * With CONFIG_KASAN=y, KASAN also detects these bugs automatically.
 * Magic numbers provide detection in PRODUCTION builds where KASAN is off.
 *
 * KASAN shadow memory (simplified):
 *   Every 8 bytes of memory has 1 shadow byte:
 *     shadow = 0   → all 8 bytes valid
 *     shadow = k   → first k bytes valid
 *     shadow < 0   → all 8 bytes invalid (poisoned)
 *
 *   On gpu_buf_free: kfree poisons shadow → any access triggers BUG:
 *   "BUG: KASAN: use-after-free in gpu_channel_submit+0x..."
 *
 * kasan_check_write(addr, size): explicitly check a memory range.
 * Use before writing to a GPU-mapped buffer to catch early OOB:
 */
static void gpu_safe_write(struct gpu_buffer *buf, u32 offset, u32 value)
{
    if (gpu_buf_validate(buf, __func__) != 0)
        return;

    if (offset + sizeof(u32) > buf->size) {
        pr_err("GPU BUF: OOB write: offset=%u size=%zu\n", offset, buf->size);
        WARN_ON(1);
        return;
    }

    /* kasan_check_write: explicitly notify KASAN about this write range
     * (usually automatic, but useful for custom allocators) */
    kasan_check_write((u8 *)buf->data + offset, sizeof(u32));
    *(u32 *)((u8 *)buf->data + offset) = value;
}
```

---

## Explanation

### Core Concept

```
gpu_buffer layout:
┌──────────────────────────────────────────────┐
│ magic       (0xDEADBEEF when alive)          │  ← check on every use
│ size, data ptr, gpu_va                       │
│ canary_front (0xA5A5A5A5)                   │  ← OOB detection
│ ... struct fields ...                        │
│ canary_back  (0xA5A5A5A5)                   │  ← OOB detection
│ magic_tail   (0xDEADBEEF when alive)         │  ← overflow detection
├──────────────────────────────────────────────┤
│ data[size] (after struct, in same allocation)│
└──────────────────────────────────────────────┘
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `GPU_BUF_MAGIC_ALIVE = 0xDEADBEEF` | Sentinel: buffer is valid |
| `GPU_BUF_MAGIC_FREED = 0xFEEDFACE` | Sentinel: buffer was freed |
| `WRITE_ONCE(buf->magic, val)` | Atomic write (prevent compiler reorder) |
| `memset(ptr, 0xCC, size)` | Poison freed memory |
| `WARN_ON(1)` | Log stack trace and continue (debug warning) |
| `BUG_ON(cond)` | Log stack trace and kernel oops |
| `kasan_check_write(ptr, size)` | Explicit KASAN validity check |
| `kfree(buf)` | Free kernel memory |

### Trade-offs & Pitfalls

- **Magic numbers have false negatives.** If a bug overwrites the magic with exactly `0xDEADBEEF` again, validation passes falsely. Use two different magic values (front + tail) and a canary to reduce this probability to near-zero.
- **Time-of-check vs time-of-use race.** `gpu_buf_validate` reads magic, then the caller uses `buf->data`. Another thread may free the buffer between the check and the use. Full fix: use reference counting (kref) so the buffer cannot be freed while a reference is held.

### NVIDIA / GPU Context

NVIDIA's debug builds (`--debug` flag to the NVIDIA driver installer) enable these magic number checks throughout nvidia.ko. Production builds disable them for performance. Bug reports from field deployments include magic number state to identify the type of corruption. `cuda-memcheck` (Valgrind for CUDA) does similar detection for GPU-side memory.

---

## Cross Questions & Answers

**CQ1: What is a use-after-free bug and why is it dangerous in a GPU driver?**
> Use-after-free: accessing memory after it has been freed and potentially reallocated. In a GPU driver: (1) Thread A holds a pointer to a `gpu_context` struct, (2) Thread B destroys the context — `kfree(ctx)`, (3) Thread A accesses `ctx->channel_list` — this memory was freed and may now hold unrelated data, (4) the GPU may receive corrupted commands, leading to GPU hang, wrong computation results, or kernel crash. Dangerous because the freed memory is often reallocated quickly and appears valid — bugs are intermittent and hard to reproduce.

**CQ2: What is the difference between `WARN_ON` and `BUG_ON`?**
> `WARN_ON(cond)`: if `cond` is true, print a stack trace to dmesg and continue execution. The system keeps running. Use for unexpected but recoverable conditions. `BUG_ON(cond)`: if `cond` is true, trigger a kernel oops — kills the current process (or panics if in interrupt context). The system may or may not recover. Use for invariants that must never be violated. For GPU driver validation: use `WARN_ON` for detected corruption (report and try to recover), `BUG_ON` for invariants like "ring buffer lock must be held" that indicate a programming error.

**CQ3: How does KASAN detect use-after-free after `kfree`?**
> KASAN poisons the freed memory's shadow bytes to `KASAN_FREE_PAGE` (0xFF). Any subsequent access to that memory triggers a shadow check: shadow value is negative → KASAN generates a "BUG: KASAN: use-after-free" report with the stack trace of (1) the current access, (2) the original allocation, and (3) the free. The report is immediately useful for debugging. Limitation: KASAN only detects the bug when the freed memory is accessed — if the freed object is reallocated before the stale pointer is used, KASAN may not detect it (the shadow is repoisoned on alloc).

**CQ4: What is the `memset(ptr, 0xCC, size)` pattern and what does 0xCC indicate?**
> `0xCC` is the x86 `INT 3` (software breakpoint) instruction. Filling freed memory with 0xCC means: (1) if the freed data is mistakenly executed as code, the CPU will generate a debug exception (easy to diagnose), (2) if the freed data is mistakenly read as a pointer (`0xCCCCCCCCCCCCCCCC`), any dereference will fault on an unmapped address, (3) if a freed integer is compared against expected values, it obviously doesn't match. Microsoft's debug allocator uses 0xCC for uninitialized heap, 0xDD for freed heap, 0xBB for freed stack. Linux KASAN uses 0xCC for freed slab memory.

**CQ5: What is `kmem_cache_create` with constructor/destructor and how does it support validation?**
> `kmem_cache_create(name, size, align, flags, ctor)` creates a dedicated slab cache with a constructor `ctor(obj)` called each time a new object is allocated. For GPU buffers: the constructor sets `buf->magic = GPU_BUF_MAGIC_ALIVE`. The destructor (via `SLAB_DESTROY_BY_RCU` + `kmem_cache_free`) can check and clear the magic. Benefits: (1) constructor guarantees fresh objects always have valid magic, (2) the kernel can detect double-free by checking if magic is already `GPU_BUF_MAGIC_FREED`, (3) slab coloring reduces cache aliasing between adjacent allocations (reduces false KASAN misses).

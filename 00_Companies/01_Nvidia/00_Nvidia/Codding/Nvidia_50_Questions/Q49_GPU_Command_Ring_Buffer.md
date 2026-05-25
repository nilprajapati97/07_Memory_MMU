# Q49: GPU Command Ring Buffer

**Section:** Performance & Algorithms | **Difficulty:** Hard | **Topics:** ring buffer, MMIO, `writel`, `wmb`, doorbell, GPU command submission, `readl`, circular buffer

---

## Question

Implement a GPU command ring buffer with MMIO-based doorbell signaling and hardware head/tail management.

---

## Answer

```c
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

/* ─── GPU Command Ring Buffer ─────────────────────────────────────────────
 *
 *  Shared between CPU (producer) and GPU (consumer):
 *
 *  CPU writes:           tail → where CPU writes next command
 *  GPU reads:            head → where GPU reads next command
 *  Available space:      (head - tail - 1 + ring_size) % ring_size
 *
 *  CPU advances tail after write → signals GPU via doorbell
 *  GPU advances head after consuming → readable by CPU
 *
 *  MMIO layout:
 *   hw_ring[0..ring_size-1]:  command slots (64-bit commands)
 *   doorbell:                  CPU writes tail here to signal GPU
 */

#define GPU_RING_SIZE     4096   /* number of command slots (must be power of 2) */
#define GPU_RING_MASK     (GPU_RING_SIZE - 1)
#define GPU_CMD_NOP       0x0000000000000000ULL
#define GPU_CMD_FENCE     0x0000000100000000ULL /* fence command type */
#define GPU_RING_TIMEOUT_MS 5000 /* 5-second stuck detection */

struct gpu_ring {
    u64 __iomem *hw_ring;  /* MMIO-mapped ring buffer (GPU-accessible) */
    u32 __iomem *doorbell; /* doorbell register: write tail to signal GPU */
    u32 __iomem *hw_head;  /* GPU's head register (read-only for CPU) */

    u32          tail;     /* CPU's view of the write pointer (local) */
    u32          cached_head; /* CPU's cached read of GPU's head       */
    u32          ring_size;   /* number of slots in ring               */

    spinlock_t   lock;         /* protect concurrent submitters        */
    wait_queue_head_t space_wq; /* wait for ring space to become available */

    struct gpu_device *gpu;
};

/* ─── Initialize ring buffer ─────────────────────────────────────────────*/
int gpu_ring_init(struct gpu_ring *ring, struct gpu_device *gpu,
                   void __iomem *bar0_base)
{
    ring->gpu       = gpu;
    ring->ring_size = GPU_RING_SIZE;
    ring->tail      = 0;
    ring->cached_head = 0;

    /*
     * MMIO layout in BAR0:
     *   0x0000: command ring (GPU_RING_SIZE × 8 bytes)
     *   0x8000: doorbell register
     *   0x8004: GPU head register
     */
    ring->hw_ring  = bar0_base + 0x0000;
    ring->doorbell = bar0_base + 0x8000;
    ring->hw_head  = bar0_base + 0x8004;

    spin_lock_init(&ring->lock);
    init_waitqueue_head(&ring->space_wq);

    /* Write initial NOP to slot 0 so ring appears valid to GPU */
    writeq(GPU_CMD_NOP, &ring->hw_ring[0]);
    wmb(); /* ensure NOP is visible before ring is enabled */

    return 0;
}

/* ─── Available space in ring ────────────────────────────────────────────*/
static u32 ring_space(struct gpu_ring *ring)
{
    /*
     * Reserve one slot: full ring (tail+1 == head) is indistinguishable
     * from empty ring (tail == head). So max usable = ring_size - 1.
     */
    return (ring->cached_head - ring->tail - 1) & GPU_RING_MASK;
}

/* ─── Refresh cached head from GPU ──────────────────────────────────────*/
static void ring_update_head(struct gpu_ring *ring)
{
    /*
     * readl: read 32-bit MMIO register.
     * GPU updates hw_head after consuming each command.
     * rmb() ensures we see GPU's latest write to hw_head.
     */
    rmb();
    ring->cached_head = readl(ring->hw_head);
}

/* ─── Wait for ring space ────────────────────────────────────────────────*/
static int ring_wait_space(struct gpu_ring *ring, u32 num_slots)
{
    unsigned long deadline = jiffies + msecs_to_jiffies(GPU_RING_TIMEOUT_MS);

    while (ring_space(ring) < num_slots) {
        ring_update_head(ring);

        if (ring_space(ring) >= num_slots)
            break;

        if (time_after(jiffies, deadline)) {
            pr_err("GPU Ring: timeout waiting for space "
                   "(need %u, avail %u, head=%u, tail=%u)\n",
                   num_slots, ring_space(ring),
                   ring->cached_head, ring->tail);
            return -ETIMEDOUT;
        }

        /* Yield CPU: allow GPU interrupt handler to advance head */
        cond_resched();
    }
    return 0;
}

/* ─── Submit a single command to the ring ────────────────────────────────*/
int ring_submit(struct gpu_ring *ring, u64 cmd)
{
    int ret;

    spin_lock(&ring->lock);

    /* Ensure there's at least one free slot */
    ret = ring_wait_space(ring, 1);
    if (ret) {
        spin_unlock(&ring->lock);
        return ret;
    }

    /*
     * Write command to ring slot at current tail position.
     * writeq: MMIO 64-bit write (write quadword to I/O memory).
     * The hw_ring is __iomem — must use writeq/writel, NOT dereference.
     */
    writeq(cmd, &ring->hw_ring[ring->tail]);

    /*
     * wmb() (write memory barrier): ensures the command write to MMIO
     * is committed BEFORE the doorbell write.
     * Without wmb(): CPU/PCIe bridge may reorder doorbell before command,
     * causing GPU to process the old command at that slot.
     */
    wmb();

    /* Advance tail */
    ring->tail = (ring->tail + 1) & GPU_RING_MASK;

    /*
     * Ring doorbell: write new tail to GPU.
     * GPU sees the new tail and processes commands up to tail.
     * writel: 32-bit MMIO write (doorbell is 32-bit).
     */
    writel(ring->tail, ring->doorbell);

    spin_unlock(&ring->lock);
    return 0;
}

/* ─── Submit a batch of commands ─────────────────────────────────────────
 * More efficient: write N commands, then ONE doorbell ring.
 */
int ring_submit_batch(struct gpu_ring *ring, const u64 *cmds, u32 count)
{
    u32 i;
    int ret;

    spin_lock(&ring->lock);

    ret = ring_wait_space(ring, count);
    if (ret) {
        spin_unlock(&ring->lock);
        return ret;
    }

    /* Write all commands first */
    for (i = 0; i < count; i++) {
        writeq(cmds[i], &ring->hw_ring[ring->tail]);
        ring->tail = (ring->tail + 1) & GPU_RING_MASK;
    }

    /* One wmb() + one doorbell for the entire batch */
    wmb();
    writel(ring->tail, ring->doorbell);

    spin_unlock(&ring->lock);
    return 0;
}

/* ─── GPU interrupt: head advance notification ───────────────────────────
 * Called from GPU completion interrupt handler.
 * Wakes up any threads waiting for ring space.
 */
void gpu_ring_completion_irq(struct gpu_ring *ring)
{
    ring_update_head(ring);
    wake_up(&ring->space_wq);
}
```

---

## Explanation

### Core Concept

```
Ring Buffer (size=8, head=2, tail=5):

Index:  0   1   2   3   4   5   6   7
       [NOP][NOP][↑H][cmd][cmd][↑T][   ][   ]
                head                tail
        ←consumed→     ←pending→    ←free→

CPU writes at tail (5,6,7,0,1) and rings doorbell.
GPU reads from head (2,3,4) and advances head.

Doorbell write:  CPU → GPU: "I've written up to tail=5"
Head MMIO read:  CPU ← GPU: "I've consumed up to head=4"
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `writeq(val, addr)` | 64-bit MMIO write (atomic on PCIe) |
| `writel(val, addr)` | 32-bit MMIO write (doorbell) |
| `readl(addr)` | 32-bit MMIO read (GPU head register) |
| `wmb()` | Write memory barrier (command before doorbell) |
| `rmb()` | Read memory barrier (before reading GPU head) |
| `& GPU_RING_MASK` | Efficient modulo for power-of-2 size ring |
| `cond_resched()` | Yield CPU while waiting for ring space |
| `init_waitqueue_head` | Initialize wait queue for space availability |

### Trade-offs & Pitfalls

- **`wmb()` before doorbell is mandatory.** Without it: on architectures with weak memory ordering (ARM, POWER), the doorbell write may reach the GPU before the command write. GPU reads an old (stale) command. x86 has strong ordering by default but `wmb()` is still needed for MMIO writes (PCIe ordering rules are separate from CPU memory ordering).
- **Single doorbell per batch.** Ringing the doorbell once per batch (not once per command) reduces PCIe MMIO write overhead significantly. MMIO writes are expensive (~200–500ns PCIe round-trip). Batch submissions amortize this cost.

### NVIDIA / GPU Context

NVIDIA GPU command rings are called "push buffers". The UVM (Universal Virtual Memory) and CUDA runtime submit GPU commands (DMA copies, compute dispatches) by writing to push buffers and ringing a doorbell in GPU BAR1. NVIDIA's push buffer format supports a "non-stall interrupt" command that generates an interrupt when GPU reaches that point — used for fence completion notification.

---

## Cross Questions & Answers

**CQ1: Why is a power-of-2 ring size important and how does it optimize modulo?**
> `(tail + 1) % ring_size` requires an integer division — ~20 cycles on modern CPUs. For `ring_size = 2^N`: `(tail + 1) & (ring_size - 1)` — a single AND instruction. For a ring buffer that submits 1 million commands/second: `% ring_size` → 20M cycles/second overhead; `& mask` → ~0. The ring_size being power-of-2 also simplifies space calculation: `(head - tail - 1) & mask` gives available slots without conditional logic.

**CQ2: What is the doorbell mechanism and why is it preferred over polling?**
> Doorbell: a dedicated MMIO register the CPU writes to signal the GPU. The GPU's PCIe endpoint logic watches the doorbell register. When a write arrives, the GPU's command processor wakes up and starts fetching from the ring buffer. Alternative — polling: GPU continuously reads the tail register in a tight loop. Polling wastes GPU power and bandwidth (PCIe reads every microsecond). Doorbell: GPU is idle (clock-gated) until the write arrives — saves ~5W at idle. Doorbells are the universal mechanism in GPUs, NICs, and other PCIe devices.

**CQ3: What happens if the ring overflows (tail catches up to head)?**
> Ring overflow: `tail + 1 == head` — no free slots. The CPU must wait. This implementation uses `ring_wait_space` with busy-wait + `cond_resched`. Production GPU drivers use interrupt-driven waits: (1) GPU fires an interrupt when it advances `head` past a threshold, (2) interrupt handler calls `wake_up(&ring->space_wq)`, (3) CPU thread wakes from `wait_event_interruptible(ring->space_wq, ring_space(ring) >= 1)`. This avoids busy-waiting at the cost of interrupt latency (~5µs). Choose based on expected wait duration.

**CQ4: What is the difference between `writeq`/`writel` and regular pointer dereference for MMIO?**
> MMIO addresses are `void __iomem *` — a distinct pointer type in the kernel. Regular dereference (`*ptr = val`) bypasses MMIO semantics: (1) the compiler may cache the value in a register (won't re-read MMIO on each access), (2) the CPU may use non-MMIO store instructions that don't enforce PCIe ordered writes. `writel`/`writeq` use `volatile` semantics (no caching) and the correct store instruction for I/O space. On x86: `writel` issues `MOV` to uncacheable (UC) memory type (set via MTRR/PAT). On ARM: uses device memory (non-cacheable, non-bufferable) mapping.

**CQ5: How do you handle GPU reset with an in-flight command ring?**
> GPU reset procedure for ring buffer: (1) stop submitting: set `ring->enabled = false`, prevent new `ring_submit` calls. (2) wait for in-flight commands: `wait_event_timeout(ring->space_wq, ring->cached_head == ring->tail, timeout)` — wait until GPU consumed all pending commands. (3) if timeout: force GPU reset anyway. (4) perform GPU reset (FLR or SW reset). (5) reinitialize ring: reset `tail = head = 0`, write NOP to slot 0, re-enable submission. (6) resubmit any commands that were in flight when reset happened (return error to callers — they must retry). This matches how NVIDIA handles GPU warm resets.

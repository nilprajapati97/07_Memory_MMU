# Q25: Atomic State Machine for GPU Power Management

**Section:** Concurrency & Synchronization | **Difficulty:** Hard | **Topics:** `atomic_t`, `atomic_cmpxchg`, state machine, CAS, GPU power states, lock-free transitions

---

## Question

Implement an atomic state machine for GPU power management transitions.

---

## Answer

```c
#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/printk.h>

/* ─── GPU Power State Enum ────────────────────────────────────────────────
 * States represent the GPU's power lifecycle.
 * Transitions must be atomic and ordered.
 */
enum gpu_power_state {
    GPU_STATE_OFF     = 0,  /* powered off                                */
    GPU_STATE_SUSPEND = 1,  /* suspended (register state saved to DRAM)   */
    GPU_STATE_IDLE    = 2,  /* powered on, idle, clocks gated             */
    GPU_STATE_ACTIVE  = 3,  /* actively executing CUDA kernels            */
};

struct gpu_device {
    atomic_t    state;      /* current power state (enum gpu_power_state) */
    atomic_t    active_ctxs; /* number of active CUDA contexts            */
    /* ... */
};

static inline const char *gpu_state_name(int s)
{
    static const char *names[] = { "OFF", "SUSPEND", "IDLE", "ACTIVE" };
    return (s >= 0 && s < ARRAY_SIZE(names)) ? names[s] : "UNKNOWN";
}

/* ─── Generic atomic state transition ────────────────────────────────────
 * CAS-based: only succeeds if the current state matches expected.
 * Returns 0 on success, -EINVAL if current state ≠ expected.
 */
static int gpu_state_transition(struct gpu_device *gpu,
                                 enum gpu_power_state from,
                                 enum gpu_power_state to)
{
    /*
     * atomic_cmpxchg: atomically compare-and-swap.
     * If *state == from, set *state = to and return old (= from).
     * If *state != from, leave *state unchanged and return old.
     */
    int old = atomic_cmpxchg(&gpu->state, from, to);

    if (old != (int)from) {
        pr_debug("GPU: transition %s→%s failed (current: %s)\n",
                 gpu_state_name(from), gpu_state_name(to),
                 gpu_state_name(old));
        return -EINVAL;
    }

    pr_debug("GPU: state %s → %s\n",
             gpu_state_name(from), gpu_state_name(to));
    return 0;
}

/* ─── Power on: OFF → IDLE ────────────────────────────────────────────────*/
int gpu_power_on(struct gpu_device *gpu)
{
    int ret;

    ret = gpu_state_transition(gpu, GPU_STATE_OFF, GPU_STATE_IDLE);
    if (ret) {
        pr_err("GPU: power_on failed — not in OFF state (current: %s)\n",
               gpu_state_name(atomic_read(&gpu->state)));
        return ret;
    }

    /* Enable power rails, ungate clocks, load firmware */
    gpu_enable_power_rails(gpu);
    gpu_ungate_clocks(gpu);
    gpu_load_firmware(gpu);

    return 0;
}

/* ─── Start work: IDLE → ACTIVE ───────────────────────────────────────────*/
int gpu_start_work(struct gpu_device *gpu)
{
    int ret;

    ret = gpu_state_transition(gpu, GPU_STATE_IDLE, GPU_STATE_ACTIVE);
    if (ret)
        return ret;

    /* Ungate compute clocks, activate CUDA SM units */
    gpu_ungate_compute_clocks(gpu);
    atomic_inc(&gpu->active_ctxs);

    return 0;
}

/* ─── Complete work: ACTIVE → IDLE ───────────────────────────────────────*/
int gpu_complete_work(struct gpu_device *gpu)
{
    int ctxs = atomic_dec_return(&gpu->active_ctxs);

    /* Only go IDLE if all contexts have finished */
    if (ctxs == 0) {
        /* Gate compute clocks to save power */
        gpu_gate_compute_clocks(gpu);
        return gpu_state_transition(gpu, GPU_STATE_ACTIVE, GPU_STATE_IDLE);
    }

    return 0;
}

/* ─── Suspend: IDLE → SUSPEND ─────────────────────────────────────────────*/
int gpu_suspend(struct gpu_device *gpu)
{
    int ret;

    /* Can only suspend from IDLE — not while ACTIVE */
    ret = gpu_state_transition(gpu, GPU_STATE_IDLE, GPU_STATE_SUSPEND);
    if (ret) {
        pr_warn("GPU: cannot suspend in state %s\n",
                gpu_state_name(atomic_read(&gpu->state)));
        return ret;
    }

    /* Save GPU register context to system RAM */
    gpu_save_register_context(gpu);
    gpu_gate_clocks(gpu);
    gpu_disable_power_rails(gpu);

    return 0;
}

/* ─── Resume: SUSPEND → IDLE ──────────────────────────────────────────────*/
int gpu_resume(struct gpu_device *gpu)
{
    int ret;

    ret = gpu_state_transition(gpu, GPU_STATE_SUSPEND, GPU_STATE_IDLE);
    if (ret)
        return ret;

    gpu_enable_power_rails(gpu);
    gpu_ungate_clocks(gpu);
    gpu_restore_register_context(gpu);

    return 0;
}

/* ─── Power off: IDLE → OFF ───────────────────────────────────────────────*/
int gpu_power_off(struct gpu_device *gpu)
{
    /* Must be IDLE — refuse to power off an ACTIVE GPU */
    return gpu_state_transition(gpu, GPU_STATE_IDLE, GPU_STATE_OFF);
}

/* ─── Non-blocking state query ────────────────────────────────────────────*/
bool gpu_is_active(struct gpu_device *gpu)
{
    return atomic_read(&gpu->state) == GPU_STATE_ACTIVE;
}

bool gpu_is_usable(struct gpu_device *gpu)
{
    int s = atomic_read(&gpu->state);
    return s == GPU_STATE_IDLE || s == GPU_STATE_ACTIVE;
}

/* ─── Forced reset: any → OFF (for TDR recovery) ─────────────────────────*/
void gpu_force_reset(struct gpu_device *gpu)
{
    /* atomic_set bypasses CAS — use only for error recovery */
    atomic_set(&gpu->state, GPU_STATE_OFF);
    atomic_set(&gpu->active_ctxs, 0);
    pr_warn("GPU: forced reset to OFF state\n");
}
```

### Valid State Transition Table

| From \ To | OFF | SUSPEND | IDLE | ACTIVE |
|-----------|-----|---------|------|--------|
| **OFF** | — | ✗ | ✅ power_on | ✗ |
| **SUSPEND** | ✗ | — | ✅ resume | ✗ |
| **IDLE** | ✅ power_off | ✅ suspend | — | ✅ start_work |
| **ACTIVE** | ✗ | ✗ | ✅ complete_work | — |

---

## Explanation

### Core Concept

`atomic_cmpxchg` implements a **lock-free state machine** where:
- State transitions are linearizable — only one concurrent transition succeeds
- No mutex required — state transitions are O(1) with hardware CAS instruction
- Invalid transitions fail immediately and deterministically

```
Two CPUs racing to start_work:
  CPU0: cmpxchg(state, IDLE, ACTIVE) → returns IDLE → SUCCESS
  CPU1: cmpxchg(state, IDLE, ACTIVE) → returns ACTIVE → FAILURE (state already changed)
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `atomic_t` | 32-bit atomic integer |
| `ATOMIC_INIT(val)` | Static initializer |
| `atomic_set(v, val)` | Unconditional write |
| `atomic_read(v)` | Atomic read |
| `atomic_cmpxchg(v, old, new)` | CAS: if *v==old, set *v=new; returns old value |
| `atomic_inc(v)` | Atomic increment |
| `atomic_dec_return(v)` | Decrement and return new value |
| `atomic_xchg(v, new)` | Unconditional swap |
| `atomic64_cmpxchg(v, old, new)` | 64-bit CAS |
| `cmpxchg(ptr, old, new)` | CAS on any pointer-sized type |

### Trade-offs & Pitfalls

- **ABA problem.** If a state transitions OFF→IDLE→OFF, a CAS expecting OFF would succeed even though the state went through IDLE — the "returned to OFF" might not be the original OFF. In this state machine, this is acceptable because all OFF states are equivalent. For pointer CAS (lock-free stacks), ABA can cause corruption.
- **`atomic_set` bypasses ordering.** Use `atomic_set` only for initialization or recovery. Normal state transitions must use `atomic_cmpxchg` to ensure only valid transitions occur.
- **No memory barrier in `atomic_read`.** `atomic_read` is a simple load. If the state change must be visible to all CPUs before subsequent operations, add `smp_mb()` after the `atomic_cmpxchg` on the transition that publishes the change.

### NVIDIA / GPU Context

NVIDIA GPU power management states in actual driver:
- `NV_PM_STATE_D0`: fully powered (= `GPU_STATE_ACTIVE`)
- `NV_PM_STATE_D3`: suspended (= `GPU_STATE_SUSPEND`)
- `NV_PM_STATE_OFF`: powered off (= `GPU_STATE_OFF`)

The RM (Resource Manager) core uses atomic CAS for all power state transitions, ensuring that concurrent CUDA stream submissions and system-initiated PM events don't race — only one entity wins the transition and the other receives an appropriate error code.

---

## Cross Questions & Answers

**CQ1: What is the ABA problem in CAS and how does it affect a GPU state machine?**
> The ABA problem: a CAS reads value A, the value changes to B then back to A, and the CAS succeeds even though intermediate changes occurred. In the GPU state machine, `GPU_STATE_OFF` is always semantically identical (the GPU is always equally "off") so ABA is harmless — transitioning OFF→IDLE→OFF then CAS-ing OFF→IDLE again is correct behavior. ABA matters in pointer-based CAS (lock-free stacks/queues) where the same pointer value after a free-and-realloc represents a different object.

**CQ2: How would you extend this state machine to support multiple simultaneous GPU instances (multi-GPU)?**
> Each `struct gpu_device` has its own `atomic_t state` — each GPU has independent state. No shared state between GPUs is needed for per-GPU power management. A separate `atomic_t system_active_gpus` can track how many GPUs are active system-wide. If all GPUs enter IDLE simultaneously, a delayed workqueue can trigger a platform-level power saving mode (PCIe L1 state, DRAM self-refresh).

**CQ3: Why is `cmpxchg` preferred over a mutex for state transitions in hot paths?**
> State transitions occur on every CUDA launch and every GPU completion interrupt — potentially millions per second. A mutex acquisition involves: spinlock, queue entry allocation, scheduler interaction. `cmpxchg` is a single hardware instruction (on x86, `LOCK CMPXCHG`), typically 10-20 cycles. For state transitions where contention is rare (most transitions succeed on first try), `cmpxchg` has 10-100x lower overhead than mutex.

**CQ4: How do you handle a state transition that requires multiple steps atomically (e.g., IDLE → ACTIVE + increment context count)?**
> Use a two-phase approach: first CAS the state, then perform side effects. If the CAS succeeds, the caller "owns" the transition and can safely perform side effects (clock ungate, context count increment) without a lock — no other caller can also be transitioning from IDLE to ACTIVE simultaneously. If the CAS fails, skip the side effects. This pattern is correct as long as side effects don't need to be atomic with the state change itself.

**CQ5: How does `atomic_cmpxchg` differ from `cmpxchg` in Linux kernel code?**
> `atomic_cmpxchg(v, old, new)` operates on `atomic_t` values and includes memory ordering guarantees suitable for SMP. `cmpxchg(ptr, old, new)` is a lower-level macro that works on any pointer-addressable type and also includes a `LOCK` prefix on x86. For `atomic_t` state variables, use `atomic_cmpxchg` for type safety and portability across architectures. `cmpxchg` is used for non-`atomic_t` variables or when working with pointers directly.

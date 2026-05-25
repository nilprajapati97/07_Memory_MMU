# Mutex — Deep-Dive Design Documents (Senior Kernel Engineer Level)

This folder is a **from-scratch, theory-first** treatment of the Linux kernel
`struct mutex`, calibrated for a senior (≈ 9-year) kernel engineer interview at
NVIDIA. It covers the data structure, fast/slow paths, optimistic spinning
(OSQ / MCS), multi-CPU contention semantics, advanced variants (`ww_mutex`,
`rt_mutex`, killable/interruptible), PREEMPT_RT conversion, and the full
deadlock + debugging toolkit (`lockdep`).

NVIDIA-relevant material — `ww_mutex` (wait/wound) is the lock class used by
DRM/GEM/TTM/dma-resv for **GPU buffer reservation** — has its own section in
[03_Mutex_Variants_RTMutex_WWMutex.md](03_Mutex_Variants_RTMutex_WWMutex.md).

---

## Documents at a Glance

| # | Document | Coverage |
|---|----------|----------|
| 1 | [01_Mutex_Internals_FastSlow_Path.md](01_Mutex_Internals_FastSlow_Path.md) | `struct mutex` layout, owner-encoded atomic, fast/slow path, OSQ optimistic spinning, handoff, wait-list queueing, slow-path scheduler interaction. |
| 2 | [02_Mutex_MultiThread_MultiCPU_Scenarios.md](02_Mutex_MultiThread_MultiCPU_Scenarios.md) | Multi-CPU contention timelines, owner spinning vs sleeping, cache-line traffic, NUMA implications, why MCS over test-and-set, fair vs unfair handoff. |
| 3 | [03_Mutex_Variants_RTMutex_WWMutex.md](03_Mutex_Variants_RTMutex_WWMutex.md) | `mutex_lock_interruptible/_killable/_trylock`, **`ww_mutex` (DRM/GPU)**, `rt_mutex` + **priority inheritance**, PREEMPT_RT auto-conversion of `mutex` ↔ `rt_mutex`. |
| 4 | [04_Mutex_Deadlocks_Lockdep_Debugging.md](04_Mutex_Deadlocks_Lockdep_Debugging.md) | ABBA deadlock, recursive lock, sleep-in-atomic, owner death, lockdep state machine, lock classes / nesting / `_nested`, `CONFIG_DEBUG_MUTEXES`. |

---

## What a `struct mutex` Actually Is

A `struct mutex` is a **sleeping, owner-tracked, single-holder** mutual-exclusion
primitive. It is the default choice for **process-context-only** critical
sections in the Linux kernel.

```
                  ┌──────────────────────────────────┐
                  │           struct mutex           │
                  ├──────────────────────────────────┤
                  │  atomic_long_t owner             │  encodes:
                  │     bits 0..2 → flags            │   • holder task ptr
                  │     bits 3..  → task_struct ptr  │   • WAITERS, HANDOFF, PICKUP
                  │  raw_spinlock_t wait_lock        │  protects wait_list
                  │  struct optimistic_spin_queue osq│  MCS queue (CONFIG_MUTEX_SPIN_ON_OWNER)
                  │  struct list_head wait_list      │  FIFO of sleeping waiters
                  │  (debug fields if enabled)       │
                  └──────────────────────────────────┘
```

Three properties that distinguish it from a spinlock:

1. **Sleeps on contention** (after a bounded optimistic spin).
2. **Tracks the owner** as a `task_struct *` — enables priority inheritance
   (via `rt_mutex` on PREEMPT_RT), deadlock detection, and `WARN` on unlock-by-non-owner.
3. **One holder at a time** — strictly binary; no recursion; no read/write split.

---

## Where Mutex Sits in the Synchronization Hierarchy

| Primitive | Holder context | Waiter sleeps? | Owner tracked | PI support | Typical use |
|-----------|----------------|----------------|---------------|------------|-------------|
| `raw_spinlock_t` | Atomic OK | Never | No | No | Lowest level, IRQ-safe, never converted under RT |
| `spinlock_t` | Atomic OK (non-RT) | Never (non-RT); Yes (RT) | No (non-RT); Yes (RT) | RT only | Short critical sections; IRQ-shared on non-RT |
| **`struct mutex`** | **Process only** | **Yes** | **Yes** | **No (RT: becomes `rt_mutex`)** | **Long, sleep-OK critical sections** |
| `struct rt_mutex` | Process only | Yes | Yes | **Yes** | RT subsystem; underlies PI-futex |
| `struct ww_mutex` | Process only | Yes | Yes | No | **DRM/GPU buffer reservation**, multi-lock acquire with wound/wait |
| `struct semaphore` | Process only | Yes | No | No | Counting; legacy; mostly discouraged |
| `struct rw_semaphore` | Process only | Yes | Reader/writer | Limited | Read-mostly shared resources |

> **Mantra:** *Mutex = the sleeping cousin of the spinlock. Same exclusivity,
> different waiting strategy, and crucially: never callable from atomic
> context.*

---

## Hard Rules (Memorize)

1. **Process context only.** Never call `mutex_lock` from a hard-IRQ, softirq,
   tasklet, or any region with `preempt_count > 0` (e.g. while holding a
   spinlock). Doing so triggers `BUG: scheduling while atomic`.
2. **Same task must lock and unlock.** Unlocking from a different task is a
   `WARN` and undefined behaviour. (Use a `semaphore` if you need
   release-from-other-task semantics.)
3. **No recursion.** `mutex_lock` on a mutex you already hold deadlocks the
   caller. (Use `mutex_trylock` or restructure.)
4. **No lock leakage across `schedule()` boundaries via `wait_event` shortcuts**
   that drop and re-take without owner-tracking awareness — use
   `mutex_lock_interruptible` if you need signal-driven abort.
5. **Lock order must be globally consistent** across the kernel. Use
   `lockdep` and `mutex_lock_nested` for documented nesting.

---

## NVIDIA-Specific Hot Topics

| Topic | Why it matters at NVIDIA |
|-------|--------------------------|
| **`ww_mutex` (wait/wound mutex)** | Used by **DRM `dma-resv`** (GEM/TTM buffer reservation) for atomic multi-buffer locking during GPU command submission. Senior GPU/DRM engineers must explain wound vs wait semantics, acquire context, `EDEADLK` rollback. |
| **PREEMPT_RT** | NVIDIA Drive / Tegra automotive stacks run RT. Understanding `mutex` → `rt_mutex` conversion, priority inheritance, sleeping spinlocks is essential. |
| **OSQ / MCS spin-on-owner** | GPU IRQ handlers feed work queues whose mutexes are heavily contended; optimistic spinning vs scheduler overhead trade-off matters. |
| **lockdep + lock classes** | Large driver subsystems (`nvidia-drm`, `nouveau`, Tegra DRM) define many mutex classes; nested-locking annotations are routine. |
| **`dma_fence` + mutex** | Fences are signalled from IRQ context — mutex use around fences requires careful design (often `spinlock_t` for fence state, `mutex` for buffer state). |

---

## Reading Order

1. **README.md** (this file) — anatomy and ground rules.
2. [01_Mutex_Internals_FastSlow_Path.md](01_Mutex_Internals_FastSlow_Path.md) — how it actually works.
3. [02_Mutex_MultiThread_MultiCPU_Scenarios.md](02_Mutex_MultiThread_MultiCPU_Scenarios.md) — what happens across CPUs.
4. [03_Mutex_Variants_RTMutex_WWMutex.md](03_Mutex_Variants_RTMutex_WWMutex.md) — variants & RT.
5. [04_Mutex_Deadlocks_Lockdep_Debugging.md](04_Mutex_Deadlocks_Lockdep_Debugging.md) — failure modes & tools.

---

## Navigation

➡ **Next:** [01 — Mutex Internals: Fast / Slow Path](01_Mutex_Internals_FastSlow_Path.md)

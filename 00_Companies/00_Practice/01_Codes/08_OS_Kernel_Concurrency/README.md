# 08 — OS / Kernel / Concurrency Primitives

This section covers the synchronisation, data-structure, and OS-kernel building blocks an embedded / systems / driver engineer is expected to know cold. The topics layer: atomic ops and memory ordering at the bottom; mutexes/semaphores/spinlocks above; data structures (ring buffers, queues, lists, timer wheels) that use them; allocators and schedulers that orchestrate them; and signal/reentrancy concerns at the boundary with the kernel.

## Topics
| # | Topic | Theme |
|---|---|---|
| 01 | [Ring Buffer](01_ring_buffer/) | Bounded FIFO; SPSC lock-free |
| 02 | [Producer–Consumer](02_producer_consumer/) | Mutex+condvar; semaphores; SPSC |
| 03 | [Thread-Safe Queue](03_thread_safe_queue/) | Coarse / two-lock / lock-free |
| 04 | [Spinlock](04_spinlock/) | TAS, TTAS, ticket, MCS |
| 05 | [Semaphore](05_semaphore/) | Counting; binary; POSIX; futex |
| 06 | [Reader–Writer Lock](06_reader_writer_lock/) | Reader/writer-pref; seqlock; RCU |
| 07 | [Deadlock](07_deadlock/) | 4 conditions; ordering; try-lock; detection |
| 08 | [Mutex vs Sem vs Spinlock](08_mutex_semaphore_spinlock/) | Decision guide; cost; ISR rules |
| 09 | [Atomic Counter](09_atomic_counter/) | `__atomic_*`, `<stdatomic.h>`, per-CPU |
| 10 | [Memory Barriers](10_memory_barriers/) | Acquire/release/full; compiler vs CPU |
| 11 | [`malloc`/`free` Impl](11_malloc_free_impl/) | Bump, freelist, buddy, slab, TLAB |
| 12 | [Optimised `memcpy`](12_memcpy_optimized/) | Word, SIMD, `rep movsb`, DMA |
| 13 | [Finite State Machine](13_state_machine/) | Switch / table / function-ptr / HSM |
| 14 | [Scheduler](14_scheduler/) | Coop / RR / fixed-prio / EDF / O(1) |
| 15 | [`container_of`](15_container_of/) | Member ptr → parent via offsetof |
| 16 | [`offsetof`](16_offsetof/) | Compile-time member offset |
| 17 | [Kernel Linked List](17_kernel_linked_list/) | Intrusive `list_head`; RCU list |
| 18 | [Timer Wheel](18_timer_wheel/) | Heap, hashed wheel, hierarchical |
| 19 | [Reentrant Functions](19_reentrant_functions/) | `_r` variants; signal-safe vs thread-safe |
| 20 | [Signal Handler](20_signal_handler/) | `sig_atomic_t`, self-pipe, `signalfd` |

## Suggested Learning Order
1. **Foundations** (memory model): 09 → 10
2. **Locks**: 04 → 05 → 08 → 06 → 07
3. **Concurrent data structures**: 01 → 02 → 03 → 17 → 18
4. **OS pieces**: 11 → 12 → 13 → 14
5. **Plumbing**: 15 → 16 (then revisit 17)
6. **Asynchrony at the OS boundary**: 19 → 20

## Cross-Section Prerequisites
- **02_Pointers** — every concurrent data structure is pointer-heavy
- **07_Memory_Storage**: 03 (`volatile`), 06 (alignment), 12 (`static`), 13 (`extern`) are assumed background
- **09_Stack_Queue** for non-concurrent baselines

## Concurrency Primitives Cheat Sheet
| Need | Reach for |
|---|---|
| Exclusion, sleep OK, user space | `pthread_mutex_t` |
| Exclusion, IRQ/atomic context | spinlock (`spin_lock_irqsave`) |
| Permits / signalling / counting | `sem_t` |
| Cross-thread flag, no lock | `<stdatomic.h>` |
| Producer ↔ consumer (1:1) | SPSC ring buffer |
| Producer ↔ consumer (N:M) | mutex + 2 condvars OR 2 semaphores + mutex |
| Read-heavy shared state | RCU or seqlock |
| Many timers | hashed / hierarchical timing wheel |
| Embedded object in a list | `list_head` + `container_of` |
| ISR → main loop wake | self-pipe / `sem_post` / `eventfd` |
| Sleep forbidden, work bounded | spinlock + atomic flag |

## Memory-Ordering Cheat Sheet
| Order | Use |
|---|---|
| `relaxed` | counters with no data dependency |
| `release` (store) | publish data (after writes, before flag) |
| `acquire` (load) | consume data (before reads, after flag) |
| `acq_rel` | RMW that both publishes & consumes |
| `seq_cst` | when in doubt; sequential model; slowest |

## Diagnostic Toolbox
- ThreadSanitizer (`-fsanitize=thread`) — data races, some lock-order
- Helgrind / DRD (Valgrind) — locks, ordering
- Linux `lockdep` (`CONFIG_PROVE_LOCKING`) — kernel lock-order
- `perf lock`, `perf sched` — contention & scheduling profile
- `eu-stack`, `gdb thread apply all bt` — deadlock diagnosis
- AddressSanitizer / LeakSanitizer — heap errors in allocators

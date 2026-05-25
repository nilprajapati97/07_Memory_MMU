# Approach 01: Lock-Free Ring Buffer (SPSC)
### ISR Design for 100 kHz Interrupts — Nvidia / Qualcomm / Google / AMD Interview Depth

---

## 1. Problem Statement & Constraints

**Scenario**: A hardware peripheral fires an interrupt 100,000 times per second
(10 µs period). Every interrupt carries one data sample. No sample may be lost.
Processing each sample takes longer than one interrupt period.

**Hard constraints**:
| Constraint | Value | Reason |
|---|---|---|
| ISR period | 10 µs | 100 kHz hardware |
| ISR time budget | < 5 µs | 50% duty-cycle rule (headroom for nested IRQs) |
| ISR stack | ~256 bytes | Cortex-M NVIC hardware push |
| No blocking in ISR | Mandatory | `mutex_lock()` could sleep → missed interrupts |
| No malloc in ISR | Mandatory | Non-deterministic latency, heap fragmentation |

**Derived requirement**: We need a data structure that:
- Can be written by ISR and read by worker **without any lock**
- Has O(1) push and pop (bounded ISR time)
- Never requires the ISR to wait

---

## 2. Architecture

```
 100 kHz Hardware IRQ
         │
         ▼  (every 10 µs)
 ┌───────────────────┐
 │     hardware_isr  │  < 5 µs execution
 │  1. read HW reg   │
 │  2. rb_push()     │  lock-free, ~0.3 µs
 │  3. sem_post()    │  non-blocking
 └────────┬──────────┘
          │  push(data)
          ▼
 ┌────────────────────────────────────────────┐
 │            Ring Buffer (4096 slots)        │
 │                                            │
 │  Cache Line 0: [head=ISR writes ]          │
 │  Cache Line 1: [tail=Worker writes]        │
 │  Cache Lines 2+: [buf[0]..buf[4095]]       │
 │                                            │
 │  head ──────────────────► (ISR advances)   │
 │  tail ◄──────────────────── (Worker advances)│
 └────────┬───────────────────────────────────┘
          │  pop(data)
          ▼
 ┌───────────────────┐
 │   worker_task()   │  High-priority RTOS task
 │  drains ENTIRE    │  processes N samples
 │  buffer per wake  │  per semaphore wake
 └───────────────────┘
```

---

## 3. Data Structure Deep Dive

```c
typedef struct {
    volatile uint32_t head;     // [offset 0x00] ISR writes exclusively
    uint8_t  _pad0[60];         // [offset 0x04] → force 64-byte cache line
    volatile uint32_t tail;     // [offset 0x40] Worker writes exclusively
    uint8_t  _pad1[60];         // [offset 0x44] → force 64-byte cache line
    uint32_t buf[4096];         // [offset 0x80] 16 KB shared data
} ring_buf_t;                   // total: 16 KB + 128 bytes overhead
```

### Why power-of-2 size?
- `index_wrap = (idx + 1) % SIZE` → integer division, ~20 cycles on Cortex-M4
- `index_wrap = (idx + 1) & MASK` → single AND instruction, 1 cycle
- At 100,000 ISR calls/sec: saves ~19 cycles × 100,000 = **1.9M cycles/sec**
- At 168 MHz: = 11.3 ms/sec CPU saved = **1.1% CPU reduction** from one optimization

### Why volatile?
GCC with `-O2` may:
- Hoist a loop-invariant read of `head` out of a loop (reading stale value forever)
- Eliminate a write to `tail` as "dead store" if it thinks no one reads it
- `volatile` tells compiler: every access must generate an actual memory instruction

### Why cache-line padding?
On multi-core (Cortex-A55 + Cortex-A78 in Snapdragon 8 Gen 3):
- Cache line = 64 bytes
- If `head` and `tail` share a line: writing `head` (by ISR on core 0) invalidates
  the cache line on core 1 (where worker reads `tail`)
- Core 1 must fetch the line from L2/L3 or main memory → ~50-200 ns penalty
- At 100 kHz: 200 ns × 100,000 = **20 ms/sec wasted** on false sharing
- Padding eliminates this entirely

---

## 4. Memory Model & Synchronization Proof

### The C11 memory model (simplified)
Every store/load to memory has an ordering. On ARM (weakly-ordered):
```
Thread A:  store X = 1;    // no ordering guarantee by default
           store Y = 2;    // CPU may reorder these
```
A thread on another core may observe `Y=2` before `X=1`.

### The SPSC invariant
We exploit a crucial property:
- **`head`** has exactly **one writer** (ISR) and one reader (worker)
- **`tail`** has exactly **one writer** (worker) and one reader (ISR)
- No variable is written by two parties simultaneously → **no RMW needed**

### Release-Acquire protocol (formal)

**ISR (producer) — RELEASE store:**
```
store buf[head] = data;      // store A: data write
DMB (release barrier)        // all stores before are globally visible
store head = next_head;      // store B: "publication" — release semantics
```

**Worker (consumer) — ACQUIRE load:**
```
load head_seen = head;       // load B: acquire semantics
DMB (acquire barrier)        // all loads after see everything before store B
load data = buf[old_head];   // load A: guaranteed to see store A
```

**Proof**: If worker observes `store B` (new head), the RELEASE barrier in ISR
guarantees `store A` happened-before `store B`. Worker's ACQUIRE barrier guarantees
`load A` happens-after `load B`. Therefore: worker always reads valid data. ∎

### What happens WITHOUT the barrier?
```
// ISR on ARM (without DMB):
STR  r0, [r1]          ; buf[head] = data     ← enters store buffer
STR  r2, [r3]          ; head = next_head     ← enters store buffer
                                              ← CPU may drain store buffer
                                                 in reverse order!
// Worker sees new head, tries to read buf[head]
// But buf[head] = data is still in ISR's store buffer — reads OLD data!
```
This is a **real hardware bug** on ARM Cortex-A. DMB prevents it.

---

## 5. Performance Analysis

### ISR execution time (Cortex-M4 @ 168 MHz, 10 µs period)

| Phase | Operation | Cycles | Time |
|---|---|---|---|
| HW push | IRQ entry + register save | 12 | 71 ns |
| | Read HW data register | 3 | 18 ns |
| | Compute next_head (AND) | 1 | 6 ns |
| | Full check (CMP + BEQ) | 2 | 12 ns |
| | Store buf[head] | 2 | 12 ns |
| | DMB sy | 8 | 48 ns |
| | Store head | 2 | 12 ns |
| | Stats increment | 3 | 18 ns |
| | Semaphore post (flag) | 3 | 18 ns |
| HW pop | IRQ exit + restore | 12 | 71 ns |
| **Total** | | **~48** | **~286 ns** |

**ISR CPU utilization**: 286 ns / 10,000 ns = **2.86%** ✅

### Buffer sizing formula
```
min_entries = interrupt_rate_Hz × worst_case_worker_wake_latency_s
            = 100,000 × 0.001   (1 ms RTOS jitter)
            = 100 entries minimum

Recommended: 4096  (40× safety margin for burst handling)
Memory cost: 4096 × 4 bytes = 16 KB
```

### Worker throughput
- Worker wakes up, drains entire buffer in one pass
- At 100 µs worker latency: ~10 samples queued per wake
- Process 10 samples/wake × N wakes/sec = same total throughput
- Semaphore overhead amortized over 10 samples → 10× more efficient than one-at-a-time

---

## 6. Edge Cases & Failure Modes

### Overflow (buffer full)
- ISR detects `next_head == tail` → drops sample, increments `overflow_count`
- Root cause: worker is too slow or buffer is too small
- Detection: monitor `overflow_count` in worker health check every N cycles
- Recovery: increase buffer size, optimize `process_data()`, raise worker priority

### Stale semaphore / missed wake
- If RTOS drops a semaphore signal (e.g., counting sem saturates at 1):
  - Worker wakes once, drains all entries → no data loss, just latency spike
  - Add fallback: if `rb_is_empty()` is false after `semaphore_wait()` times out,
    re-drain regardless

### 64-bit data on 32-bit CPU (data tearing)
- If `rb_data_t` were `uint64_t` on a 32-bit core:
  - ISR writes high word, gets interrupted, writes low word → partial value
  - Worker reads high word (new) + low word (old) = CORRUPT
- Fix A: disable IRQs briefly around 64-bit write in ISR
- Fix B: use C11 `_Atomic uint64_t` with `memory_order_relaxed` (approach 05)
- Fix C: pack two 32-bit values into one 32-bit word (if data allows)

### Compiler over-optimization
- GCC `-O3` with link-time optimization (LTO) may inline across translation units
  and optimize away `volatile` accesses it "knows" aren't used externally
- Fix: use `__attribute__((noinline))` on ISR, compile ISR in separate TU

### ISR re-entrancy (nested interrupts)
- If a higher-priority IRQ preempts `hardware_isr()` mid-push:
  - SPSC invariant still holds: `head` is only written by ISR
  - Only one ISR writes head (per ring buffer) — safe
  - Problem: two ISRs sharing ONE ring buffer → MPSC race → NOT safe
  - Solution: one ring buffer per ISR source (see Q5 below)

---

## 7. Trade-offs vs Other Approaches

| Criterion | SPSC Ring Buffer | Double Buffer | DMA Ping-Pong | Linux Workqueue |
|---|---|---|---|---|
| CPU ISR overhead | Low (~0.3 µs) | Very Low | Near zero | Low |
| Latency to worker | Very low (1 entry) | N/2 entries | N/2 entries | Moderate |
| Memory | Fixed 16 KB | 2 × N × 4B | 2 × N × 4B | kfifo + work |
| Code complexity | Low | Medium | High | Medium |
| Hardware req. | None | None | DMA engine | Linux kernel |
| Data granularity | Per sample | Per block | Per block | Per sample |
| Portability | High | High | Platform-specific | Linux only |
| Zero-copy | No (copy to buf) | Yes (ptr swap) | Yes (ptr to DMA buf) | No |

**Best for**: Moderate-frequency (< 1 MHz), low-latency, portable embedded code

---

## 8. Interview Q&A — Nvidia / Qualcomm / Google / AMD Level

---

### Q1: Why must buffer size be a power of 2? What if it isn't?

**Expected answer level**: Know the *why*, not just the *what*.

**A**: Power-of-2 allows index wrap via bitmask: `(idx + 1) & MASK` — single AND
instruction (1 cycle on all architectures). The alternative is modulo: `(idx + 1) % SIZE`
which uses integer division — 20-40 cycles on ARM Cortex-M, similar on x86.

If the hardware constraint forces a non-power-of-2 size (e.g., 1500 for Ethernet MTU):
- Option A: Use unsigned integer wrap + compare: `if (++idx == SIZE) idx = 0;`
  (2 cycles: increment + conditional branch)
- Option B: Round up to next power of 2 and waste the extra slots

---

### Q2: Why do we need BOTH `volatile` AND a memory barrier? Isn't one enough?

**A**: They operate at different levels:

**`volatile`** prevents **compiler-level** reordering. GCC treats volatile accesses
as observable side effects and will not eliminate, merge, or reorder them relative
to each other. But volatile gives NO guarantee about hardware-level reordering.

**Memory barrier (`DMB`)** prevents **CPU-level** reordering. ARM Cortex-A has an
out-of-order pipeline and a store buffer. Stores are placed in the store buffer
and drained to cache in an order the CPU chooses for performance — potentially
different from program order. `DMB` forces all stores queued before the barrier
to drain to the cache before any store after the barrier becomes globally visible.

**Without volatile**: Compiler hoists `rb->head` read out of a loop → worker
always sees the same head value → empty buffer even with data present.

**Without DMB**: On ARM Cortex-A, ISR stores `buf[head] = data` then `head = next_head`.
Store buffer drains `head` first (shorter latency path), then `buf[head]`. Worker
sees new `head`, loads `buf[old_head]` → reads stale data. **Real hardware bug.**

**On x86**: x86 uses TSO (Total Store Order). Stores are always observed in program
order. Compiler fence (empty `__asm__ volatile`) is sufficient. `DMB` equivalent
would be `MFENCE` but is unnecessary for SPSC on x86.

---

### Q3: What is "false sharing" and how does it affect this ring buffer?

**A**: False sharing occurs when two logically-independent variables share a CPU
cache line (typically 64 bytes). When core 0 writes variable A and core 1 writes
variable B — even if A and B don't interfere logically — they invalidate each
other's cache line because the hardware cache coherency protocol (MESI) operates
at cache-line granularity.

In our ring buffer: `head` and `tail` are both 4-byte integers. Without padding,
they'd be at offset 0 and 4 — same 64-byte cache line.

- ISR on Core 0 writes `head` → Core 1's cache line containing `tail` is invalidated
- Worker on Core 1 reads `tail` → cache miss → fetch from L2 (~10 cycles)
- Worker on Core 1 writes `tail` → Core 0's line is invalidated
- ISR reads `tail` (for full check) → cache miss → fetch from L2

At 100 kHz ISR rate: 100,000 × 10 cycles = 1M extra cycles/sec = ~6ms/sec
on a 168 MHz core — measurable overhead.

60-byte padding between `head` and `tail` ensures they occupy separate cache lines.
Zero cache line ping-pong. This is also why Linux kernel uses `____cacheline_aligned_in_smp`.

---

### Q4: What happens if the worker doesn't drain the buffer fast enough?

**A**: Overflow. `rb_push_from_isr()` detects `next_head == tail` and drops the
sample. `overflow_count` is incremented.

**Root causes**:
1. `process_data()` is too slow → optimize or move heavy work to lower-priority task
2. Worker priority too low → raise priority (should be highest non-ISR task)
3. Buffer too small → increase `RB_SIZE` (costs RAM, eliminates transient bursts)
4. OS scheduler jitter → use RTOS with deterministic scheduling (no Linux CFS)

**Detection strategy**: Monitor `overflow_count` ratio:
```
overflow_rate = (overflow_count_now - overflow_count_last) / sample_interval
if (overflow_rate > 0.001%) → alert, tune system
```

**Never silently drop data without a counter**: makes debugging impossible.

---

### Q5: This is SPSC. What changes for Multiple Producers (MPSC)?

**A**: SPSC safety breaks with multiple writers. Two ISRs could simultaneously
compute `next_head = (rb->head + 1) & MASK` — both see the same head value,
both write to `buf[head]` — one sample is overwritten, head is double-incremented.

**Solutions**:

**Option A**: One ring buffer per ISR source (preferred for fixed ISR count)
```c
ring_buf_t g_uart_rb;  // UART ISR → worker_uart
ring_buf_t g_adc_rb;   // ADC ISR → worker_adc
```
Maintains SPSC invariant. Worker round-robins across buffers.

**Option B**: C11 atomic compare-exchange for MPSC head update
```c
uint32_t old = atomic_load(&head);
uint32_t next;
do {
    next = (old + 1) & MASK;
} while (!atomic_compare_exchange_weak(&head, &old, next));
buf[old] = data;  // WARNING: another producer may read stale buf[old]
```
This gives linearizable head update but NOT atomic data+head publication.
Need additional per-slot "committed" flag. Complex — use a dedicated MPSC
algorithm (Dmitry Vyukov's MPSC queue).

**Option C**: Disable interrupts briefly
```c
uint32_t primask = __get_PRIMASK();
__disable_irq();
rb_push(&rb, data);  // now safe: only one ISR runs at a time
__set_PRIMASK(primask);
```
Simple but increases interrupt latency. Acceptable if critical section < 1 µs.

---

### Q6: How would you test this without hardware?

**A**: Multi-layer test strategy:

**1. Unit test (host, single-threaded)**:
- Verify empty/full detection
- Verify push/pop ordering (FIFO property)
- Verify overflow counter increments correctly
- Test wrap-around: fill buffer, drain, fill again

**2. Concurrency test (host, multi-threaded)**:
- Compile with `-DSIMULATION`, use `pthreads`
- ISR thread: push 1M items at 100 kHz rate
- Worker thread: drain continuously
- Verify: `total_pushed == total_popped + overflow_count`

**3. Race detector**:
- `gcc -fsanitize=thread` (ThreadSanitizer / TSan)
- TSan instruments every memory access, detects data races at runtime
- Note: TSan may report false positives on lock-free code using `volatile` —
  use C11 `_Atomic` (approach 05) for cleaner TSan output

**4. Formal verification (advanced)**:
- TLA+ model of the SPSC protocol
- CBMC (C Bounded Model Checker): exhaustively check memory ordering

**5. Hardware-in-loop**:
- Logic analyzer on IRQ pin: measure ISR period jitter
- JTAG debugger: sample `overflow_count` in real time
- Oscilloscope: ISR entry/exit pins — verify < 5 µs execution

---

### Q7: What is the difference between `memory_order_release` (C11) and `DMB` (ARM)?

**A**: Semantically equivalent, syntactically different.

`DMB sy` (ARM assembly) = `memory_order_seq_cst` barrier = full bidirectional barrier.
It prevents ALL memory operation reordering: load-load, load-store, store-load, store-store.

`memory_order_release` (C11) = one-directional barrier:
- Prevents loads and stores that appear BEFORE the release-store from being reordered
  to appear AFTER it. (Roughly equivalent to `DMB st` — store-only barrier.)
- Does NOT prevent subsequent loads from being reordered before it.
- More efficient than full `DMB sy` on architectures that support one-way barriers.

For our SPSC:
- ISR push: needs only `memory_order_release` on head store (data write must precede head)
- Worker pop: needs `memory_order_acquire` on head load (see data after head)
- `DMB sy` is over-kill but correct. C11 `_Atomic` with acquire/release is tighter.

On ARM Cortex-A: `memory_order_release` compiles to `stlr` (Store-Release) instruction —
no DMB needed, single instruction. More efficient.

---

### Q8: How would you adapt this for a Qualcomm Hexagon DSP?

**A**: Hexagon (QDSP6) has a different memory model:

1. **No cache coherency with ARM cores by default**: DSP has its own L1/L2 caches.
   Shared memory must be in uncached regions or explicitly flushed/invalidated.

2. **Hardware semaphores**: Hexagon has hardware mutex registers (`HMUTEX`).
   For pure DSP-internal SPSC, `volatile` + `barrier()` intrinsic suffices.

3. **FastRPC / shared memory**: When ARM ISR writes data and DSP worker reads it:
   - Use ION memory (physically contiguous, cache-coherent across domains)
   - Or use explicit cache flush from ARM side after write: `dmac_flush_range()`
   - DSP side: `hexagon_cache_invalidate()` before read

4. **Vector-friendly layout**: Hexagon handles 128-byte (HVX) vectors efficiently.
   Align ring buffer to 128 bytes. Use HVX bulk copy in worker if processing
   multiple samples at once.

The SPSC logic is identical; the memory configuration and cache management differ.

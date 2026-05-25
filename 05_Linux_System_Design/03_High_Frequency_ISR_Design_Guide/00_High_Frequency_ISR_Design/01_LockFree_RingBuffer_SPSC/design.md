# 01 — Lock-Free SPSC Ring Buffer

## What Problem Does This Solve?

At 100,000 interrupts/sec, the ISR fires every **10 µs**. The ISR must finish in **< 5 µs** (50% budget). It cannot process the data — it can only collect it. The ring buffer bridges the ISR (fast producer) and the worker task (slow consumer) without any locks.

---

## Architecture

```
 ┌──────────────┐  IRQ every 10 µs   ┌──────────────────────────────────────────┐
 │  Hardware    │ ──────────────────► │  ISR  (interrupt context)                │
 │  (UART/ADC/  │                     │  1. Read PERIPH->DR    (~1 cycle)         │
 │   SPI/TIM)   │                     │  2. rb_push_from_isr() (~8 cycles total) │
 └──────────────┘                     │  3. g_data_ready = 1                     │
                                      └──────────────────┬───────────────────────┘
                                                         │ lock-free push
                                                         ▼
                              ┌───────────────────────────────────────────┐
                              │  ring_buf_t  (4096 × uint32_t = 16 KB)   │
                              │  ┌──────────────────────────────────────┐ │
                              │  │ head → [d][d][d][d][d][...][empty]   │ │
                              │  │                         tail →        │ │
                              │  └──────────────────────────────────────┘ │
                              └───────────────────────────────────────────┘
                                                         │ lock-free pop
                                                         ▼
                              ┌────────────────────────────────────────────┐
                              │  Worker Task  (RTOS task context)          │
                              │  Wake on g_data_ready                      │
                              │  BATCH DRAIN: while(rb_pop()) { process() }│
                              └────────────────────────────────────────────┘
```

---

## Data Structure Layout

```c
typedef struct {
    volatile uint32_t head;          /* offset  0 — ISR writes, worker reads  */
    volatile uint32_t tail;          /* offset  4 — worker writes, ISR reads  */
    uint32_t          buf[4096];     /* offset  8 — 16 KB ring storage        */
    volatile uint32_t overflow_count;/* offset +8 — monitoring only           */
    volatile uint32_t total_pushed;
    volatile uint32_t total_popped;
} ring_buf_t;
```

**Why power-of-2 size?**  
Modular indexing with `& MASK` is a single AND instruction.  
`% 4096` needs a division (~20 cycles on Cortex-M4).  
At 100 kHz × 2 index updates = 200,000 ops/sec — the AND matters.

---

## Memory Ordering — The Core Correctness Proof

### Without barrier (BROKEN on ARM):

```
ISR:      STR [buf+head], data    ← CPU may reorder this AFTER head update
          STR [head], next_head   ← consumer sees new head but stale buf data!
Worker:   LDR head                ← sees next_head
          LDR [buf+old_head]      ← reads STALE data (write buffer not flushed)
```

### With barrier (CORRECT):

```
ISR:      STR [buf+head], data    ← data write
          DMB SY                  ← RELEASE: all stores above are globally visible
          STR [head], next_head   ← head update — only happens AFTER buf write

Worker:   LDR head                ← sees next_head
          DMB SY                  ← ACQUIRE: all stores before producer's DMB are now visible
          LDR [buf+old_head]      ← guaranteed to see correct data
```

This is a **release-acquire synchronization pair** — the same model C11 uses with `memory_order_release` / `memory_order_acquire`.

---

## Why No Lock Is Needed (SPSC Proof)

| Property | Reason |
|---|---|
| Single producer | ISR is the only writer of `head` — no race on head |
| Single consumer | Worker is the only writer of `tail` — no race on tail |
| Index separation | `head` and `tail` written by different contexts — no write-write race |
| Data write order | DMB guarantees buf[head] written before head published |
| Buffer full check | `next_head == tail` — consumer cannot advance tail during this check in a single-core system |

> **Multi-core caveat**: On SMP (Cortex-A, x86 multi-socket), the above holds if the ISR and worker are pinned to the same core, OR if memory barriers are used on both sides (which they are here via `DMB SY`).

---

## Performance Analysis

| Operation | Cycles (Cortex-M4 @ 168 MHz) | Time |
|---|---|---|
| ISR fast path (no overflow) | ~10 cycles | ~60 ns |
| Worker pop (one item) | ~8 cycles | ~48 ns |
| ISR CPU overhead at 100 kHz | 10 × 100,000 / sec | 1.0 M cycles/sec = 0.6% of 168 MHz |

**Buffer sizing formula:**
```
min_size = interrupt_rate × worst_case_worker_latency
         = 100,000/sec × 1 ms
         = 100 entries minimum

With 40× safety margin and power-of-2: use 4096
```

---

## Common Interview Questions

**Q: Why must RING_BUF_SIZE be a power of 2?**  
A: Enables `(head + 1) & MASK` instead of `% SIZE`. Single AND vs division instruction. Critical at 100 kHz (200k modular ops/sec).

**Q: What happens if you use `volatile` but no `DMB`?**  
A: `volatile` prevents compiler reordering. It does NOT prevent the ARM CPU from reordering stores in hardware (write buffer / out-of-order execution). `DMB SY` flushes the write buffer, ordering CPU memory accesses. Both are needed on ARM.

**Q: Can you make this MPSC (multiple producers / single consumer)?**  
A: SPSC is lock-free. MPSC requires either: (a) a mutex protecting `head` in the ISR — valid only if all producers are at the same or lower priority, or (b) a CAS loop on `head` — requires `LDREX/STREX` on ARM.

**Q: How do you test this for race conditions?**  
A: On x86, compile with `-fsanitize=thread`. TSan detects data races through shadow memory. On ARM hardware: vary ISR priority and clock speed, stress with a second RTOS task that reads/writes concurrently.

**Q: How would you adapt this to NVIDIA Tegra or Qualcomm Snapdragon (ARMv8-A)?**  
A: Replace `DMB SY` with `STLR` (Store-Release) and `LDAR` (Load-Acquire) — single-instruction release/acquire available on ARMv8-A. Compiler: use `__atomic_store_n(&head, next_head, __ATOMIC_RELEASE)`.

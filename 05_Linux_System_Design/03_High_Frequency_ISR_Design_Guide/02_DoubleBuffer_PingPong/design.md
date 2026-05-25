# Approach 02: Double Buffer / Ping-Pong
### Zero-Copy Block Transfer between ISR and Worker — Interview Depth

---

## 1. Concept & Motivation

The ring buffer (approach 01) handles data **one sample at a time**. Every ISR
push is visible to the worker immediately. This is great for low-latency but
requires the worker to wake up frequently.

Double buffering changes the granularity to **blocks of N samples**:
- ISR fills Buffer A while worker processes Buffer B
- When A is full: they **swap roles**
- No data copy — worker gets a direct pointer to the full buffer

**Real-world usage**:
- STM32 ADC DMA with circular buffer (this is what HAL does internally)
- Audio codec drivers (I2S ping-pong)
- Video frame double-buffering (display controller)
- USB bulk transfer endpoint buffers
- UART RX with configurable block size

---

## 2. Architecture

```
 100 kHz Hardware IRQ
         │
         ▼  (every 10 µs, fast path: 5 cycles)
 ┌──────────────────────────────────┐
 │   hardware_isr()                 │
 │   data = read_hw_register()      │
 │   buf[active][write_pos] = data  │ ← ~3 cycles
 │   write_pos++                    │ ← ~1 cycle
 │   if (write_pos == 512):         │ ← ~1 cycle
 │     → slow path (swap)           │ ← ~30 cycles (1/512 calls)
 └──────────────────────────────────┘
                │
        (every 512 samples = every 5.12 ms)
                │ buffer full → swap
                ▼
 ┌─────────────────────────────────────────┐
 │   BUFFER SWAP (atomic from ISR view)    │
 │                                         │
 │   BEFORE swap:                          │
 │   ┌─────────────┐  ┌─────────────┐      │
 │   │  BUF A      │  │  BUF B      │      │
 │   │  [FULL]     │  │  [EMPTY]    │      │
 │   │  ← ISR fills│  │             │      │
 │   └─────────────┘  └─────────────┘      │
 │                                         │
 │   AFTER swap:                           │
 │   ┌─────────────┐  ┌─────────────┐      │
 │   │  BUF A      │  │  BUF B      │      │
 │   │  [READY]    │  │  [EMPTY]    │      │
 │   │  ← Worker   │  │ ← ISR fills │      │
 │   │    processes│  │             │      │
 │   └─────────────┘  └─────────────┘      │
 └─────────────────────────────────────────┘
                │
                │ ready=1, sem_post
                ▼
 ┌──────────────────────┐
 │   worker_task()      │  wakes ~195 times/sec
 │   ptr = get_ready()  │  zero-copy read
 │   process_buffer()   │  512 samples at once
 │   db_release()       │  free buffer for ISR
 └──────────────────────┘
```

---

## 3. Buffer Swap Atomicity — Critical Analysis

The swap operation in `db_push_from_isr()`:
```c
db->ready_buf = db->active;    // (1) tell worker which buffer
db->ready     = 1u;            // (2) signal ready (publication)
db->active    = db->active ^ 1u; // (3) swap ISR to other buffer
db->write_pos = 0u;            // (4) reset write position
```

**Is this atomic?** On Cortex-M (single-core): YES.
- Cortex-M NVIC disables lower-priority interrupts during ISR execution
- Worker (task/thread) cannot preempt the ISR
- From ISR's perspective: (1)-(4) are uninterruptible

**Is this atomic on multi-core Cortex-A?** NOT by default.
- If worker runs on Core 1 and ISR on Core 0:
  - Worker could observe `ready=1` between steps (2) and (3)
  - Worker calls `db_get_ready()` while ISR is mid-swap
  - Worker sees `ready_buf = db->active` (old value, before swap)
  - But after step (3), ISR starts writing to `buf[active ^ 1]` = same buf
  - **DATA RACE**: worker reads while ISR writes to the same buffer

**Fix for multi-core**: Use `_Atomic uint8_t ready` with `memory_order_seq_cst`,
or use a spinlock around the swap. On Cortex-A with GIC (interrupt controller),
the ISR and worker typically run on the same core for this exact reason —
or use the C11 atomic approach (see Approach 05).

---

## 4. Memory Ordering

### ISR (producer) — buffer full path

```
Step 1: buf[active][0..511] = data[0..511]     ← 512 store A's
        (these happen across 512 ISR invocations)

Step 2: MEM_BARRIER_RELEASE()                  ← barrier
        "All buf[] stores are visible before ready_buf and ready"

Step 3: ready_buf = active                     ← store B (which buf)
Step 4: ready     = 1                          ← store C (publication)
Step 5: active    ^= 1                         ← swap (ISR-only write)
Step 6: write_pos  = 0                         ← reset (ISR-only write)
```

### Worker (consumer) — processing path

```
Step 1: if (ready == 0) return NULL            ← early exit

Step 2: MEM_BARRIER_ACQUIRE()                  ← synchronize with ISR's release
        "All reads after this see ISR's pre-release writes"

Step 3: ptr = buf[ready_buf]                   ← store A's guaranteed visible

Step 4: process(ptr, 512)                      ← worker reads buf safely

Step 5: MEM_BARRIER_RELEASE()                  ← ensure reads done before release
Step 6: ready = 0                              ← free slot for ISR
```

**Happens-before chain**:
ISR: `store buf[]` → DMB → `store ready=1` → (acquire) → Worker: `load buf[]`

---

## 5. Performance Comparison with Ring Buffer

| Metric | Ring Buffer (01) | Double Buffer (02) |
|---|---|---|
| ISR cycles per sample | ~25 (push + DMB) | ~5 (fast path only) |
| ISR CPU @ 100 kHz | ~2.5% | ~0.5% |
| Worker wake rate | Up to 100,000/sec | 195/sec (100k/512) |
| Context switch overhead | Up to 100k/sec | ~195/sec |
| Latency (sample→worker) | < 1 µs (immediate) | 5.12 ms (block) |
| Memory per buffer | 16 KB (4096 × 4B) | 4 KB (2×512×4B) |
| Processing granularity | Per sample | Per block |
| Zero copy | No | Yes |

**Winner by use case**:
- Low latency, variable rate: Ring Buffer
- High throughput, block processing (FFT, DMA, audio): Double Buffer

---

## 6. Edge Cases

### Overflow: worker too slow
- ISR fills active buffer, tries to swap, but `ready == 1` (worker hasn't released)
- ISR cannot safely swap → resets `write_pos` to 0, overwrites active buffer
- `overflow_cnt++`
- **Mitigation**: ensure worker finishes `process_buffer()` within one fill period
  (5.12 ms for 512 samples at 100 kHz)

### Partial buffer on shutdown
- System shuts down with `write_pos = 237` (buffer half-full)
- Remaining 237 samples never handed off to worker
- **Fix**: explicit flush function:
  ```c
  void db_flush(double_buf_t *db) {
      if (db->write_pos > 0) {
          // pad remaining slots with zeros or sentinel
          // force handoff to worker
      }
  }
  ```

### Double buffer depth = 2 is the minimum
- If processing is slower than fill rate for one full buffer period:
  no number of "ping-pong" helps — need ring buffer (more slots)
- Or: increase `DB_HALF_SIZE` to give worker more time between handoffs

---

## 7. Interview Q&A — Staff/Principal Engineer Level

---

### Q1: Why is the buffer swap `active ^= 1` safe from a correctness standpoint?

**A**: The key insight: after the swap, the ISR writes ONLY to `buf[new_active]`
and the worker reads ONLY from `buf[old_active]` (= `ready_buf`). They are by
definition different buffers (0 XOR 1 = 1, 1 XOR 1 = 0). As long as:
1. ISR sets `ready_buf = active` BEFORE `active ^= 1`
2. Worker reads `ready_buf` AFTER observing `ready = 1`
...there is no window where both access the same buffer.

The MEM_BARRIER between data writes and `ready = 1` creates the synchronization
point. After the barrier, ISR starts writing to the new buffer; worker starts
reading from the old one.

---

### Q2: How does this compare to DMA ping-pong (approach 03)?

**A**: Double buffering in software is essentially DMA ping-pong emulated by
the ISR. The difference:

| | Software Ping-Pong | DMA Ping-Pong |
|---|---|---|
| Who fills buffer | ISR, one sample at a time | DMA controller, burst |
| ISR rate | 100 kHz (every sample) | 195 Hz (every N samples) |
| CPU involvement | ISR per sample | IRQ per N samples only |
| CPU savings | ~0.5% ISR + reduced wakes | ~0.02% (DMA does the work) |
| Hardware needed | None | DMA engine |

DMA ping-pong is strictly better if the peripheral supports DMA transfers.
Software double-buffer is the fallback when DMA is unavailable or the data
source doesn't have a DMA-accessible FIFO.

---

### Q3: Describe the "ABA problem" — does it apply here?

**A**: The ABA problem in CAS (compare-and-swap): thread sees value A, another
thread changes it to B then back to A, original thread's CAS succeeds incorrectly.

In double buffering: `ready` goes 0→1→0. If ISR checks `ready == 0` (to see if
it can swap), then worker sets `ready = 0`, then another ISR calls again and
sees `ready == 0` — this is the correct state, not a false positive.

The ABA problem doesn't apply here because:
1. Only ISR writes to `active` and `write_pos` — no ABA possible
2. `ready` transitions are intentional: ISR→1 means "ready", worker→0 means "done"
   The ISR never falsely interprets `ready = 0` as "previously consumed"

ABA matters for multi-producer lock-free stacks/queues where the pointer to
a node can be recycled. Not applicable to this simple flag-based protocol.

---

### Q4: What is the minimum worker deadline to guarantee no overflow?

**A**:
```
Worker deadline = DB_HALF_SIZE / interrupt_rate
               = 512 / 100,000 Hz
               = 5.12 ms
```
Worker must call `db_release()` within 5.12 ms of receiving the buffer.
If worker deadline > 5.12 ms → increase `DB_HALF_SIZE` proportionally:
```
Required DB_HALF_SIZE = interrupt_rate × max_worker_latency
                      = 100,000 × 0.010  (10 ms latency)
                      = 1000 samples → round to 1024
```
Increasing buffer size increases latency but reduces required worker throughput.

---

### Q5: How would you implement triple-buffering? When is it needed?

**A**: Triple buffering adds a third buffer as a "flight buffer" — ISR always
has a fresh empty buffer to write to even if worker is still processing.

```
State machine with 3 buffers (0, 1, 2):
  fill_buf:  ISR is writing to this
  ready_buf: full, waiting for worker
  free_buf:  empty, available for next swap

On full:
  swap(fill_buf, free_buf)   // ISR gets a fresh buffer immediately
  signal(ready_buf)          // worker can take its time
```

When needed:
- Worker has variable-length processing (sometimes fast, sometimes slow)
- Two-buffer system would overflow on slow processing cycles
- GPU rendering pipeline: back buffer (fill) → middle buffer (ready) → front (display)

Cost: +1 buffer = +N×4 bytes RAM. Worth it when overflow_cnt > 0 despite
adequate average worker throughput (burst handling).

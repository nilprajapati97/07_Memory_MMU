# 02 — Ping-Pong Double Buffer

## What Problem Does This Solve?

The ring buffer wakes the worker for **every sample** (up to 100,000 times/sec). For block-oriented processing (FFT, FIR filter, protocol frames), you need all N samples before any processing can start. The double buffer collects samples silently and only wakes the worker once per complete block.

**Result**: at 100 kHz with block size 256:
- Ring buffer wake rate: up to 100,000/sec
- Double buffer wake rate: 100,000 / 256 = **390/sec** (256× fewer)

---

## Architecture

```
 ISR (100,000/sec)                         Worker (390/sec)
      │                                          │
      ▼                                          ▼
 ┌────────────────────────────────────────────────────┐
 │              double_buf_t                          │
 │                                                    │
 │  buf[0][0..255]  ← ISR filling (active=0)         │
 │                                                    │
 │  buf[1][0..255]  ← Worker processing (ready_buf=1)│
 └────────────────────────────────────────────────────┘
        │                          │
   ISR writes here           Worker reads here
   (no conflict —            (no conflict —
    different buf)            different buf)
```

**The key**: while the ISR fills `buf[active]`, the worker processes `buf[1-active]`. They are always in different buffers — zero contention, no locks.

---

## State Machine — Buffer Swap

```
State: active=0, idx=0..255, ready=0
  │
  │  ISR: buf[0][idx++] = sample  (fast path, 256 calls)
  │
  ▼
State: active=0, idx=255 (FULL)
  │
  │  ISR swap path:
  │    ready_buf = 0          ← record which buffer filled
  │    active    = 1          ← swap to buffer 1
  │    idx       = 0          ← reset write pointer
  │    DMB                    ← RELEASE
  │    ready     = 1          ← signal worker
  │
  ▼
State: active=1, idx=0, ready=1 (worker wakes)
  │
  │  Worker:
  │    DMB (ACQUIRE)
  │    process(buf[ready_buf=0])
  │    db_release()  → ready=0
  │
  ▼
State: active=1, idx=0..255, ready=0  (cycle repeats)
```

---

## Timing Budget

```
Sample rate:           100,000 Hz
Block size (DB_BUF_LEN): 256 samples
Time to fill one buffer: 256 / 100,000 = 2.56 ms

Worker deadline: process 256 samples in < 2.56 ms

Example workloads that fit:
  ARM CMSIS arm_fir_f32(256 taps): ~50 µs on Cortex-M7 @ 216 MHz ✅
  256-point FFT (CMSIS):           ~100 µs on Cortex-M4 @ 168 MHz ✅
  CRC-32 over 256×4 = 1024 bytes:  ~10 µs ✅
  memcpy 1024 bytes to output buf:  ~5 µs ✅
```

---

## Memory Ordering — Swap Correctness

### What must be visible when worker reads `ready=1`:

```
ISR writes:
  buf[0][0]   = sample_0    ┐
  buf[0][1]   = sample_1    │  All these must be globally
  ...                       │  visible BEFORE ready=1
  buf[0][255] = sample_255  ┘
  DMB SY                    ← RELEASE barrier
  ready_buf   = 0
  ready       = 1           ← published last

Worker reads:
  while (!db->ready) {}     ← polls for ready=1
  DMB SY                    ← ACQUIRE barrier
  buf[0][0..255]            ← guaranteed to see all data above
```

---

## Overflow Handling

Overflow occurs when the worker hasn't called `db_release()` before the next buffer fills (2.56 ms deadline missed).

```
Strategy: reset write pointer, drop new buffer
  overflow_cnt++
  idx = 0    ← overwrite into active buffer from start
  return false

Alternative strategies:
  (a) Triple buffering — adds a third buffer to absorb one overflow
  (b) Reduce block size — smaller N = shorter deadline = less processing per wake
  (c) Optimize worker  — use SIMD/DSP intrinsics to meet the deadline
```

---

## Double Buffer vs Ring Buffer — When to Use Which

| Criteria | Ring Buffer | Double Buffer |
|---|---|---|
| Processing model | Sample-by-sample | Block-at-a-time |
| Worker wake rate | Up to 100k/sec | 390/sec at N=256 |
| Worker deadline | Unbounded (average rate) | Hard: N / rate = 2.56 ms |
| Memory usage | 4096 × 4 = 16 KB | 2 × 256 × 4 = 2 KB |
| Per-sample ISR overhead | ~8 cycles | ~3 cycles (fast path) |
| Suitable for | Streaming protocols, logging | FFT, FIR, block coding |

---

## Common Interview Questions

**Q: What happens if the worker takes longer than 2.56 ms?**  
A: The ISR reaches `DB_BUF_LEN` samples, checks `ready=1`, sees the worker is still busy, increments `overflow_cnt`, and resets `idx=0`. The next 256 samples overwrite the active buffer from the start. The previous buffer is also lost (worker still holds it). Two consecutive blocks are dropped.

**Q: Is the buffer swap atomic on a multi-core system?**  
A: `active ^= 1` is a single 32-bit XOR — atomic as a load-modify-store only if done with LDREX/STREX on ARM. On Cortex-M (single-core), the ISR preempts the task, so no concurrent access. On Cortex-A (multi-core), you need an atomic swap or run ISR and worker on the same core.

**Q: How is this different from DMA half/full-transfer?**  
A: Conceptually identical — DMA half-transfer ≈ "buffer A full", DMA full-transfer ≈ "buffer B full". The difference: DMA offloads the sample-by-sample writes to hardware, eliminating the ISR per-sample overhead entirely. Double buffer in software still has an ISR per sample (just a very cheap one: 3 cycles).

**Q: Why use XOR swap (`active ^= 1`) instead of `active = 1 - active`?**  
A: Both work. XOR is one instruction (EOR on ARM). `1 - active` is also one instruction (RSB or SUB). Both are equally valid. XOR makes the intent clearer: "toggle between 0 and 1".

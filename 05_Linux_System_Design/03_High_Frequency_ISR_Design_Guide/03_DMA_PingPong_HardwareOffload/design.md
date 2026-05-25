# Approach 03: DMA Ping-Pong (Hardware Offload)
### Zero-CPU Data Transfer — The Industry Standard for High-Frequency Embedded

---

## 1. Why DMA? The Fundamental Problem with ISR-Driven Approaches

At 100 kHz with ISR-per-sample:
- CPU is interrupted 100,000 times/sec
- Each interrupt: pipeline flush (~12 cycles), register save, handler, restore
- Pure interrupt overhead: ~25 cycles × 100,000 = **2.5M cycles/sec**
- On 168 MHz MCU: **1.5% CPU burned just handling interrupt overhead**

**DMA (Direct Memory Access)** moves data from peripheral to memory autonomously:
- Peripheral FIFO → RAM with zero CPU involvement
- CPU interrupted only when a significant chunk is ready: 2× per `N` samples
- At N=256: 100,000 / 256 = **390 IRQ/sec instead of 100,000/sec**
- CPU interrupt overhead: 390 × 25 cycles = 9,750 cycles/sec = **0.006% CPU**

**This is why every production high-frequency embedded system uses DMA.**

---

## 2. DMA Controller Architecture

```
                    AHB/AXI Bus
                         │
   ┌──────────────────────┼──────────────────────────┐
   │  DMA Controller      │                           │
   │                      │                           │
   │  Stream 0:           │     Stream N:             │
   │  src: ADC1->DR  ─────┤     src: SPI1->DR         │
   │  dst: buf[0..511]    │     dst: other_buf[]       │
   │  mode: CIRCULAR      │                           │
   │  count: 512          │                           │
   │  IRQ: HTC + TC       │                           │
   └──────────────────────┴───────────────────────────┘
               │                 │
           HTC IRQ           TC IRQ
         (buf[0..255]      (buf[256..511]
          is ready)          is ready)
               │                 │
               ▼                 ▼
          ┌──────────────────────────┐
          │   CPU: IRQ Handler       │
          │   - Invalidate D-cache   │
          │   - post(message_queue)  │
          └────────────┬─────────────┘
                       │
                       ▼
          ┌──────────────────────────┐
          │   Worker Task            │
          │   - process 256 samples  │
          │   - FFT / filter / log   │
          └──────────────────────────┘
```

---

## 3. DMA Circular Mode: State Machine

```
Initial:  DMA_NDTR = 512 (count down register)
          DMA_M0AR = &buf[0]    (target memory address)

─────────────────────────────────────────────────────
Cycle 1:
  DMA fills buf[0]       → NDTR = 511
  DMA fills buf[1]       → NDTR = 510
  ...
  DMA fills buf[255]     → NDTR = 256  ← HTC IRQ FIRES
  DMA fills buf[256]     → NDTR = 255
  ...
  DMA fills buf[511]     → NDTR = 0    ← TC IRQ FIRES

Cycle 2 (automatic wrap):
  DMA resets NDTR = 512
  DMA fills buf[0]       → NDTR = 511  (overwriting Cycle 1 data!)
  ...
─────────────────────────────────────────────────────

                 ← CPU window to process buf[0..255] →
                                ← CPU window to process buf[256..511] →
```

**Critical**: CPU MUST finish processing each half before DMA wraps around
and overwrites it. Processing window = `DMA_HALF_SIZE / sample_rate = 2.56 ms`.

---

## 4. Cache Coherency — The Most Common Bug in DMA Code

### The Problem (very common in hardware/kernel interviews)

```
System: Cortex-M7 (32KB D-cache enabled)

1. CPU writes buf[0..255] = 0  (initial state in cache)
2. DMA configured: ADC1->DR → buf[0..255] (DMA bypasses cache!)
3. DMA writes buf[0..255] via AHB bus  → goes to RAM, NOT cache
4. HTC IRQ fires: "buf[0..255] is ready"
5. CPU reads buf[0]  → CACHE HIT! Returns STALE value (zero), NOT DMA data!
```

This is a **silent data corruption bug**. No crash, no error — just wrong data.
Extremely hard to debug without cache-aware analysis.

### Solution A: Cache Invalidation (preferred — best performance)

```c
// After HTC IRQ, BEFORE CPU reads:
SCB_InvalidateDCache_by_Addr((uint32_t*)&buf[0], 256 * 4);
// This marks cache lines for [buf[0]..buf[255]] as INVALID
// Next CPU read is a cache MISS → fetches from RAM → sees DMA data ✓
DSB(); // Data Synchronization Barrier: wait for invalidation to complete
```

### Solution B: Uncached DMA Buffer (simpler — slightly slower)

```c
// Mark DMA buffer as uncached in MPU:
ARM_MPU_SetRegionEx(0,                           // region 0
    (uint32_t)&dma_buf,                          // base address
    ARM_MPU_RASR(0,                              // execute never
        ARM_MPU_AP_FULL,                         // full access
        0,                                       // non-shareable
        1,                                       // B=1 (bufferable)
        0,                                       // C=0 (non-cacheable)
        0,                                       // S=0
        ARM_MPU_REGION_SIZE_2KB));
```

Every CPU read goes directly to RAM. No stale cache data. 10-20 cycles slower
per read vs cached access. Acceptable for large block DMA reads.

### Solution C: Linux dma_alloc_coherent() (kernel approach)

```c
// Allocates DMA-coherent memory (uncached + write-combining typically)
void *cpu_addr = dma_alloc_coherent(dev, size, &dma_addr, GFP_KERNEL);
// cpu_addr: CPU virtual address (uncached)
// dma_addr: DMA physical address for peripheral programming
// No cache maintenance needed. Correct on all ARM variants.
```

### Solution D: Explicit sync on Linux (for cached DMA buffers)

```c
// Before DMA transfer (CPU→DMA direction): flush cache
dma_sync_single_for_device(dev, dma_addr, size, DMA_TO_DEVICE);

// After DMA transfer (DMA→CPU direction): invalidate cache
dma_sync_single_for_cpu(dev, dma_addr, size, DMA_FROM_DEVICE);
```

---

## 5. Performance Numbers

### CPU utilization breakdown

| Component | ISR-per-sample | DMA Ping-Pong |
|---|---|---|
| IRQ overhead (cycles) | 25 × 100,000/s = 2.5M | 25 × 390/s = 9,750 |
| Data copy cycles | 0 (ISR reads reg directly) | 0 (DMA does it) |
| Cache invalidation | N/A | ~50 cycles × 390/s = 19,500 |
| Worker wake overhead | Up to 100,000/s | 390/s |
| **Total overhead** | **~2.5M cycles/s** | **~30K cycles/s** |
| **CPU utilization** | **1.5%** | **0.018%** |

**DMA saves 83× CPU cycles for interrupt overhead alone.**

### Memory bandwidth
- 100,000 samples/sec × 4 bytes = **400 KB/sec** continuous DMA bandwidth
- STM32H7 AHB bus: 480 MB/sec capacity → DMA is 0.08% of bus bandwidth
- DMA barely impacts CPU memory accesses

---

## 6. DMA vs Other Approaches — Complete Comparison

| Criterion | Ring Buffer (01) | Double Buffer (02) | DMA Ping-Pong (03) |
|---|---|---|---|
| ISR fires | 100,000/sec | 100,000/sec | 390/sec |
| CPU ISR overhead | ~1.5% | ~0.5% (fast path) | ~0.018% |
| Data copy | Yes (to ring buf) | No (ptr swap) | No (ptr to DMA buf) |
| Latency | < 1 sample | N/2 samples | N/2 samples |
| Cache coherency issue | No | No | Yes — must invalidate |
| Hardware DMA needed | No | No | Yes |
| Code complexity | Low | Medium | High |
| Buffer management | SW | SW | HW + SW |
| Error recovery | Easy | Easy | Must handle DMA errors |

---

## 7. Interview Q&A — Nvidia / Qualcomm / AMD Level

---

### Q1: Explain the DMA cache coherency problem and all solutions.

**A**: (Full answer in Section 4 above.)

The key insight interviewers want: DMA is a bus master that writes to physical
RAM. The CPU's L1 D-cache is unaware of DMA writes. The cache holds stale
pre-DMA values. CPU reads return wrong data silently.

Solutions ranked by preference:
1. `dma_alloc_coherent()` (Linux) — simplest, always correct
2. Cache invalidation per half-buffer — best performance on bare metal
3. MPU-uncached region — simple on bare metal, no per-transfer maintenance
4. Explicit `dma_sync_single_for_cpu()` (Linux with non-coherent buffers)

**Interviewer follow-up**: "What if you forget to invalidate on Cortex-M7?"
**A**: Silent data corruption. The CPU reads stale zeros or old data. System
appears functional (no crash, no fault) but produces wrong results. This is
why Valgrind/sanitizers don't catch it — it's a hardware memory model issue,
not a software race condition.

---

### Q2: What is the DMA double-buffer mode (vs software ping-pong)?

**A**: Some DMA controllers (STM32 BDMA, Qualcomm BAM) have hardware
double-buffer mode: two memory addresses (M0AR, M1AR) are hardware-managed.

```
Hardware double-buffer:
  - DMA fills M0AR (buf_A)  → fires HTC → switches to M1AR automatically
  - DMA fills M1AR (buf_B)  → fires TC  → switches back to M0AR automatically
  - CPU can safely update M0AR/M1AR while DMA uses the OTHER buffer
  - Even more robust than software ping-pong
```

Software double-buffer (approach 02 + 03):
- One circular buffer, CPU splits it logically into two halves
- Same effect, works on DMA controllers without hardware double-buffer mode

Qualcomm BAM (Bus Access Manager) supports linked-list descriptors:
- ISR fires after each descriptor completes (similar to HTC/TC)
- Multiple descriptors can be pre-programmed for zero-gap transfers

---

### Q3: How does NVIDIA handle DMA for high-bandwidth sensor input (e.g., camera)?

**A**: NVIDIA Tegra/Orin uses VI (Video Input) + ISP pipeline with dedicated
DMA engines. Key differences:

1. **Dedicated DMA channels per sensor** (up to 8 MIPI CSI lanes)
2. **Hardware synchronization**: VI writes to a ring of frame buffers. CPU
   gets IRQ only when a complete frame is ready.
3. **Cache-coherent memory**: Tegra SMMU (System Memory Management Unit) with
   cache coherency. L2 cache is shared between ARM and VI — DMA writes are
   cache-coherent automatically.
4. **Zero-copy to GPU**: Frame buffers are mapped into GPU's address space
   via IOMMU. CPU hands buffer handle (fd) to GPU via V4L2/DMABUF — no copy.

The ISR rate for a 60 fps camera = 60 IRQ/sec (one per frame), regardless of
pixel rate (e.g., 4K = 8 million pixels/frame, millions of DMA transfers between IRQs).

---

### Q4: What is NDTR (Number of Data to Transfer) and how does it help debug?

**A**: NDTR is a read-only register in the DMA controller that counts down
from initial transfer size to 0, then resets (circular mode).

Reading NDTR at any time tells you exactly how far the DMA has progressed:
```c
uint32_t ndtr = DMA1_Stream0->NDTR;
uint32_t current_fill_pos = DMA_BUF_SIZE - ndtr;
```

Debugging uses:
1. **Overflow investigation**: Did worker process buf[0..255] before DMA
   overwrapped? Check if NDTR < 256 when HTC fires again.
2. **Transfer stuck detection**: NDTR not changing → DMA starved (peripheral FIFO empty).
3. **Latency measurement**: Time between HTC IRQ and when NDTR crosses mid-point.

On Linux: `/sys/kernel/debug/dma/<channel>/ndtr` (on some drivers).

---

### Q5: What is a DMA FIFO underrun and how do you prevent it?

**A**: DMA FIFO underrun occurs when the peripheral demands data faster than
the DMA can supply it (in memory-to-peripheral direction), or when DMA tries
to burst-write to memory but the AHB bus is congested.

For peripheral-to-memory (our case — ADC to RAM):
- Peripheral FIFO (ADC result register) fills faster than DMA drains it
- Peripheral sets an overrun flag, new data is discarded
- Root cause: DMA burst size too small, AHB bus contention

Fixes:
1. **Increase DMA FIFO threshold**: `DMA_FIFOMODE_ENABLE + DMA_FIFO_THRESHOLD_FULL`
   — DMA bursts 4 words at once instead of 1 word × 4 times. More efficient.
2. **Reduce AHB bus contention**: Don't run CPU-intensive code that accesses flash
   (instruction fetch) during DMA. Place critical code in ITCM (not AHB).
3. **Match DMA burst size to peripheral FIFO depth**: ADC has 1-word FIFO →
   use 1-word DMA burst. SPI has 8-word FIFO → use 8-word burst.
4. **DMA arbitration**: Set DMA stream priority to `DMA_PRIORITY_VERY_HIGH` if
   multiple DMA streams compete for the same AHB bus.

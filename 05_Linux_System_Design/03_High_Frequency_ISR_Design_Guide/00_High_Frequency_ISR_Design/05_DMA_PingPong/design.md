# 05 — DMA Half/Full Transfer Ping-Pong

## The Core Insight

At 100 kHz, the CPU services **100,000 interrupts/sec** — one every 10 µs. Even a 5-cycle ISR consumes:

```
5 cycles × 100,000/sec = 500,000 cycles/sec
At 168 MHz: 500k / 168M = 0.3% CPU just for ISR overhead
Plus context save/restore: +12 cycles each = 2.4M cycles/sec = 1.4% CPU
```

**DMA eliminates per-sample interrupts entirely.** The DMA controller streams data from the peripheral to memory autonomously. The CPU wakes only when a half-buffer or full-buffer completes.

---

## Architecture

```
 Peripheral          DMA Controller          CPU (ISR)
 (ADC/UART/SPI)
      │                    │                     │
      │ sample (every 10µs)│                     │
      ├──────────────────► │                     │
      │                    │ DMA writes           │
      │                    │ buf[0]               │
      │                    │ buf[1]               │
      │                    │  ...                 │
      │                    │ buf[255]             │
      │                    │───── HALF-TRANSFER ─►│ DMA_HalfTransfer_ISR()
      │                    │                     │ Sets htc_pending=1
      │                    │ buf[256]             │
      │                    │  ...                 │
      │                    │ buf[511]             │
      │                    │───── FULL-TRANSFER ─►│ DMA_FullTransfer_ISR()
      │                    │ (wraps to buf[0])   │ Sets tc_pending=1
      │                    │                     │
      │                    ▼                     ▼
                       DMA running          Worker Task
                       continuously         polls pending flags
                                            processes half-buffers
```

---

## DMA Setup (STM32 HAL Reference)

```c
/* Configure DMA for circular mode with half/full-transfer interrupts */
hdma.Init.Mode         = DMA_CIRCULAR;      /* wrap automatically */
hdma.Init.Direction    = DMA_PERIPH_TO_MEMORY;
hdma.Init.PeriphInc    = DMA_PINC_DISABLE;
hdma.Init.MemInc       = DMA_MINC_ENABLE;
hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
hdma.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;

HAL_DMA_Init(&hdma);
HAL_DMA_Start_IT(&hdma,
    (uint32_t)&ADC1->DR,           /* source: ADC data register  */
    (uint32_t)dma_buf,             /* destination: our buffer    */
    DMA_BUF_LEN);                  /* 512 transfers before wrap  */

/* Register callbacks */
HAL_DMA_RegisterCallback(&hdma, HAL_DMA_XFER_HALFCPLT_CB_ID, DMA_HalfTransfer_ISR);
HAL_DMA_RegisterCallback(&hdma, HAL_DMA_XFER_CPLT_CB_ID,     DMA_FullTransfer_ISR);
```

---

## Interrupt Rate Reduction

```
Without DMA:
  CPU interrupts = sample rate = 100,000/sec
  ISR runs every 10 µs

With DMA (DMA_BUF_LEN = 512):
  Half-transfer IRQ every: 256 samples / 100,000 Hz = 2.56 ms
  Full-transfer IRQ every: 512 samples / 100,000 Hz = 5.12 ms
  Total CPU IRQs/sec = 2 × (100,000 / 512) = 390/sec

Reduction: 100,000 → 390 = 99.6% fewer interrupts
```

---

## Cache Coherency (Critical for Cortex-M7 / Cortex-A / NVIDIA / Qualcomm)

When D-cache is enabled:

```
DMA writes:  Physical memory (SRAM) ← DMA bypasses CPU cache
CPU reads:   Cache → may read STALE cached data from before DMA transfer

Problem: Worker calls process_fn(buf, 256) and reads old data.
```

**Solutions by platform:**

| Platform | Solution |
|---|---|
| STM32H7 (Cortex-M7) | `SCB_InvalidateDCache_by_Addr(buf, len)` before reading |
| Linux / Cortex-A | `dma_sync_single_for_cpu()` — invalidates cache for DMA region |
| Qualcomm Hexagon DSP | Mark DMA buffer as uncached in MMU; use `qdsp6_cache_cleaninv()` |
| NVIDIA Tegra (camera ISP) | Use `dma_alloc_coherent()` — allocates uncacheable memory |
| Any ARM | Configure DMA region as Device/Normal-Non-Cacheable in MPU/MMU |

**In this implementation:** the portable simulation doesn't enable D-cache. The `/* cache invalidation needed here */` comment in `dma_pingpong.c` marks where you add platform-specific invalidation.

---

## Memory Ordering in ISR → Worker Path

```
DMA completes filling buf[0..255]
    ↓
DMA_HalfTransfer_ISR() fires:
    buf data already in memory (DMA is a memory system master)
    MEM_BARRIER()       ← RELEASE: ensures buf visible before flag
    htc_pending = 1

Worker polls:
    if (htc_pending):
        MEM_BARRIER()   ← ACQUIRE: see all memory written before flag
        process_fn(buf, 256)
        htc_pending = 0
```

---

## What Happens Without DMA (for comparison)

```c
/* Software equivalent — ISR fires 100,000 times/sec */
void ADC_ISR_no_dma(void)
{
    buf[write_idx++] = ADC1->DR;     /* 1 store per interrupt */
    if (write_idx == 256) {
        write_idx = 0;
        signal_worker();
    }
}
/* This IS the double buffer approach (dir 02) —
 * software implementation of what DMA does in hardware */
```

DMA offloads the `buf[idx++] = ADC->DR` writes entirely to hardware. The CPU only sees the interrupt every 256 samples instead of every sample.

---

## Common Interview Questions

**Q: How does DMA circular mode work? What is the NDTR register?**  
A: NDTR (Number of Data To transfer Register) counts down from DMA_BUF_LEN to 0. When it reaches 0, the DMA controller automatically reloads it to DMA_BUF_LEN and fires the full-transfer interrupt (TC). At NDTR = DMA_BUF_LEN / 2, it fires the half-transfer interrupt (HTC). The destination address pointer wraps to `buf[0]`. The CPU never reprograms the DMA — it runs continuously.

**Q: You mentioned cache coherency. How do NVIDIA GPU projects handle this?**  
A: NVIDIA Tegra/AGX platforms use `dma_alloc_coherent()` for camera and ISP DMA buffers — this allocates physically contiguous, cache-coherent (uncached or write-through) memory. The camera ISP DMA writes directly to these buffers; the CPU reads without cache invalidation. For high-throughput pipelines (4K at 60fps), they also use IOMMU to map DMA to virtual addresses and `dma_buf` for zero-copy sharing between GPU and CPU.

**Q: What is DMA FIFO underrun and how do you prevent it?**  
A: DMA FIFO underrun occurs when the peripheral requests data faster than the DMA can supply it (or vice versa). On STM32: configure `DMA_FIFOMODE_ENABLE` and `DMA_FIFO_THRESHOLD_FULL` to buffer 4 words before transferring. For ADC with high sample rates: ensure DMA priority is set to `DMA_PRIORITY_HIGH` or `VERY_HIGH` so it preempts lower-priority DMA streams.

**Q: Can you use DMA with SPI at 100 kHz?**  
A: Yes. SPI DMA is common. Configure SPI peripheral in DMA receive mode: SPI_DR → DMA → buf. The SPI peripheral drives the clock; DMA captures incoming bytes. At 100 kHz sample rate with 16-bit samples: 200 kbps data — well within SPI DMA capability (SPI can run at 50 MHz+).

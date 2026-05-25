#ifndef DMA_CONFIG_H
#define DMA_CONFIG_H

/*
 * dma_config.h — DMA Ping-Pong (Circular Double Buffer) Configuration
 *
 * Platform model: ARM Cortex-M7 / STM32H7 style DMA controller.
 * Concepts apply identically to:
 *   - Qualcomm Hexagon QDSP6 DMA (BAM / GPI)
 *   - NVIDIA Tegra/Xavier GPCDMA
 *   - TI C66x EDMA3
 *   - Linux DMA engine (dmaengine API)
 *
 * ============================================================
 * HOW DMA PING-PONG WORKS
 * ============================================================
 *
 * The DMA controller is configured in CIRCULAR + DOUBLE_BUFFER mode:
 *
 *   Memory: [ buf[0] ... buf[255] | buf[256] ... buf[511] ]
 *             ← First Half (M0) →   ← Second Half (M1) →
 *
 * DMA fills continuously:
 *   1. DMA fills buf[0..255]      → fires HALF_TRANSFER_COMPLETE (HTC) IRQ
 *   2. DMA fills buf[256..511]    → fires TRANSFER_COMPLETE (TC) IRQ
 *   3. DMA wraps back to buf[0]   → fires HTC again, and so on.
 *
 * CPU ISR actions:
 *   HTC IRQ: buf[0..255] is STABLE (DMA writing to second half).
 *            CPU processes buf[0..255] concurrently with DMA filling 256..511.
 *   TC  IRQ: buf[256..511] is STABLE (DMA wrapping to first half).
 *            CPU processes buf[256..511] concurrently with DMA filling 0..255.
 *
 * CPU ISR rate reduction:
 *   Original ISR rate: 100,000 / sec (one IRQ per sample, software approach)
 *   DMA ping-pong:     100,000 / 256 = ~390 IRQ/sec
 *   CPU savings:       99.6% reduction in interrupt overhead!
 *
 * ============================================================
 * CACHE COHERENCY ISSUE (CRITICAL for Cortex-M7 / Cortex-A)
 * ============================================================
 *
 * Cortex-M7 has a D-cache (data cache). DMA controller is a bus master
 * that writes directly to AHB/AXI bus — BYPASSING the CPU cache.
 *
 * Problem:
 *   1. DMA writes buf[0..255] to RAM (D-cache does not know about this)
 *   2. CPU reads buf[0..255] — gets STALE DATA from cache, not DMA data!
 *
 * Fix: Invalidate D-cache for the DMA buffer region BEFORE CPU reads it.
 *   SCB_InvalidateDCache_by_Addr(buf, DMA_HALF_SIZE * sizeof(dma_sample_t));
 *
 * Alternative: Mark DMA buffer as uncached (MPU: non-cacheable, bufferable).
 *   Pro: No manual cache management
 *   Con: CPU reads are slower (no cache benefit, direct AHB access ~10 cycles)
 *
 * On Qualcomm Hexagon DSP:
 *   hexagon_cache_invalidate() for BAM DMA
 *   ION memory with CACHED|CLEAN flag for shared ARM↔DSP DMA buffers
 *
 * On NVIDIA Tegra:
 *   tegra_cache_flush_and_inv_range() before handing buffer to CPU
 *   Or use coherent DMA memory: dma_alloc_coherent() in Linux
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---- Configuration -------------------------------------------------------- */

#define DMA_BUF_SIZE    512u        /* Total DMA circular buffer (samples)    */
#define DMA_HALF_SIZE   (DMA_BUF_SIZE / 2u)  /* Samples per half            */
#define DMA_SAMPLE_BYTES 4u         /* sizeof(dma_sample_t)                   */

_Static_assert((DMA_BUF_SIZE & 1u) == 0u, "DMA_BUF_SIZE must be even (two halves)");
_Static_assert(DMA_BUF_SIZE >= 4u,         "DMA_BUF_SIZE must be at least 4");

/* ---- Types ---------------------------------------------------------------- */

typedef uint32_t dma_sample_t;

/*
 * dma_circular_buf_t — the actual DMA target memory
 *
 * MUST be aligned to cache line (32 bytes on Cortex-M7, 64 on Cortex-A).
 * MUST be in DMA-accessible RAM (not CCM/DTCM on STM32H7).
 * On Linux: allocated with dma_alloc_coherent() or dma_alloc_wc().
 */
typedef struct {
    dma_sample_t samples[DMA_BUF_SIZE];
} __attribute__((aligned(32))) dma_circular_buf_t;

/* Callback: called from IRQ context with pointer to stable half */
typedef void (*dma_data_cb_t)(const dma_sample_t *half, uint32_t count);

typedef enum {
    DMA_STATE_UNINIT    = 0,
    DMA_STATE_RUNNING   = 1,
    DMA_STATE_OVERFLOW  = 2,  /* Worker too slow, DMA overwrote unread data */
    DMA_STATE_ERROR     = 3,  /* DMA transfer error (FIFO underrun, etc.)   */
} dma_state_t;

/* DMA controller state */
typedef struct {
    dma_state_t      state;
    dma_data_cb_t    on_half_ready;    /* callback fired from IRQ             */
    volatile bool    worker_busy;      /* worker is processing a half         */
    volatile uint32_t htc_count;       /* Half Transfer Complete IRQ count    */
    volatile uint32_t tc_count;        /* Transfer Complete IRQ count         */
    volatile uint32_t overflow_count;  /* worker-busy on IRQ fire             */
    volatile uint32_t error_count;     /* DMA error IRQ count                 */
} dma_ctrl_t;

/* ---- API ------------------------------------------------------------------ */

/*
 * dma_setup() — configure DMA controller for circular ping-pong
 * Must be called once before dma_start().
 */
void dma_setup(dma_ctrl_t *ctrl, dma_circular_buf_t *buf, dma_data_cb_t cb);

/* dma_start() — enable DMA transfers (starts filling buf) */
void dma_start(dma_ctrl_t *ctrl);

/* dma_stop() — disable DMA, abort in-progress transfer */
void dma_stop(dma_ctrl_t *ctrl);

/*
 * dma_half_transfer_irq() — called from DMA HTC interrupt vector
 * Do NOT call from user code.
 */
void dma_half_transfer_irq(dma_ctrl_t *ctrl, dma_circular_buf_t *buf);

/*
 * dma_full_transfer_irq() — called from DMA TC interrupt vector
 * Do NOT call from user code.
 */
void dma_full_transfer_irq(dma_ctrl_t *ctrl, dma_circular_buf_t *buf);

/*
 * dma_error_irq() — called from DMA error interrupt vector
 */
void dma_error_irq(dma_ctrl_t *ctrl);

/* Worker signals processing complete */
void dma_worker_done(dma_ctrl_t *ctrl);

#endif /* DMA_CONFIG_H */

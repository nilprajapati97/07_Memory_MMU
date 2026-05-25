/*
 * dma_isr.c — DMA Ping-Pong IRQ Handlers
 *
 * Two IRQ handlers:
 *   dma_half_transfer_irq() — DMA has filled buf[0..HALF_SIZE-1]
 *   dma_full_transfer_irq() — DMA has filled buf[HALF_SIZE..SIZE-1]
 *
 * Both handlers:
 *   1. Validate DMA is still in RUNNING state
 *   2. Check worker is free (not processing previous half)
 *   3. Invalidate D-cache for the filled half (if cache enabled)
 *   4. Issue memory barrier (ensure DMA writes visible to CPU)
 *   5. Set worker_busy flag
 *   6. Call on_half_ready() callback with pointer to stable half
 *
 * The CPU ISR rate at 100 kHz / 256 half-size = ~390 IRQ/sec.
 * Each IRQ handler executes in < 1 µs (cache invalidation dominates).
 */

#include "dma_config.h"

/* ---- Memory barrier ------------------------------------------------------- */

#if defined(__ARM_ARCH)
    /* DSB: Data Synchronization Barrier — stronger than DMB.
     * Ensures all memory accesses AND cache maintenance operations complete.
     * Required after cache invalidate before reading the invalidated region. */
    #define MEM_SYNC_BARRIER()  __asm__ __volatile__("dsb sy" ::: "memory")
    #define MEM_DATA_BARRIER()  __asm__ __volatile__("dmb sy" ::: "memory")
#else
    #define MEM_SYNC_BARRIER()  __asm__ __volatile__("" ::: "memory")
    #define MEM_DATA_BARRIER()  __asm__ __volatile__("" ::: "memory")
#endif

/* ---- Cache invalidation helper -------------------------------------------- */

/*
 * cache_invalidate_region() — invalidate D-cache for [addr, addr+size_bytes)
 *
 * On Cortex-M7 with D-cache enabled:
 *   SCB_InvalidateDCache_by_Addr((uint32_t*)addr, size_bytes);
 *
 * Why needed:
 *   DMA writes to physical RAM, bypassing the CPU's data cache.
 *   CPU cache still holds stale data (pre-DMA values).
 *   Cache invalidation marks cache lines as invalid → forces CPU to
 *   re-fetch from RAM on next access → sees actual DMA-written data.
 *
 * Must be called BEFORE CPU reads the DMA buffer.
 * Must be called AFTER DMA has finished writing to the region (IRQ fires).
 *
 * Cache line size: 32 bytes on Cortex-M7, 64 bytes on Cortex-A55/A78.
 * Invalidate in 32/64-byte granules — partial line invalidate corrupts
 * adjacent valid data in the same line.
 *
 * On Qualcomm Hexagon: hexagon_cache_invalidate(addr, size)
 * On Linux (generic): dma_sync_single_for_cpu(dev, dma_addr, size, DMA_FROM_DEVICE)
 */
static void cache_invalidate_region(void *addr, size_t size_bytes)
{
#if defined(__ARM_ARCH) && defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1)
    /* CMSIS-Core: SCB_InvalidateDCache_by_Addr */
    extern void SCB_InvalidateDCache_by_Addr(uint32_t *addr, int32_t dsize);
    SCB_InvalidateDCache_by_Addr((uint32_t *)addr, (int32_t)size_bytes);
    MEM_SYNC_BARRIER(); /* DSB: ensure invalidation complete before CPU reads */
#else
    (void)addr;
    (void)size_bytes;
    MEM_DATA_BARRIER(); /* Still need barrier for store-buffer ordering */
#endif
}

/* ---- DMA Setup & Control -------------------------------------------------- */

void dma_setup(dma_ctrl_t *ctrl, dma_circular_buf_t *buf, dma_data_cb_t cb)
{
    ctrl->state          = DMA_STATE_UNINIT;
    ctrl->on_half_ready  = cb;
    ctrl->worker_busy    = false;
    ctrl->htc_count      = 0u;
    ctrl->tc_count       = 0u;
    ctrl->overflow_count = 0u;
    ctrl->error_count    = 0u;

    /*
     * Platform-specific DMA configuration (STM32H7 example):
     *
     * DMA_InitTypeDef init = {
     *   .Request             = DMA_REQUEST_ADC1,
     *   .Direction           = DMA_PERIPH_TO_MEMORY,
     *   .PeriphInc           = DMA_PINC_DISABLE,
     *   .MemInc              = DMA_MINC_ENABLE,
     *   .PeriphDataAlignment = DMA_PDATAALIGN_WORD,   // 32-bit source
     *   .MemDataAlignment    = DMA_MDATAALIGN_WORD,   // 32-bit dest
     *   .Mode                = DMA_CIRCULAR,          // wrap around
     *   .Priority            = DMA_PRIORITY_HIGH,
     *   .FIFOMode            = DMA_FIFOMODE_DISABLE,  // direct mode
     * };
     * HAL_DMA_Init(&hdma_adc1, &init);
     *
     * // Enable Half Transfer + Transfer Complete interrupts
     * __HAL_DMA_ENABLE_IT(&hdma_adc1, DMA_IT_HT | DMA_IT_TC | DMA_IT_TE);
     *
     * // Link to NVIC
     * HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 2, 0);
     * HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
     */
    (void)buf;
}

void dma_start(dma_ctrl_t *ctrl)
{
    ctrl->state = DMA_STATE_RUNNING;
    /*
     * HAL_ADC_Start_DMA(&hadc1, (uint32_t*)buf->samples, DMA_BUF_SIZE);
     * Or for UART: HAL_UART_Receive_DMA(&huart1, buf->samples, DMA_BUF_SIZE);
     */
}

void dma_stop(dma_ctrl_t *ctrl)
{
    ctrl->state = DMA_STATE_UNINIT;
    /* HAL_DMA_Abort(&hdma_adc1); */
}

/* ---- IRQ Handlers --------------------------------------------------------- */

/*
 * dma_half_transfer_irq()
 *
 * Called when DMA completes buf[0..HALF_SIZE-1].
 * DMA is NOW writing to buf[HALF_SIZE..SIZE-1] — first half is stable.
 *
 * Timeline (concurrent with this handler):
 *   DMA:    writing buf[256..511]        ← DMA activity
 *   CPU:    processing buf[0..255]       ← our work
 *
 * The two halves are physically separate memory regions.
 * No cache conflict, no DMA vs CPU contention. Perfect parallelism.
 */
void dma_half_transfer_irq(dma_ctrl_t *ctrl, dma_circular_buf_t *buf)
{
    if (ctrl->state != DMA_STATE_RUNNING) {
        return;
    }

    ctrl->htc_count++;

    if (ctrl->worker_busy) {
        /*
         * Worker is still processing the PREVIOUS half.
         * DMA has already overwritten the old data.
         * This is a hard overflow — data loss is unavoidable at this point.
         * Root cause: worker took > 2.56 ms (256/100kHz) to process one half.
         */
        ctrl->overflow_count++;
        ctrl->state = DMA_STATE_OVERFLOW;
        return;
    }

    /*
     * Cache invalidation for FIRST half: buf[0..HALF_SIZE-1]
     * DMA wrote to physical RAM. CPU cache has stale data.
     * Invalidate cache lines covering [buf->samples, buf->samples + HALF_SIZE*4).
     */
    cache_invalidate_region(
        &buf->samples[0],
        DMA_HALF_SIZE * DMA_SAMPLE_BYTES
    );

    /* Mark worker as busy BEFORE calling callback (callback may run async) */
    ctrl->worker_busy = true;

    /* Hand off first half — zero-copy pointer into DMA buffer */
    if (ctrl->on_half_ready) {
        ctrl->on_half_ready(&buf->samples[0], DMA_HALF_SIZE);
    }
}

/*
 * dma_full_transfer_irq()
 *
 * Called when DMA completes buf[HALF_SIZE..SIZE-1].
 * DMA wraps back to buf[0] — second half is now stable.
 *
 * Timeline:
 *   DMA:    writing buf[0..255] (wrapped)     ← DMA activity
 *   CPU:    processing buf[256..511]           ← our work
 */
void dma_full_transfer_irq(dma_ctrl_t *ctrl, dma_circular_buf_t *buf)
{
    if (ctrl->state != DMA_STATE_RUNNING) {
        return;
    }

    ctrl->tc_count++;

    if (ctrl->worker_busy) {
        ctrl->overflow_count++;
        ctrl->state = DMA_STATE_OVERFLOW;
        return;
    }

    /* Cache invalidation for SECOND half */
    cache_invalidate_region(
        &buf->samples[DMA_HALF_SIZE],
        DMA_HALF_SIZE * DMA_SAMPLE_BYTES
    );

    ctrl->worker_busy = true;

    if (ctrl->on_half_ready) {
        ctrl->on_half_ready(&buf->samples[DMA_HALF_SIZE], DMA_HALF_SIZE);
    }
}

void dma_error_irq(dma_ctrl_t *ctrl)
{
    ctrl->error_count++;
    ctrl->state = DMA_STATE_ERROR;
    /*
     * DMA errors: FIFO overrun, transfer error, direct mode error.
     * Recovery: stop, reconfigure, restart DMA.
     * In production: notify watchdog, trigger safe state.
     * HAL: check __HAL_DMA_GET_FLAG(&hdma, DMA_FLAG_FEIF0_4)
     */
}

void dma_worker_done(dma_ctrl_t *ctrl)
{
    /* Worker signals it has finished processing the current half */
    ctrl->worker_busy = false;
}

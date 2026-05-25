/*
 * dma_pingpong.c — DMA Ping-Pong Controller Implementation
 *
 * Cache coherency note (Cortex-M7 / Cortex-A / NVIDIA Tegra / Qualcomm):
 *   When D-cache is enabled, DMA writes to physical memory but the CPU
 *   may read stale cached data.  Before calling process_fn(), the worker
 *   must invalidate the cache lines for the target half-buffer:
 *
 *     SCB_InvalidateDCache_by_Addr((uint32_t *)buf_ptr, DMA_HALF_LEN * sizeof(dma_data_t));
 *     __DSB();
 *
 *   This is omitted in this portable implementation (simulation target).
 *   In production: use an uncached MPU region or DMA coherent memory.
 */

#include "dma_pingpong.h"

#if defined(__ARM_ARCH)
    #define MEM_BARRIER() __asm__ volatile ("dmb sy" ::: "memory")
#else
    #define MEM_BARRIER() __asm__ volatile ("" ::: "memory")
#endif

void dma_ctrl_init(dma_ctrl_t *ctrl)
{
    ctrl->htc_pending      = 0u;
    ctrl->tc_pending       = 0u;
    ctrl->htc_count        = 0u;
    ctrl->tc_count         = 0u;
    ctrl->overflow_cnt     = 0u;
    ctrl->processed_halves = 0u;
}

void DMA_HalfTransfer_ISR(dma_ctrl_t *ctrl)
{
    /*
     * DMA just finished filling buf[0..HALF_LEN-1].
     * DMA continues filling buf[HALF_LEN..LEN-1] concurrently.
     *
     * ISR only sets a flag — NEVER processes data here.
     * The 2.56 ms until the next interrupt gives the worker plenty of time.
     */
    ctrl->htc_count++;

    if (ctrl->htc_pending) {
        /* Worker hasn't consumed the previous HTC yet — overflow */
        ctrl->overflow_cnt++;
    }

    MEM_BARRIER();          /* RELEASE: DMA data in buf[0..HALF_LEN-1] visible */
    ctrl->htc_pending = 1u;
}

void DMA_FullTransfer_ISR(dma_ctrl_t *ctrl)
{
    /*
     * DMA just finished buf[HALF_LEN..LEN-1].
     * DMA wraps to buf[0] automatically (circular mode, NDTR reloads).
     */
    ctrl->tc_count++;

    if (ctrl->tc_pending) {
        ctrl->overflow_cnt++;
    }

    MEM_BARRIER();          /* RELEASE: DMA data in buf[HALF_LEN..LEN-1] visible */
    ctrl->tc_pending = 1u;
}

uint32_t dma_worker_poll(dma_ctrl_t *ctrl,
                         void (*process_fn)(const dma_data_t *, uint32_t))
{
    uint32_t count = 0u;

    if (ctrl->htc_pending) {
        MEM_BARRIER();                              /* ACQUIRE: see DMA writes */
        process_fn(ctrl->buf, DMA_HALF_LEN);        /* process first half      */
        ctrl->htc_pending = 0u;
        ctrl->processed_halves++;
        count++;
    }

    if (ctrl->tc_pending) {
        MEM_BARRIER();
        process_fn(ctrl->buf + DMA_HALF_LEN, DMA_HALF_LEN); /* second half    */
        ctrl->tc_pending = 0u;
        ctrl->processed_halves++;
        count++;
    }

    return count;
}

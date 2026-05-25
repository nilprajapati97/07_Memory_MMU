/*
 * dma_pingpong.h — DMA Half/Full Transfer Ping-Pong Controller
 *
 * Guide §5 insight:
 *   Without DMA: CPU takes 100,000 interrupts/sec to receive data.
 *   With DMA:    DMA streams data to memory automatically.
 *                CPU only interrupts at half-buffer and full-buffer points.
 *
 *   Example: DMA_BUF_LEN=512, sample rate=100 kHz
 *     DMA fills buf[0..255]   in 2.56 ms → Half-Transfer IRQ fires
 *     DMA fills buf[256..511] in 2.56 ms → Full-Transfer  IRQ fires
 *     DMA wraps to buf[0] automatically (circular mode)
 *
 *   CPU interrupt rate: 2 × (100,000 / 512) ≈ 390 IRQs/sec
 *   CPU overhead reduction: 100,000 → 390 = 99.6% reduction
 *
 * Hardware setup (STM32 HAL example):
 *   HAL_DMA_Start_IT(&hdma, (uint32_t)&ADC1->DR, (uint32_t)buf, DMA_BUF_LEN);
 *   Register HAL_DMA_XFER_HALFCPLT_CB_ID → DMA_HalfTransfer_ISR()
 *   Register HAL_DMA_XFER_CPLT_CB_ID     → DMA_FullTransfer_ISR()
 */

#ifndef DMA_PINGPONG_H
#define DMA_PINGPONG_H

#include <stdint.h>
#include <stdbool.h>

/* ---- Configuration ------------------------------------------------------- */

#define DMA_BUF_LEN   512u          /* total DMA circular buffer length       */
#define DMA_HALF_LEN  (DMA_BUF_LEN / 2u)

typedef uint32_t dma_data_t;

/* ---- DMA control block --------------------------------------------------- */

typedef struct {
    dma_data_t        buf[DMA_BUF_LEN]; /* DMA writes here (HW → memory)     */
    volatile uint8_t  htc_pending;      /* half-transfer complete pending     */
    volatile uint8_t  tc_pending;       /* full-transfer complete pending     */
    volatile uint32_t htc_count;        /* total half-transfer IRQs fired     */
    volatile uint32_t tc_count;         /* total full-transfer IRQs fired     */
    volatile uint32_t overflow_cnt;     /* worker overrun count               */
    volatile uint32_t processed_halves; /* total half-buffers worker consumed */
} dma_ctrl_t;

/* ---- API ----------------------------------------------------------------- */

void dma_ctrl_init(dma_ctrl_t *ctrl);

/**
 * DMA_HalfTransfer_ISR() — Call from DMA half-transfer interrupt handler.
 * Signals that buf[0..HALF_LEN-1] is ready for the worker.
 */
void DMA_HalfTransfer_ISR(dma_ctrl_t *ctrl);

/**
 * DMA_FullTransfer_ISR() — Call from DMA full-transfer interrupt handler.
 * Signals that buf[HALF_LEN..LEN-1] is ready for the worker.
 * DMA hardware wraps back to buf[0] automatically.
 */
void DMA_FullTransfer_ISR(dma_ctrl_t *ctrl);

/**
 * dma_worker_poll() — Process any pending half-buffers. Call from task context.
 * @param process_fn  Callback invoked with (buffer_ptr, sample_count).
 * @return number of half-buffers processed this call (0 = nothing pending).
 */
uint32_t dma_worker_poll(dma_ctrl_t *ctrl,
                         void (*process_fn)(const dma_data_t *, uint32_t));

#endif /* DMA_PINGPONG_H */

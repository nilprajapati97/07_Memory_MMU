#define _POSIX_C_SOURCE 200809L   /* nanosleep */

/*
 * isr_demo.c — DMA Ping-Pong ISR + Worker Simulation
 *
 * Demonstrates interrupt rate reduction from DMA:
 *   Without DMA: 100,000 CPU interrupts/sec
 *   With DMA (DMA_BUF_LEN=512): ~390 CPU interrupts/sec  (99.6% reduction)
 *
 * Simulation maps directly to real hardware:
 *   dma_sim_thread()     → DMA controller (hardware fills buffer autonomously)
 *   DMA_HalfTransfer_ISR → NVIC interrupt handler (fires at half-full)
 *   DMA_FullTransfer_ISR → NVIC interrupt handler (fires at full)
 *   main() worker loop   → High-priority RTOS task
 *
 * Compile:
 *   gcc -std=c11 -O2 -DSIMULATION dma_pingpong.c isr_demo.c -lpthread -o test_dma
 */

#include "dma_pingpong.h"
#include <stdio.h>

/* ---- Global state -------------------------------------------------------- */

static dma_ctrl_t g_dma;

/* ---- Processing callback ------------------------------------------------- */

static void process_half(const dma_data_t *buf, uint32_t len)
{
    (void)buf;
    (void)len;
    /* Real: FIR filter, FFT, protocol decode, CRC check */
}

/* ==========================================================================
 * SIMULATION
 * ========================================================================== */
#ifdef SIMULATION

#include <pthread.h>
#include <time.h>

/* 400 half-buffers = 200 HTC + 200 TC events */
#define SIM_HALVES  400u

static volatile int g_sim_done = 0;

/*
 * dma_sim_thread — simulates the DMA controller filling memory.
 *
 * Each sample takes 10 µs (100 kHz).
 * DMA_HALF_LEN samples per half → 256 × 10 µs = 2.56 ms per half.
 * Fires HTC after even halves, TC after odd halves.
 */
static void *dma_sim_thread(void *arg)
{
    (void)arg;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000 };  /* 10 µs */

    for (uint32_t h = 0u; h < SIM_HALVES; h++) {
        /* Simulate DMA filling one half-buffer sample by sample */
        for (uint32_t s = 0u; s < DMA_HALF_LEN; s++) {
            nanosleep(&ts, NULL);
        }

        /* Fire the interrupt for this half */
        if ((h & 1u) == 0u)
            DMA_HalfTransfer_ISR(&g_dma);  /* even half → HTC */
        else
            DMA_FullTransfer_ISR(&g_dma);  /* odd  half → TC  */
    }

    g_sim_done = 1;
    return NULL;
}

int main(void)
{
    dma_ctrl_init(&g_dma);

    printf("DMA simulation: %u half-buffers (%u samples each) @ 100 kHz\n",
           SIM_HALVES, DMA_HALF_LEN);
    printf("Expected CPU interrupt rate: ~%.0f IRQs/sec (vs 100,000 without DMA)\n",
           (double)SIM_HALVES / ((double)SIM_HALVES * DMA_HALF_LEN / 100000.0));

    pthread_t tid;
    pthread_create(&tid, NULL, dma_sim_thread, NULL);

    /* Worker poll loop — RTOS: block on event flag or semaphore */
    while (!g_sim_done || g_dma.htc_pending || g_dma.tc_pending) {
        dma_worker_poll(&g_dma, process_half);
    }

    pthread_join(tid, NULL);

    printf("--- DMA Simulation Results ---\n");
    printf("Halves processed : %u / %u\n", g_dma.processed_halves, SIM_HALVES);
    printf("HTC IRQs fired   : %u\n",      g_dma.htc_count);
    printf("TC  IRQs fired   : %u\n",      g_dma.tc_count);
    printf("Overflow count   : %u\n",      g_dma.overflow_cnt);
    printf("Status: %s\n", g_dma.overflow_cnt == 0u ? "PASS" : "FAIL");

    return (int)g_dma.overflow_cnt;
}

#endif /* SIMULATION */

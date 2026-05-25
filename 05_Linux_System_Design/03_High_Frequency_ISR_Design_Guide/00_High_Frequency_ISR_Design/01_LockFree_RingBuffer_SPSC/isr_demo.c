#define _POSIX_C_SOURCE 200809L   /* nanosleep */

/*
 * isr_demo.c — ISR + Worker Task for SPSC Ring Buffer
 *
 * This file shows:
 *   1. hardware_isr():  reads data, pushes to ring buffer, signals worker.
 *   2. worker_task():   drains the entire buffer in one pass (batch drain).
 *
 * Critical design: worker drains ALL pending items per wake, not just one.
 *   Wrong:  pop one item → go back to sleep → wake again → pop one → ...
 *   Right:  wake → drain entire buffer → sleep
 *
 *   At 100 kHz with 1-sample drain: 100,000 context switches/sec.
 *   With batch drain: only as many context switches as semaphore posts
 *   (often much less if worker is slower than ISR burst).
 *
 * Compile (simulation):
 *   gcc -std=c11 -O2 -DSIMULATION ring_buffer.c isr_demo.c -lpthread -o test_rb
 */

#include "ring_buffer.h"
#include <stdint.h>
#include <stdio.h>

/* ---- Global state -------------------------------------------------------- */

static ring_buf_t g_rb;

/*
 * Semaphore: volatile uint32_t acting as a binary flag.
 * In RTOS:
 *   FreeRTOS: SemaphoreHandle_t + xSemaphoreGiveFromISR() / xSemaphoreTake()
 *   CMSIS:    osSemaphoreId_t   + osSemaphoreRelease()    / osSemaphoreAcquire()
 */
static volatile uint32_t g_data_ready = 0u;

/* ---- Simulated hardware register ----------------------------------------- */

static inline rb_data_t read_hw_register(void)
{
    /* Real: return UART1->RDR;  or ADC1->DR;  or SPI1->DR; */
    static uint32_t seq = 0u;
    return seq++;
}

/* ---- ISR handler ---------------------------------------------------------
 *
 * Execution budget at 100 kHz:
 *   Period = 10 µs.  ISR must finish in < 5 µs (50% budget rule).
 *   At 100 MHz Cortex-M4: 5 µs = 500 cycles.
 *   This ISR: ~8–12 cycles (register read + push + signal).
 *
 * Rule: read hardware data FIRST — clears the interrupt flag on most
 *       peripherals (UART, SPI, ADC). Delay reading → spurious re-entry.
 */
void hardware_isr(void)
{
    /* ✅ Step 1: Read hardware data immediately */
    rb_data_t data = read_hw_register();

    /* ✅ Step 2: Push to lock-free ring buffer (no blocking, no malloc) */
    if (rb_push_from_isr(&g_rb, data)) {
        /* ✅ Step 3: Signal worker — non-blocking post only */
        g_data_ready = 1u;
    }
    /* On overflow: push returns false, overflow_count incremented in rb_push */
}

/* ---- Worker task ---------------------------------------------------------
 *
 * Runs at high RTOS priority, below interrupt level.
 * Wakes when ISR posts g_data_ready, then drains ALL queued items.
 *
 * BATCH DRAIN is essential:
 *   If ISR runs at 100 kHz and each push sets g_data_ready = 1, but the
 *   worker sleeps between each pop, the system has 100,000 context switches/sec.
 *   Batch drain: wake once, drain everything, back to sleep.
 */
static void process(rb_data_t data)
{
    (void)data;
    /* Real workload examples:
     *   - Apply IIR/FIR filter coefficient
     *   - Append to larger output buffer
     *   - Run protocol state machine
     *   - Compute checksum incrementally */
}

void worker_task(void)
{
    rb_init(&g_rb);

    while (1) {
        /* Block until ISR signals data available */
        while (!g_data_ready) { /* RTOS: block on semaphore here */ }
        g_data_ready = 0u;

        /* ✅ BATCH DRAIN: process all pending samples in one pass */
        rb_data_t data;
        while (rb_pop(&g_rb, &data)) {
            process(data);
        }

        /* Health check: log if drops occurred */
        if (g_rb.overflow_count > 0u) {
            /* Real: log to error counter, set fault LED, send telemetry */
        }
    }
}

/* ==========================================================================
 * SIMULATION HARNESS
 * gcc -std=c11 -O2 -DSIMULATION ring_buffer.c isr_demo.c -lpthread -o test_rb
 * ========================================================================== */
#ifdef SIMULATION

#include <pthread.h>
#include <time.h>

#define SIM_TOTAL_INTERRUPTS  100000u   /* 1 second at 100 kHz */

static void *isr_thread(void *arg)
{
    (void)arg;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000 };  /* 10 µs */

    for (uint32_t i = 0u; i < SIM_TOTAL_INTERRUPTS; i++) {
        hardware_isr();
        nanosleep(&ts, NULL);
    }
    return NULL;
}

int main(void)
{
    rb_init(&g_rb);

    printf("Simulation: %u interrupts @ 100 kHz\n", SIM_TOTAL_INTERRUPTS);

    pthread_t tid;
    pthread_create(&tid, NULL, isr_thread, NULL);

    uint32_t total_popped = 0u;
    while (total_popped < SIM_TOTAL_INTERRUPTS) {
        while (!g_data_ready) { /* spin */ }
        g_data_ready = 0u;

        rb_data_t data;
        while (rb_pop(&g_rb, &data)) {
            process(data);
            total_popped++;
        }
    }

    pthread_join(tid, NULL);

    printf("--- Results ---\n");
    printf("Pushed   : %u\n", g_rb.total_pushed);
    printf("Popped   : %u\n", g_rb.total_popped);
    printf("Overflow : %u\n", g_rb.overflow_count);
    printf("Status   : %s\n",
           (g_rb.overflow_count == 0u &&
            g_rb.total_pushed == SIM_TOTAL_INTERRUPTS) ? "PASS" : "FAIL");

    return (int)g_rb.overflow_count;
}

#endif /* SIMULATION */

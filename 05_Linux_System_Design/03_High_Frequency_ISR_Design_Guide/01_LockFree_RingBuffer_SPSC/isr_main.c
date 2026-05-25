#define _POSIX_C_SOURCE 200809L   /* nanosleep, clock_gettime */

/*
 * isr_main.c — ISR integration + worker task for SPSC Ring Buffer
 *
 * Target: ARM Cortex-M (bare-metal or RTOS)
 * Tested: host x86_64 via SIMULATION build (gcc -DSIMULATION -lpthread)
 *
 * Compile for embedded:
 *   arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -O2 -c isr_main.c ring_buffer.c
 *
 * Compile for simulation:
 *   gcc -std=c11 -O2 -DSIMULATION isr_main.c ring_buffer.c -lpthread -o test_rb
 */

#include "ring_buffer.h"
#include <stdint.h>
#include <stdio.h>

/* ---- Global state --------------------------------------------------------- */

/*
 * Place in non-cached SRAM if DMA also writes here (Cortex-M MPU region).
 * On bare metal: add to linker script section .noinit to avoid zero-init.
 */
static ring_buf_t  g_rb;
static rb_stats_t  g_stats;

/* ---- Semaphore abstraction ------------------------------------------------
 *
 * Map to your RTOS:
 *   FreeRTOS:  xSemaphoreHandle + xSemaphoreGiveFromISR / xSemaphoreTake
 *   Zephyr:    k_sem + k_sem_give (ISR-safe) / k_sem_take
 *   CMSIS-RTOS2: osSemaphoreId_t + osSemaphoreRelease / osSemaphoreAcquire
 *   Bare metal: volatile flag + WFE/SEV (ARM event signaling)
 */
typedef volatile uint32_t semaphore_t;
static semaphore_t g_data_ready = 0u;

static inline void semaphore_post_from_isr(semaphore_t *sem)
{
    /*
     * FreeRTOS equivalent:
     *   BaseType_t xHigherPriorityTaskWoken = pdFALSE;
     *   xSemaphoreGiveFromISR(sem_handle, &xHigherPriorityTaskWoken);
     *   portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
     */
    *sem = 1u;
}

static inline void semaphore_wait(semaphore_t *sem)
{
    /*
     * FreeRTOS equivalent: xSemaphoreTake(sem_handle, portMAX_DELAY);
     * Bare metal: __WFE() (wait-for-event, saves power between interrupts)
     */
    while (*sem == 0u) { /* yield in RTOS; WFE on bare metal */ }
    *sem = 0u;
}

/* ---- Hardware data read --------------------------------------------------- */

static inline uint32_t read_hardware_data(void)
{
    /*
     * Replace with actual peripheral register read:
     *   UART:  return USART1->DR;
     *   ADC:   return ADC1->DR;
     *   Timer: return TIM2->CCR1;
     *   SPI:   return SPI1->DR;
     *
     * IMPORTANT: Reading the data register typically clears the interrupt
     * pending flag in hardware. This MUST be the very first operation in
     * the ISR to prevent re-triggering.
     *
     * On ARM: readl() / __IO uint32_t access generates a load instruction
     * that crosses the peripheral bridge, which acts as a barrier.
     */
    return 0xCAFEBABEu; /* placeholder */
}

/* ---- ISR handler ----------------------------------------------------------
 *
 * hardware_isr() — Vector table entry, fires at 100 kHz (every 10 µs)
 *
 * Timing budget on Cortex-M4 @ 168 MHz:
 *   10 µs = 1680 cycles total period
 *   Target: < 5 µs = < 840 cycles (50% rule)
 *
 *   Operation breakdown:
 *     IRQ entry / stacking (M4 hw):  12 cycles  (~71 ns)
 *     read_hardware_data():           3 cycles  (~18 ns)
 *     rb_push_from_isr():            ~25 cycles (~149 ns)
 *       - compute next_head:          1 cycle
 *       - full check:                 2 cycles
 *       - store buf[head]:            2 cycles
 *       - DMB:                       ~8 cycles
 *       - store head:                 2 cycles
 *       - increment counter:          3 cycles
 *     semaphore_post_from_isr():      3 cycles  (~18 ns)
 *     IRQ exit / unstacking:         12 cycles  (~71 ns)
 *   TOTAL: ~57 cycles = ~0.34 µs @ 168 MHz  ✅ (3.4% of period budget)
 *
 * Attributes:
 *   __attribute__((interrupt)) — GCC: save/restore FPU context if used
 *   __attribute__((section(".text.isr"))) — place in ITCM for fast fetch
 */
__attribute__((section(".text.isr")))
void hardware_isr(void)
{
    /* Step 1: Read data register IMMEDIATELY — clears hardware pending flag */
    uint32_t raw_data = read_hardware_data();

    /* Step 2: Push to ring buffer (lock-free, ~25 cycles, never blocks) */
    if (!rb_push_from_isr(&g_rb, &g_stats, raw_data)) {
        /*
         * Buffer full — sample dropped.
         * g_stats.overflow_count already incremented inside rb_push_from_isr.
         * Do NOT log here (printf is off-limits in ISR).
         * Do NOT assert here (could halt system at 100 kHz — catastrophic).
         * Strategy: monitor overflow_count in worker health check.
         */
        return;
    }

    /* Step 3: Wake worker — non-blocking, ISR MUST NOT call blocking APIs */
    semaphore_post_from_isr(&g_data_ready);
}

/* ---- Worker task ----------------------------------------------------------
 *
 * worker_task() — High-priority RTOS task (or main loop on bare metal)
 *
 * Design decision: DRAIN ENTIRE BUFFER per wake cycle.
 *
 * Naive approach (wrong for high-freq):
 *   while(1) { wait(sem); pop_one(); process_one(); }
 *   Problem: at 100 kHz, this means 100,000 semaphore ops/sec.
 *   Each op: ~1-5 µs context switch overhead = up to 50% CPU wasted
 *   on scheduling overhead alone.
 *
 * Correct approach (batch drain):
 *   while(1) { wait(sem); while(!empty) { pop_all(); process_batch(); } }
 *   Wake once, drain N samples. Semaphore overhead amortized over N samples.
 *   At 100 µs worker latency: N ≈ 10 samples per wake → 10× less overhead.
 *
 * This is the same principle as Linux NAPI (network driver polling) and
 * DPDK bulk dequeue.
 */
static void process_data(uint32_t data)
{
    /*
     * Your processing here. Examples:
     *   - Protocol decode (UART frame assembly)
     *   - ADC sample filtering (moving average, FIR)
     *   - Event logging to circular log buffer
     *   - CRC accumulation
     *   - FFT input staging
     */
    (void)data;
}

void worker_task(void)
{
    rb_init(&g_rb, &g_stats);

    while (1) {
        /* Block until ISR signals at least one sample is available */
        semaphore_wait(&g_data_ready);

        /*
         * Drain ALL available entries in one pass.
         * Loop continues even if ISR pushes more entries mid-drain —
         * this is safe and desirable (drain as much as possible before
         * going back to sleep).
         */
        rb_data_t sample;
        while (rb_pop(&g_rb, &g_stats, &sample)) {
            process_data(sample);
        }

        /* Health monitoring — run periodically, not every wake cycle */
        static uint32_t health_tick = 0u;
        if (++health_tick >= 1000u) {
            health_tick = 0u;
            if (g_stats.overflow_count > 0u) {
                /* Log, send to diagnostics, or assert in debug build */
                printf("[WARN] Overflow: %u | Pushed: %u | Popped: %u | Used: %u/%u\n",
                       g_stats.overflow_count,
                       g_stats.total_pushed,
                       g_stats.total_popped,
                       rb_used(&g_rb),
                       RB_SIZE);
            }
        }
    }
}

/* ==========================================================================
 * SIMULATION / TEST HARNESS
 * Build: gcc -std=c11 -O2 -DSIMULATION isr_main.c ring_buffer.c -lpthread -o test
 * ========================================================================== */
#ifdef SIMULATION

#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define SIM_INTERRUPT_COUNT 100000u  /* 1 second @ 100 kHz */

static void *isr_simulator_thread(void *arg)
{
    (void)arg;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000 }; /* 10 µs */

    for (uint32_t i = 0u; i < SIM_INTERRUPT_COUNT; i++) {
        hardware_isr();
        nanosleep(&ts, NULL);
    }
    return NULL;
}

int main(void)
{
    rb_init(&g_rb, &g_stats);

    printf("Starting simulation: %u interrupts @ 100 kHz\n", SIM_INTERRUPT_COUNT);

    pthread_t isr_thread;
    pthread_create(&isr_thread, NULL, isr_simulator_thread, NULL);

    /* Worker: drain until all expected samples processed */
    uint32_t processed = 0u;
    rb_data_t sample;

    while (processed < SIM_INTERRUPT_COUNT) {
        /* Poll in simulation (replace with semaphore_wait in production) */
        while (rb_pop(&g_rb, &g_stats, &sample)) {
            process_data(sample);
            processed++;
        }
    }

    pthread_join(isr_thread, NULL);

    printf("--- Results ---\n");
    printf("Total pushed:    %u\n", g_stats.total_pushed);
    printf("Total popped:    %u\n", g_stats.total_popped);
    printf("Overflow count:  %u\n", g_stats.overflow_count);
    printf("Final used:      %u\n", rb_used(&g_rb));
    printf("Status: %s\n",
           (g_stats.overflow_count == 0 && g_stats.total_pushed == g_stats.total_popped)
           ? "PASS" : "FAIL");

    return (int)g_stats.overflow_count;
}
#endif /* SIMULATION */

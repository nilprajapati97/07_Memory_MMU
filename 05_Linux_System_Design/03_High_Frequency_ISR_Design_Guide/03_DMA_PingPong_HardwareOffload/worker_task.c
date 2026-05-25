#define _POSIX_C_SOURCE 200809L   /* nanosleep */

/*
 * worker_task.c — DMA Ping-Pong Worker + Simulated IRQ Vectors
 *
 * The worker receives pointers to DMA buffer halves via a message queue
 * populated by the IRQ callback. It processes each half independently.
 *
 * Key: worker NEVER touches the other half (DMA is writing to it).
 * True hardware parallelism: DMA fills ↔ CPU processes simultaneously.
 */

#include "dma_config.h"
#include <stdio.h>
#include <stddef.h>

/* ---- Global state --------------------------------------------------------- */

static dma_ctrl_t       g_dma_ctrl;
static dma_circular_buf_t g_dma_buf __attribute__((aligned(32)));

/* ---- Message queue (IRQ → Worker) ----------------------------------------
 *
 * The DMA callback runs from IRQ context. It must NOT block.
 * It stores the buffer pointer + count in a simple message slot.
 * Worker polls (or waits on semaphore) for a pending message.
 *
 * For RTOS: replace with xQueueSendFromISR() / xQueueReceive()
 *           or k_msgq_put() / k_msgq_get()
 */
typedef struct {
    const dma_sample_t  *buf;
    uint32_t             count;
    volatile uint8_t     pending;
} dma_msg_t;

static volatile dma_msg_t g_msg;

/* Semaphore abstraction */
typedef volatile uint32_t sem_t_s;
static sem_t_s g_data_sem = 0u;
static void sem_post_isr(sem_t_s *s) { *s = 1u; }
static void sem_wait_w(sem_t_s *s)   { while (!*s); *s = 0u; }

/* ---- DMA callback (called from IRQ context) ------------------------------ */

static void dma_callback(const dma_sample_t *half_buf, uint32_t count)
{
    /*
     * IRQ context: must NOT block, allocate memory, or call RTOS blocking APIs.
     *
     * Store the pointer — zero copy. Worker will read directly from DMA buffer.
     * This is safe because:
     *   - HTC callback: DMA is writing to buf[HALF_SIZE..SIZE-1], not buf[0..HALF_SIZE-1]
     *   - TC  callback: DMA is writing to buf[0..HALF_SIZE-1], not buf[HALF_SIZE..SIZE-1]
     * The pointer we're handing to worker points to the STABLE half.
     */
    g_msg.buf     = half_buf;
    g_msg.count   = count;

    /* Compiler + hardware barrier: ensure buf/count written before pending */
    __asm__ __volatile__("" ::: "memory");
    g_msg.pending = 1u;

    sem_post_isr(&g_data_sem);
}

/* ---- Sample processing ---------------------------------------------------- */

/*
 * process_half() — called from worker context with a stable DMA half-buffer
 *
 * This function can use:
 *   - Floating point (FPU)
 *   - Memory allocation
 *   - Logging / printf
 *   - RTOS APIs (semaphore, queue, event flags)
 *   - DSP libraries (CMSIS-DSP arm_fir_f32, arm_rfft_fast_f32)
 *
 * Must complete within DMA_HALF_SIZE / interrupt_rate = 256 / 100,000 = 2.56 ms
 * to avoid overflow (worker_busy flag still set when next IRQ fires).
 */
static void process_half(const dma_sample_t *buf, uint32_t count)
{
    /*
     * Examples of what you'd do here:
     *
     * 1. ADC signal processing (CMSIS-DSP):
     *    arm_fir_f32(&fir_state, (float32_t*)buf, output, count);
     *
     * 2. 256-point FFT:
     *    arm_rfft_fast_f32(&fft_inst, (float32_t*)buf, fft_output, 0);
     *
     * 3. Protocol decode (SPI/I2S frame):
     *    for (uint32_t i = 0; i < count; i++) {
     *        uint8_t  channel = (buf[i] >> 24) & 0x0F;
     *        uint32_t value   = buf[i] & 0x00FFFFFF;
     *        accumulate(channel, value);
     *    }
     *
     * 4. CRC computation:
     *    crc = crc32_update(crc, buf, count * sizeof(dma_sample_t));
     *
     * 5. Ring buffer relay (DMA → higher-level ring buffer):
     *    for (uint32_t i = 0; i < count; i++)
     *        rb_push_from_isr(&g_output_rb, &stats, buf[i]);
     */
    (void)buf;
    (void)count;
}

/* ---- Worker task ---------------------------------------------------------- */

void worker_task(void)
{
    dma_setup(&g_dma_ctrl, &g_dma_buf, dma_callback);
    dma_start(&g_dma_ctrl);

    printf("DMA ping-pong started. Rate: 100 kHz, Half-size: %u samples\n",
           DMA_HALF_SIZE);
    printf("Worker wake rate: ~%u IRQ/sec\n", 100000u / DMA_HALF_SIZE);

    while (1) {
        /* Wait for DMA half-transfer signal */
        sem_wait_w(&g_data_sem);

        if (g_msg.pending) {
            /* Cache the message locally (IRQ could update it again) */
            const dma_sample_t *buf   = g_msg.buf;
            uint32_t            count = g_msg.count;

            /* Acquire barrier: ensure we see DMA-written data */
            __asm__ __volatile__("" ::: "memory");
            g_msg.pending = 0u;

            /* Process the stable half — DMA is writing to the OTHER half */
            process_half(buf, count);

            /* Signal DMA controller that we're done with this half */
            dma_worker_done(&g_dma_ctrl);
        }

        /* Health check */
        if (g_dma_ctrl.overflow_count > 0u) {
            printf("[ERROR] DMA overflow! Count: %u. Worker must be faster.\n",
                   g_dma_ctrl.overflow_count);
            printf("        htc_count=%u, tc_count=%u\n",
                   g_dma_ctrl.htc_count, g_dma_ctrl.tc_count);
        }
        if (g_dma_ctrl.state == DMA_STATE_ERROR) {
            printf("[ERROR] DMA transfer error! Count: %u\n", g_dma_ctrl.error_count);
            /* Recovery: restart DMA */
            dma_stop(&g_dma_ctrl);
            dma_start(&g_dma_ctrl);
        }
    }
}

/* ---- Simulated IRQ vector table entries ----------------------------------- */

/*
 * In the real system, these are placed in the vector table:
 *   .word DMA1_Stream0_IRQHandler   ; in startup_stm32h7xx.s
 *
 * The NVIC fires these automatically when DMA flags are set.
 * Here we simulate them being called from a timer thread.
 */
void DMA1_Stream0_IRQHandler(void)
{
    /*
     * Real implementation checks DMA status flags:
     *
     * if (__HAL_DMA_GET_FLAG(&hdma, DMA_FLAG_HTIF0_4)) {
     *     __HAL_DMA_CLEAR_FLAG(&hdma, DMA_FLAG_HTIF0_4);
     *     dma_half_transfer_irq(&g_dma_ctrl, &g_dma_buf);
     * }
     * if (__HAL_DMA_GET_FLAG(&hdma, DMA_FLAG_TCIF0_4)) {
     *     __HAL_DMA_CLEAR_FLAG(&hdma, DMA_FLAG_TCIF0_4);
     *     dma_full_transfer_irq(&g_dma_ctrl, &g_dma_buf);
     * }
     * if (__HAL_DMA_GET_FLAG(&hdma, DMA_FLAG_TEIF0_4)) {
     *     __HAL_DMA_CLEAR_FLAG(&hdma, DMA_FLAG_TEIF0_4);
     *     dma_error_irq(&g_dma_ctrl);
     * }
     */
    dma_half_transfer_irq(&g_dma_ctrl, &g_dma_buf);
}

/* ==========================================================================
 * SIMULATION
 * gcc -std=c11 -O2 -DSIMULATION worker_task.c dma_isr.c -lpthread -o test_dma
 * ========================================================================== */
#ifdef SIMULATION

#include <pthread.h>
#include <time.h>
#include <string.h>

#define SIM_SAMPLES  102400u  /* ~1 second @ 100 kHz */

static uint32_t g_sim_sample = 0u;

/* Simulate DMA filling the buffer and firing IRQs */
static void *dma_sim_thread(void *arg)
{
    (void)arg;
    /* 256 samples per half × 10 µs per sample = 2.56 ms per half */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 2560000 }; /* 2.56 ms */

    uint32_t halves = SIM_SAMPLES / DMA_HALF_SIZE;

    for (uint32_t h = 0u; h < halves; h++) {
        /* Simulate DMA filling the current half */
        uint32_t half_idx = h & 1u;
        for (uint32_t i = 0u; i < DMA_HALF_SIZE; i++) {
            g_dma_buf.samples[half_idx * DMA_HALF_SIZE + i] = g_sim_sample++;
        }

        /* Fire the appropriate IRQ */
        if (half_idx == 0u) {
            dma_half_transfer_irq(&g_dma_ctrl, &g_dma_buf);
        } else {
            dma_full_transfer_irq(&g_dma_ctrl, &g_dma_buf);
        }

        nanosleep(&ts, NULL);
    }
    return NULL;
}

int main(void)
{
    dma_setup(&g_dma_ctrl, &g_dma_buf, dma_callback);
    dma_start(&g_dma_ctrl);

    uint32_t total_processed = 0u;
    uint32_t expected_halves = SIM_SAMPLES / DMA_HALF_SIZE;

    pthread_t tid;
    pthread_create(&tid, NULL, dma_sim_thread, NULL);

    while (total_processed < expected_halves) {
        uint32_t p = g_msg.pending;
        if (p) {
            const dma_sample_t *b = g_msg.buf;
            uint32_t c = g_msg.count;
            __asm__ __volatile__("" ::: "memory");
            g_msg.pending = 0u;
            process_half(b, c);
            dma_worker_done(&g_dma_ctrl);
            total_processed++;
        }
    }

    pthread_join(tid, NULL);

    printf("--- DMA Ping-Pong Simulation Results ---\n");
    printf("Total halves processed: %u / %u\n", total_processed, expected_halves);
    printf("HTC count:              %u\n", g_dma_ctrl.htc_count);
    printf("TC  count:              %u\n", g_dma_ctrl.tc_count);
    printf("Overflow count:         %u\n", g_dma_ctrl.overflow_count);
    printf("Error count:            %u\n", g_dma_ctrl.error_count);
    printf("Status: %s\n", g_dma_ctrl.overflow_count == 0u ? "PASS" : "FAIL");

    return (int)g_dma_ctrl.overflow_count;
}
#endif /* SIMULATION */

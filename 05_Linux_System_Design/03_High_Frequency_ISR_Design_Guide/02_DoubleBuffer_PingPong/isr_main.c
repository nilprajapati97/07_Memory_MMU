#define _POSIX_C_SOURCE 200809L   /* nanosleep */

/*
 * isr_main.c — Double Buffer ISR integration and worker task
 *
 * Key design insight:
 *   ISR overhead per sample = ~5 cycles (fast path, 99.8% of calls)
 *   ISR overhead on swap    = ~30 cycles (slow path, once per 512 samples)
 *   Worker is NEVER woken for individual samples — only for full buffers.
 *
 *   Effective worker wake rate: 100,000 / 512 ≈ 195 wakes/sec (vs 100,000/sec)
 *   → 512× fewer context switches than naive per-sample approach
 *
 * Compile for simulation:
 *   gcc -std=c11 -O2 -DSIMULATION isr_main.c double_buffer.c -lpthread -o test_db
 */

#include "double_buffer.h"
#include <stdint.h>
#include <stdio.h>

/* ---- Global state --------------------------------------------------------- */

static double_buf_t g_db;

/* ---- Semaphore (same RTOS abstraction as approach 01) -------------------- */

typedef volatile uint32_t semaphore_t;
static semaphore_t g_buffer_ready = 0u;

static inline void sem_post_isr(semaphore_t *s) { *s = 1u; }
static inline void sem_wait(semaphore_t *s)     { while (!*s); *s = 0u; }

/* ---- Hardware data read --------------------------------------------------- */

static inline uint32_t read_hw_register(void)
{
    /* Platform: return PERIPH->DR; */
    return 0xABCDABCDu;
}

/* ---- ISR handler ----------------------------------------------------------
 *
 * hardware_isr() — fires 100,000 times/sec
 *
 * Fast path execution: ~30 ns @ 168 MHz (< 5 cycles + peripheral read)
 *
 * The ISR is NOT waking the worker every call — only when a full buffer
 * is complete (every 512 samples = every 5.12 ms).
 *
 * This is analogous to DMA half-transfer interrupts (approach 03) but
 * implemented entirely in software — no DMA hardware required.
 */
void hardware_isr(void)
{
    uint32_t data = read_hw_register();

    bool buffer_full = db_push_from_isr(&g_db, data);

    if (buffer_full) {
        /*
         * A complete buffer of DB_HALF_SIZE samples is ready.
         * Wake worker to process it. This happens ~195 times/sec.
         * The semaphore is non-blocking (ISR rule: never block).
         */
        sem_post_isr(&g_buffer_ready);
    }
}

/* ---- Worker task ----------------------------------------------------------
 *
 * Processes one complete buffer (DB_HALF_SIZE samples) per wake.
 * Runs in RTOS task context — can use floating point, logging, etc.
 *
 * Worker latency budget:
 *   Worker must complete processing BEFORE the next buffer fills.
 *   Next buffer fill time = DB_HALF_SIZE / interrupt_rate
 *                         = 512 / 100,000 = 5.12 ms
 *   Worker MUST finish process_buffer() in < 5.12 ms.
 *   (Contrast with ring buffer approach: per-sample processing budget
 *   is unbounded as long as average rate keeps up.)
 */
static void process_buffer(const db_data_t *buf, uint32_t count)
{
    /*
     * Process all count samples from buf.
     * Examples:
     *   - FIR filter over 512 samples (CMSIS DSP: arm_fir_f32)
     *   - 512-point FFT
     *   - Protocol frame decode
     *   - Bulk memcpy to larger output buffer
     *   - CRC32 over the block
     *
     * Key: the worker never needs to acquire a lock here.
     * The buffer is "owned" exclusively by the worker until db_release().
     */
    (void)buf;
    (void)count;
}

void worker_task(void)
{
    db_init(&g_db);

    while (1) {
        /* Block until ISR signals a complete buffer */
        sem_wait(&g_buffer_ready);

        uint32_t         count = 0u;
        const db_data_t *buf   = db_get_ready(&g_db, &count);

        if (buf != NULL && count > 0u) {
            process_buffer(buf, count);

            /*
             * MUST call db_release() after processing.
             * This allows ISR to reuse this buffer slot.
             * Forgetting this causes permanent overflow (ISR sees ready=1 forever).
             */
            db_release(&g_db);
        }

        /* Health check */
        if (g_db.overflow_cnt > 0u) {
            printf("[WARN] Double buffer overflow: %u buffers dropped\n",
                   g_db.overflow_cnt);
        }
    }
}

/* ==========================================================================
 * SIMULATION
 * gcc -std=c11 -O2 -DSIMULATION isr_main.c double_buffer.c -lpthread -o test_db
 * ========================================================================== */
#ifdef SIMULATION

#include <pthread.h>
#include <unistd.h>
#include <time.h>

/*
 * Use a multiple of DB_HALF_SIZE so every sample lands in a complete buffer.
 * 196 × 512 = 100,352 — closest multiple of 512 to 100,000.
 */
#define SIM_TOTAL_SAMPLES  (196u * DB_HALF_SIZE)   /* 100,352 at 100 kHz */

static volatile int g_isr_done = 0;

static void *isr_thread(void *arg)
{
    (void)arg;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000 }; /* 10 µs */

    for (uint32_t i = 0u; i < SIM_TOTAL_SAMPLES; i++) {
        hardware_isr();
        nanosleep(&ts, NULL);
    }
    g_isr_done = 1;
    return NULL;
}

int main(void)
{
    db_init(&g_db);

    uint32_t total_processed = 0u;

    pthread_t tid;
    pthread_create(&tid, NULL, isr_thread, NULL);

    while (total_processed < SIM_TOTAL_SAMPLES) {
        uint32_t         count = 0u;
        const db_data_t *buf   = db_get_ready(&g_db, &count);
        if (buf && count > 0u) {
            process_buffer(buf, count);
            db_release(&g_db);
            total_processed += count;
        } else if (g_isr_done) {
            /* ISR finished but last partial buffer won't fill — stop here */
            break;
        }
    }

    pthread_join(tid, NULL);

    printf("--- Double Buffer Simulation Results ---\n");
    printf("Total samples processed : %u\n", total_processed);
    printf("Total buffer handoffs   : %u\n", g_db.total_handoffs);
    printf("Overflow (dropped bufs) : %u\n", g_db.overflow_cnt);
    printf("Expected handoffs       : %u\n", SIM_TOTAL_SAMPLES / DB_HALF_SIZE);
    printf("Status: %s\n", g_db.overflow_cnt == 0u ? "PASS" : "FAIL");

    return (int)g_db.overflow_cnt;
}

#endif /* SIMULATION */

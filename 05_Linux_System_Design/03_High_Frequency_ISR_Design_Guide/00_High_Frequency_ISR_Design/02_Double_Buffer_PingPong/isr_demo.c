#define _POSIX_C_SOURCE 200809L   /* nanosleep */

/*
 * isr_demo.c — ISR + Worker Task for Ping-Pong Double Buffer
 *
 * Key difference from ring buffer approach:
 *   Ring buffer:    worker wakes for EACH sample (up to 100,000 wakes/sec)
 *   Double buffer:  worker wakes for EACH COMPLETE BLOCK (390 wakes/sec at
 *                   DB_BUF_LEN=256 and 100 kHz rate)
 *
 *   This is the software equivalent of DMA half/full-transfer interrupts.
 *
 * Worker deadline: process DB_BUF_LEN samples BEFORE the next buffer fills.
 *   Deadline = DB_BUF_LEN / sample_rate = 256 / 100,000 = 2.56 ms
 *
 * Compile:
 *   gcc -std=c11 -O2 -DSIMULATION double_buffer.c isr_demo.c -lpthread -o test_db
 */

#include "double_buffer.h"
#include <stdint.h>
#include <stdio.h>

/* ---- Global state -------------------------------------------------------- */

static double_buf_t g_db;

/* ---- Simulated hardware register ----------------------------------------- */

static inline db_data_t read_hw(void)
{
    static uint32_t c = 0u;
    return c++;
}

/* ---- ISR handler ---------------------------------------------------------
 *
 * Fast path:  ~3 cycles (array store + increment + compare)
 * Swap path:  ~15 cycles (once every DB_BUF_LEN = 256 samples)
 *
 * ISR spends 99.6% of calls on the fast path.
 */
void hardware_isr(void)
{
    db_push_from_isr(&g_db, read_hw());
    /* Note: we do NOT check the return value here for simplicity.
     * In production: if db_push returns true → post semaphore to wake worker. */
}

/* ---- Worker task ---------------------------------------------------------
 *
 * Processes one complete block (DB_BUF_LEN samples) per wake.
 * Deadline: must complete process_block() in < 2.56 ms at 100 kHz.
 */
static void process_block(const db_data_t *buf, uint32_t count)
{
    (void)buf;
    (void)count;
    /* Real workload: arm_fir_f32(), 256-point FFT, CRC-32, bulk memcpy */
}

void worker_task(void)
{
    db_init(&g_db);

    while (1) {
        /* Poll for ready buffer — RTOS: block on semaphore or event flag */
        uint32_t         count = 0u;
        const db_data_t *buf   = db_get_ready(&g_db, &count);

        if (buf != NULL && count > 0u) {
            process_block(buf, count);

            /* MUST call db_release() — allows ISR to reuse this buffer.
             * Forgetting this: ISR sees ready=1 forever → every new buffer
             * overflows (overflow_cnt increments every DB_BUF_LEN samples). */
            db_release(&g_db);
        }
    }
}

/* ==========================================================================
 * SIMULATION HARNESS
 * gcc -std=c11 -O2 -DSIMULATION double_buffer.c isr_demo.c -lpthread -o test_db
 * ========================================================================== */
#ifdef SIMULATION

#include <pthread.h>
#include <time.h>

/* Must be a multiple of DB_BUF_LEN — last partial buffer never triggers ready */
#define SIM_TOTAL  (390u * DB_BUF_LEN)     /* 390 complete buffers ~= 100k samples */

static volatile int g_isr_done = 0;

static void *isr_thread(void *arg)
{
    (void)arg;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000 };  /* 10 µs */

    for (uint32_t i = 0u; i < SIM_TOTAL; i++) {
        hardware_isr();
        nanosleep(&ts, NULL);
    }
    g_isr_done = 1;
    return NULL;
}

int main(void)
{
    db_init(&g_db);

    pthread_t tid;
    pthread_create(&tid, NULL, isr_thread, NULL);

    uint32_t total = 0u;
    while (total < SIM_TOTAL) {
        uint32_t         count = 0u;
        const db_data_t *buf   = db_get_ready(&g_db, &count);

        if (buf && count > 0u) {
            process_block(buf, count);
            db_release(&g_db);
            total += count;
        } else if (g_isr_done) {
            break;  /* ISR done and no more ready buffers */
        }
    }

    pthread_join(tid, NULL);

    printf("--- Double Buffer Simulation ---\n");
    printf("Total processed : %u\n", total);
    printf("Buffer handoffs : %u\n", g_db.total_handoffs);
    printf("Overflow        : %u\n", g_db.overflow_cnt);
    printf("Expected handoffs: %u\n", SIM_TOTAL / DB_BUF_LEN);
    printf("Status: %s\n", g_db.overflow_cnt == 0u ? "PASS" : "FAIL");

    return (int)g_db.overflow_cnt;
}

#endif /* SIMULATION */

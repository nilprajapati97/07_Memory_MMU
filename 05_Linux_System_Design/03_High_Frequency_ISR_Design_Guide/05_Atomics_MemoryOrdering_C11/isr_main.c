#define _POSIX_C_SOURCE 200809L   /* nanosleep */

/*
 * isr_main.c — C11 Atomic Queue ISR integration and worker
 *
 * Compile (host, simulation):
 *   gcc -std=c11 -O2 -DSIMULATION isr_main.c atomic_queue.c -lpthread -o test_aq
 *
 * Compile (ARM bare metal):
 *   arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -std=c11 -O2 \
 *     -c isr_main.c atomic_queue.c
 *
 * Key advantage over approach 01 for multi-core:
 *   On ARMv8-A (Cortex-A55/A78), GCC compiles:
 *     memory_order_release store → STLR (1 instruction, no DMB)
 *     memory_order_acquire load  → LDAR (1 instruction, no DMB)
 *   vs approach 01's volatile + explicit DMB:
 *     STR + DMB SY (2 instructions, stronger barrier)
 *
 *   STLR/LDAR are 20-30% faster than STR+DMB on Cortex-A because:
 *   - DMB is a full bidirectional barrier (prevents all reorderings)
 *   - STLR/LDAR are one-directional (release/acquire) — weaker = faster
 *   - CPU can optimize more aggressively with weaker guarantees
 */

#include "atomic_queue.h"
#include <stdint.h>
#include <stdio.h>

/* ---- Global state --------------------------------------------------------- */

static atomic_queue_t g_aq;
static aq_stats_t     g_stats;

/* ---- Semaphore (C11 atomic-based flag) ------------------------------------ */

static _Atomic uint32_t g_sem = 0u;

static inline void sem_post_isr(void)
{
    /*
     * Release store: ensures all prior ISR operations (aq_push) are visible
     * to the worker BEFORE it wakes up and starts reading.
     */
    atomic_store_explicit(&g_sem, 1u, memory_order_release);
}

static inline void sem_wait_worker(void)
{
    /*
     * Acquire load: ensures worker sees all ISR operations that preceded
     * the release store above.
     */
    uint32_t v;
    do {
        v = atomic_load_explicit(&g_sem, memory_order_acquire);
    } while (v == 0u);
    atomic_store_explicit(&g_sem, 0u, memory_order_relaxed);
}

/* ---- Hardware data -------------------------------------------------------- */

static inline uint32_t read_hw_data(void)
{
    return 0xFEEDF00Du; /* Replace: peripheral->data_register */
}

/* ---- ISR handler ---------------------------------------------------------- */

/*
 * hardware_isr() — fires 100,000 times/sec
 *
 * C11 atomic version: no platform-specific intrinsics.
 * Compiles correctly on ARM, x86, RISC-V, MIPS.
 *
 * The only architecture-specific behavior is what instructions the
 * compiler emits for atomic_store_explicit(..., memory_order_release).
 * GCC/Clang choose the optimal instruction for each target automatically.
 *
 * On ARMv8-A: generates STLR (release store, no separate DMB)
 * On ARMv7-M: generates STR + DMB ISH
 * On x86:     generates MOV (TSO model, no fence needed)
 */
void hardware_isr(void)
{
    uint32_t data = read_hw_data();

    if (!aq_push_from_isr(&g_aq, &g_stats, data)) {
        /* Overflow: counted inside aq_push_from_isr */
        return;
    }

    sem_post_isr();
}

/* ---- Worker task ---------------------------------------------------------- */

static void process(uint32_t data)
{
    (void)data;
}

void worker_task(void)
{
    aq_init(&g_aq, &g_stats);

    while (1) {
        sem_wait_worker();

        aq_data_t sample;
        while (aq_pop(&g_aq, &g_stats, &sample)) {
            process(sample);
        }

        if (atomic_load_explicit(&g_stats.overflow_count, memory_order_relaxed) > 0u) {
            printf("[WARN] Overflow: %u\n",
                   atomic_load_explicit(&g_stats.overflow_count, memory_order_relaxed));
        }
    }
}

/* ==========================================================================
 * SIMULATION
 * gcc -std=c11 -O2 -DSIMULATION isr_main.c atomic_queue.c -lpthread -o test_aq
 * ========================================================================== */
#ifdef SIMULATION

#include <pthread.h>
#include <time.h>

#define SIM_COUNT 100000u

static void *isr_sim(void *arg)
{
    (void)arg;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000 };
    for (uint32_t i = 0u; i < SIM_COUNT; i++) {
        hardware_isr();
        nanosleep(&ts, NULL);
    }
    return NULL;
}

int main(void)
{
    aq_init(&g_aq, &g_stats);

    pthread_t tid;
    pthread_create(&tid, NULL, isr_sim, NULL);

    uint32_t processed = 0u;
    aq_data_t sample;

    while (processed < SIM_COUNT) {
        while (aq_pop(&g_aq, &g_stats, &sample)) {
            process(sample);
            processed++;
        }
    }

    pthread_join(tid, NULL);

    uint32_t pushed    = atomic_load_explicit(&g_stats.total_pushed,   memory_order_relaxed);
    uint32_t popped    = atomic_load_explicit(&g_stats.total_popped,   memory_order_relaxed);
    uint32_t overflows = atomic_load_explicit(&g_stats.overflow_count, memory_order_relaxed);

    printf("--- C11 Atomic Queue Results ---\n");
    printf("Pushed:   %u\n", pushed);
    printf("Popped:   %u\n", popped);
    printf("Overflow: %u\n", overflows);
    printf("Status: %s\n", overflows == 0u ? "PASS" : "FAIL");

    return (int)overflows;
}
#endif /* SIMULATION */

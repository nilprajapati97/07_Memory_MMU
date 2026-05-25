/*
 * isr_antipatterns.c — Common ISR Mistakes and Why They Break
 *
 * !! EDUCATIONAL REFERENCE ONLY — DO NOT USE IN PRODUCTION !!
 *
 * Each pattern shows:
 *   1. What the broken code looks like
 *   2. WHY it breaks (root cause)
 *   3. What the consequence is at runtime
 *   4. The correct fix
 *
 * Guide §4 MUST NOT list: no printf, no malloc, no blocking,
 * no non-reentrant functions, no mutex, no floating point (without FPU save).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * ANTI-PATTERN 1: printf() inside ISR
 * ============================================================ */

void UART_ISR_bad_printf(void)
{
    uint8_t byte = *((volatile uint8_t *)0x40011004u);

    /* ❌ WRONG: printf() is NOT safe in an ISR.
     *
     * Why it breaks:
     *   printf() internally calls malloc() and acquires flockfile() (a mutex).
     *   If the task was inside printf() when this ISR fired, the lock is
     *   already held.  This ISR tries to acquire the same lock → DEADLOCK.
     *
     *   Even without locking: printf() on UART is polling/blocking — it spins
     *   until the TX FIFO is empty.  At 9600 baud, one character = 1 ms.
     *   Your 10 µs ISR budget is exceeded 100×.
     *
     * Consequence: System hangs. Interrupt latency blows up from nanoseconds
     *              to milliseconds. All lower-priority tasks starve.
     *              Stack overflow from printf's internal 512-byte buffer.
     *
     * Fix: Never I/O in ISR. Log to a ring buffer; worker prints at leisure. */
    printf("Byte: 0x%02X\n", byte);  /* ❌ DEADLOCK / STACK OVERFLOW */
}

/* ============================================================
 * ANTI-PATTERN 2: malloc() / free() inside ISR
 * ============================================================ */

typedef struct { uint32_t data; uint32_t timestamp; } event_t;

void Timer_ISR_bad_malloc(void)
{
    /* ❌ WRONG: malloc() acquires the heap lock (a mutex or critical section).
     *
     * Why it breaks:
     *   The heap allocator maintains a free-list protected by a lock.
     *   If the task was in malloc() when this ISR fired, the heap lock
     *   is held.  This ISR calls malloc() → tries to acquire the same lock
     *   → DEADLOCK (on some RTOSes) or heap corruption (on bare-metal).
     *
     *   Additional problem: malloc() may call sbrk() → system call → slow.
     *   Heap fragmentation from repeated small ISR allocations.
     *
     * Fix: Pre-allocate a static pool of event_t at startup.
     *      Use a lock-free free-list or a ring buffer of fixed-size structs.
     *      Never allocate dynamic memory at interrupt time. */
    event_t *ev = (event_t *)malloc(sizeof(event_t));  /* ❌ DEADLOCK RISK */
    if (ev) {
        ev->data = 42u;
        /* do something */
        free(ev);  /* ❌ free() has the same lock problem */
    }
}

/* ============================================================
 * ANTI-PATTERN 3: Blocking delay inside ISR
 * ============================================================ */

void ADC_ISR_bad_blocking(void)
{
    /* ❌ WRONG: Any spin-loop or sleep inside an ISR wastes CPU in interrupt
     *          context, blocking ALL lower-priority interrupts and tasks.
     *
     * Why it breaks:
     *   On ARM Cortex-M, ISRs run with PRIMASK clear (or at an elevated
     *   BASEPRI level).  A spin loop keeps the CPU inside the ISR handler,
     *   preventing the RTOS scheduler, SysTick, and all lower-priority
     *   IRQs from running.
     *
     * Consequence at 100 kHz: Even a 1 µs spin = 10% CPU stolen from tasks.
     *   A 100-cycle spin = 595 ns @ 168 MHz per interrupt = 5.95% overhead.
     *   RTOS tick jitter increases.  Real-time deadlines are missed.
     *
     * Fix: Never delay in an ISR.  If a peripheral needs settling time
     *      (e.g., ADC input mux switch), configure it BEFORE the ISR fires,
     *      or use a timer peripheral to sequence the operations. */
    for (volatile int i = 0; i < 1000; i++) { /* ❌ blocking spin delay */ }
    uint32_t result = *((volatile uint32_t *)0x40012040u);
    (void)result;
}

/* ============================================================
 * ANTI-PATTERN 4: RTOS mutex lock inside ISR
 * ============================================================ */

typedef struct { volatile int locked; } fake_mutex_t;
static fake_mutex_t g_mutex;
static uint32_t g_shared;

static void fake_mutex_lock(fake_mutex_t *m)   { while (m->locked); m->locked = 1; }
static void fake_mutex_unlock(fake_mutex_t *m) { m->locked = 0; }

void SPI_ISR_bad_mutex(void)
{
    /* ❌ WRONG: Acquiring a mutex in an ISR causes priority inversion deadlock.
     *
     * Why it breaks:
     *   Scenario:
     *     1. Low-priority task acquires g_mutex and starts processing.
     *     2. SPI IRQ fires (higher priority) → preempts the task.
     *     3. This ISR calls fake_mutex_lock() → spins forever.
     *        The task that holds the mutex can NEVER run (it was preempted).
     *        → DEADLOCK.
     *
     *   Real RTOS mutexes with priority inheritance avoid the deadlock but
     *   they STILL BLOCK.  Blocking in an ISR is always wrong.
     *
     * Fix: Use a lock-free ring buffer.  The ISR never touches the mutex.
     *      RTOS provides _FromISR variants: xQueueSendFromISR(),
     *      xSemaphoreGiveFromISR() — these are NON-BLOCKING. */
    fake_mutex_lock(&g_mutex);      /* ❌ BLOCKS — causes deadlock */
    g_shared = 0xDEADBEEFu;
    fake_mutex_unlock(&g_mutex);
}

/* ============================================================
 * ANTI-PATTERN 5: Floating-point without FPU context save
 * ============================================================ */

static volatile float g_iir_output = 0.0f;
static float          g_alpha      = 0.1f;

void ADC_ISR_bad_float(void)
{
    uint32_t raw = *((volatile uint32_t *)0x40012040u);

    /* ❌ WRONG: FPU registers (S0–S31) are NOT automatically saved on
     *          ARM Cortex-M4/M7 exception entry (by default).
     *
     * Why it breaks:
     *   The Cortex-M FPU uses lazy context save (FPCCR.LSPEN=1 by default).
     *   If the task was using the FPU when the ISR fires, the FPU state is
     *   NOT saved until the ISR actually uses the FPU.  When that happens,
     *   the hardware saves S0–S15 but the timing is implementation-dependent.
     *   Certain nested IRQ sequences can leave S registers in an undefined state.
     *
     *   Worse: some toolchains with -mfloat-abi=softfp may not save FP regs
     *   at all in the interrupt prologue.  The task resumes with corrupted
     *   S0–S15 → silent floating-point data corruption in task's computation.
     *
     * Fix options (in order of preference):
     *   1. Do FP math in the worker task — NOT in the ISR.       (BEST)
     *   2. Ensure FPCCR.ASPEN=1 + FPCCR.LSPEN=1 (usually default).
     *   3. Manually push/pop S0–S15 in ISR prologue/epilogue.
     *   4. Use integer fixed-point arithmetic in the ISR. */
    g_iir_output = g_alpha * (float)raw + (1.0f - g_alpha) * g_iir_output; /* ❌ */
}

/* ============================================================
 * ANTI-PATTERN 6: Non-reentrant library function in ISR
 * ============================================================ */

static char log_buf[64] = "event,123,456";

void DMA_ISR_bad_strtok(void)
{
    /* ❌ WRONG: strtok() uses a hidden static pointer (internal state).
     *
     * Why it breaks:
     *   strtok() stores the "current position" in a static variable.
     *   If the task was in the middle of strtok() when this ISR fires,
     *   and this ISR also calls strtok(), both share the same static state.
     *   The ISR overwrites the task's parser position → both parsers produce
     *   garbage results.
     *
     * Other non-reentrant functions: strerror(), gmtime(), asctime(),
     *   rand(), getenv(), ctime() — all use internal static storage.
     *
     * Fix: Use reentrant variants: strtok_r() (POSIX), strtok_s() (C11 Annex K).
     *      Or better: never do string parsing in an ISR. */
    char *token = strtok(log_buf, ",");  /* ❌ corrupts task's strtok state */
    (void)token;
}

/* ============================================================
 * ANTI-PATTERN 7: Missing volatile + memory barrier on shared flag
 * ============================================================ */

static uint32_t data_buf[16];
static uint32_t write_ptr  = 0u;  /* ❌ NOT volatile */
static uint32_t flag_ready = 0u;  /* ❌ NOT volatile */

void ISR_bad_no_volatile_no_barrier(void)
{
    data_buf[write_ptr] = 0xCAFEu;

    /* ❌ WRONG: Two problems:
     *
     * Problem A — No volatile:
     *   The compiler may cache write_ptr and flag_ready in CPU registers.
     *   The worker task reads stale values from registers, not memory.
     *   With optimisation -O2: the compiler may eliminate the read entirely,
     *   seeing that "flag_ready is never written in the worker loop body."
     *
     * Problem B — No memory barrier:
     *   On ARM (weakly ordered memory model), the CPU may reorder:
     *     STR r0, [data_buf]     ← store to data buffer
     *     STR r1, [flag_ready]   ← store to flag
     *   The CPU may execute the flag store BEFORE the data store is visible
     *   to other observers (another core, DMA, or write buffer drain).
     *   The worker sees flag_ready=1 but data_buf still has old data.
     *
     * Fix: declare flag_ready as volatile AND add __DMB() before the write.
     *      See isr_correct.c → UART1_ISR_correct() for the correct pattern. */
    flag_ready = 1u;  /* ❌ may be visible before data_buf write on ARM */
}

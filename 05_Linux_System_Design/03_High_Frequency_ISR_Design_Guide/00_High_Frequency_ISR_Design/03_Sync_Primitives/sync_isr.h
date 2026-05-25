/*
 * sync_isr.h — Synchronization Primitives for ISR/Task Communication
 *
 * Guide §3 Rules Table:
 *
 *  Context           | Use                            | Never use
 *  ------------------|--------------------------------|------------------------
 *  ISR → Task signal | semaphore post_from_isr()      | mutex_lock() (BLOCKS)
 *  Shared flag       | volatile + memory barrier      | plain variable
 *  ISR critical sect | disable_irq() / enable_irq()   | RTOS mutex
 *  64-bit on 32-bit  | safe_read_u64() + disable_irq  | two 32-bit reads
 *
 * This header provides portable implementations of each primitive.
 */

#ifndef SYNC_ISR_H
#define SYNC_ISR_H

#include <stdint.h>

/* ---- Memory barrier ------------------------------------------------------ */

#if defined(__ARM_ARCH)
    /** Full system memory barrier — orders ALL memory accesses across the barrier */
    #define MEM_BARRIER()    __asm__ volatile ("dmb sy" ::: "memory")
    /** Compiler-only fence — prevents reordering in the compiler only, not CPU */
    #define COMPILER_FENCE() __asm__ volatile (""       ::: "memory")
#else
    /* x86/simulation: CPU has TSO — loads/stores are ordered; compiler fence suffices */
    #define MEM_BARRIER()    __asm__ volatile ("" ::: "memory")
    #define COMPILER_FENCE() __asm__ volatile ("" ::: "memory")
#endif

/* ---- IRQ enable/disable (ARM Cortex-M) ----------------------------------- */

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)

    /**
     * disable_irq() — Mask all interrupts by setting PRIMASK.
     * Returns the previous PRIMASK value so it can be restored.
     * Use when you need an atomic read-modify-write of shared state.
     */
    static inline uint32_t disable_irq(void)
    {
        uint32_t primask;
        __asm__ volatile ("mrs %0, primask" : "=r"(primask));
        __asm__ volatile ("cpsid i"         ::: "memory");
        return primask;
    }

    /**
     * enable_irq() — Restore interrupts to state before disable_irq().
     * Pass the value returned by disable_irq().
     */
    static inline void enable_irq(uint32_t primask)
    {
        __asm__ volatile ("msr primask, %0" :: "r"(primask) : "memory");
    }

#else  /* Simulation / x86 */

    static inline uint32_t disable_irq(void) { COMPILER_FENCE(); return 0u; }
    static inline void     enable_irq(uint32_t s) { (void)s; COMPILER_FENCE(); }

#endif

/* ---- Volatile semaphore -------------------------------------------------- */

/**
 * isr_sem_t — Lightweight binary semaphore for ISR→Task signalling.
 *
 * RTOS equivalents:
 *   FreeRTOS:   xSemaphoreGiveFromISR() / xSemaphoreTake()
 *   CMSIS-RTOS: osSemaphoreRelease()    / osSemaphoreAcquire()
 *   Zephyr:     k_sem_give()            / k_sem_take()
 */
typedef volatile uint32_t isr_sem_t;

/**
 * sem_post_from_isr() — Non-blocking signal from ISR context.
 * Memory barrier before write ensures all ISR data writes are visible
 * to the task BEFORE the task sees the semaphore set.
 */
static inline void sem_post_from_isr(isr_sem_t *s)
{
    MEM_BARRIER();  /* RELEASE: all ISR data writes complete before sem visible */
    *s = 1u;
}

/**
 * sem_wait() — Spin-wait in task context.
 * In production: replace spin with RTOS block (saves CPU during wait).
 */
static inline void sem_wait(isr_sem_t *s)
{
    while (!*s) { /* spin */ }
    *s = 0u;
    MEM_BARRIER();  /* ACQUIRE: see all data written before sem_post_from_isr */
}

/* ---- Volatile flag helpers ----------------------------------------------- */

/** vflag_read() — Read a volatile flag with acquire semantics. */
static inline uint32_t vflag_read(volatile uint32_t *f)
{
    uint32_t v = *f;
    MEM_BARRIER();
    return v;
}

/** vflag_write() — Write a volatile flag with release semantics. */
static inline void vflag_write(volatile uint32_t *f, uint32_t val)
{
    MEM_BARRIER();
    *f = val;
}

/* ---- Safe 64-bit read on 32-bit CPU -------------------------------------- */

/**
 * safe_read_u64() — Atomically read a 64-bit counter on a 32-bit MCU.
 *
 * Problem: On a 32-bit CPU, a 64-bit load compiles to two 32-bit LDR
 * instructions.  If an ISR fires between the two loads and updates the
 * counter, you read mismatched halves — a "torn read".
 *
 *   CPU: LDR r0, [p+0]   ← reads low word
 *   ISR fires! Counter increments (low wraps, high +1)
 *   CPU: LDR r1, [p+4]   ← reads NEW high word  ← TORN
 *
 * Solution: disable interrupts for the two-word load.
 */
uint64_t safe_read_u64(volatile uint64_t *p);

#endif /* SYNC_ISR_H */

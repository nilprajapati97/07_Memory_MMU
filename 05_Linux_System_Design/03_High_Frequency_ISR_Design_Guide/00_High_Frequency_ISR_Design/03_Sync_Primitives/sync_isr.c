/*
 * sync_isr.c — Synchronization Primitives Implementation
 */

#include "sync_isr.h"

uint64_t safe_read_u64(volatile uint64_t *p)
{
    /*
     * Why this is necessary on 32-bit CPUs:
     *
     *   A 64-bit value occupies two 32-bit words.
     *   Reading it compiles to either LDRD (if aligned) or two LDR instructions.
     *
     *   Scenario without protection:
     *
     *     Task:  LDR  r0, [p+0]      ← reads low word   (e.g. 0xFFFFFFFF)
     *            -- ISR fires here --
     *     ISR:   increments *p: low=0x00000000, high++ (e.g. high=0x00000001)
     *            -- ISR returns --
     *     Task:  LDR  r1, [p+4]      ← reads NEW high word (0x00000001)
     *
     *     Result: task got r1:r0 = 0x00000001_FFFFFFFF  (WRONG — torn read)
     *             Correct values were either 0x00000000_FFFFFFFF or 0x00000001_00000000
     *
     *   Fix: disable interrupts for the two-word read so no ISR can fire between them.
     *
     *   Alternative for read-only shared counters: retry loop
     *     do {
     *         lo = p->low;
     *         hi = p->high;
     *     } while (lo != p->low);    // if low changed, ISR fired — retry
     *
     *   Alternative with C11: _Atomic uint64_t + atomic_load()
     *   On ARMv8-A: uses LDP (load pair) which is atomic for aligned 64-bit.
     */
    uint32_t saved = disable_irq();
    uint64_t val   = *p;
    enable_irq(saved);
    return val;
}

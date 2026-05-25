/*
 * isr_correct.c — Canonical Correct ISR Patterns
 *
 * Guide §4 checklist — showing MUST DO items with inline comments.
 *
 * Target: ARM Cortex-M4 @ 168 MHz, 100 kHz interrupt rate.
 * Per-interrupt budget: 10 µs = 1680 cycles.
 * ISR must use < 50% → < 840 cycles (< 5 µs).
 * These patterns: 8–25 cycles each.
 */

#include <stdint.h>
#include <stdbool.h>

/* ---- Memory barrier ------------------------------------------------------ */

#if defined(__ARM_ARCH)
    #define MEM_BARRIER() __asm__ volatile ("dmb sy" ::: "memory")
#else
    #define MEM_BARRIER() __asm__ volatile ("" ::: "memory")
#endif

/* ============================================================
 * PATTERN 1: UART receive ISR — read, push, signal
 *
 * ISR budget breakdown:
 *   Read USART->DR   : 1 cycle  (peripheral register load)
 *   Overflow check   : 2 cycles (compare + branch)
 *   Array store      : 1 cycle
 *   DMB              : 1–10 cycles (ARM implementation dependent)
 *   Index update     : 1 cycle
 *   Signal worker    : 1 cycle
 *   Total            : ~8–15 cycles = 0.09 µs @ 168 MHz
 * ============================================================ */

#define RING_BUF_SIZE   4096u
#define RING_BUF_MASK   (RING_BUF_SIZE - 1u)

static volatile uint32_t head        = 0u;
static volatile uint32_t tail        = 0u;
static          uint8_t  rx_buf[RING_BUF_SIZE];
static volatile uint32_t data_ready  = 0u;
static volatile uint32_t overflow_ct = 0u;

void UART1_ISR_correct(void)
{
    /* ✅ RULE 1: Read hardware register IMMEDIATELY — clears the interrupt flag.
     *    On STM32: reading USART->DR (or SR + DR) clears RXNE flag.
     *    If you check any other condition first and the flag isn't cleared,
     *    the interrupt re-fires as soon as you return → infinite loop. */
    uint8_t byte = *((volatile uint8_t *)0x40011004u);  /* USART1->DR (example) */

    /* ✅ RULE 2: Overflow detection — count drops, never silently discard */
    uint32_t next = (head + 1u) & RING_BUF_MASK;
    if (next == tail) {
        overflow_ct++;          /* ✅ 32-bit aligned write is atomic on Cortex-M */
        return;                 /* Drop this byte — buffer full */
    }

    /* ✅ RULE 3: Store data BEFORE updating the index */
    rx_buf[head] = byte;

    /* ✅ RULE 4: Memory barrier between data write and index update */
    MEM_BARRIER();

    /* ✅ RULE 5: Publish new index — consumer can now safely read rx_buf[old_head] */
    head = next;

    /* ✅ RULE 6: Non-blocking signal only — never block in ISR */
    data_ready = 1u;
}

/* ============================================================
 * PATTERN 2: SysTick — increment-only counter
 *
 * Correct because:
 *   - 32-bit aligned increment is a single STR on Cortex-M (atomic)
 *   - No function calls, no branches, no data dependencies
 *   - Completes in 1–2 cycles
 * ============================================================ */

static volatile uint32_t g_tick_ms = 0u;

void SysTick_ISR_correct(void)
{
    /* ✅ Single 32-bit increment — atomic on 32-bit aligned address */
    g_tick_ms++;
}

uint32_t get_tick_ms(void)
{
    /* ✅ volatile ensures fresh load from memory, not cached register */
    return g_tick_ms;
}

/* ============================================================
 * PATTERN 3: ADC DMA complete — flag only, zero data copies
 *
 * The DMA hardware fills the buffer.  The ISR only records WHICH
 * half of the buffer is ready — no data touching in ISR at all.
 * ============================================================ */

#define DMA_HALF 256u

static uint32_t           adc_buf[2][DMA_HALF];  /* DMA writes here */
static volatile uint8_t   ready_half   = 0u;
static volatile uint32_t  buf_ready    = 0u;

void DMA1_Stream0_HalfTransfer_ISR_correct(void)
{
    /* ✅ Just set flags — no data processing, no copies, no loops */
    ready_half = 0u;                /* First half is ready */
    MEM_BARRIER();
    buf_ready  = 1u;
}

void DMA1_Stream0_FullTransfer_ISR_correct(void)
{
    /* ✅ DMA wrapped automatically — signal second half */
    ready_half = 1u;
    MEM_BARRIER();
    buf_ready  = 1u;
}

/* ============================================================
 * PATTERN 4: Safe 64-bit counter update in ISR
 *
 * If this is the ONLY ISR that writes g_event_count, and it's at
 * a fixed priority level (no higher-priority ISR reads it), then
 * the ++ is safe — no other context can preempt.
 *
 * If a higher-priority ISR or task can read g_event_count while
 * this ISR might be writing: use disable_irq() guard or _Atomic.
 * ============================================================ */

static volatile uint64_t g_event_count = 0u;

void EventCounter_ISR_correct(void)
{
    /* ✅ Within a single ISR at fixed priority, 64-bit ++ is safe:
     *    No other context preempts this ISR at same/lower priority. */
    g_event_count++;
}

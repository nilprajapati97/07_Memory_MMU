# 04 — ISR Design Rules and Anti-Patterns

## The Golden Rule

> **ISR does MINIMUM work. Worker task does ALL heavy lifting.**

At 100 kHz, the ISR fires every 10 µs. The ISR must complete in **< 5 µs** (50% budget rule). Everything else runs in a high-priority RTOS task.

---

## The Complete Checklist

### ISR MUST ✅

| Rule | Why |
|---|---|
| Read data register immediately | Clears interrupt flag on UART/SPI/ADC — prevents re-entry |
| Store to ring buffer or set flag only | Minimal work — no processing |
| Use no dynamic memory (no malloc/free) | malloc() acquires heap lock → deadlock if task holds it |
| Use no blocking calls | Blocking in ISR = CPU stuck forever in interrupt context |
| Complete in < 50% of interrupt period | At 100 kHz: < 5 µs = 500 cycles @ 100 MHz |
| Post semaphore/event (non-blocking) | Only `_FromISR` variants allowed |
| Count overflows explicitly | Never silently drop data |
| Use `volatile` + memory barrier on shared state | Prevents compiler/CPU reordering |

### ISR MUST NOT ❌

| Rule | Consequence if violated |
|---|---|
| Process or filter data | Exceeds time budget — other IRQs starved |
| Call RTOS blocking APIs | Deadlock — ISR can never block |
| Use `printf()` / `sprintf()` | printf locks, calls malloc, polls UART — all illegal |
| Use `malloc()` / `free()` | Heap lock deadlock |
| Access slow peripherals (I2C polling, SPI polling) | I2C at 400 kHz = 2.5 µs per byte — blows budget |
| Use floating point (without FPU context save) | Corrupts task's FPU register state |
| Call non-reentrant functions (`strtok`, `rand`, ...) | Corrupts static internal state shared with tasks |

---

## Timing Budget at 100 kHz

```
Interrupt period = 1 / 100,000 = 10 µs

ISR budget breakdown:
  Hardware interrupt latency (Cortex-M4):  12 cycles = 71 ns
  ISR entry/exit (push/pop context):       12 cycles = 71 ns
  Read hardware register:                   2 cycles = 12 ns
  Ring buffer push + DMB + index update:   15 cycles = 89 ns
  Semaphore post (volatile write):          2 cycles = 12 ns
  ─────────────────────────────────────────────────────────
  Total ISR overhead:                      43 cycles = 255 ns

ISR CPU usage:  43 cycles × 100,000/sec = 4,300,000 cycles/sec
                At 168 MHz: 4.3M / 168M = 2.6% CPU for ISR context

Remaining budget for tasks: 97.4% CPU
```

---

## Common Mistake Analysis

### Mistake 1: printf() in ISR

```c
void UART_ISR(void) {
    uint8_t b = USART1->DR;
    printf("Got: %02X\n", b);  // ❌
}
```

**What happens:**
1. Task is inside `printf()` holding `flockfile()` mutex.
2. ISR fires. ISR calls `printf()`. `printf()` tries to acquire the same mutex.
3. Mutex is already held by the task. ISR spins forever.
4. Task can never run (preempted by ISR). Mutex never released.
5. **System deadlocked.**

---

### Mistake 2: malloc() in ISR

```c
void Timer_ISR(void) {
    event_t *e = malloc(sizeof(event_t));  // ❌
}
```

**What happens:**
1. The C library `malloc()` uses a lock to protect the heap free-list.
2. If the task was mid-allocation when the ISR fired, the heap lock is held.
3. ISR's `malloc()` tries to acquire the same lock → deadlock.

**Fix:** Pre-allocate a static pool. Use a ring buffer of fixed-size structs.

---

### Mistake 3: No volatile, no barrier

```c
uint32_t data = 0;  // ❌ not volatile
uint32_t flag = 0;  // ❌ not volatile

void ISR(void) {
    data = sensor_read();
    flag = 1;  // ❌ no barrier — may be reordered before data write on ARM
}

void task(void) {
    while (!flag);    // ❌ compiler may cache flag in register — infinite loop
    use(data);        // ❌ may see stale data even if flag was seen as 1
}
```

**Fix:**

```c
volatile uint32_t data = 0;   // ✅
volatile uint32_t flag = 0;   // ✅

void ISR_fixed(void) {
    data = sensor_read();
    __asm__ volatile ("dmb sy" ::: "memory");  // ✅ RELEASE
    flag = 1;
}

void task_fixed(void) {
    while (!flag);
    __asm__ volatile ("dmb sy" ::: "memory");  // ✅ ACQUIRE
    use(data);
}
```

---

## Correct ISR Anatomy

```
void PERIPHERAL_ISRHandler(void)
{
    /* ① Read hardware register FIRST — clears interrupt flag */
    uint32_t data = PERIPH->DR;

    /* ② Overflow check */
    uint32_t next = (head + 1u) & MASK;
    if (next == tail) {
        overflow++;
        return;              /* ③ Early return on full — never block */
    }

    /* ④ Store data */
    buf[head] = data;

    /* ⑤ Memory barrier (RELEASE) */
    __DMB();

    /* ⑥ Publish index */
    head = next;

    /* ⑦ Non-blocking signal */
    g_data_ready = 1u;

    /* ⑧ Return immediately — never loop, never delay, never call blocking APIs */
}
```

---

## Common Interview Questions

**Q: Why must you read the data register first in a UART ISR?**  
A: On most ARM UARTs (STM32, NXP, Nordic), reading the data register (`USART->DR` or `USART->RDR`) also clears the `RXNE` (Receive Not Empty) interrupt flag. If you don't read it first, the interrupt flag remains set and the ISR fires again immediately after it returns — infinite re-entry loop.

**Q: You said floating point is dangerous. When is it safe?**  
A: Safe when: (a) FPU lazy context save is enabled (`FPCCR.ASPEN=1` and `FPCCR.LSPEN=1`, which is the ARM reset default), AND (b) the ISR doesn't nest with other ISRs that use the FPU, AND (c) you're aware that the first FPU instruction in the ISR triggers a stack frame extension (8 extra words = 32 bytes). Best practice: still avoid FP in ISR; push to task.

**Q: What's the maximum safe ISR execution time at 100 kHz?**  
A: 50% rule gives 5 µs. In practice, leave margin for nested interrupts and worst-case peripheral latency. Target ≤ 2 µs (200 cycles @ 100 MHz). If you need more: use DMA to reduce ISR rate from 100k/sec to 2× per buffer cycle.

**Q: Can you call `xQueueSendFromISR()` (FreeRTOS) in an ISR?**  
A: Yes — that's exactly the right pattern. FreeRTOS `_FromISR` variants are non-blocking; they return `pdTRUE` if a higher-priority task was woken. You then call `portYIELD_FROM_ISR(xHigherPriorityTaskWoken)` at the end of the ISR to trigger a context switch immediately after ISR return, so the woken task runs without extra latency.

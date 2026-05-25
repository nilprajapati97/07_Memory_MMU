## Designing ISR for High-Frequency Interrupts (100 kHz example)

At **1000 interrupts / 10ms = 100,000 interrupts/sec**, you have only **10 µs per interrupt**. The ISR must finish well within that window. The golden rule: **ISR does minimum work, deferred task does heavy lifting.**

---

## Core Design: ISR + Ring Buffer + Worker Task

```
[ Hardware ] --IRQ--> [ ISR: push to ring buffer ] --> [ Worker Task: process data ]
```

### 1. Lock-Free Ring Buffer (Single Producer / Single Consumer)

```c
#define RING_BUF_SIZE  4096   /* Must be power-of-2 */
#define RING_BUF_MASK  (RING_BUF_SIZE - 1)

typedef struct {
    volatile uint32_t head;   /* written by ISR (producer)    */
    volatile uint32_t tail;   /* written by worker (consumer) */
    uint32_t          buf[RING_BUF_SIZE];
} ring_buf_t;

/* ISR side — called 100k times/sec */
void ISR_handler(void) {
    uint32_t next_head = (rb.head + 1) & RING_BUF_MASK;

    if (next_head == rb.tail) {
        /* OVERFLOW: drop or set overflow flag */
        overflow_count++;
        return;
    }

    rb.buf[rb.head] = read_hardware_data();  /* fast register read */
    
    /* Memory barrier BEFORE updating head */
    __DMB();  /* ARM: Data Memory Barrier */
    rb.head = next_head;
    
    /* Signal worker (semaphore post — non-blocking) */
    semaphore_post_from_isr(&data_ready_sem);
}

/* Worker Task — runs in thread context */
void worker_task(void) {
    while (1) {
        semaphore_wait(&data_ready_sem);

        while (rb.tail != rb.head) {       /* drain all pending */
            uint32_t data = rb.buf[rb.tail];
            __DMB();
            rb.tail = (rb.tail + 1) & RING_BUF_MASK;
            process(data);
        }
    }
}
```

**Why lock-free works here**: Single producer (ISR) + single consumer (worker) = no mutex needed. `volatile` + memory barrier ensures visibility.

---

## 2. Double Buffering (for burst/block data)

When ISR collects data in chunks (e.g., ADC DMA):

```c
uint32_t buf_A[N], buf_B[N];
volatile uint8_t active = 0;   /* 0 = A filling, 1 = B filling */
volatile uint8_t idx   = 0;

void ISR_handler(void) {
    uint32_t *active_buf = (active == 0) ? buf_A : buf_B;
    active_buf[idx++] = read_data();

    if (idx >= N) {
        idx = 0;
        active ^= 1;          /* swap buffers atomically */
        signal_worker();      /* process the filled buffer */
    }
}
```

---

## 3. Synchronization Primitives — Rules

| Context | What to use | What NOT to use |
|---|---|---|
| ISR → Task signal | Semaphore `post_from_isr()` | `mutex_lock()` (blocks!) |
| Shared flag | `volatile` + memory barrier | plain variable |
| Critical section in ISR | `disable_irq()` / `enable_irq()` | RTOS mutex |
| 64-bit data on 32-bit CPU | atomic read (disable IRQ briefly) | two 32-bit reads |

```c
/* Safe 64-bit read on 32-bit MCU */
uint64_t safe_read_64(volatile uint64_t *p) {
    uint32_t primask = disable_irq();
    uint64_t val = *p;
    enable_irq(primask);
    return val;
}
```

---

## 4. ISR Design Rules (Priority Checklist)

```
ISR MUST:
  ✅ Read data register immediately (clear interrupt flag)
  ✅ Push to ring buffer or set flag only
  ✅ Use no dynamic memory (malloc/free)
  ✅ Use no blocking calls (printf, mutex_lock, sleep)
  ✅ Complete in < 50% of interrupt period (< 5 µs for 100kHz)
  ✅ Post semaphore/event to wake worker

ISR MUST NOT:
  ❌ Process or filter data
  ❌ Call RTOS blocking APIs
  ❌ Access slow peripherals (I2C, SPI with polling)
  ❌ Use floating point (unless FPU context saved)
  ❌ Call non-reentrant functions
```

---

## 5. DMA — Best Option for 100 kHz

At 100 kHz, even a minimal ISR fires 100,000 times/sec. **Use DMA with half-transfer + full-transfer interrupts** instead:

```
DMA fills buffer[0..N/2]  → Half-Transfer IRQ fires → process first half
DMA fills buffer[N/2..N]  → Full-Transfer IRQ fires → process second half
```

```c
/* DMA double-buffer with ping-pong */
uint32_t dma_buf[2][512];   /* ping-pong */

void DMA_HalfTransfer_ISR(void) {
    post_to_worker(dma_buf[0], 512);  /* process first half */
}

void DMA_FullTransfer_ISR(void) {
    post_to_worker(dma_buf[1], 512);  /* process second half */
}
```

This reduces interrupt rate from **100,000/sec to just 2× per buffer cycle**.

---

## 6. Overflow Detection

Never silently drop data:

```c
typedef struct {
    uint32_t overflow_count;
    uint32_t total_received;
    uint32_t total_processed;
} isr_stats_t;

volatile isr_stats_t stats;

/* In ISR overflow path */
stats.overflow_count++;   /* atomic on 32-bit aligned word */
```

Monitor `overflow_count` — if non-zero, your worker is too slow or buffer is too small.

---

## Summary Architecture

```
Hardware IRQ (100kHz)
      |
      v
[ ISR: ~1-2 µs ]
  - Read data register
  - Write to ring buffer (lock-free)
  - Post semaphore
      |
      v
[ Ring Buffer: 4096 entries ]  <-- size = interrupt_rate × max_latency
      |
      v
[ Worker Thread (high priority) ]
  - Drain entire buffer in one pass
  - Filter, transform, store
```

**Key tuning**: Ring buffer size = `interrupt_rate × worst_case_worker_latency`. For 100 kHz with 1 ms worst-case latency → minimum 100 entries, use **4096** for safety margin.
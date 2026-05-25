# 03 — Synchronization Primitives for ISR/Task Communication

## The Core Rule

ISRs run at interrupt priority — they preempt tasks but cannot be preempted by tasks. This asymmetry means:

> **Any synchronization primitive that can BLOCK is illegal in an ISR.**

A blocked ISR never returns → the CPU is stuck in interrupt context → all lower-priority interrupts and all RTOS tasks are starved forever.

---

## Rules Table (from Guide §3)

| Context | What to Use | What NEVER to Use | Why |
|---|---|---|---|
| ISR → Task signal | `semaphore_post_from_isr()` | `mutex_lock()` | mutex blocks; _FromISR variant is non-blocking |
| Shared flag | `volatile` + memory barrier | plain variable | compiler/CPU may cache or reorder without these |
| ISR critical section | `disable_irq()` / `enable_irq()` | RTOS mutex | RTOS mutex blocks; disable_irq is instantaneous |
| 64-bit on 32-bit CPU | `safe_read_u64()` + disable_irq | two 32-bit reads | torn reads produce wrong values |

---

## Primitive 1 — Volatile + Memory Barrier (Shared Flag)

### What `volatile` does:
- Tells the compiler: "this variable can change outside the normal control flow."
- Prevents the compiler from caching the variable in a register.
- Prevents the compiler from eliminating reads/writes it considers redundant.

### What `volatile` does NOT do:
- Does NOT prevent CPU hardware from reordering memory accesses.
- Does NOT generate any barrier instruction.
- Does NOT provide any atomicity guarantee.

```c
/* ❌ WRONG — no barrier */
volatile uint32_t flag = 0;
uint32_t data = 42;

void ISR(void) {
    data = 42;          /* CPU may reorder this AFTER flag=1 on ARM */
    flag = 1;           /* Worker sees flag=1 but reads stale data */
}

/* ✅ CORRECT — volatile + barrier */
void ISR_correct(void) {
    data = 42;
    __asm__ volatile ("dmb sy" ::: "memory");  /* RELEASE */
    flag = 1;           /* Guaranteed visible AFTER data write */
}

void worker(void) {
    while (!flag) {}
    __asm__ volatile ("dmb sy" ::: "memory");  /* ACQUIRE */
    use(data);          /* Guaranteed to see data=42 */
}
```

### Memory model — why ARM needs DMB but x86 doesn't:

| Architecture | Store ordering | Need HW barrier? |
|---|---|---|
| ARM Cortex-M (ARMv7-M) | Weakly ordered — stores can be reordered | Yes — `DMB SY` |
| ARM Cortex-A (ARMv7-A/ARMv8-A) | Weakly ordered | Yes — `DMB SY` or `STLR/LDAR` |
| x86 / x86-64 | Total Store Order (TSO) — stores globally ordered | No HW barrier (compiler fence only) |
| RISC-V | Weakly ordered | Yes — `FENCE` |

---

## Primitive 2 — Binary Semaphore (ISR → Task Signal)

```
ISR context:                         Task context:
  sem_post_from_isr()                  sem_wait()
      │                                    │
      │  1. MEM_BARRIER() [RELEASE]        │  1. spin until sem != 0
      │  2. sem = 1                        │  2. sem = 0
      │                                    │  3. MEM_BARRIER() [ACQUIRE]
      │                                    │  4. read shared data
      └─────────── sem=1 visible ─────────►│
```

**The barrier placement is critical:**
- `sem_post_from_isr`: barrier BEFORE writing `sem=1` (RELEASE — ensures data visible before the flag)
- `sem_wait`: barrier AFTER reading `sem=1` (ACQUIRE — ensures data visible before processing)

**RTOS equivalents:**

| RTOS | Post from ISR | Wait in task |
|---|---|---|
| FreeRTOS | `xSemaphoreGiveFromISR()` | `xSemaphoreTake()` |
| CMSIS-RTOS v2 | `osSemaphoreRelease()` | `osSemaphoreAcquire()` |
| Zephyr | `k_sem_give()` | `k_sem_take()` |
| ThreadX | `tx_semaphore_put()` | `tx_semaphore_get()` |

---

## Primitive 3 — disable_irq() / enable_irq() (Critical Section)

Use when you need an **atomic read-modify-write** of data that the ISR could interrupt.

```c
/* ✅ Safe: disable interrupts for the critical section only */
void update_shared_state(void) {
    uint32_t saved = disable_irq();   /* PRIMASK = 1, IRQs masked */
    shared_var++;                     /* Atomic modify — no ISR can interrupt */
    enable_irq(saved);                /* Restore previous PRIMASK */
}
```

**Duration**: disable_irq() critical sections should be **< 1 µs** (< 168 cycles @ 168 MHz). Any longer increases interrupt latency and jitter.

**Never nest**: If you call disable_irq() twice without restore, the second enable_irq() restores the FIRST primask (potentially re-enabling interrupts earlier than intended). Always save/restore the return value.

---

## Primitive 4 — safe_read_u64() (Torn Read Prevention)

### The problem: 64-bit torn read on 32-bit CPU

```
CPU clock cycles:
  t=0:  LDR r0, [p+0]         ← reads low word  (e.g. 0xFFFFFFFF)
  t=1:  [ISR fires! p is incremented: low=0x00000000, high=0x00000001]
  t=2:  LDR r1, [p+4]         ← reads high word  (NOW it's 0x00000001)

Result: r1:r0 = 0x00000001_FFFFFFFF  ← WRONG (neither old nor new value)
Correct: either 0x00000000_FFFFFFFF or 0x00000001_00000000
```

### Solution:

```c
uint64_t safe_read_u64(volatile uint64_t *p) {
    uint32_t saved = disable_irq();   /* prevent ISR between the two LDR */
    uint64_t val   = *p;              /* both LDRs execute without preemption */
    enable_irq(saved);
    return val;
}
```

### Alternative — retry loop (if ISR only increments, no disable needed):

```c
uint64_t safe_read_u64_retry(volatile uint32_t *lo, volatile uint32_t *hi) {
    uint32_t h, l;
    do {
        h = *hi;
        l = *lo;
    } while (h != *hi);  /* if high changed, ISR fired between reads — retry */
    return ((uint64_t)h << 32) | l;
}
```

---

## Common Interview Questions

**Q: Why can't you use a mutex in an ISR?**  
A: A mutex can block if another thread holds it. An ISR cannot block — it runs at interrupt priority with no RTOS scheduler. Blocking an ISR means the CPU never returns from interrupt context → all tasks starve.

**Q: When is `volatile` alone sufficient (without DMB)?**  
A: Only on x86/x86-64 with TSO memory model (compiler fence equivalent to HW barrier for ordinary stores), OR on single-threaded code where the ISR and task share a flag but no data ordering is needed (e.g., a raw counter you only read, never depend on in a sequence).

**Q: What is priority inversion, and how does it relate to ISRs?**  
A: Priority inversion: a high-priority task waits for a resource held by a low-priority task. In ISR context: if the ISR needs a mutex held by a task, the ISR spins forever (deadlock variant). Solution: design ISRs to be lock-free (ring buffer, atomic flag) — they never wait for task-context resources.

**Q: On ARMv8-A (Cortex-A55/A76), what replaces DMB SY?**  
A: `STLR` (Store-Release) and `LDAR` (Load-Acquire). These are single instructions that combine the store/load with the appropriate memory barrier. More efficient than `STR` + `DMB SY`. Compilers generate them automatically with C11 `_Atomic` + `memory_order_release`/`memory_order_acquire`.

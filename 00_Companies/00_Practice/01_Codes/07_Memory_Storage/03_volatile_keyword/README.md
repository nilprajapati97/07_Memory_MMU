# 03 — `volatile` Keyword in Embedded Code

## Problem
Explain why `volatile` is critical in embedded systems and identify every situation that requires it.

## Why It Matters
Wrong `volatile` ⇒ bug class that is invisible at `-O0` and appears only after enabling optimisation. The four scenarios below cover ~100% of real-world uses. Missing `volatile` on an MMIO register is one of the most expensive bugs to debug because the compiler legitimately optimises away "redundant" reads or merges adjacent writes.

## What `volatile` Actually Means
A `volatile`-qualified object may be **modified by something the compiler cannot see**. The compiler must therefore:
1. **Re-read** the object from memory on every access (no caching in registers).
2. **Write** to memory on every assignment (no removing "dead" stores).
3. Not **reorder** accesses to the same `volatile` object across other `volatile` accesses.

`volatile` does **not** provide atomicity, memory barriers across CPUs, or thread synchronisation. (Those need `_Atomic`, `mb()`, `mutex`, etc.)

## The Four Required Use Cases

### Use Case 1 — Memory-Mapped I/O (MMIO Registers)
A hardware peripheral changes the value at a fixed address. The CPU never touched it; the compiler thinks the value is stable.
```c
#define UART_STATUS  (*(volatile uint32_t *)0x40011000)
#define UART_DATA    (*(volatile uint32_t *)0x40011004)

while (!(UART_STATUS & TX_READY)) { /* spin */ }
UART_DATA = 'A';
```
Without `volatile`, the compiler hoists `UART_STATUS` out of the loop → infinite spin.

### Use Case 2 — Variables Shared With an ISR
The main loop reads a flag set by an interrupt handler. From the main thread's view, the flag changes "magically".
```c
volatile sig_atomic_t g_data_ready = 0;

void ISR_RXC(void)            { g_data_ready = 1; }
void main_loop(void) {
    while (!g_data_ready) {}  // need volatile or compiler caches 0
    g_data_ready = 0;
    process_packet();
}
```
Pair with masked interrupts / atomic ops if the variable is wider than `sig_atomic_t` or accessed multi-byte.

### Use Case 3 — Variables Modified by Another Thread
Same shape as ISR case. `volatile` is **necessary but not sufficient** — it stops compiler reordering for that object but does **not** insert CPU memory barriers nor guarantee atomic 32/64-bit access on all architectures. Prefer `_Atomic` (C11) or `stdatomic.h` primitives for real cross-thread sharing.

### Use Case 4 — `setjmp` / `longjmp` Locals
Auto variables modified between `setjmp` and `longjmp` must be `volatile`, otherwise the saved register copy is restored and your change is lost.
```c
jmp_buf env;
void f(void) {
    volatile int retries = 0;
    if (setjmp(env)) { retries++; if (retries < 3) ... }
    do_risky_thing();   // may longjmp(env, 1)
}
```

## Comparison: When to Reach for Which Tool
| Need | Tool |
|---|---|
| Hardware register | `volatile uint32_t *` |
| ISR ↔ main flag | `volatile sig_atomic_t` |
| Cross-thread int counter | `_Atomic` / `atomic_int` |
| Cross-CPU ordering | memory barriers (`smp_mb`, `atomic_thread_fence`) |
| Critical section | mutex / spinlock |
| Constant in ROM | `const` (often `const volatile` for hardware revision register) |

## Key Insight
`volatile` is about **what the compiler is forbidden to assume**. It addresses **visibility from the compiler's point of view**, not synchronisation between cores. Misusing `volatile` as a thread-sync primitive is a classic bug.

## Pitfalls
- Using `volatile` to "fix" a race instead of a mutex — does not provide atomicity or barriers
- Casting away `volatile` to call a non-volatile API (`memcpy` on MMIO) — UB, and some compilers emit wrong code
- `volatile` on the pointer vs on the pointee: `volatile uint32_t *p` ≠ `uint32_t * volatile p`
  - `volatile T *p` — pointee is volatile (MMIO data)
  - `T * volatile p` — pointer itself is volatile (rare)
  - `volatile T * volatile p` — both
- Adjacent writes to `volatile`: not merged, may bus-cycle each one (matters on slow buses)
- `volatile` does NOT mean atomic — a 64-bit write on a 32-bit MCU is still two transactions

## Interview Tips
1. State the meaning: "compiler may not cache or elide accesses". Then list the four use cases.
2. Volunteer the misuse: "`volatile` is not a thread-sync primitive — for that I'd use `_Atomic` or a mutex."
3. Memorise the pointer placement table; expect one tricky declaration.
4. Mention the common MMIO macro idiom (`(*(volatile uint32_t *)0xADDR)`).

## Related / Follow-ups
- [04_hardware_register (Embedded Gotchas)](../../11_Embedded_Gotchas/04_hardware_register/)
- C11 `_Atomic` / `<stdatomic.h>`
- Memory barriers (acquire/release, see [10_memory_barriers](../../08_OS_Kernel_Concurrency/10_memory_barriers/))
- Linux kernel `READ_ONCE` / `WRITE_ONCE` macros (a "safer volatile")

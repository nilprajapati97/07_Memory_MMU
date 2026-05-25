# 01 — Ring (Circular) Buffer

## Problem
Fixed-size FIFO backed by a contiguous array, where indices wrap around. Producers `push`, consumers `pop`. Must distinguish **empty** from **full**.

```
capacity = 8
head ─┐     ┌─ tail
      v     v
[ . . A B C . . . ]   size=3
```

## Why It Matters
The workhorse data structure of embedded systems: UART RX/TX, kernel logs (`printk`), audio/video pipelines, DMA descriptor rings, lockfree IPC. Tests bit-tricks, memory ordering, and the empty-vs-full disambiguation.

## Approaches

### Approach 1 — Two Indices + Count Field (Simplest)
Store `head`, `tail`, and `count`. Empty ⇔ `count==0`; full ⇔ `count==cap`.
```text
push(x):
    if count == cap: return FULL
    buf[tail] = x
    tail = (tail + 1) mod cap
    count++
pop():
    if count == 0: return EMPTY
    x = buf[head]
    head = (head + 1) mod cap
    count--
    return x
```
- Cap need not be power of two; `mod` is general.
- `count` is a shared variable → not lockfree-friendly.

### Approach 2 — Sacrifice One Slot (Two Indices Only)
Don't use a count; treat full as `tail+1 == head`. One slot always unused.
- Empty: `head == tail`
- Full:  `(tail + 1) mod cap == head`
- Capacity = `cap - 1` usable slots.
- Safe for **SPSC lock-free** (no shared count).

### Approach 3 — Power-of-Two Capacity + Free-Running Indices (Best Lock-Free)
Let `head` and `tail` be 32- or 64-bit free-running counters; address = `index & (cap-1)` where `cap` is power of two. Empty/full computed by subtracting:
```text
size = tail - head       // unsigned wrap is OK
empty: size == 0
full:  size == cap
```
- No modulo, just mask — fast on every CPU.
- SPSC lock-free with one release/acquire pair (see [10_memory_barriers](../10_memory_barriers/)).
- This is Linux's `kfifo` design.

### Approach 4 — Multi-Producer / Multi-Consumer
- Coarse mutex around the whole buffer: simple, but contended.
- LMAX Disruptor-style: per-slot sequence numbers + CAS on the index. Complex but scales.
- Two-lock queue (Michael-Scott on top of a linked list, not array).

ASCII (SPSC, cap=8 power-of-two):
```
producer (only writer of tail)        consumer (only writer of head)
   |                                       ^
   v                                       |
   write buf[tail & 7]                     read buf[head & 7]
   store_release(tail+1)                   load_acquire(tail) → see write
```

## Comparison
| Variant | Indices | Cap restriction | Multi-thread | Lock-free |
|---|---|---|---|---|
| Count + 2 indices | 3 vars | none | needs lock | no |
| Sacrifice slot | 2 vars | none | SPSC safe | yes (SPSC) |
| Pow-2 mask + free-run | 2 vars | power of two | SPSC safe; MPMC needs CAS | yes (SPSC) |
| Mutex around any of above | + lock | none | yes | no |
| Disruptor / per-slot seq | per-slot | pow-2 | MPMC | yes |

## Key Insight
- The "**empty == full**" ambiguity has only three solutions: keep a count, sacrifice a slot, or use absolute (non-wrapping) counters and compare difference.
- Power-of-two capacity replaces division (`% cap`) with a single `& (cap-1)` mask — orders of magnitude faster on common CPUs.
- Lock-free SPSC works because **only one writer per index**; cross-CPU visibility needs `release` on the writer and `acquire` on the reader.

## Pitfalls
- Forgetting `volatile` or `_Atomic` on indices accessed by both ISR and main loop
- Memory ordering: writing data then `tail++` without a release fence → consumer reads stale data
- Non-pow-2 cap with `&` mask → wraps wrong
- Signed overflow on free-running indices → use `unsigned`
- `count` updated by two threads without atomic RMW → race
- ISR pushes, main thread pops, but `pop` uses interrupt-unsafe code (e.g. `printf`) inside critical section
- Resizing a ring buffer mid-flight → requires draining; usually unsupported

## Interview Tips
1. Clarify: SPSC, SPMC, MPSC, MPMC? capacity constraints? interrupt context?
2. Lead with the "empty vs full" question — interviewer wants you to mention the three resolutions.
3. Show power-of-two mask version; cite `kfifo` if you know it.
4. Mention memory ordering for SPSC lock-free — separates senior from junior answer.

## Related / Follow-ups
- [02_producer_consumer](../02_producer_consumer/) — uses ring buffers as the shared queue
- [03_thread_safe_queue](../03_thread_safe_queue/) — linked-list alternative for unbounded
- [10_memory_barriers](../10_memory_barriers/) — release/acquire semantics
- Linux `kfifo`, FreeRTOS stream buffers
- LMAX Disruptor (MPMC ring with per-slot sequence)

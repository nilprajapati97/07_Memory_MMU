# 18 — Timer Wheel

## Problem
Manage thousands or millions of pending timers efficiently: schedule one, cancel one, find which are due — all faster than O(log N) per tick.

## Why It Matters
TCP retransmits, keepalives, sleep queues, watchdogs, RTOS delays. With many timers and frequent expiry checks, the data structure choice dominates cost. Tests algorithms + cache awareness + real systems knowledge.

## Approaches

### Approach 1 — Sorted Linked List
Insert in order by expiry; head = next to fire. Each tick: pop expired heads.
- Insert: O(N)
- Cancel: O(1) with handle, O(N) by search
- Tick: O(1) amortised
- Fine for tens of timers; awful for thousands.

### Approach 2 — Min-Heap (Priority Queue)
Insert / extract-min in O(log N).
- Insert: O(log N)
- Cancel: O(log N) with handle and decrease-key support; O(N) without — many heap libs only support lazy deletion (mark expired)
- Tick: O(log N) per expired timer
- Standard in many user-space implementations (libevent, libuv).

### Approach 3 — Hashed Timing Wheel (Single Wheel)
Array of `N_BUCKETS` slots (a circular buffer); each slot holds a list of timers due in that tick. `cursor` advances each tick; fire all timers in the slot it lands on.
```text
bucket = (expiry_tick) mod N_BUCKETS
```
- Insert / cancel: O(1)
- Tick: O(1) + O(k) for k timers firing
- Limitation: timers with expiry > N_BUCKETS ticks need extra handling.

### Approach 4 — Hierarchical Timing Wheel (Multi-Wheel)
Like a clock with seconds/minutes/hours: small fast wheel for near-term timers, plus coarser wheels for longer ranges. When the outer wheel advances, it "cascades" timers into finer wheels.
- Insert / cancel: O(1)
- Tick: O(1) amortised
- Supports arbitrary expiry ranges with bounded memory.
- Used in Linux kernel (`tvec_base`), Netty (Java), RFC 6298 TCP timers, kafka request timers.

### Approach 5 — Timer-Wheel + Heap Hybrid
Near-term timers in a wheel (O(1)); far-future timers in a heap; promote from heap to wheel when they approach due.
- Bounds wheel memory; keeps O(log N) only for the rarely-touched far-future tail.

ASCII (Single hashed wheel, 8 buckets, cursor at 3):
```
buckets:  [0] [1] [2] [3] [4] [5] [6] [7]
                       ▲
                       cursor (this tick fires timers in bucket 3)
timer t with expiry=10, current_tick=2 → bucket = 10 mod 8 = 2
```

ASCII (Hierarchical, two-level: seconds wheel 60, minutes wheel 60):
```
                    ┌── minutes wheel (60 slots) ──┐
                    [ ][ ][T][ ][ ]…             cascades into ↓
                          │ (when cursor reaches T, walk its list,
                          │  re-insert each timer into seconds wheel
                          │  based on its remaining ticks)
                    ┌── seconds wheel (60 slots) ──┐
                    [ ][ ][ ][ ][ ]…
                          ▲ cursor advances each tick; fires whatever is there
```

## Comparison
| Structure | Insert | Cancel | Tick (k due) | Memory | Notes |
|---|---|---|---|---|---|
| Sorted list | O(N) | O(1)+find | O(k) | minimal | small N |
| Min-heap | O(log N) | O(log N)/lazy | O(k log N) | minimal | libevent default |
| Single wheel | O(1) | O(1) | O(k) | O(slots) | bounded horizon |
| Hierarchical | O(1) | O(1) | O(k)+ rare cascade | O(slots × levels) | Linux kernel |
| Wheel + heap | O(1)/O(log) | O(1)/O(log) | O(k) | bounded | best general |

## Key Insight
- "**Bucketize by expiry tick**" trades a tiny amount of memory for O(1) insert/cancel — the workhorse of any system with many timers.
- Hierarchical wheels scale to arbitrary expirations by **cascading**: each tick of the coarse wheel reschedules its slot's timers into a finer wheel.
- The fundamental trade-off: heap gives you a precise next-expiry time (good when timers are sparse and far apart); wheels give you O(1) ops at the price of bucket-granularity firing time (good when many timers).

## Pitfalls
- Cancelling a fired timer (callback already running) → coordinate via "cancel flag" or epoch
- Re-arming a timer from inside its own callback → must use the post-callback API, not mid-tick
- Drift: ticks driven by a periodic interrupt may slip; track absolute time, not "+= ticks"
- Cascading timers under a lock that the callback also takes → deadlock
- Long callback inside a tick → all later timers in that tick are delayed; use a "fire queue" and drain outside the wheel lock
- Bucket overflow if all timers land in the same slot (e.g. periodic with period = N_BUCKETS) → vary slot count or rotate
- Cancellation by walking the bucket list → O(bucket size); store a back-pointer to the node for O(1)

## Real-World References
- Linux kernel: `timer_list` / `hrtimer` (rb-tree for high-res), classic `tvec_base` hierarchical wheel
- Netty: HashedWheelTimer
- Varghese & Lauck 1987, *Hashed and Hierarchical Timing Wheels* (the foundational paper)
- Kafka request purgatory
- HAProxy connection timeouts

## Interview Tips
1. Start with min-heap as the textbook answer; then propose wheels for the scale case.
2. Draw the single-wheel diagram and explain bucket = `expiry mod N`.
3. Hierarchical = "clock analogy" — minutes + seconds wheels with cascading.
4. Cite firing granularity vs precision trade-off — wheels round to a tick.
5. Mention cancellation handle to avoid O(bucket) search.

## Related / Follow-ups
- [14_scheduler](../14_scheduler/) — also event-driven structures
- Linux `hrtimer` (rb-tree) for nanosecond precision
- libevent / libuv timer queues
- TCP RFC 6298 (RTO timer)
- Skip list as alternative O(log N) priority queue

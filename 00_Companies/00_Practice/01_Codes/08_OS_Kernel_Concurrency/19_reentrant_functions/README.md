# 19 — Reentrant Functions

## Problem
A function is **reentrant** if it can be safely called again — by the same thread (from a signal handler or nested call) or by another thread — while a previous invocation is still in progress. Stricter than "thread-safe".

## Why It Matters
Signal handlers, ISRs, recursive callbacks. Many standard library functions are thread-safe **but not reentrant** because they take internal locks; calling them from a signal handler that interrupts the lock holder deadlocks.

## Reentrant vs Thread-Safe
| | Reentrant | Thread-safe |
|---|---|---|
| Definition | safe under nested same-thread call (e.g. signal) | safe under concurrent call from multiple threads |
| Uses locks? | typically **no** | often yes |
| Uses static/global mutable data? | no | may, if locked |
| `strtok` | no | no (uses static) |
| `strtok_r` | **yes** | yes |
| `malloc` | no | yes (uses lock) |
| `printf` | no | yes (locks stdout) |

**Reentrant ⇒ thread-safe** (usually). The reverse does **not** hold.

## Rules for Reentrancy
A function is reentrant if **all** of:
1. Uses **no** static or global mutable data.
2. Returns no pointer to static data; caller provides buffers.
3. Calls only reentrant functions.
4. Does not depend on a lock the caller might already hold.

## Approaches to Make Code Reentrant

### Approach 1 — `_r` Variants (Caller-Supplied Buffer)
POSIX exposes `_r` versions for non-reentrant API:
```text
strtok      →  strtok_r(s, delim, &saveptr)
rand        →  rand_r(&seed)
ctime       →  ctime_r(t, buf)
gethostbyname → gethostbyname_r(...)
```
- Pure functions of inputs + caller state → no shared mutable.

### Approach 2 — Thread-Local Storage (`_Thread_local`)
Move "static" state to per-thread storage. Thread-safe but **not** reentrant if a signal interrupts the function (signal runs on same thread).
- Solves multi-threading; doesn't solve signal-handler reentrancy.

### Approach 3 — Async-Signal-Safe Subset
POSIX defines a list of functions safe to call from signal handlers (man `signal-safety(7)`):
- `write`, `read`, `_exit`, `signal`, `kill`, `sem_post`, `sig_atomic_t` ops, …
- **Not** in the list: `malloc`, `printf`, `pthread_mutex_*`, `fopen` — almost everything stateful.

### Approach 4 — Lock-Free Snapshot of State
For functions that must read shared state from a signal handler, publish via atomics so the handler reads a consistent snapshot without locking.
```text
atomic_store_release(&pub_ptr, new_copy);
/* handler */
auto p = atomic_load_acquire(&pub_ptr); read(p);
```

### Approach 5 — Self-Pipe / `signalfd` (Avoid Doing Anything in the Handler)
Handler writes a byte to a pipe (one of the few async-signal-safe APIs); main loop reads the pipe and does the real work in normal context.
- Sidesteps the reentrancy problem entirely.

## Concrete Example
```c
/* NOT reentrant: returns pointer to static */
char *bad_int_to_str(int n) {
    static char buf[16];
    snprintf(buf, sizeof buf, "%d", n);
    return buf;
}

/* Reentrant: caller-supplied buffer */
char *good_int_to_str(int n, char *buf, size_t cap) {
    snprintf(buf, cap, "%d", n);
    return buf;
}
```

## Why `malloc` Isn't Reentrant
`malloc` takes a global (or per-arena) lock. If a signal handler interrupts the main thread **while it holds the malloc lock** and the handler calls `malloc` → deadlock (same thread trying to relock a non-recursive mutex, or spin forever on the bit-lock).

The only signal-safe memory: pre-allocated buffers + an async-signal-safe primitive (`sig_atomic_t` flag, `write`, `sem_post`).

## Comparison
| Need | Tool |
|---|---|
| Multi-thread sharing of a counter | `<stdatomic.h>` |
| Per-thread "static" state | `_Thread_local` |
| Signal-safe communication to main | self-pipe / `signalfd` / `sig_atomic_t` flag |
| Reentrant version of stdlib fn | `_r` variant |
| Reentrant memory in signal context | pre-allocated buffer |

## Pitfalls
- Assuming "thread-safe" implies "signal-safe" — it usually doesn't (locks can deadlock against the same thread)
- Using `printf`, `malloc`, or `pthread_mutex_lock` from a signal handler → undefined behaviour / deadlock
- `_Thread_local` static is per-thread but **not** per-signal-handler invocation → still non-reentrant in handler
- Custom recursive locks "fix" same-thread re-acquire but don't restore reentrancy of the data structure operations
- `errno` is per-thread (since POSIX 2001) but signal handlers must save and restore it because the interrupted code may be mid-syscall
- C standard guarantees only `sig_atomic_t` reads/writes are async-signal-safe (and not even ordered without a fence)
- `fork()` in multithreaded process: child has a copy of all locks **in whatever state they were** at fork — only call async-signal-safe functions until `exec`

## Interview Tips
1. State the difference between reentrant and thread-safe in one sentence; give `strtok_r` as the canonical example.
2. List the four rules for reentrancy.
3. For signals, mention the **self-pipe trick** and `signalfd` as the modern best practice.
4. Cite `malloc`/`printf` as thread-safe-but-not-reentrant.
5. Mention `fork` + locks pitfall — senior-level depth.

## Related / Follow-ups
- [20_signal_handler](../20_signal_handler/)
- [05_semaphore](../05_semaphore/) (`sem_post` is async-signal-safe)
- [09_atomic_counter](../09_atomic_counter/)
- POSIX `signal-safety(7)`
- `pthread_atfork` for fork/lock handling

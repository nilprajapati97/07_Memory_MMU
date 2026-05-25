# 20 — Signal Handlers

## Problem
A POSIX signal arrives asynchronously; the kernel pauses the thread and invokes a handler function in the same thread's context. What can the handler safely do?

## Why It Matters
Misbehaving signal handlers cause some of the hardest-to-reproduce bugs: deadlocks when interrupting `malloc`, corruption from non-atomic accesses, lost signals from incorrect mask/queue assumptions. Critical for daemons, debuggers, embedded systems, and any program that uses timers, `SIGINT`, or `SIGTERM`.

## Rules of Engagement
1. The handler runs on **some thread** that hasn't masked the signal — usually the main thread, but POSIX gives no guarantee.
2. The handler interrupts arbitrary code — **including** while it holds a mutex or is mid-`malloc`.
3. Only **async-signal-safe** functions (`signal-safety(7)`) are legal in the handler.
4. Reads/writes between handler and main code must use `volatile sig_atomic_t` (or atomics) and a fence.
5. `errno` must be saved and restored if the handler calls anything that may set it.

## Approaches

### Approach 1 — `volatile sig_atomic_t` Flag (Polling)
Handler sets a flag; main loop polls and acts.
```c
volatile sig_atomic_t g_quit = 0;
void on_sig(int s) { g_quit = 1; }

while (!g_quit) { do_work(); }
cleanup();
```
- Minimal, correct, oldest pattern.
- Latency = main loop poll interval.
- `sig_atomic_t` (C standard) guarantees atomic single read/write.

### Approach 2 — Self-Pipe Trick
Handler writes one byte to a pipe; `epoll`/`select` in main loop wakes up and reads it.
```c
int sigfd[2]; pipe(sigfd);
void on_sig(int s) { write(sigfd[1], "x", 1); }   // write is async-signal-safe

/* in event loop */
epoll_ctl(ep, EPOLL_CTL_ADD, sigfd[0], ...);
/* on read: drain pipe + handle event */
```
- Integrates signals with `epoll`/`select` event loops.
- No polling; works on any UNIX.

### Approach 3 — `signalfd` (Linux)
Read pending signals as ordinary file descriptor events.
```c
sigset_t m; sigemptyset(&m); sigaddset(&m, SIGINT);
sigprocmask(SIG_BLOCK, &m, NULL);
int sfd = signalfd(-1, &m, 0);
/* epoll_ctl(EPOLL_CTL_ADD, sfd) */
struct signalfd_siginfo info;
read(sfd, &info, sizeof info);    // synchronous, in main context
```
- Signals consumed via `read`; no asynchronous handler at all.
- Best practice for modern Linux daemons; no async-signal-safety constraint.

### Approach 4 — `sem_post` from Handler
`sem_post` is async-signal-safe. Handler posts to a semaphore; worker thread waits.
- Cleaner than self-pipe inside a single process.
- Limited to thread wake-up; can't carry data.

### Approach 5 — Dedicated Signal Thread + `sigwait`
Block all signals in every thread except one that calls `sigwait`/`sigwaitinfo` in a loop. Handler-less.
```c
sigset_t m; sigfillset(&m); pthread_sigmask(SIG_BLOCK, &m, NULL);
/* signal thread: */
int sig; sigwait(&m, &sig); handle(sig);
```
- Most flexible: handle in normal thread context, no async-signal-safety constraint.
- POSIX-standard alternative to `signalfd`.

### Approach 6 — `sigaction` Over `signal`
`signal()` semantics are historically inconsistent (BSD vs SysV — does it reset after firing?). Always use `sigaction()`:
```c
struct sigaction sa = {0};
sa.sa_handler = on_sig;
sigemptyset(&sa.sa_mask);
sa.sa_flags = SA_RESTART;        // auto-restart interrupted syscalls
sigaction(SIGINT, &sa, NULL);
```

## What's Async-Signal-Safe?
Mostly low-level syscalls: `write`, `read`, `_exit`, `signal`, `sigaction`, `kill`, `pipe`, `fork`, `execve`, `sem_post`, `sig_atomic_t` ops, simple POSIX I/O. **NOT** safe: `malloc`/`free`, `printf` (locks stdio), `pthread_mutex_lock` (deadlock), most stdlib, anything that touches `errno` without save/restore.

## Comparison
| Approach | Code in handler | Event-loop integration | Linux-only | When |
|---|---|---|---|---|
| `sig_atomic_t` flag | minimal | poll | no | simple programs |
| Self-pipe | `write` only | yes (`select`/`epoll`) | no | classic event loops |
| `signalfd` | none (no handler) | yes (`epoll`) | yes | modern Linux daemon |
| `sem_post` | one line | thread wakeup | no | multi-thread |
| Dedicated `sigwait` thread | none | n/a | no | most flexible |
| `sigaction` w/ handler | small | poll/self-pipe | no | legacy or simple |

## Key Insight
- **The "best" handler does almost nothing** — it sets a flag, writes a byte, or posts a semaphore. The real work happens in normal context, free of async-signal-safety constraints.
- Modern Linux code prefers `signalfd` or `sigwait` and avoids handlers entirely.
- Always use `sigaction`, not `signal` — predictable semantics.

## Pitfalls
- `printf` / `malloc` in handler → deadlock when interrupting the lock holder
- Forgetting to save/restore `errno` → handler's syscall trashes the interrupted code's errno
- Non-atomic global accesses from handler → torn reads/writes; use `sig_atomic_t` or atomics
- Returning normally from a `SIGSEGV` handler without `siglongjmp` → kernel re-runs the faulting instruction → loop forever
- Signal masks aren't inherited the way you expect across threads; use `pthread_sigmask`
- A signal delivered while in a syscall: if `SA_RESTART` is set the syscall auto-retries; otherwise it returns `EINTR`. Choose explicitly.
- Real-time signals (SIGRTMIN..SIGRTMAX) are **queued**; standard signals (`SIGINT` etc.) are **not** — multiple deliveries collapse into one. Don't count standard signals.
- `fork` in multithreaded program after signals are delivered: handler runs in child too — beware

## Interview Tips
1. State the async-signal-safety constraint first; explain *why* (handler interrupts arbitrary state including malloc lock).
2. Show the `volatile sig_atomic_t` flag pattern.
3. Volunteer the self-pipe trick / `signalfd` as the modern alternative.
4. Always use `sigaction`, not `signal`; explain `SA_RESTART`.
5. Mention queueing difference between RT and standard signals.

## Related / Follow-ups
- [19_reentrant_functions](../19_reentrant_functions/) — defines safe vs unsafe
- [05_semaphore](../05_semaphore/) — `sem_post` async-signal-safety
- POSIX `signal-safety(7)`, `sigaction(2)`, `signalfd(2)`, `sigwait(3)`
- C11 `_Atomic`, `atomic_signal_fence`
- libevent / libuv signal integration

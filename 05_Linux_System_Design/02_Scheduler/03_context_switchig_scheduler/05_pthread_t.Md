## `pthread_t` — Thread Handle/ID Type

`pthread_t` is an **opaque data type** (defined in `<pthread.h>`) that represents a **thread identifier** — essentially a handle to a thread, similar to how a file descriptor represents an open file.

---

### What it actually is (internally)

On Linux (glibc), `pthread_t` is:
```c
typedef unsigned long int pthread_t;
```
It's just an integer that the OS/library uses internally to identify a thread's control block (TCB) in the kernel/userspace thread table.

---

### How it behaves in your code

```c
pthread_t t1, t2;   // just two uninitialized integers — no thread exists yet
```

**Step 1 — Thread creation assigns the ID:**
```c
pthread_create(&t1, NULL, print_odd, NULL);
//              ↑
//    OS creates the thread and writes its ID into t1
//    t1 now holds something like: 140234567890432 (kernel thread ID)
```

**Step 2 — `pthread_join` uses it to track completion:**
```c
pthread_join(t1, NULL);
//            ↑
//    "Wait until the thread with ID t1 exits"
//    main() BLOCKS here until print_odd() returns NULL
```

---

### Memory layout in your `main()`

```
Stack of main():
┌──────────────┐
│  t1 = 12345  │  ← handle to thread running print_odd()
│  t2 = 67890  │  ← handle to thread running print_even()
└──────────────┘
       │                │
       ▼                ▼
  Thread t1          Thread t2
  (print_odd)        (print_even)
  has its own        has its own
  stack + registers  stack + registers
```

---

### Key behaviors

| Behavior | Detail |
|---|---|
| `t1` and `t2` are **local to `main()`** | They live on main's stack |
| **Not the same as PID** | `pthread_t` is a userspace/library ID; the kernel thread ID is different (`gettid()`) |
| **Opaque** | Never compare with `==` directly; use `pthread_equal(t1, t2)` |
| After `pthread_join()` returns | `t1` becomes invalid — the thread is gone |
| `pthread_self()` | Any thread can call this to get its own `pthread_t` |

---

### What happens if you lose the `pthread_t`?

```c
// If you did this instead:
pthread_create(NULL, ...);   // ERROR — can't join, can't cancel the thread
```

Without storing `t1`/`t2`, you'd have no way to call `pthread_join()`, and `main()` might return before the threads finish — causing the process to exit and **killing all threads abruptly**.

---

### One-line summary for interview:

> `pthread_t` is an opaque handle (internally a `unsigned long`) that `pthread_create()` fills in so you can later call `pthread_join()`, `pthread_cancel()`, or `pthread_equal()` on a specific thread.
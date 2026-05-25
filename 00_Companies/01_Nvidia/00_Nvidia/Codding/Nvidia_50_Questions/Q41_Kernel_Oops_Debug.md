# Q41: Kernel Oops Debugging

**Section:** Performance & Debugging | **Difficulty:** Hard | **Topics:** kernel oops, NULL pointer dereference, `addr2line`, `crash`, `kgdb`, KASAN, `ftrace`, call stack decoding

---

## Question

Analyze and fix a kernel oops caused by a NULL pointer dereference in a GPU driver. Demonstrate how to use `addr2line` and other kernel debugging tools.

---

## Answer

```c
/* ─── Buggy Code Causing Oops ────────────────────────────────────────────*/

/* BAD: no NULL check before dereference */
int gpu_submit_bad(struct gpu_channel *ch, struct gpu_cmd *cmd)
{
    /* If ch is NULL (e.g., context teardown race), this oops: */
    writel(cmd->opcode, ch->ring_base + ch->tail * 4);  /* NULL deref! */
    ch->tail = (ch->tail + 1) % ch->ring_size;
    return 0;
}

/* ─── The Oops output would look like: ──────────────────────────────────
 *
 * BUG: kernel NULL pointer dereference, address: 0000000000000048
 * #PF: supervisor write access in kernel mode
 * #PF: error_code(0x0002) - not-present page
 * PGD 0 P4D 0
 * Oops: 0002 [#1] SMP NOPTI
 * CPU: 3 PID: 1234 Comm: cuda_app Tainted: G  O  5.15.0-nvidia
 * RIP: 0010:gpu_submit_bad+0x4f/0xa0 [nvidia]
 * RSP: 0018:ffffc90001a3bd60 EFLAGS: 00010246
 * RAX: 0000000000000000 RBX: ffff8880142f3400 RCX: 0000000000000009
 * RDX: 0000000000000000 RSI: ffff88803a2c1000 RDI: 0000000000000000
 * Call Trace:
 *   gpu_channel_submit+0x7a/0x1f0 [nvidia]
 *   nvkms_ioctl+0x2a0/0x600 [nvidia]
 *   __x64_sys_ioctl+0x5a/0xc0
 *   do_syscall_64+0x3b/0x90
 *   entry_SYSCALL_64_after_hwframe+0x61/0xcb
 * ---[ end trace 8d9e4a1f2b8c7650 ]---
 */

/* ─── Step 1: Decode the crash address with addr2line ────────────────────
 *
 * RIP shows: gpu_submit_bad+0x4f
 * Command:
 *   addr2line -ifCe /path/to/nvidia.ko 0x4f
 *   # Or with vmlinux:
 *   addr2line -ifCe /usr/lib/debug/lib/modules/$(uname -r)/build/vmlinux \
 *             ffffffff82104f00
 *
 * Output:
 *   gpu_submit_bad
 *   /build/nvidia/gpu_channel.c:42
 *   (inlined by) gpu_channel_submit
 *   /build/nvidia/gpu_channel.c:89
 *
 * Line 42 is: writel(cmd->opcode, ch->ring_base + ch->tail * 4);
 * ch is NULL (RAX = 0x0), ch->ring_base offset = 0x48 (matches fault addr).
 */

/* ─── Step 2: Root cause analysis ───────────────────────────────────────
 *
 * Race condition:
 *   Thread A (ioctl): gpu_ctx_find(ctx_id) → ch != NULL
 *   Thread B (cleanup): gpu_ctx_destroy → sets ch = NULL in context struct
 *   Thread A: uses ch (now NULL after B's write) → oops
 *
 * Fix: reference counting + NULL check.
 */

/* ─── FIXED Code ─────────────────────────────────────────────────────────*/
int gpu_submit_fixed(struct gpu_channel *ch, struct gpu_cmd *cmd)
{
    if (unlikely(!ch)) {
        pr_err("GPU: gpu_submit called with NULL channel\n");
        return -EINVAL;
    }

    if (unlikely(!ch->ring_base)) {
        pr_err("GPU: channel %p has NULL ring_base (not initialized?)\n", ch);
        return -EINVAL;
    }

    if (unlikely(ch->tail >= ch->ring_size)) {
        pr_err("GPU: channel tail %u out of bounds (size %u)\n",
               ch->tail, ch->ring_size);
        return -EINVAL;
    }

    writel(cmd->opcode, ch->ring_base + ch->tail * 4);
    ch->tail = (ch->tail + 1) % ch->ring_size;
    return 0;
}

/* ─── KASAN detection (compile-time sanitizer) ───────────────────────────
 * Enable: CONFIG_KASAN=y, CONFIG_KASAN_INLINE=y
 *
 * KASAN output for out-of-bounds write:
 *   ==================================================================
 *   BUG: KASAN: slab-out-of-bounds in gpu_ring_write+0x60/0x80 [nvidia]
 *   Write of size 4 at addr ffff88801c3a0100 by task cuda_app/1234
 *   Allocated by task 1234:
 *     kzalloc+0x2a/0x40
 *     gpu_channel_create+0x58/0xc0 [nvidia]
 *   ...
 *   ==================================================================
 */

/* ─── ftrace: trace function calls leading to crash ─────────────────────
 *
 * Enable tracing for GPU driver functions:
 *   echo function_graph > /sys/kernel/debug/tracing/current_tracer
 *   echo 'gpu_*' > /sys/kernel/debug/tracing/set_ftrace_filter
 *   echo 1 > /sys/kernel/debug/tracing/tracing_on
 *   # reproduce the crash
 *   cat /sys/kernel/debug/tracing/trace | tail -50
 *
 * Output shows the exact call sequence leading to the oops:
 *   0)  2.123 us  | gpu_ctx_find() { ... }
 *   0)  0.892 us  | gpu_channel_submit() {
 *   0)  0.012 us  |   gpu_submit_bad();  <-- oops here
 *   0)            |   } /* oops */
 */

/* ─── Using 'crash' tool for post-mortem analysis ─────────────────────
 *
 * After kdump saves vmcore:
 *   crash /usr/lib/debug/vmlinux /proc/vmcore
 *
 * Inside crash:
 *   > bt            -- show backtrace at crash
 *   > dis -l gpu_submit_bad+0x4f   -- disassemble around crash
 *   > p ch          -- print value of ch (should be 0x0 = NULL)
 *   > kmem -s       -- slab allocator stats
 *   > mod -s nvidia -- load nvidia.ko symbols
 */
```

---

## Explanation

### Core Concept

```
Oops → Decode → Fix pipeline:

  kernel oops (ring buffer)
       │
       ▼
  dmesg | grep -A 30 "BUG:"
  Extract: RIP = module+offset, fault address
       │
       ▼
  addr2line -ifCe nvidia.ko <offset>
  → source file + line number
       │
       ▼
  Understand why (race? use-after-free? OOB?)
       │
       ├── NULL deref → add NULL check
       ├── Use-after-free → fix lifetime (kref, RCU)
       └── OOB → add bounds check (KASAN helps find)
```

### Key APIs / Macros Used

| Tool / API | Purpose |
|------------|---------|
| `addr2line -ifCe nvidia.ko 0x4f` | Decode crash offset to source line |
| `KASAN` (`CONFIG_KASAN=y`) | Compiler-instrumented memory error detector |
| `ftrace` / `function_graph` | Trace function call sequence |
| `crash` + vmcore | Post-mortem kernel debugger |
| `kgdb` | Live kernel debugger (requires serial console) |
| `CONFIG_LOCKDEP` | Detect lock ordering violations at runtime |
| `unlikely(cond)` | Branch hint: this condition is rare (branch predictor) |
| `pr_err(fmt, ...)` | Log error to kernel ring buffer |

### Trade-offs & Pitfalls

- **`unlikely()` around NULL checks.** Use `unlikely(!ptr)` for checks on paths that should never happen in production — the CPU's branch predictor assumes `!ptr` is false. Zero performance cost on happy path.
- **KASAN overhead.** KASAN adds ~2× memory usage (shadow memory) and ~1.5–2× runtime overhead. Only use in development/testing builds. KASAN is not enabled in production NVIDIA drivers.

### NVIDIA / GPU Context

NVIDIA CI pipeline runs Linux kernel builds with KASAN + `CONFIG_DEBUG_PAGEALLOC` to catch GPU driver memory bugs before release. `nvidia-bug-report.sh` captures the full dmesg including any oops for customer support analysis.

---

## Cross Questions & Answers

**CQ1: What information does the `RIP` register in an oops tell you?**
> `RIP` (instruction pointer) contains the exact address of the instruction that caused the fault. The oops format shows it as `module+offset`: e.g., `gpu_submit_bad+0x4f/0xa0 [nvidia]` means: offset 0x4f bytes into function `gpu_submit_bad`, which is 0xa0 bytes total in size, in the `nvidia` module. With `addr2line -ifCe nvidia.ko 0x4f`, you get the exact source file and line number. This narrows the entire crash investigation from "somewhere in the GPU driver" to a specific source line.

**CQ2: What is KASAN and what types of bugs does it detect?**
> KASAN (Kernel Address Sanitizer) instruments every memory access at compile time and maintains a "shadow memory" that tracks whether each byte of memory is valid. Detects: (1) heap out-of-bounds (slab/kmalloc buffer overrun), (2) use-after-free (accessing freed memory before reallocation), (3) stack out-of-bounds, (4) global variable out-of-bounds. Does NOT detect: NULL pointer dereference (a VM fault does that), logical errors, race conditions (use KCSAN for data races). KASAN catches bugs that are otherwise hard to detect because the memory access appears valid but reads/writes wrong data.

**CQ3: How do you use `kgdb` to debug a live kernel crash?**
> kgdb requires a second machine connected via serial console. Setup: (1) enable `CONFIG_KGDB=y`, `CONFIG_KGDB_SERIAL_CONSOLE=y`, (2) boot with `kgdboc=ttyS0,115200 kgdbwait` (kernel waits for debugger at boot), (3) on debug machine: `gdb vmlinux`, `target remote /dev/ttyS0`. In gdb: `bt` for backtrace, `p variable` to print, `b function` to set breakpoint, `c` to continue. kgdb halts ALL CPUs when a breakpoint is hit — entire system is frozen. Not usable in production. For production: use `crash` + vmcore from kdump.

**CQ4: What is a kernel panic and how does it differ from a kernel oops?**
> **Oops**: a recoverable (sometimes) kernel error. The kernel logs the fault, kills the offending process, and continues running. A NULL dereference in a kernel module typically causes an oops and kills the process using the faulted driver. **Panic**: unrecoverable kernel error — the kernel prints the oops then calls `panic()`. System halts or reboots. Triggered by: oops in interrupt context, double fault, stack overflow, explicitly via `BUG()`. Set `kernel.panic_on_oops=1` to convert all oops to panics (useful in production to avoid corrupted state).

**CQ5: How does `CONFIG_LOCKDEP` help debug deadlocks in GPU drivers?**
> LOCKDEP tracks the order in which locks are acquired. If a driver ever acquires `lock_A` then `lock_B` on any CPU, LOCKDEP records this ordering. If a second CPU acquires `lock_B` then `lock_A`, LOCKDEP immediately reports a potential deadlock (ABBA pattern) — even before an actual deadlock occurs. For GPU drivers: LOCKDEP catches `mutex` in IRQ context, inconsistent lock ordering across code paths, and self-deadlock (taking a lock already held). Overhead is significant (3–5×) — only for development builds. Reports via `dmesg | grep "possible circular locking"`.

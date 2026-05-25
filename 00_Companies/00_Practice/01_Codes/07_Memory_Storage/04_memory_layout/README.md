# 04 — Memory Layout of a C Program

## Problem
Describe the segments of a running C program and what lives in each — text, data, bss, heap, stack — and the tools used to inspect them.

## Why It Matters
Critical for debugging segfaults, understanding binary size, ROM/RAM budgeting on embedded targets, and answering "where does this variable live?". Maps directly to linker scripts on bare-metal systems.

## The Segments

```
High addresses
+-----------------------------+
|         Stack               |   grows downward
|           v                 |
|           .                 |
|           ^                 |
|         Heap                |   grows upward (sbrk) / scattered (mmap)
+-----------------------------+
|         BSS                 |   zero-initialised globals/statics
+-----------------------------+
|         Data                |   initialised globals/statics
+-----------------------------+
|         Rodata              |   string literals, const globals
+-----------------------------+
|         Text (code)         |   read-only, executable
+-----------------------------+
Low addresses
```

### Segment 1 — Text (.text)
- Compiled machine code + (sometimes) read-only constants on tiny systems.
- Permissions: **R-X** (read + execute, no write — guarded by MMU; W^X policy).
- Source: function bodies.
- Lives in **ROM/flash** on bare-metal targets.

### Segment 2 — Rodata (.rodata)
- String literals (`"hello"`), `const` globals, jump tables.
- Permissions: **R--**. Writes → SIGSEGV.
- Common bug: `char *p = "hi"; p[0] = 'H';` ← writes to read-only → crash.
- Use `const char *` to make the compiler reject this at compile time.

### Segment 3 — Data (.data)
- Globals/statics with **non-zero** initial values.
- Permissions: **RW-**.
- On embedded: stored in flash; copied to RAM during startup (`__data_start`/`__data_end` in the linker script).

### Segment 4 — BSS (.bss) — "Block Started by Symbol"
- Globals/statics that are **zero** or uninitialised.
- Occupies **no space in the binary** (only a size record). Cleared to zero by C runtime startup before `main`.
- Example: `static int big[1024*1024];` adds 4 MB to BSS but ~0 bytes to the executable on disk.

### Segment 5 — Heap
- Grows upward via `sbrk` (small allocs) or scattered via `mmap` (large allocs ≥128 KB by default on glibc).
- Managed by allocator (`malloc`/`free`).
- Per-process; not shared between processes.

### Segment 6 — Stack
- Per-thread, grows downward on most ABIs (x86-64, ARM AAPCS).
- Default size: 8 MB on Linux (configurable via `ulimit -s`, `pthread_attr_setstacksize`).
- Contains: return addresses, saved registers, local variables, function arguments not in registers.

### Tooling — How to Inspect
| Tool | Use |
|---|---|
| `size a.out` | quick text/data/bss sizes |
| `nm a.out` | every symbol + section letter (T=text, D=data, B=bss, R=rodata) |
| `objdump -h a.out` | section headers w/ addresses, perms |
| `objdump -d a.out` | disassemble .text |
| `readelf -l a.out` | program headers (loadable segments + perms) |
| `cat /proc/<pid>/maps` | actual VM layout at runtime |
| `pmap <pid>` | nicer view of /proc/maps |
| `valgrind --tool=massif` | heap profiling over time |

Example `size` output:
```
   text    data     bss     dec     hex
   1452     560      24    2036     7f4  a.out
```

## Comparison: Where Does X Live?
| Construct | Segment |
|---|---|
| `void f() {}` | text |
| `"hello"` literal | rodata |
| `const int K = 5;` (global) | rodata |
| `int g = 5;` (global) | data |
| `int g;` (global) | bss |
| `static int s;` inside function | bss |
| `static int s = 7;` inside function | data |
| `int *p = malloc(n);` | pointer on stack/data, buffer on heap |
| `int local;` | stack |
| `int big[100];` inside function | stack |
| `int big[100];` global | bss |

## Key Insight
The **disk image** stores text, rodata, and data. The **runtime image** additionally has bss (zeroed), heap (grown on demand), and per-thread stack. Embedded startup code (`crt0`) is what copies data from flash → RAM and clears bss before invoking `main`.

## Pitfalls
- Writing to a string literal — UB, often crashes
- Huge globals exploding BSS (memory present at run-time only)
- Confusing "binary size" (text+data) with "RAM footprint" (data+bss+heap+stack)
- Stack growing into heap on systems without guard pages (bare-metal) → silent corruption
- Embedded: forgetting that `const` globals live in flash, but only if the linker script puts `.rodata` there
- Forgetting that thread stacks are separate — each `pthread_create` allocates a new stack region

## Interview Tips
1. Draw the diagram. Always start with text at the bottom, stack at the top.
2. Mention the **W^X** rule (no segment is both writable and executable in modern OS).
3. For "how big is my program in RAM?" → bss + data + heap + per-thread stacks.
4. For embedded: explain `crt0` data-copy and bss-zero. Cite `__data_start`, `__bss_end` symbols.

## Related / Follow-ups
- [01_stack_vs_heap](../01_stack_vs_heap/)
- [12_static_keyword](../12_static_keyword/) — where statics live
- Linker scripts (`.ld` files) on ARM Cortex-M
- ASLR — segments are randomised per-process for security
- Position-Independent Executables (PIE)

# 12 — `static` Keyword (Inside Function vs File Scope)

## Problem
Explain the two distinct meanings of `static` and the rules for each.

## Why It Matters
`static` does **completely different** things depending on where it appears. Mixing the meanings, or putting `static` in a header file, causes reentrancy bugs, multiple-instance surprises, and linker confusion.

## The Two Approaches

### Approach 1 — `static` Inside a Function (Block Scope)
```c
int next_id(void) {
    static int id = 0;     // initialised ONCE, before main
    return ++id;
}
```
- Storage duration: **static** (whole program lifetime).
- Scope: **block** (only `next_id` sees the name).
- Linkage: **none**.
- Lives in **`.data`** if initialiser ≠ 0, else **`.bss`** (zero-initialised).
- Initialised exactly **once** — re-entering the function does not re-init.
- For non-trivial initialisers (C99+), evaluated on first entry; **not thread-safe** without explicit guard.

ASCII trace:
```
call 1 → id starts at 0 (init), increments → 1
call 2 → id = 1, increments → 2
call 3 → id = 2, increments → 3
```

### Approach 2 — `static` at File Scope (Translation Unit)
```c
// file_a.c
static int g_cache;            // hidden from other TUs
static void helper(void) {...}
```
- Storage duration: static (same as without `static`).
- Scope: file.
- Linkage: **internal** — symbol not visible to the linker outside this TU.
- Default tool for module-private state and helpers.

### Approach 3 — `static` in a Header (Anti-Pattern... Mostly)
```c
// bad.h
static int counter = 0;   // every .c that #includes this gets its OWN copy
```
- Each translation unit gets a separate object.
- For **variables**: almost always a bug — you wanted shared, got per-file.
- For **`static inline` functions**: **correct** — see [11_inline_vs_macros](../11_inline_vs_macros/).

### Approach 4 — `static` on a Function Parameter Array (C99 Rarely Asked)
```c
void f(int a[static 10]) { ... }
```
Tells the compiler `a` points to at least 10 elements (optimisation hint). Different keyword reuse — not the same family.

## Comparison Table
| Form | Duration | Scope | Linkage | Typical use |
|---|---|---|---|---|
| `static` local var | static | block | none | per-function persistent state |
| `static` global var | static | file | internal | module-private state |
| `static` function | static | file | internal | module-private helper |
| `static` in header (var) | static | per-TU | none/internal | **almost always wrong** |
| `static inline` in header | static | per-TU | internal | **correct header idiom** |

## Key Insight
`static` toggles different attributes depending on where it's applied:
- **Inside a block** → upgrades **duration** (auto → static); scope and linkage unchanged.
- **At file scope** → downgrades **linkage** (external → internal); duration and scope unchanged.

Same keyword, opposite levels of the language.

## Pitfalls
- Static local + threads = data race. Lock or use `_Thread_local`.
- Static local + recursion: only ONE copy exists across recursive calls; usually a bug.
- Function-local static initialiser with non-constant expression (C99+): first-call init can race in multithreaded code. C++11 fixed this with thread-safe `static`; C did not.
- `static` global in a header → silent per-TU copies; debug-confusing.
- A `static` function declared in a header but defined nowhere → unused-warning per file; usually wrong.
- Returning the address of a function-local `static` is **safe** (lifetime is whole program) — but creates a hidden global, not reentrant.

## Interview Tips
1. State the two meanings up front in one sentence each.
2. Volunteer the "static local in multithreaded code" caveat.
3. Mention `static inline` in headers as the one good `static`-in-header case.
4. Returning `&local_static` works but breaks reentrancy — say so before they ask.

## Related / Follow-ups
- [02_storage_classes](../02_storage_classes/), [13_extern_keyword](../13_extern_keyword/)
- [11_inline_vs_macros](../11_inline_vs_macros/) — `static inline` correctness
- `_Thread_local` (C11) — per-thread static storage
- Singleton vs static-local — same trade-offs
- Reentrancy vs thread-safety — [19_reentrant_functions](../../08_OS_Kernel_Concurrency/19_reentrant_functions/)

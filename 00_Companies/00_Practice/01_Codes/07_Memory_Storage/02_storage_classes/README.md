# 02 — Storage Classes (auto, register, static, extern)

## Problem
Explain each C storage-class specifier — what storage duration, scope, and linkage it implies — and when to use it.

## Why It Matters
Storage class governs **lifetime, visibility, and linkage**: the three properties that determine whether a variable is one symbol shared across files, a per-function private cache, or destroyed at scope exit. Wrong choice ⇒ link errors, races, surprising state across calls.

## The Four Storage Classes (plus typedef)

### Angle 1 — `auto`
- Default for local variables (rarely written explicitly).
- Storage duration: **automatic** (stack-allocated, destroyed at scope exit).
- Linkage: **none**.
```c
void f(void) { auto int x = 5; }  // identical to: int x = 5;
```
Notable: in **C++** `auto` was repurposed as type deduction (C++11). In C it still means "automatic storage" and is a no-op keyword.

### Angle 2 — `register`
- Hint to compiler to keep variable in a CPU register.
- Storage duration: automatic.
- Restriction: cannot take its address (`&x` is a compile error).
```c
register int i; for (i = 0; i < N; i++) ...
```
Modern compilers ignore the hint (their register allocator is better than yours). Useful historically; today its main effect is the `&` ban (occasionally exploited for safety).

### Angle 3 — `static`
Two distinct meanings depending on where it appears:

**3a. Static inside a function** — persistent across calls, initialised once.
```c
int counter(void) { static int n = 0; return ++n; }  // remembers between calls
```
- Storage duration: **static** (whole program lifetime).
- Scope: block (only `counter` sees it).
- Linkage: none.
- Initialised once at program start (zero-init if no initialiser).
- **Not thread-safe**; protect with mutex if shared.

**3b. Static at file scope** — internal linkage (private to translation unit).
```c
static int g_cache = 0;          // hidden from other .c files
static void helper(void) { ... } // file-private function
```
- Storage duration: static.
- Linkage: **internal** — symbol not exported; prevents accidental clashes.
- The default tool for module-private state.

### Angle 4 — `extern`
- Declares a name defined elsewhere; tells the linker "look in another TU".
- Storage duration: static.
- Linkage: **external** (or matches a prior declaration).
```c
// header.h
extern int g_count;          // declaration only
// file1.c
int g_count = 0;             // definition
// file2.c
#include "header.h"
void f(void) { g_count++; }
```
- Functions are `extern` by default — declaration in header, definition in one `.c`.
- `extern` on a local variable inside a function refers to the file-scope object.

### Bonus — `typedef`
Technically classed as a storage-class specifier in the grammar, but creates a type alias, not storage. Covered in [10_typedef_vs_define](../10_typedef_vs_define/).

## Comparison Table
| Specifier | Duration | Scope | Linkage | Typical use |
|---|---|---|---|---|
| `auto` | automatic | block | none | Locals (implicit) |
| `register` | automatic | block | none | Legacy hint; bans `&` |
| `static` (local) | static | block | none | Per-function counter / cache |
| `static` (file) | static | file | internal | Module-private state |
| `extern` | static | file | external | Cross-file sharing |
| `typedef` | (no storage) | scope of decl | n/a | Type alias |

## Key Insight
Three axes — duration, scope, linkage — are **independent**. `static` toggles linkage at file scope but toggles duration at block scope. `extern` exists primarily to provide a declaration without a definition.

## Pitfalls
- `static` in a header file → each `#include` site gets its **own** copy (probably not what you want for variables; sometimes correct for `static inline` functions)
- Forgetting that block-scope `static` is initialised exactly **once** — re-entering the function does **not** re-init
- Multiple definitions of a non-`static` global across TUs → linker error (multiple definition)
- `extern int a = 5;` is a definition (with an initialiser) — only the `extern` keyword is meaningless here
- `register` variable's address taken → compile error (useful enforcement trick)
- Static locals + threads = data race (must lock or use thread-local)

## Interview Tips
1. Be explicit: every `static`/`extern` question really asks about duration vs linkage. Name both.
2. Volunteer the **threading caveat** for static locals — separates you from textbook answers.
3. Show an example of `static` for module privacy vs `extern` for cross-file sharing — most candidates blur the two.

## Related / Follow-ups
- [12_static_keyword](../12_static_keyword/) — deeper dive on the two `static` meanings
- [13_extern_keyword](../13_extern_keyword/) — `extern "C"`, header-only globals
- C11 `_Thread_local` — thread-local storage class
- `const` and `volatile` are *type qualifiers*, not storage classes (different category)

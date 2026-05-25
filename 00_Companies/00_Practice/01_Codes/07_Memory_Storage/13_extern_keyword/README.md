# 13 — `extern` Keyword & `extern "C"`

## Problem
Use `extern` to share variables and functions across translation units; understand `extern "C"` for C/C++ interoperability.

## Why It Matters
Linkage errors ("multiple definition", "undefined reference") usually trace to misuse of `extern`. C/C++ linking interop requires `extern "C"` to disable name mangling.

## The Approaches

### Approach 1 — Sharing a Variable Across Files
```c
// counter.h
extern int g_count;          // DECLARATION (no storage)

// counter.c
int g_count = 0;             // DEFINITION (one and only)

// other.c
#include "counter.h"
void bump(void) { g_count++; }
```
- One definition, many declarations — the **One-Definition Rule** for C.
- Header has only `extern int g_count;`; otherwise every includer creates its own definition → linker error.

### Approach 2 — Declaring Functions
Functions are **`extern` by default**.
```c
// math_utils.h
int  gcd(int a, int b);       // declaration; `extern` implicit
// math_utils.c
int  gcd(int a, int b) { ... } // definition
```
Writing `extern` on function declarations is allowed but redundant.

### Approach 3 — `extern` Inside a Function (Local Declaration)
```c
void f(void) {
    extern int g_count;       // refers to the file-scope g_count
    g_count++;
}
```
Lets you reference a global without including its header — rarely useful, mostly a code smell.

### Approach 4 — `extern "C"` (C++ → Calling C)
C++ mangles function names to encode argument types (overloading support). C does not. To call C from C++, suppress mangling:
```cpp
// public C API header consumed by both languages
#ifdef __cplusplus
extern "C" {
#endif

int  c_function(int x);
void c_init(void);

#ifdef __cplusplus
}
#endif
```
- Applies to **declarations** of functions and variables.
- Works on function pointers too: `extern "C" int (*cb)(int);`
- Cannot overload `extern "C"` functions (no mangling = unique name).

### Approach 5 — `extern` with `const`
- In **C**: file-scope `const` has **external** linkage by default. `static const` for internal.
- In **C++**: file-scope `const` has **internal** linkage by default. Use `extern const` for external sharing.

## Comparison Table
| Construct | Purpose | Where allowed |
|---|---|---|
| `extern int x;` | declare variable defined elsewhere | header, file scope, inside function |
| `extern int x = 5;` | **definition** (initialiser overrides `extern`) | file scope |
| `extern int f(void);` | declare function (redundant) | any |
| `extern "C" { ... }` (C++) | disable C++ name mangling for declarations inside | C++ only |
| `extern inline` | C99 — provide external symbol for inline functions | rare; prefer `static inline` |

## Key Insight
- `extern` separates **declaration** ("a symbol with this name exists somewhere") from **definition** ("this is the actual storage / code").
- Headers should contain only declarations; definitions live in exactly one `.c`.
- `extern "C"` lives in the C++ language, not C; its purpose is **linkage compatibility**, not language switching.

## Pitfalls
- Putting `int g = 0;` in a header → linker error "multiple definition" once two `.c` files include it
- Forgetting `extern "C"` in a C header consumed by C++ → linker "undefined reference" with mangled symbol names
- `extern "C"` doesn't change C++ semantics inside the function body; it only disables mangling for the declared symbol
- Initialising in the `extern` declaration turns it into a definition (`extern int x = 0;` is a definition)
- Mixing `static` and `extern` in a header for the same name — contradictory
- Function pointers across the boundary: `typedef extern "C" int (*cb_t)(int);` is needed; pointer types remember their linkage
- Header that's both C and C++ → guard with `#ifdef __cplusplus`

## Interview Tips
1. State the "one definition, many declarations" rule.
2. Show the `#ifdef __cplusplus extern "C" { ... } #endif` block from memory.
3. Mention C++ const = internal vs C const = external — easy follow-up.
4. Recommend `static inline` in headers (see [11_inline_vs_macros](../11_inline_vs_macros/)) over `extern inline` to avoid C99 inline rules.

## Related / Follow-ups
- [12_static_keyword](../12_static_keyword/), [02_storage_classes](../02_storage_classes/)
- [14_include_guards](../14_include_guards/)
- C++ name mangling (Itanium ABI)
- `nm`/`objdump` to verify what symbols a `.o` exports
- Symbol visibility (`__attribute__((visibility("hidden")))`) for shared libraries

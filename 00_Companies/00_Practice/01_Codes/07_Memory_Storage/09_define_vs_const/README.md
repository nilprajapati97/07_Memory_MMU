# 09 — `#define` vs `const`

## Problem
Compare textual `#define` constants with `const` variables in C; when to prefer each.

## Why It Matters
`#define` is a **preprocessor token replacement** — no type, no scope, no symbol. `const` produces a **typed, scoped, linkable object**. Choosing wrong leads to obscure errors, broken debuggers, and runtime memory you didn't expect.

## The Four Axes (approaches)

### Axis 1 — Type Safety
```c
#define PI 3.14159        // no type — context decides; risky in printf
const  double PI = 3.14159;  // typed, compiler enforces
```
- Macros are subject to integer promotion / floating defaults at each use.
- `const` carries the type; mismatches caught at compile time.

### Axis 2 — Scope & Lifetime
- `#define` has **file scope from the directive to `#undef` or EOF**; not block-scoped.
- `const` follows normal scoping: block, file, or extern.
```c
void f(void) {
    const int LIMIT = 10;   // visible only inside f
}
```

### Axis 3 — Symbol Table & Debuggability
- `const` appears in the symbol table → debugger can print its name; sanitizers/coverage understand it.
- `#define` is gone by the time the compiler sees code → gdb can't show "MAX_FOO".

### Axis 4 — Memory & Linkage
- File-scope `const int K = 5;` is a real object in `.rodata` (unless optimised out).
- Same as `extern const int K;` for sharing across TUs (need definition in one).
- `#define` produces no object — no address, can't take `&`.

## Comparison Table
| Property | `#define` | `const` |
|---|---|---|
| Phase | preprocessor | compiler |
| Type | none (text) | typed |
| Scope | from-directive | normal C scoping |
| Address | n/a (`&MACRO` → error) | yes (`&K`) |
| Visible in debugger | no | yes |
| Function-like form | yes (`#define MAX(a,b) ...`) | use inline function |
| Array dimension (C) | yes | **no** (file-scope const isn't an ICE in C) |
| Array dimension (C++) | yes | yes |
| `switch case` label | yes | **no** in C (needs ICE) |
| Compile-time evaluation | always | usually (since C23 / via `enum` workaround) |

### Cases Only `#define` Can Handle in C
- Conditional compilation: `#ifdef DEBUG`
- Token concatenation / stringification: `#x`, `a##b`
- Array dimension at file scope: `int buf[BUF_SZ];` (file-scope `const int` is not an integer constant expression in C)
- `case BUF_SZ:` in a `switch`

### Cases Only `const` Can Handle
- Scoped constant (block / function)
- Typed constant the debugger can show
- Pointer-to-`const` to expose immutable view of mutable data

### The Middle Ground — `enum`
```c
enum { BUF_SZ = 256 };
int buf[BUF_SZ];      // legal — enum constants ARE integer constant expressions
```
- Typed (`int`), file/block-scoped, debugger-visible, usable in array dims & case labels.
- The common C trick to get the best of both.

## Key Insight
`#define` is text; `const` is a typed object. Use `const` whenever the constant has a value and a type; use `#define` when you need preprocessor features (conditional compilation, stringification, macros, file-scope array size in C). For "integer constant of a useful size", `enum` beats both.

## Pitfalls
- `#define SQUARE(x) x*x` then `SQUARE(1+2)` → `1+2*1+2` = 5 (parenthesise!)
- `#define KB 1024` then `printf("%d", KB)` works; with `const long KB = 1024;` printf format must match (`%ld`)
- `const int N = 10; int a[N];` is illegal in C at file scope (but legal in C++); use `enum` or `#define`
- File-scope `const` has external linkage by default in C (use `static const` to keep private). In C++ file-scope `const` has internal linkage.
- Macros pollute the global namespace → use `ALL_CAPS_PREFIX_`
- A "magic literal" inside one file → prefer `static const` (typed, scoped, debuggable)

## Interview Tips
1. Lead with "preprocessor vs compiler" — that's the root distinction.
2. Volunteer the `enum` middle-ground for integer constants.
3. Cite a macro-paren bug to show pragmatism.
4. For multi-file headers, mention `extern const` declaration + single definition pattern.

## Related / Follow-ups
- [10_typedef_vs_define](../10_typedef_vs_define/)
- [11_inline_vs_macros](../11_inline_vs_macros/)
- `constexpr` in C++ / C23
- ICE (integer constant expression) definition in the C standard

# 11 — Inline Functions vs Macros

## Problem
When should code be a function-like macro (`#define MAX(a,b) ...`) and when an `inline` function? What does `static inline` mean exactly?

## Why It Matters
Macros gain speed by avoiding call overhead but pay in type safety and debuggability. `inline` lets the compiler obtain the same speed without the macro pitfalls — but only if you understand C's inline linkage rules, which trip up most candidates.

## The Approaches

### Approach 1 — Function-Like Macro
```c
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
```
- Pure text substitution; no symbol, no type, no debugger entry.
- **No call overhead** by construction.
- **Side-effect bug**: `MAX(i++, j)` evaluates `i++` twice.
- Works on any type.

### Approach 2 — `inline` Function (C99)
```c
inline int max_int(int a, int b) { return a > b ? a : b; }
```
- Compiler **may** inline; behaviour controlled by optimisation level.
- Type-checked, single argument evaluation, debugger-visible.
- Per-type — one per type needed (or use `_Generic` / C++ templates).
- **Linkage** is subtle (see Approach 4).

### Approach 3 — `static inline` (The Common Header Pattern)
```c
static inline int max_int(int a, int b) { return a > b ? a : b; }
```
- **Internal linkage** — each TU gets a private copy, no linker conflicts.
- **Always safe to put in a header**.
- If not inlined, you pay duplicate code size (one copy per TU). Usually negligible.
- This is the **default modern C idiom** for header-only helpers.

### Approach 4 — `inline` without `static` — The Trap
C99/C11 rules:
- An `inline` (non-static) declaration is an **inline definition** — usable for inlining but **does not** create an external symbol.
- You must provide **exactly one** non-`inline` external definition somewhere (often via `extern inline` or omitting the keyword in one TU).
- Without that, calls that aren't inlined → linker error "undefined reference".
- This rule trips up almost everyone — that's why `static inline` is preferred.

### Approach 5 — Force-Inline / No-Inline Attributes
```c
__attribute__((always_inline)) static inline int hot(void);
__attribute__((noinline))      void cold(void);
```
For performance-critical or debugging cases.

## Comparison Table
| Property | Macro | `static inline` | `inline` (non-static) |
|---|---|---|---|
| Type-safe | no | yes | yes |
| Single-eval args | no | yes | yes |
| Debuggable | no | yes | yes |
| In headers | yes | yes | header + one TU definition |
| Recursion | usually no | yes | yes |
| Side-effect-safe | only if written carefully | yes | yes |
| Speed (when inlined) | full | full (if inlined) | full (if inlined) |
| Generic over types | yes | per-type | per-type |

## Key Insight
Macros buy speed with **textual substitution** and pay with safety. `inline` buys speed with **compiler choice** and keeps safety, at the cost of linkage rules. Use **`static inline` in headers** as the default; use macros only when you genuinely need preprocessor features (generic types, token pasting, conditional compilation, removing arguments entirely).

## Pitfalls — Macros
- **Side effects**: `MAX(a++, b)` evaluates `a++` twice
- **Parenthesisation**: `#define SQ(x) x*x` then `SQ(1+2)` → 5
- **Argument count**: `#define LOG(fmt, args) printf(fmt, args)` blows up on multi-arg — use `...` and `__VA_ARGS__` (C99)
- **Operator-precedence**: always wrap whole body and each argument
- **Multiple statements**: use `do { ... } while(0)` idiom so semicolons behave
```c
#define SWAP(a, b, T) do { T _t = (a); (a) = (b); (b) = _t; } while (0)
```

## Pitfalls — Inline
- Defining a non-static `inline` in a header without one external definition → link error
- Recursive inline functions are allowed (the recursive call won't actually be inlined)
- Taking the address of an inline function forces emission of an actual symbol
- `inline` is a **hint**; compiler may ignore it. `always_inline` forces it (GCC/Clang).
- C99 `inline` semantics differ from C++ — porting code between languages requires care
- Pre-C99 (`-std=c89`) has no `inline` keyword — use GCC's `__inline__` or `static`

## Interview Tips
1. Lead with the side-effect example (`MAX(i++, j)`). Universally understood.
2. State the **`static inline` in headers** default and explain why (no linkage rule pitfall).
3. Mention `do { ... } while (0)` for multi-statement macros — a standard pattern interviewers expect.
4. Discuss the C99 inline-definition rule when "what does `inline` actually do?" comes up.

## Related / Follow-ups
- [09_define_vs_const](../09_define_vs_const/), [10_typedef_vs_define](../10_typedef_vs_define/)
- `_Generic` (C11) — type-generic functions without macros
- Variadic macros and `__VA_ARGS__`
- `__attribute__((always_inline))`, `__attribute__((noinline))`
- Linux kernel `min`/`max` macros — type-safe (use `typeof`) to avoid the side-effect trap

# 05 — `sizeof` Operator

## Problem
Predict the result of `sizeof` for every kind of operand: built-in types, structs (with padding), arrays, pointers, expressions.

## Why It Matters
`sizeof` is **compile-time** (except for VLAs) and returns `size_t`. It is the bedrock of safe allocation (`malloc(n * sizeof *p)`) and a perpetual source of subtle bugs (array vs pointer decay, signed/unsigned comparisons).

## The Six Operand Categories

### 1 — Primitive Types (Implementation-Defined Minimums)
| Type | C standard min | Typical x86-64 Linux |
|---|---|---|
| `char` | 1 | 1 |
| `short` | 2 | 2 |
| `int` | 2 | 4 |
| `long` | 4 | 8 |
| `long long` | 8 | 8 |
| `float` | (n/a) | 4 |
| `double` | | 8 |
| `long double` | | 16 (x87 80-bit padded) |
| pointer | — | 8 (LP64), 4 (ILP32) |

Always use `sizeof(type)` rather than hard-coded constants for portability.

### 2 — Pointers — Always Same Size on a Given Target
```c
sizeof(int*)   == sizeof(char*) == sizeof(void(*)(void))   // all equal
```
The size depends on the **address width** of the target, not the pointee.
- Exception: on some segmented or capability architectures (CHERI), pointers carry bounds → 16 bytes.

### 3 — Structs (with Padding — see [06_struct_padding](../06_struct_padding/))
```c
struct A { char c; int i; };
// sizeof(A) == 8 on x86-64: 1 + 3 pad + 4
```
`sizeof(struct)` = sum of member sizes + internal padding + trailing padding (so arrays of the struct stay aligned).

### 4 — Arrays — Total Bytes, Not Element Count
```c
int a[10];
sizeof(a)            // 40  (10 * sizeof(int))
sizeof(a) / sizeof(a[0])   // 10  — element count idiom
```
**Decay trap**: when passed to a function, an array becomes a pointer.
```c
void f(int a[10]) { sizeof(a); }  // 8 (pointer!), NOT 40
```
The `[10]` in the parameter is documentation; the parameter type is `int *`.

### 5 — VLA (Variable Length Array, C99)
`sizeof` becomes **run-time** evaluation when applied to a VLA:
```c
void f(int n) { int v[n]; size_t s = sizeof(v); }  // run-time n*sizeof(int)
```
Side effects in `sizeof` operand are **not** evaluated for non-VLA, but **are** for VLA.

### 6 — Expressions — Operand Not Evaluated (Non-VLA)
```c
int i = 0;
size_t s = sizeof(i++);   // s = sizeof(int); i is still 0
```
The compiler only inspects the type; the expression isn't executed. Useful for type queries without runtime cost.

## Comparison Table — Result of `sizeof(...)` on x86-64 Linux
| Operand | Result | Notes |
|---|---|---|
| `int` | 4 | |
| `int *` | 8 | pointer = address width |
| `char[20]` | 20 | full array |
| `char *` (string literal arg) | 8 | decayed |
| `"hello"` (literal expression) | 6 | array `char[6]`, includes `\0` |
| `struct{char;int;}` | 8 | padding |
| `(int)3.14` | 4 | type of cast expression |
| `'A'` (char constant in C) | 4 | C: `int`; **C++**: 1 (`char`) |
| `0` | 4 | int literal |
| `(size_t)0` | 8 | depends on size_t width |

## Key Insight
- `sizeof` is a **compile-time operator** returning `size_t`, **except for VLAs**.
- The operand's **declared type** is what matters — and arrays decay to pointers across function boundaries.

## Pitfalls
- `sizeof(arr) / sizeof(arr[0])` inside a function that received `arr` as a parameter → divides pointer-size by element-size (wrong)
- Mixing signed `int` with `sizeof` (which is unsigned `size_t`) → unwanted promotion in comparisons
- `sizeof(char) == 1` always (by definition); writing `sizeof(char)` is redundant but harmless
- `sizeof` does **not** count `\0` when applied to `strlen`-style **lengths**; it **does** count it when applied to a string literal
- `sizeof(*p)` is safe even if `p` is `NULL` (operand not evaluated) — useful idiom: `p = malloc(n * sizeof *p)`
- Bitfields and `sizeof` on them is **not allowed**

## Interview Tips
1. Whiteboard the array-decay trap — it catches >50% of candidates.
2. State "`sizeof` is compile-time" before any answer; mention the VLA exception.
3. Show the canonical safe-alloc idiom: `T *p = malloc(n * sizeof *p);` — survives if you change `T` later.
4. For char-constant question, distinguish C (`int`) from C++ (`char`).

## Related / Follow-ups
- [06_struct_padding](../06_struct_padding/) — predicting struct size
- `offsetof` macro — implemented via pointer arithmetic, ties to sizeof
- `_Alignof` / `_Alignas` (C11) — alignment counterparts
- Flexible array members — `sizeof` does not include the trailing array

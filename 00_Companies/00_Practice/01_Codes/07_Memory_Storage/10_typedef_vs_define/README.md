# 10 — `typedef` vs `#define`

## Problem
Compare creating a type alias with `typedef` to creating a textual alias with `#define`.

```c
typedef unsigned int  uint;
#define UINT          unsigned int

uint  a, b;   // both are unsigned int  ✓
UINT  c, d;   // both are unsigned int  ✓
```

## Why It Matters
Looks identical on simple cases — diverges sharply with pointers, function pointers, and composite types. Wrong choice produces silent type bugs that propagate across the codebase.

## The Four Approaches

### Approach 1 — `typedef` (Type Alias, Compiler-Level)
- A real declaration; participates in name lookup, scoping, debug info.
- Block- or file-scoped.
- Knows about the full type structure (handles pointer/array/function rules correctly).
```c
typedef int* IntPtr;
IntPtr a, b;            // a and b are BOTH int*  ✓
```

### Approach 2 — `#define` (Token Substitution, Preprocessor-Level)
- Pure text replacement before the compiler runs.
- File-scope from `#define` to `#undef`.
```c
#define INT_PTR int*
INT_PTR a, b;           // expands to: int* a, b;
                        // a is int*, b is plain int  ✗
```
The classic pointer-typedef pitfall.

### Approach 3 — `typedef` for Function Pointers
Without typedef, function-pointer syntax is awful. typedef tames it:
```c
typedef int (*Comparator)(const void*, const void*);
Comparator cmp = my_compare;
qsort(a, n, sizeof *a, cmp);

// without typedef:
int (*cmp2)(const void*, const void*) = my_compare;
```

### Approach 4 — `typedef` for Opaque Types (Information Hiding)
In headers, expose a typedef without the struct definition:
```c
// list.h
typedef struct list_s List;          // opaque
List *list_create(void);
void  list_destroy(List *);

// list.c
struct list_s { /* private members */ };
```
Callers use `List *` without knowing the layout — the standard C technique for ADTs.

## Comparison Table
| Property | `typedef` | `#define` |
|---|---|---|
| Phase | compile | preprocess |
| Scope | normal C scope | file (from directive) |
| Type-aware | yes | no — text only |
| Pointer alias of multiple vars | safe | broken (`#define IPTR int*` trap) |
| Function-pointer alias | clean | awkward, error-prone |
| Debug-symbol presence | yes | no |
| Composite types (structs/arrays) | yes | only as text |
| Conditional compilation | no | yes |

## Key Insight
`typedef` creates a **type** that the compiler treats as a first-class entity (it can be pointed to, used in `sizeof`, expressed in error messages). `#define` is a **textual macro** that the compiler never sees in its original form — making it both more powerful (any token sequence) and more dangerous (no semantic awareness).

## Pitfalls
- `#define PTR int*; PTR a, b;` → only `a` is a pointer; `b` is `int`
- `typedef int Arr[10]; Arr x;` declares `x` as `int[10]` — but `Arr* p` is `int (*)[10]`, not `int**`. Mind it.
- `typedef` inside a function is legal, but rarely useful
- Pre-existing typedefs in system headers (`size_t`, `pid_t`) — don't redefine
- Naming convention: standard practice is `_t` suffix (`int32_t`, `pthread_mutex_t`)
- Mixing `typedef struct X X;` and `struct X` references — they refer to the same type; pick one style and stay consistent

## Interview Tips
1. Run the "two pointers in one declaration" example; it nails the difference.
2. For function pointers, show both forms — typedef wins on readability.
3. Mention opaque types as the canonical C-ADT pattern.
4. State the `_t` naming convention (interviewers expect it).

## Related / Follow-ups
- [09_define_vs_const](../09_define_vs_const/)
- Forward declarations (`struct Foo;`)
- `typeof` (GCC extension) → infer existing type
- Strong typedef workarounds (single-member struct wrap)

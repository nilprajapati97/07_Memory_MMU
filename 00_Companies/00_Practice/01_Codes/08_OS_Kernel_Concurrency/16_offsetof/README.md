# 16 — `offsetof` Macro

## Problem
Given a struct type and one of its member names, return the byte offset of that member from the start of the struct.

```c
#include <stddef.h>
size_t off = offsetof(struct work, node);   // e.g. 8
```

## Why It Matters
Building block for `container_of`, serialisation, hardware register layouts, and any reflective access to struct fields in C. The standard provides it because the obvious hand-rolled form is technically undefined behaviour.

## The Approaches

### Approach 1 — Standard `offsetof` from `<stddef.h>`
```c
#include <stddef.h>
size_t o = offsetof(type, member);
```
- Defined by ISO C; the **only** sanctioned way.
- Implementation may use compiler builtin (`__builtin_offsetof`) — always portable & correct, even with multiple inheritance / weird C++ contexts.

### Approach 2 — Classic Hand-Rolled Form
```c
#define offsetof(T, m)  ((size_t)&(((T *)0)->m))
```
Logic: pretend address 0 is a `T*`; take address of member `m`; the value is its offset.
- **Technically UB** under strict ISO C: it forms a member access on a null pointer.
- Compiles and gives the right answer on every real compiler.
- Old code (pre-C89 hosts without `<stddef.h>`) used it; modern code should not.

### Approach 3 — Computing It At Runtime (Don't)
```c
T tmp;
size_t o = (char *)&tmp.m - (char *)&tmp;
```
- Costs a real local; not a constant expression — can't be used as array dim, case label, `_Static_assert`.

### Approach 4 — Builtin
```c
size_t o = __builtin_offsetof(T, m);   // GCC/Clang
```
What `<stddef.h>` expands to on these compilers.

## What You Get From It
- `offsetof` is a **constant expression** (of type `size_t`) → usable in `_Static_assert`, case labels, global initialisers, array sizes (C90 only — VLA in C99).
- Accounts for **padding** the compiler inserted.
- Cannot be applied to a **bit field** (offsets to bit fields are not byte-aligned and not defined by standard).

## Comparison
| Form | Standard | Constant expr | Caveats |
|---|---|---|---|
| `offsetof` (stddef) | yes | yes | use this |
| `&((T*)0)->m` | UB strictly; works | yes | legacy only |
| Runtime subtract | yes | **no** | wastes a local |
| `__builtin_offsetof` | GCC/Clang | yes | what stddef expands to |

## Key Insight
- `offsetof` exists because the obvious manual form (`&((T*)0)->m`) is **not** strictly conforming C — yet it has always been needed. The standard responded by adopting the macro and granting implementations leeway to make it work (often via a compiler builtin).
- Member offsets are **fixed at compile time** — a constant property of the type.
- Pair with `container_of` to walk between a member pointer and its parent struct.

## Pitfalls
- `offsetof(T, bit_field_member)` → UB / not supported
- Using on non-POD types in C++ (constructors, virtuals) → UB or compiler diagnostic
- Forgetting `#include <stddef.h>` → undefined macro
- Computing offsets of nested members: `offsetof(T, outer.inner)` is allowed in C99+ and `__builtin_offsetof`
- Some compilers fail in C90 mode on nested member designators — stick to single-level for max portability
- Don't conflate with `sizeof` — `offsetof(T, last) + sizeof(last)` may be **less than** `sizeof(T)` because of trailing padding
- Negative offsets (some weird unions / packed structures) are not possible — `size_t` is unsigned
- Wire/file formats: pair `offsetof` with `_Static_assert(sizeof(T) == N, "layout drift")` to catch padding regressions

## Common Uses
1. **`container_of`** — recover containing struct from member pointer.
2. **Serialisation tables**:
```c
struct field { const char *name; size_t off; size_t size; };
struct field fields[] = {
    { "x", offsetof(point, x), sizeof(((point*)0)->x) },
    { "y", offsetof(point, y), sizeof(((point*)0)->y) },
};
```
3. **Padding audit**:
```c
_Static_assert(offsetof(packet, payload) == 8, "header changed!");
```
4. **Hardware register definitions**:
```c
#define REG_STATUS_OFFSET  offsetof(struct dev_regs, status)
```

## Interview Tips
1. State that `offsetof` is in `<stddef.h>` and is a constant expression.
2. Show the legacy `&((T*)0)->m` form, note it's strictly UB but historically how it worked.
3. Mention the bit-field restriction.
4. Pair with `container_of` if asked "given a member pointer, get the parent" — same family.

## Related / Follow-ups
- [15_container_of](../15_container_of/)
- [06_struct_padding](../../07_Memory_Storage/06_struct_padding/)
- [17_kernel_linked_list](../17_kernel_linked_list/)
- `_Static_assert` for layout invariants
- Compiler builtins (`__builtin_offsetof`)

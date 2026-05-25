# 08 — Union vs Struct

## Problem
Explain how `union` differs from `struct`, list legitimate use cases, and beware the type-punning rules.

## Why It Matters
Unions are the C tool for **type punning**, **tagged variants**, and saving memory in mutually-exclusive states. Misusing them runs afoul of strict aliasing and implementation-defined behaviour.

## The Four Use Cases (treat as approaches)

### Use 1 — Memory Sharing for Mutually Exclusive Data
All members start at offset 0 and share storage. `sizeof(union)` = max member size + padding for alignment.
```c
union Sample {
    int     i;
    float   f;
    char    bytes[4];
};
union Sample s;
s.i = 0x40490FDB;     // approx π in IEEE-754
printf("%f\n", s.f);  // type-pun — see caveats
```
- **Only the last-written member is well-defined** to read in standard C *unless* you use the special exceptions below.

### Use 2 — Endianness Checker
```c
int is_little_endian(void) {
    union { uint32_t u; uint8_t b[4]; } x = { .u = 1 };
    return x.b[0] == 1;
}
```
Reading bytes via a union is the **most portable** way to inspect byte order; strict aliasing is not violated because `char`/`unsigned char` may alias anything.

### Use 3 — Tagged Union (Sum Type / Variant)
Pair the union with a discriminator tag in an outer struct:
```c
typedef struct {
    enum { INT_T, FLOAT_T, STR_T } tag;
    union {
        int    i;
        float  f;
        char  *s;
    } u;
} Value;
```
The canonical safe pattern. The tag tells you which arm to read.

### Use 4 — Hardware Register Overlay
Map a peripheral register both as a 32-bit word and as a bit-field struct (often via anonymous union):
```c
typedef union {
    uint32_t raw;
    struct {
        uint32_t enable : 1;
        uint32_t mode   : 3;
        uint32_t        : 4;
        uint32_t count  : 24;
    } b;
} CTRL_REG;
```
Lets firmware code switch between bulk word writes and named-field reads.

## Comparison Table — Struct vs Union
| Property | `struct` | `union` |
|---|---|---|
| Member offsets | sequential, padded | all start at 0 |
| `sizeof` | sum + padding | max + padding |
| Members live simultaneously | yes | **no** (one at a time) |
| Initialisation | `{a, b, c}` | `{ .name = value }` (designated) |
| Typical use | aggregate state | variant / overlay |

## Key Insight
A union does **not** convert data between member types. It only **reinterprets** the same bytes through a different type lens. In standard C, reading a member you didn't write is **implementation-defined** (subject to strict aliasing) — GCC explicitly documents that union-based type-punning is allowed, but generic standard C does not guarantee it.

## Pitfalls
- Reading a union member after writing a different one → impl-defined / UB in some interpretations. GCC and Clang document it as well-defined; MSVC similarly. Standard says "unspecified value".
- `memcpy` is the **portable, standard-blessed** type-pun: `memcpy(&f, &i, sizeof f);`
- Strict-aliasing: casting `int*` to `float*` to dereference is UB. Unions are typically allowed; `memcpy` always is.
- Padding inside a union — non-active members may contain undefined bytes
- Cannot `memcmp` two unions meaningfully if different members were last written
- Active member is **not** tracked by the compiler — you must store the tag yourself
- Anonymous unions inside structs (C11) flatten naming: `s.enable` rather than `s.b.enable`

## Interview Tips
1. State the offset rule first: "all members share offset 0".
2. Show the tagged-union pattern unprompted — that's how unions are used safely.
3. Volunteer the strict-aliasing caveat; mention `memcpy` as the always-safe alternative.
4. For endianness checks, explain why `char[]` lens is OK (the standard exception).

## Related / Follow-ups
- Strict aliasing rule — [08_strict_aliasing (Embedded Gotchas)](../../11_Embedded_Gotchas/08_strict_aliasing/)
- `memcpy` as the portable type-pun
- C11 anonymous structs/unions
- "Pascal variant record" — historical predecessor of tagged unions
- Rust enums — modern strongly-typed tagged unions for comparison

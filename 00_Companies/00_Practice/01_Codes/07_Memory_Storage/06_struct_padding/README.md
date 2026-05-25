# 06 — Struct Padding & Packing

## Problem
Predict the size and layout of a struct, and control it via `#pragma pack`, `__attribute__((packed))`, or member reordering.

## Why It Matters
Determines on-wire/on-disk formats (network packets, MMIO register banks, file headers), DMA descriptor layouts, and cache footprint of hot data. Wrong padding ⇒ corrupted parsing, mis-aligned access faults on ARM, performance loss.

## The Rules

### Rule 1 — Each Member Sits at a Multiple of Its Alignment
- `char` (align 1), `short` (2), `int`/`float` (4), `long`/`double`/pointer (8 on 64-bit).
- Compiler inserts **internal padding** to satisfy this.

### Rule 2 — Struct's Own Alignment = Largest Member's Alignment
So an array of structs keeps every member aligned.

### Rule 3 — Trailing Padding Makes `sizeof(struct)` a Multiple of Its Alignment

## Approaches to Layout Control

### Approach 1 — Default (Natural) Alignment
```c
struct A { char c; int i; short s; };
```
Layout on x86-64:
```
offset  0  1  2  3  4  5  6  7  8  9  10 11
        c  P  P  P  i  i  i  i  s  s  P  P
            ^^^^^^^             ^^^^^
       3 bytes padding      2 bytes trailing
sizeof(A) = 12, alignof(A) = 4
```

### Approach 2 — Reorder Members Largest → Smallest
```c
struct B { int i; short s; char c; };
//        0  1  2  3  4  5  6  7
//        i  i  i  i  s  s  c  P
//                              ^ 1 trailing
sizeof(B) = 8 (saved 4 bytes)
```
**Best zero-cost optimisation.** Always try reorder before reaching for `packed`.

### Approach 3 — `#pragma pack(n)` (MSVC + GCC)
Sets max alignment for **subsequent** structs to `n`. Always save/restore with push/pop.
```c
#pragma pack(push, 1)
struct Wire { uint8_t op; uint32_t addr; uint16_t len; };  // size = 7
#pragma pack(pop)
```

### Approach 4 — `__attribute__((packed))` (GCC/Clang per-struct)
Same effect, per-declaration, more reliable than pragma in mixed compilers.
```c
struct __attribute__((packed)) Hdr {
    uint8_t  ver;
    uint32_t magic;
    uint16_t flags;
};  // size = 7
```

### Approach 5 — Per-Member Alignment with `_Alignas` (C11) / `__attribute__((aligned(N)))`
```c
struct C {
    char buf[100];
    _Alignas(64) int hot;   // force 64-byte alignment (cache line)
};
```
Useful to put hot fields on their own cache line (false-sharing avoidance).

### Approach 6 — Hand-Pack via Bit Fields
For protocol headers where every bit counts. See [07_bit_fields](../07_bit_fields/).

## Comparison Table
| Method | Portable | Per-struct | Effect | Caveats |
|---|---|---|---|---|
| Reorder | yes | yes | shrinks via alignment | first thing to try |
| `#pragma pack` | mostly | scope-based | force align 1..n | save/restore push/pop |
| `__attribute__((packed))` | GCC/Clang | yes | force align 1 | may produce slow unaligned access |
| `_Alignas` | C11 | per-member | force ≥ N | platform-limited max |
| Manual layout | yes | yes | precise | tedious, error-prone |

## ASCII: Cache-Line View
```
| 64-byte cache line                                              |
| hot_var | _ | _ | _ | _ | _ | _ | other unrelated members... |
```
A `_Alignas(64)` puts `hot_var` at line start; surrounding pad prevents false sharing between cores.

## Key Insight
Padding is **alignment-driven**, not arbitrary. Predict size by walking members and rounding each offset up to its alignment. Trailing padding rounds total size up to the struct's own alignment.

## Pitfalls
- **Packed struct + unaligned access on ARM/MIPS** → bus fault. Compiler may emit byte-by-byte access (slow) or raise.
- Taking address of a packed member → pointer doesn't satisfy alignment → UB if dereferenced as the full type. GCC warns.
- Different compilers / pragmas → on-wire layout silently changes. Always assert `_Static_assert(sizeof(struct X) == 7, ...)` for protocol structs.
- Endianness — packing doesn't fix byte order; use `htons`/`ntohl` or explicit shifts.
- Bit-field order within a byte is implementation-defined → not portable across compilers.
- Forgetting trailing padding when memcmp'ing structs → compares uninitialised padding → false negatives. Use `memset` or per-field compare.

## Interview Tips
1. Predict size step-by-step on the whiteboard; mark padding.
2. Show the **reorder-saves-bytes** insight first; `packed` only when reorder isn't enough.
3. Mention the **ARM unaligned-access** trap when discussing `packed`.
4. Cite `_Static_assert(sizeof(...) == expected, "...")` as the production safeguard for on-wire structs.

## Related / Follow-ups
- [05_sizeof_operator](../05_sizeof_operator/)
- [07_bit_fields](../07_bit_fields/)
- `offsetof` macro — find each member's actual offset
- False sharing in concurrent code (cache-line padding)
- Network protocol parsing — packed vs `memcpy`-and-byteswap pattern

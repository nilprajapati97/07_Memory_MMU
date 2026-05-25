# 07 — Bit Fields in Structures

## Problem
Define struct members that occupy a specified number of bits; understand the portability traps.

```c
struct Flags {
    unsigned int ready    : 1;
    unsigned int error    : 1;
    unsigned int priority : 3;
    unsigned int          : 0;   // align to next storage unit
    unsigned int seq      : 8;
};
```

## Why It Matters
Compact representation for hardware registers, flag sets, protocol headers. But bit-field layout is **implementation-defined** in three ways (bit order, allocation order, straddling) — so they appear less in portable protocol code than you'd expect; `bit-mask + shift` macros are usually safer.

## The Two Approaches

### Approach 1 — Bit Fields (Syntax)
- Use **unsigned** integer types (signed is allowed but sign of bit-field is implementation-defined).
- Width `0` forces the next field to start at a new allocation unit.
- Cannot take address of a bit field (`&flags.ready` → error).
- Cannot have an array of bit fields.

Layout intent for the example above on a typical little-endian x86 GCC:
```
byte 0    [ priority(3) | error | ready ]   (bits 7..3 unused or next field)
byte 1    [ seq(8) ]
```
But layout is **not portable** — see Pitfalls.

### Approach 2 — Bit-Mask Macros (Portable Alternative)
```c
typedef uint16_t flags_t;
#define F_READY      (1u << 0)
#define F_ERROR      (1u << 1)
#define F_PRIO_MASK  (0x7u << 2)
#define F_PRIO_SHIFT 2
#define F_SEQ_MASK   (0xFFu << 8)
#define F_SEQ_SHIFT  8

f |= F_READY;
prio = (f & F_PRIO_MASK) >> F_PRIO_SHIFT;
```
- Fully portable, byte-order-independent.
- Verbose but explicit; the embedded-industry default.

### Approach 3 — Anonymous Union of Bit Field + Whole Word (Inspection)
```c
typedef union {
    uint32_t raw;
    struct {
        uint32_t ready : 1;
        uint32_t error : 1;
        uint32_t prio  : 3;
        uint32_t pad   : 27;
    } b;
} ctrl_t;
```
Lets debugger / wire format see the raw word while code uses named fields. Still subject to layout caveats.

### Approach 4 — Packed Struct + Bit Field
Combining `__attribute__((packed))` with bit fields disables some alignment but **not** the layout-implementation-defined issues.

## Comparison Table
| Method | Portable | Readability | Speed | Recommended |
|---|---|---|---|---|
| Bit fields | **no** (impl-defined) | high | depends; sometimes byte ops | intra-compiler use only |
| Bit-mask macros | **yes** | medium | fast (shift+and) | production, networking |
| Union of bits + raw | partly | high | fast | debugging registers |
| Packed + bit fields | no | high | risk of slow unaligned | embedded MMIO only |

## Key Insight
Bit fields look nice but the standard leaves three things implementation-defined:
1. **Bit allocation direction** (LSB-first vs MSB-first within a storage unit).
2. Whether bit fields can **straddle storage units**.
3. Size and alignment of the underlying storage unit.

This means a struct that "works" on GCC/x86 may serialise differently on MSVC/ARM.

## Pitfalls
- Sending bit-field structs over the network → other endpoint sees different layout
- Endianness flips bit order in many compilers — `ready` may be bit 0 *or* bit 7
- Using `signed int : 1` — value is `-1` or `0`, not `0` or `1` (signed two's-complement of 1 bit)
- Cannot use `sizeof(field)` or `&field`
- Atomic update of a multi-bit-field write is **not** guaranteed — adjacent bit fields may share the storage unit; one writer's read-modify-write can clobber another's bits (memory model gotcha)
- `:0` (zero-width) requires no name and forces alignment to the next unit

## Interview Tips
1. Always say "implementation-defined" and list the three things. That alone earns the point.
2. State your default for production: bit-mask macros + explicit shifts. Bit fields only inside a single TU on a single compiler.
3. Mention atomicity: multi-bit-field updates aren't atomic at the storage-unit level, which surprises people in lock-free code.
4. Bonus: with C11 atomics, neighbouring bit fields share a memory location → atomic on one races the other; the standard added rules but they're easy to violate.

## Related / Follow-ups
- [06_struct_padding](../06_struct_padding/)
- Atomic types and "memory location" definition in C11
- MMIO register descriptions via header generators (CMSIS device headers use bit fields heavily, intentionally compiler-specific)
- Flexible array members vs bit fields — different problems

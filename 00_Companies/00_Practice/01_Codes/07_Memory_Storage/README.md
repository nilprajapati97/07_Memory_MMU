# 07 — Memory, Storage Classes & Compilation

This section is the C-language "systems literacy" battery. It covers where data lives (segments, stack, heap), how the compiler names and links it (storage classes, `static`, `extern`), what the optimiser is allowed to assume (`volatile`, `const`), and how to lay out aggregate types for hardware and protocols (padding, packing, bit fields, unions). Every embedded / kernel / driver interview leans heavily here.

## Topics
| # | Topic | Theme |
|---|---|---|
| 01 | [Stack vs Heap](01_stack_vs_heap/) | Lifetime, speed, failure modes |
| 02 | [Storage Classes](02_storage_classes/) | `auto`, `register`, `static`, `extern`, `typedef` |
| 03 | [`volatile` Keyword](03_volatile_keyword/) | MMIO, ISRs, setjmp, threads |
| 04 | [Memory Layout](04_memory_layout/) | `.text`, `.data`, `.bss`, heap, stack |
| 05 | [`sizeof` Operator](05_sizeof_operator/) | Compile-time vs VLA, array decay |
| 06 | [Struct Padding & Packing](06_struct_padding/) | Alignment, `#pragma pack`, `__attribute__((packed))` |
| 07 | [Bit Fields](07_bit_fields/) | Impl-defined layout, atomicity pitfalls |
| 08 | [Union vs Struct](08_union_vs_struct/) | Tagged variants, type punning, endian check |
| 09 | [`#define` vs `const`](09_define_vs_const/) | Preprocessor vs typed object, `enum` middle ground |
| 10 | [`typedef` vs `#define`](10_typedef_vs_define/) | Type alias vs text alias, opaque types |
| 11 | [Inline vs Macros](11_inline_vs_macros/) | Type safety, `static inline` header pattern |
| 12 | [`static` Keyword](12_static_keyword/) | Block-static vs file-static linkage |
| 13 | [`extern` Keyword](13_extern_keyword/) | One-definition rule, `extern "C"` |
| 14 | [Include Guards vs `#pragma once`](14_include_guards/) | Re-inclusion prevention |

## Suggested Learning Order
1. **Foundation**: 04 → 01 → 05 (where things live, then sizes)
2. **Naming & linking**: 02 → 12 → 13 → 14 (storage classes ↔ linker behaviour)
3. **Compiler hints**: 03 → 11 → 09 → 10 (`volatile`, inline, `const`, `typedef`)
4. **Aggregate layout**: 06 → 07 → 08 (padding → bit fields → unions)

## Cross-Section Prerequisites
- **02_Pointers** before this section (every topic involves addresses and dereference)
- **11_Embedded_Gotchas** is a natural sequel (declarations, strict aliasing, UB)
- **08_OS_Kernel_Concurrency** topics 09 (atomic counter), 10 (memory barriers), 11 (malloc impl) extend the memory model

## Cheat-Sheet — Pick the Right Tool
| Need | Tool |
|---|---|
| Persistent per-function counter | `static` local |
| Module-private global | `static` at file scope |
| Cross-file shared symbol | `extern` decl + single definition |
| Constant integer for array dim (C) | `enum` or `#define` |
| Typed scoped constant | `const` |
| Header-only inline helper | `static inline` |
| MMIO register | `volatile T*` cast |
| Cross-thread flag | `_Atomic`, NOT `volatile` |
| Wire/disk format | reorder + `_Static_assert(sizeof == N)` |
| C↔C++ interop header | `extern "C"` guard with `#ifdef __cplusplus` |
| Header guard | `#pragma once` (or include guard if portability extreme) |

## Diagnostic Toolbox
- `size`, `nm`, `objdump -h`, `readelf -l` — segments and symbols
- `/proc/<pid>/maps`, `pmap` — runtime memory map
- `valgrind`, `AddressSanitizer`, `LeakSanitizer` — heap errors
- `-fstack-protector`, `-D_FORTIFY_SOURCE=2` — stack hardening
- `_Static_assert(sizeof(T) == N, "layout drift")` — catch padding regressions

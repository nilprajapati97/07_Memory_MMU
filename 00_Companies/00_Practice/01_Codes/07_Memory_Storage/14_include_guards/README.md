# 14 — Include Guards vs `#pragma once`

## Problem
Prevent a header from being included more than once per translation unit. Compare the classic `#ifndef`/`#define`/`#endif` idiom with `#pragma once`.

```c
// classic
#ifndef MY_HEADER_H
#define MY_HEADER_H
/* declarations */
#endif

// pragma
#pragma once
/* declarations */
```

## Why It Matters
Double-inclusion causes "redefinition" errors, struct redeclaration, and at best wastes compile time. Picking the wrong guard style hurts portability or build performance.

## The Approaches

### Approach 1 — Include Guards (`#ifndef`/`#define`/`#endif`)
```c
#ifndef PROJECT_MODULE_NAME_H
#define PROJECT_MODULE_NAME_H

/* declarations */

#endif /* PROJECT_MODULE_NAME_H */
```
- **Portable** — works on every C/C++ compiler ever.
- **Manual** — requires a unique macro per header.
- Cost: compiler reads file, sees the guard, may still open/parse the file once per inclusion (modern compilers cache this; old ones did not).

### Approach 2 — `#pragma once`
```c
#pragma once
/* declarations */
```
- Supported by GCC, Clang, MSVC, IAR, ARMCC — practically universal.
- Compiler uses **file identity** (inode / path canonicalisation) — guaranteed unique even if two files share the same guard macro.
- Faster — compiler can skip opening the file on subsequent inclusions.
- **Not in the C/C++ standard** (de facto only).
- Can mis-behave when:
  - The same file is reachable via multiple paths (symlinks, network mounts with different canonicalisation)
  - Build systems copy headers between locations

### Approach 3 — Belt-and-Braces (Both)
```c
#pragma once
#ifndef MY_HEADER_H
#define MY_HEADER_H
/* declarations */
#endif
```
- Pays a tiny double-cost; gains both speed (where `#pragma once` is honoured) and the portable fallback.
- Common in cross-compiler libraries.

### Approach 4 — Compiler-Generated (No Guard at All)
Some build systems force compilers to assume any header without a guard is safe (`-Wno-pragma-once-outside-header`). **Not recommended** — fragile.

## Comparison Table
| Property | Include guard | `#pragma once` |
|---|---|---|
| Standardised | yes | no (de facto everywhere) |
| Unique-name burden | yes (must invent) | no (file identity) |
| Symlink/path duplicates | tolerated (same macro) | may double-include |
| Compile speed | compiler can optimise | skipped re-open |
| Conflict if two files share name | collision possible | none |
| Verbosity | 3 lines | 1 line |

## Key Insight
- Include guards rely on a **macro name**; `#pragma once` relies on **file identity**.
- The "best practice" is project-wide consistency — pick one, document it, lint for it.

## Pitfalls
- **Guard collision**: two unrelated headers both define `UTILS_H` → second one is silently skipped, declarations missing, mysterious errors. Use long namespaced names like `PROJECT_MODULE_SUBMODULE_H`.
- Typos in the guard macro (`#endif` matching wrong `#ifndef`) → unbounded inclusion or vanishing declarations.
- `#pragma once` on a header that's intentionally included multiple times (X-macros, configuration include tricks) → breaks the technique. Use plain `#include` for those, no guard.
- Different compilers canonicalise paths differently → `#pragma once` may miss a duplicate.
- Build systems that copy headers (header amalgamation) can create paths that confuse `#pragma once`.
- Header that includes itself recursively (rare bug) — guards stop infinite recursion; without them, preprocessor recursion limit is hit.

## Best Practices
1. **In most projects today**: use `#pragma once`. It's universally supported by the compilers you'll meet, and avoids macro-name collisions.
2. **For maximum portability** (writing a public library aimed at exotic toolchains): use include guards with a long, namespaced macro: `MYLIB_MODULE_HEADER_H_INCLUDED`.
3. **Either way**: keep the rule consistent and enforce it with clang-tidy or a linter.
4. Never put non-idempotent statements (executable code via `__attribute__((constructor))` etc.) in a header without considering re-inclusion.

## Interview Tips
1. Show both forms; state that `#pragma once` is the practical default but non-standard.
2. Cite macro-collision as the killer drawback of include guards if names aren't namespaced.
3. Mention X-macros as the case where you intentionally *don't* want a guard.
4. Mention precompiled headers / module systems (C++20 modules, Clang's modules) as the future direction.

## Related / Follow-ups
- [13_extern_keyword](../13_extern_keyword/) — one-definition rule
- X-macros — intentional multi-include trick
- Precompiled headers (`gch`, `pch`)
- C++20 modules
- Header-only libraries — include-guard discipline matters more

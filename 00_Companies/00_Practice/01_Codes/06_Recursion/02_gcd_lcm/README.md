# 02 — GCD / LCM (Euclidean Algorithm)

## Problem
Compute `gcd(a, b)` (greatest common divisor) and `lcm(a, b) = a*b / gcd(a,b)` for non-negative integers.

```
gcd(48, 18) = 6
lcm(4, 6)   = 12
```

## Why It Matters
Underpins reducing fractions, modular arithmetic (extended Euclidean for inverses), CRT, cryptography. GCD itself is one of the oldest algorithms still in active use.

## Approaches — GCD

### Approach 1 — Subtraction (Original Euclid)
```text
gcd(a, b):
    while a != b:
        if a > b: a -= b
        else:     b -= a
    return a
```
- Time: **O(max(a,b))** worst case — slow when one is much larger
- Historically interesting; never use in code

### Approach 2 — Recursive Modulo (Best Default)
```text
gcd(a, b) = a if b == 0 else gcd(b, a mod b)
```
- Time: **O(log min(a,b))** — Fibonacci-like worst case
- Tail-recursive; compilers usually emit a loop

### Approach 3 — Iterative Modulo
```text
while b != 0:
    a, b = b, a mod b
return a
```
- Same complexity; safer on stack-limited targets

### Approach 4 — Binary GCD (Stein's)
Replaces division with shifts & subtracts — wins on hardware without fast division (some MCUs).
```text
binary_gcd(a, b):
    if a == 0: return b
    if b == 0: return a
    shift = ctz(a | b)             // common power of 2
    a >>= ctz(a)
    do:
        b >>= ctz(b)
        if a > b: swap(a, b)
        b -= a
    while b != 0
    return a << shift
```
- Time: **O(log² min(a,b))** word-ops; faster wall-time on division-poor cores
- Uses count-trailing-zeros builtin

### Approach 5 — Extended Euclidean
Returns `(g, x, y)` with `a·x + b·y = g`. Foundation for modular inverse.
```text
ext_gcd(a, b):
    if b == 0: return (a, 1, 0)
    (g, x1, y1) = ext_gcd(b, a mod b)
    return (g, y1, x1 - (a / b) * y1)
```

## Approaches — LCM
```text
lcm(a, b) = a / gcd(a,b) * b      // divide first to avoid overflow
```
Always divide before multiplying to keep intermediate values small.

## Comparison
| Approach | Time | Space | Notes |
|---|---|---|---|
| Subtraction | O(max) | 1 | Teaching only |
| **Modulo (rec/iter)** | log min | 1 | **Default** |
| Binary GCD | log² | 1 | No-division hardware |
| Extended | log min | log min | For inverses, CRT |

## Key Insight
`gcd(a, b) = gcd(b, a mod b)` because any common divisor of `a, b` also divides `a − k·b` for any integer `k`, and `a mod b = a − ⌊a/b⌋·b`.

## Pitfalls
- `a mod b` when `b == 0` — handle the base case **first**
- Negative inputs — use `abs()` or define behaviour
- `lcm(a,b) = a*b/gcd(a,b)` may overflow; do `a / gcd * b`
- `lcm(0, x) = 0` (not undefined) — be explicit
- Extended Euclidean's `x, y` can be negative — that's correct, don't `abs` them

## Interview Tips
1. State the recurrence in one line; write 3-line iterative version.
2. Mention `O(log min(a,b))` — interviewer wants you to recognise it's not O(n).
3. If they probe further: extended Euclidean → modular inverse → "this is how RSA computes the private key".

## Related / Follow-ups
- Modular inverse `a^-1 mod m` (extended Euclidean)
- Chinese Remainder Theorem
- gcd of an array — fold `gcd` over elements
- Reduce a fraction to lowest terms

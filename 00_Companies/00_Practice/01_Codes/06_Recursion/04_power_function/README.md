# 04 — Power Function `x^n` in O(log n)

## Problem
Compute `x^n` where `x` is real (or modular int) and `n` is an integer (may be negative or zero).

```
pow(2, 10) = 1024
pow(3, 0)  = 1
pow(2, -3) = 0.125
```

## Why It Matters
Tests recognition of "halve the work" structure. Used in modular exponentiation (RSA, Diffie–Hellman), matrix powers (Fibonacci), polynomial evaluation.

## Approaches

### Approach 1 — Naive Multiplication
```text
r = 1
for _ in 1..n: r *= x
```
- Time: **O(n)**, Space: **O(1)**

### Approach 2 — Recursive Fast Power (Best Recursive)
```text
pow(x, n):
    if n == 0: return 1
    if n < 0:  return 1 / pow(x, -n)
    h = pow(x, n/2)
    if n is even: return h * h
    else:         return h * h * x
```
- Time: **O(log n)**, Space: **O(log n)** stack
- Compute `pow(x, n/2)` **once**, square it

### Approach 3 — Iterative Fast Power (Best Default)
```text
r = 1; base = x; e = abs(n)
while e > 0:
    if e & 1: r *= base
    base *= base
    e >>= 1
return n >= 0 ? r : 1/r
```
ASCII trace for `pow(3, 13)` (13 = 0b1101):
```
e=13(1101) bit1 → r=3        base=9
e=6 (0110) bit0 → r=3        base=81
e=3 (0011) bit1 → r=243      base=6561
e=1 (0001) bit1 → r=1594323  base=...
e=0 stop → 1594323 = 3^13 ✓
```
- Time: **O(log n)**, Space: **O(1)**

### Approach 4 — Modular Exponentiation
Same as Approach 3 but take `mod m` after every multiply. Foundation of RSA / DH.
```text
mod_pow(x, n, m):
    r = 1; base = x mod m
    while n > 0:
        if n & 1: r = (r * base) mod m
        base = (base * base) mod m
        n >>= 1
    return r
```
- Watch for `base*base` overflow → use 64-bit or `__int128`

### Approach 5 — Matrix Power
Same fast-power skeleton on 2×2 (or larger) matrices to compute linear recurrences in O(log n) — see Fibonacci.

## Comparison
| Approach | Time | Space | When to use |
|---|---|---|---|
| Naive | n | 1 | Tiny n |
| **Recursive fast** | log n | log n | Teaching |
| **Iterative fast** | log n | 1 | **Default** |
| Modular | log n | 1 | Cryptography |
| Matrix | log n · k³ | log n | Linear recurrences |

## Key Insight
- `x^n = (x^(n/2))² · x^(n mod 2)`.
- Each iteration squares the base and consumes one bit of the exponent → O(log n).

## Pitfalls
- `pow(x, n/2) * pow(x, n/2)` recomputes the same call — must store in a variable
- Negative `n`: handle separately; `1 / pow(x, -n)`. Beware `n == INT_MIN` (negating overflows) → cast to wider type
- `n == 0`: return 1 (even for `x == 0`, by convention)
- Floating-point: repeated squaring accumulates rounding; acceptable for most uses
- Modular: forgetting parentheses → operator-precedence bug

## Interview Tips
1. Lead with iterative fast power — show you understand bit decomposition of `n`.
2. Mention overflow-safe modular variant if they bring up RSA, hashes, or "large numbers".
3. Follow-up favourite: implement `pow(x, n)` with `n` as `INT_MIN` — corner case.

## Related / Follow-ups
- Fibonacci via matrix expo (see [01_factorial_fibonacci](../01_factorial_fibonacci/))
- Modular inverse via Fermat's little theorem (`a^(p-2) mod p`)
- Polynomial evaluation (Horner's method)
- Square root without `sqrt()` (see Number/Math section)

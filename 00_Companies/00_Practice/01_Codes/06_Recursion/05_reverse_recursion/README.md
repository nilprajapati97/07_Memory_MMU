# 05 — Reverse a String / Number Using Recursion

## Problem
Reverse a C string (in place) or an integer (digit-reverse) using recursion.

```
reverse_str("hello") -> "olleh"
reverse_int(12345)   -> 54321
```

## Why It Matters
Probes understanding of recursion termination, in-place mutation, and tail vs head recursion. Integer variant adds overflow awareness.

## Approaches — String

### S-1: Head–Tail Swap Recursion
```text
reverse(s, l, r):
    if l >= r: return
    swap(s[l], s[r])
    reverse(s, l+1, r-1)
```
- Time: **O(n)**, Space: **O(n)** stack

### S-2: Tail Recursion + Helper
Identical to S-1 with accumulator-style indices; modern compilers TCO it.

### S-3: Recursion + Build New String
```text
reverse(s):
    if len(s) <= 1: return s
    return reverse(s[1:]) + s[0]
```
- Time: **O(n²)** because of repeated concat
- Anti-pattern in C (no string concatenation operator); teaching only

### S-4: Stack Simulation
Push chars one-by-one, pop into output buffer. Iterative — included for contrast.
- Time: **O(n)**, Space: **O(n)** heap

### S-5: Two-Pointer Iterative
The non-recursive baseline — preferred in production (no stack risk).

## Approaches — Integer

### I-1: Recursive with Place Multiplier
```text
reverse(n, rev=0):
    if n == 0: return rev
    return reverse(n / 10, rev * 10 + n % 10)
```
- Time: **O(log₁₀ n)**, Space: **O(log₁₀ n)** stack
- Tail-recursive

### I-2: Recursive Building From the Top
```text
reverse(n):
    if n < 10: return n
    digits = log10(n) + 1
    return (n % 10) * 10^(digits-1) + reverse(n / 10)
```
- Recomputes `digits` each call → O(log² n); pass it as parameter to fix

### I-3: Iterative (Baseline)
```text
rev = 0
while n > 0: rev = rev*10 + n%10; n /= 10
```

### I-4: Reverse with Overflow Guard
```text
INT_MAX = 2147483647
while n != 0:
    d = n % 10
    if rev >  INT_MAX/10 or (rev == INT_MAX/10 and d > 7): return 0
    if rev < -INT_MAX/10-1 ... similarly
    rev = rev*10 + d
    n /= 10
```
- The LeetCode-7 canonical safe version

## Comparison
| Variant | Time | Space | Notes |
|---|---|---|---|
| Str head-tail | n | n stack | Classic recursion demo |
| Str build new | n² | n | Anti-pattern in C |
| **Str iterative two-ptr** | n | 1 | **Production** |
| Int tail recursive | log n | log n | Clean recursion form |
| **Int iterative + overflow** | log n | 1 | **Default for code** |

## Key Insight
- **String reverse**: swap symmetric positions `[l, r]` and recurse on `[l+1, r-1]`. Base case `l >= r` handles both even and odd lengths.
- **Integer reverse**: peel digits from the low end with `% 10`, shift the accumulator up with `* 10`. Reversal is digit-by-digit construction, not recursion on the high end.

## Pitfalls
- Recursion depth = string length → stack overflow on 1 MB strings (embedded targets often have 4–8 KB stacks)
- String terminator `\0` — don't swap it; use `strlen(s) - 1` as `r`
- UTF-8: reversing bytes corrupts multi-byte glyphs — reverse code-points instead
- Integer: negative numbers (`% 10` returns negative in C99+), `INT_MIN` (cannot be negated), trailing zeros (`100` → `001` → `1`)
- Leading zeros in output are lost (intentional — `1000` reversed is `1`)

## Interview Tips
1. Ask: in-place? null-terminated? signed int? overflow handling required?
2. For strings, **show recursive then immediately offer iterative** ("recursion uses O(n) stack which is unsafe for big inputs").
3. For ints, write the overflow-safe version — interviewers grade on it.

## Related / Follow-ups
- Palindrome check (recursive or two-pointer)
- Reverse words in a sentence (in-place: reverse all, then each word)
- Reverse linked list (pointer rewire, not swap)
- [Bit Manipulation 07_reverse_bits](../../01_Bit_Manipulation/07_reverse_bits/)

# 01 — Factorial & Fibonacci (with & without Memoization)

## Problem
- **Factorial:** `n! = 1·2·3·…·n`, with `0! = 1`.
- **Fibonacci:** `F(0)=0, F(1)=1, F(n)=F(n-1)+F(n-2)`.

```
fact(5) = 120
fib(10) = 55
```

## Why It Matters
The canonical recursion teaching pair. Factorial showcases linear recursion + overflow. Fibonacci showcases **overlapping subproblems** — the gateway from naive recursion to DP, memoization, tabulation, and matrix exponentiation.

## Approaches — Factorial

### F-1: Recursive
```text
fact(n) = 1 if n <= 1 else n * fact(n-1)
```
- Time: **O(n)**, Space: **O(n)** stack

### F-2: Iterative
```text
r = 1
for i in 2..n: r *= i
```
- Time: **O(n)**, Space: **O(1)** — always preferred for plain factorial

### F-3: Tail-Recursive
```text
fact(n, acc=1) = acc if n <= 1 else fact(n-1, acc*n)
```
- Same complexity; compiler may TCO (GCC `-O2` often does). C standard does not guarantee TCO.

### F-4: Big-Integer
For `n > 20`, `64-bit` overflows. Use arbitrary-precision (array of digits, GMP, etc.).

## Approaches — Fibonacci

### B-1: Naive Recursion (Exponential — Anti-pattern)
```text
fib(n) = n if n < 2 else fib(n-1) + fib(n-2)
```
Recursion tree for `fib(5)`:
```
                  fib(5)
              /          \
          fib(4)         fib(3)
         /     \         /    \
      fib(3) fib(2)  fib(2) fib(1)
       ...     ...    ...
```
- Time: **O(φⁿ) ≈ O(1.618ⁿ)**, Space: **O(n)** stack
- Shows the *overlap*: `fib(3)` computed twice, `fib(2)` thrice — only used to motivate memoization

### B-2: Top-Down Memoization
```text
memo = {}
fib(n):
    if n in memo: return memo[n]
    if n < 2: return n
    memo[n] = fib(n-1) + fib(n-2)
    return memo[n]
```
- Time: **O(n)**, Space: **O(n)** (table + stack)

### B-3: Bottom-Up Tabulation
```text
dp[0]=0; dp[1]=1
for i in 2..n: dp[i] = dp[i-1] + dp[i-2]
```
- Time: **O(n)**, Space: **O(n)**

### B-4: Iterative, Two Variables (Best Default)
```text
a, b = 0, 1
for _ in 1..n: a, b = b, a+b
return a
```
- Time: **O(n)**, Space: **O(1)** — production answer

### B-5: Matrix Exponentiation
`[[1,1],[1,0]]^n` has `F(n+1)` and `F(n)` in its first row. Use fast power.
- Time: **O(log n)**, Space: **O(log n)** for recursion (or O(1) iterative)

### B-6: Fast Doubling
```text
F(2k)   = F(k) * (2*F(k+1) - F(k))
F(2k+1) = F(k+1)² + F(k)²
```
- Time: **O(log n)**, Space: **O(log n)** stack — simplest log-n method

## Comparison
| Method | Time | Space | When to use |
|---|---|---|---|
| Naive recursion (fib) | φⁿ | n | Teaching only |
| Memoization | n | n | When recursive form is natural |
| Tabulation | n | n | DP exposition |
| **Two-variable iterative** | n | 1 | **Default** |
| Matrix expo / fast doubling | log n | log n | Huge `n` (e.g. modulo prime) |

## Key Insight
The naive Fibonacci recursion **recomputes** the same subproblem exponentially many times. Caching turns it linear; replacing the cache with two scalars makes it constant-space. For sub-linear, exploit the algebraic identity (matrix or doubling).

## Pitfalls
- `int` overflow: `F(47) > INT_MAX`; `13! > INT_MAX`; `20! > INT64_MAX`
- Stack overflow on naive recursion for big `n` (and definitely on embedded targets)
- Negative `n` — define behaviour (raise, return 0)
- Forgetting `0! = 1` and `F(0) = 0`
- Memoization with mutable global table → not reentrant

## Interview Tips
1. Start by saying "naive Fibonacci is exponential — let me show why" → draw the tree → "memoize" → "actually I only need the last two values".
2. For factorial, mention overflow at `n=13` (32-bit) and `n=21` (64-bit) before writing code.
3. If asked for `F(10^18) mod p` → matrix expo or fast doubling.

## Related / Follow-ups
- Climbing stairs / Tribonacci — same DP shape
- Count number of ways to tile 2×n board
- nCr with memoization (Pascal's triangle)
- Generic DP: "overlapping subproblems + optimal substructure"

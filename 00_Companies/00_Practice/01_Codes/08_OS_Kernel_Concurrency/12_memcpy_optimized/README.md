# 12 — Optimised `memcpy`

## Problem
Copy `n` bytes from `src` to `dst`. Beat naive byte-by-byte loops by leveraging alignment, wide loads/stores, unrolling, and hardware DMA where available.

## Why It Matters
`memcpy` is the second-most-called function after `malloc` in many systems. A 2× speedup translates directly to throughput. Tests pointer arithmetic, alignment math, and pipeline/SIMD awareness.

## Approaches

### Approach 1 — Naive Byte Copy
```text
for i in 0..n-1: dst[i] = src[i]
```
- Always correct (any alignment, any size).
- ~1 byte/cycle on modern CPUs — 8× to 64× slower than wide stores.

### Approach 2 — Word-At-A-Time (Aligned)
When both pointers share alignment, copy in `size_t`-sized chunks (4 or 8 bytes); handle leading/trailing tail bytes.
```text
copy leading bytes until both aligned (or n exhausted)
loop while n >= sizeof(word):
    *(word*)dst = *(word*)src
    dst += W; src += W; n -= W
copy trailing bytes
```
- ~8× speedup on 64-bit machines.
- If alignments differ, use `memmove`-style fixup or just stick to bytes.

### Approach 3 — Loop Unrolling
Copy 4 or 8 words per iteration to reduce loop overhead and let the CPU pipeline parallelism shine.
```text
while n >= 32:
    w0 = src[0]; w1 = src[1]; w2 = src[2]; w3 = src[3]
    dst[0]=w0; dst[1]=w1; dst[2]=w2; dst[3]=w3
    dst += 32; src += 32; n -= 32
```
- 1.5–2× over plain word copy on out-of-order CPUs.

### Approach 4 — SIMD (Vectorised) Copy
Use 16/32/64-byte vector registers.
- x86: SSE (`movdqa`/`movdqu`), AVX (`vmovdqa`), AVX-512.
- ARM: NEON (`vld1q`/`vst1q`).
- Non-temporal stores (`movntdq`) bypass cache for huge copies (>L2).
- Library `memcpy` does this for you; modern glibc picks an implementation at runtime via IFUNC.

### Approach 5 — `rep movsb` (x86)
Surprisingly competitive on recent Intel/AMD due to **ERMSB** (Enhanced REP MOVSB). For medium-sized copies it's the fastest single-instruction path.
```text
asm("rep movsb" : "+D"(dst), "+S"(src), "+c"(n));
```

### Approach 6 — DMA Engine (Embedded / GPU)
Hand the copy to a DMA controller; CPU is free during transfer.
- Worth it only for **large** transfers — DMA setup has per-call overhead.
- Cache coherence: software must flush/invalidate caches around the DMA region on platforms without coherent DMA.

### Approach 7 — Handle Overlap with `memmove`
`memcpy` is UB on overlapping regions. `memmove` checks direction and copies backward when needed.

## Comparison
| Approach | Bytes/cycle | Code complexity | Caveats |
|---|---|---|---|
| Byte copy | ~1 | trivial | works for any align |
| Word copy | ~8 (64-bit) | low | needs alignment match |
| Unrolled word | ~12 | medium | branch overhead reduced |
| SIMD (AVX2) | ~32 | high (intrinsics) | runtime CPU detection |
| `rep movsb` (ERMSB) | comparable to SIMD | one line | x86-specific |
| DMA | ≈ memory BW | very high (setup) | best for KB+ transfers |

## Key Insight
- The bottleneck is **memory bandwidth**, not instructions. Once you saturate the bus, more cleverness is moot.
- For small copies (< a few hundred bytes), inlined unrolled word copies beat library `memcpy` (fewer branches, no call overhead).
- For huge copies (> L2/L3), non-temporal stores or DMA win by not polluting the cache.
- libc `memcpy` is one of the most heavily-tuned functions in existence — beat it only with specific knowledge of the size distribution and platform.

## Pitfalls
- **Aliasing**: `memcpy(p, p+1, n)` overlaps → UB; use `memmove`
- Unaligned access faults on ARMv5, some MMU-disabled embedded modes — copy bytes until aligned
- Reading/writing past the buffer to "round up to word boundary" → buffer overflow
- Non-temporal stores must be followed by `sfence` to make them visible
- DMA without cache flush on non-coherent hardware → consumer sees stale data
- Calling `memcpy(NULL, NULL, 0)` is UB in C (passing NULL to memcpy is UB even when n==0) — guard at boundaries
- `strcpy` is not a faster `memcpy` — it scans for `\0`; don't confuse

## Interview Tips
1. Start with the byte copy, then propose word-at-a-time + alignment fixup.
2. Mention that real `memcpy` uses SIMD and CPU dispatch.
3. Volunteer the overlap rule (`memcpy` vs `memmove`).
4. For embedded: bring up DMA and cache coherence.
5. Mention that **measured** speedup matters — micro-benchmark cache effects.

## Related / Follow-ups
- [06_struct_padding](../../07_Memory_Storage/06_struct_padding/), [04_memory_layout](../../07_Memory_Storage/04_memory_layout/)
- glibc `memcpy` IFUNC dispatch
- Agner Fog's optimisation manuals
- Linux DMA-API; ARM cache maintenance (`dsb`, `dc cvac`)
- `__builtin_memcpy` (compiler intrinsic — often inlined to optimal sequence)

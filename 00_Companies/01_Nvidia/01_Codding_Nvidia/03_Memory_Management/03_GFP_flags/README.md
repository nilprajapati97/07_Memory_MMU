# Q21: Explain GFP flags: GFP_KERNEL, GFP_ATOMIC

## In-depth Explanation (Nvidia Interview Style)

- **GFP_KERNEL:** Default flag for normal kernel allocations; can sleep.
- **GFP_ATOMIC:** Used in atomic context; cannot sleep.

### Interview Tips
- Know which contexts require GFP_ATOMIC.
- Be ready to discuss memory allocation failures and fallbacks.

# Memory Functions Implementation

## Functions Covered
- `memcpy` - Copy memory (no overlap)
- `memmove` - Copy memory (handles overlap)
- `memset` - Fill memory with byte
- `memcmp` - Compare memory

## Approaches

### memcpy
1. **Byte-by-byte** - Simple, portable
2. **Word-wise** - Optimized, aligned access

### memmove
1. **Direction check** - Forward/backward based on overlap

### Key Differences
- **memcpy**: Faster, no overlap handling
- **memmove**: Slower, handles overlap

## Interview Tips
- Always ask about overlap
- Mention alignment for optimization
- Discuss undefined behavior
- Know when to use which function

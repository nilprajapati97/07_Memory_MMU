# Custom Memory Allocator

## Three Approaches

### Approach 1: Simple Free List Allocator
**Concept**: Maintain a linked list of free blocks

**Pros**:
- Simple to implement
- Good for variable-sized allocations
- Low overhead

**Cons**:
- Fragmentation
- Linear search for free blocks
- No coalescing

**Use Case**: General purpose, learning

### Approach 2: Buddy Allocator
**Concept**: Split blocks into power-of-2 sizes

**Pros**:
- Fast allocation/deallocation
- Easy coalescing
- Predictable behavior

**Cons**:
- Internal fragmentation
- Limited to power-of-2 sizes
- More complex

**Use Case**: Kernel memory management, embedded systems

### Approach 3: Pool Allocator
**Concept**: Pre-allocate fixed-size blocks

**Pros**:
- O(1) allocation/deallocation
- No fragmentation
- Cache-friendly
- Predictable performance

**Cons**:
- Fixed block size
- Wasted space if blocks too large
- Need multiple pools for different sizes

**Use Case**: Object pools, game engines, real-time systems

## Comparison Table

| Feature | Simple | Buddy | Pool |
|---------|--------|-------|------|
| Allocation | O(n) | O(log n) | O(1) |
| Deallocation | O(1) | O(log n) | O(1) |
| Fragmentation | High | Medium | None |
| Overhead | Low | Medium | Low |
| Complexity | Low | High | Low |

## Key Concepts

### 1. Block Header
```c
typedef struct block {
    size_t size;
    int is_free;
    struct block *next;
} block_t;
```

### 2. Free List
- Linked list of available blocks
- First-fit, best-fit, or worst-fit strategies

### 3. Coalescing
- Merge adjacent free blocks
- Reduces fragmentation

### 4. Alignment
```c
size = (size + 7) & ~7;  // Align to 8 bytes
```

## Interview Tips

1. **Explain trade-offs**: Each approach has pros/cons
2. **Mention fragmentation**: Internal vs external
3. **Discuss alignment**: Why it matters
4. **Know use cases**: When to use which allocator
5. **Understand overhead**: Metadata per block

## Common Questions

**Q: Why not just use malloc?**
A: Custom allocators provide:
- Better performance for specific patterns
- Predictable behavior
- Reduced fragmentation
- Memory pool management

**Q: How to prevent fragmentation?**
A: 
- Use buddy system
- Coalesce free blocks
- Use pool allocators for fixed sizes
- Segregated free lists

**Q: What is alignment?**
A: Memory addresses aligned to word boundaries for:
- Performance (CPU access)
- Hardware requirements
- Atomic operations

## Real-World Examples

### Linux Kernel
- Buddy allocator for pages
- Slab allocator for objects
- SLUB allocator (improved slab)

### Game Engines
- Pool allocators for game objects
- Stack allocators for per-frame data
- Custom allocators for specific subsystems

### Embedded Systems
- Fixed-size pools
- Static allocation
- Predictable behavior critical

## Implementation Checklist

- [ ] Block metadata structure
- [ ] Free list management
- [ ] Allocation strategy (first-fit, best-fit)
- [ ] Deallocation and coalescing
- [ ] Alignment handling
- [ ] Error handling (out of memory)
- [ ] Debugging support (leak detection)
- [ ] Thread safety (if needed)

## Advanced Features

### 1. Coalescing
```c
void coalesce(block_t *block) {
    if (block->next && block->next->is_free) {
        block->size += block->next->size + sizeof(block_t);
        block->next = block->next->next;
    }
}
```

### 2. Best-Fit Search
```c
block_t *find_best_fit(size_t size) {
    block_t *best = NULL;
    block_t *curr = free_list;
    
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            if (!best || curr->size < best->size) {
                best = curr;
            }
        }
        curr = curr->next;
    }
    
    return best;
}
```

### 3. Memory Pool with Multiple Sizes
```c
typedef struct {
    pool_t small;   // 32 bytes
    pool_t medium;  // 128 bytes
    pool_t large;   // 512 bytes
} multi_pool_t;
```

## Testing Strategies

1. **Allocation patterns**: Sequential, random, mixed
2. **Stress testing**: Allocate until exhausted
3. **Fragmentation**: Allocate/free patterns
4. **Memory leaks**: Track all allocations
5. **Performance**: Measure allocation time

## Further Reading

- "The Art of Computer Programming" - Knuth
- Linux kernel memory management
- "Game Engine Architecture" - Gregory
- "Operating Systems: Three Easy Pieces"

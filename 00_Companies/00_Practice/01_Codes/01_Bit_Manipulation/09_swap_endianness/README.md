# Swap Endianness (htonl-like)

## Approaches

### Approach 1: Bit Shifting (Manual)
- Time: O(1), Space: O(1)
- Portable
- Interview answer

### Approach 2: Union (Byte Access)
- Time: O(1), Space: O(1)
- More readable
- Explicit byte manipulation

### Approach 3: GCC Builtin (Best)
- Time: O(1) - single instruction
- Uses BSWAP instruction
- Production choice

## Network Byte Order
- `htonl()` - host to network long
- `ntohl()` - network to host long
- Network = Big Endian
- Most systems = Little Endian

# Platform Compatibility - ARM32, ARM64, and x86

This document explains how the custom `malloc` implementation works on ARM32, ARM64, and x86 platforms, with a deep dive into platform-specific considerations.

## Platform-Specific Considerations

### 1. Memory Alignment
- **ARM32/ARM64**: Requires memory alignment to 4 or 8 bytes for optimal performance.
- **x86**: Alignment is less strict but aligning to 8 bytes improves performance.
- **Implementation**: The `mmap` system call ensures proper alignment, and the `BLOCK_SIZE` is designed to be a multiple of the alignment requirement.

### 2. System Calls
- **ARM32/ARM64**: Uses the same `mmap` system call as x86, as it is part of the POSIX standard.
- **x86**: Fully compatible with `mmap`.
- **Implementation**: The code is platform-independent as it relies on POSIX-compliant system calls.

### 3. Pointer Size
- **ARM32**: Pointers are 4 bytes.
- **ARM64/x86-64**: Pointers are 8 bytes.
- **Implementation**: The `size_t` type is used for sizes, ensuring compatibility across platforms.

### 4. Endianness
- **ARM**: Can be little-endian or big-endian.
- **x86**: Always little-endian.
- **Implementation**: Endianness does not affect this implementation as it does not rely on byte-level operations.

## Testing on Different Platforms
- **ARM32/ARM64**: Use QEMU or a physical device to test the implementation.
- **x86**: Test on a standard Linux system.

## Performance Considerations
- **ARM**: Optimized for low-power devices.
- **x86**: Optimized for high-performance systems.
- **Implementation**: The linked list and `mmap` usage ensure efficient memory management on all platforms.

## Conclusion
This implementation is designed to be portable and efficient, making it suitable for a wide range of platforms, including ARM32, ARM64, and x86.

# ***Bit Manipulation:  "Write a function to count the number of set bits in a 64-bit integer efficiently."***

***Given a memory address, check if it is aligned to a 4KB page boundary.***


# Bit Manipulation Utilities

This directory contains C code for two common bit manipulation tasks:

## 1. Counting Set Bits in a 64-bit Integer

**Function:**

```c
unsigned int count_set_bits(uint64_t n);
```

**How it works:**
- Uses Brian Kernighan’s algorithm: repeatedly clears the least significant set bit until the number becomes zero.
- Each iteration counts one set bit, so the loop runs only as many times as there are set bits (very efficient).

**Example:**
```c
uint64_t val = 0xF0F0F0F0F0F0F0F0ULL;
unsigned int count = count_set_bits(val); // count = 32
```

## 2. Checking 4KB Page Alignment

**Function:**

```c
int is_aligned_4kb(void *addr);
```

**How it works:**
- A 4KB page boundary means the address is a multiple of 4096 (0x1000).
- Checks if the lower 12 bits are zero: `(uintptr_t)addr & 0xFFF == 0`.

**Example:**
```c
void *addr = (void*)0x1000;
int aligned = is_aligned_4kb(addr); // aligned = 1 (true)
```

---

See `bit_manipulation.c` for code and demo usage.
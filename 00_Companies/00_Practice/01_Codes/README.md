# C Programming Interview Preparation - Complete Guide

## 📚 Topics Overview

| # | Topic | Questions | Priority | Status |
|---|-------|-----------|----------|--------|
| 1 | [Bit Manipulation](01_Bit_Manipulation/) | 15 | ⭐⭐⭐ | ✅ COMPLETE |
| 2 | [Pointers](02_Pointers/) | 12 | ⭐⭐⭐ | 🚧 In Progress |
| 3 | [Linked Lists](03_Linked_Lists/) | 15 | ⭐⭐⭐ | 📋 Pending |
| 4 | [Strings](04_Strings/) | 10 | ⭐⭐⭐ | 📋 Pending |
| 5 | [Arrays](05_Arrays/) | 14 | ⭐⭐⭐ | 📋 Pending |
| 6 | [Recursion](06_Recursion/) | 7 | ⭐⭐ | 📋 Pending |
| 7 | [Memory & Storage](07_Memory_Storage/) | 14 | ⭐⭐ | 📋 Pending |
| 8 | [OS/Kernel/Concurrency](08_OS_Kernel_Concurrency/) | 20 | ⭐⭐⭐ | 📋 Pending |
| 9 | [Stack & Queue](09_Stack_Queue/) | 7 | ⭐⭐ | 📋 Pending |
| 10 | [Tree/Graph](10_Tree_Graph/) | 7 | ⭐ | 📋 Pending |
| 11 | [Embedded Gotchas](11_Embedded_Gotchas/) | 10 | ⭐⭐ | 📋 Pending |
| 12 | [Algorithms/DSA](12_Algorithms_DSA/) | 5 | ⭐⭐ | 📋 Pending |
| 13 | [Number/Math](13_Number_Math/) | 7 | ⭐ | 📋 Pending |

**Total: 143 Questions**

## 🎯 Top 20 Must-Prepare

See [00_Top20_MustPrepare](00_Top20_MustPrepare/) for the most critical questions.

| # | Question | Topic | Difficulty |
|---|----------|-------|------------|
| 1 | Reverse linked list | Linked Lists | Medium |
| 2 | Detect & remove loop | Linked Lists | Medium |
| 3 | Implement memcpy/memmove | Pointers | Medium |
| 4 | Implement strcpy/strlen | Pointers | Easy |
| 5 | Endianness detection | Bit Manipulation | Easy |
| 6 | Count set bits | Bit Manipulation | Easy |
| 7 | Reverse bits | Bit Manipulation | Medium |
| 8 | container_of & offsetof | OS/Kernel | Hard |
| 9 | Circular ring buffer | OS/Kernel | Medium |
| 10 | Producer-consumer | OS/Kernel | Hard |
| 11 | LRU cache | Algorithms | Hard |
| 12 | Hardware register access | Embedded | Easy |
| 13 | Function pointers | Pointers | Medium |
| 14 | Bit fields & packed struct | Memory | Medium |
| 15 | Custom malloc/free | OS/Kernel | Hard |
| 16 | Kadane's algorithm | Arrays | Medium |
| 17 | Merge sorted lists | Linked Lists | Medium |
| 18 | Find middle of list | Linked Lists | Easy |
| 19 | Palindrome linked list | Linked Lists | Medium |
| 20 | State machine | OS/Kernel | Medium |

## 🚀 Quick Start

```bash
# Navigate to a topic
cd 01_Bit_Manipulation

# Read the guide
cat README.md

# Test an implementation
cd 02_count_set_bits
gcc approach1_kernighan.c -o test && ./test
```

## 📖 Study Plan

### Week 1: Fundamentals
- Day 1-2: Bit Manipulation (✅ Complete)
- Day 3-4: Pointers & Memory Functions
- Day 5-6: Strings
- Day 7: Review & Practice

### Week 2: Data Structures
- Day 1-3: Linked Lists (Critical!)
- Day 4-5: Arrays
- Day 6: Stack & Queue
- Day 7: Review & Practice

### Week 3: Advanced Topics
- Day 1-2: Recursion
- Day 3-4: OS/Kernel/Concurrency
- Day 5: Memory & Storage Classes
- Day 6-7: Review & Mock Interviews

### Week 4: Specialization
- Day 1-2: Embedded Gotchas
- Day 3-4: Algorithms/DSA
- Day 5: Tree/Graph (if needed)
- Day 6-7: Final Review & Practice

## 🏢 Company-Specific Focus

### Qualcomm, Intel, NVIDIA (Embedded/Driver)
- ⭐⭐⭐ Bit Manipulation
- ⭐⭐⭐ Pointers & Memory
- ⭐⭐⭐ Linked Lists
- ⭐⭐⭐ OS/Kernel/Concurrency
- ⭐⭐ Embedded Gotchas

### Samsung, Broadcom, TI (Firmware)
- ⭐⭐⭐ Bit Manipulation
- ⭐⭐⭐ Pointers
- ⭐⭐ Memory & Storage
- ⭐⭐ Embedded Gotchas
- ⭐⭐ Arrays & Strings

### Systems Software (Google, Microsoft, Apple)
- ⭐⭐⭐ Linked Lists
- ⭐⭐⭐ Arrays
- ⭐⭐⭐ Strings
- ⭐⭐ Algorithms/DSA
- ⭐⭐ Tree/Graph

## 📝 Interview Tips

### Before Interview
1. Review Top 20 must-prepare questions
2. Practice explaining your approach
3. Know time/space complexity
4. Prepare questions to ask

### During Interview
1. Clarify requirements
2. Discuss approach before coding
3. Think aloud
4. Test with edge cases
5. Optimize if asked

### Common Mistakes
- Not handling NULL pointers
- Off-by-one errors
- Memory leaks
- Integer overflow
- Undefined behavior

## 🛠️ Compilation & Testing

```bash
# Compile with warnings
gcc -Wall -Wextra -std=c99 program.c -o program

# With optimizations
gcc -O2 -march=native program.c -o program

# Debug build
gcc -g -O0 program.c -o program
gdb ./program

# Memory check
valgrind --leak-check=full ./program
```

## 📚 Resources

- Each topic has detailed README
- Multiple approaches per question
- Time/space complexity analysis
- Interview tips and gotchas
- Test cases included

## 🎓 Difficulty Levels

- **Easy**: Basic implementation, straightforward logic
- **Medium**: Requires optimization or clever approach
- **Hard**: Complex algorithm or multiple concepts

## ⚡ Quick Reference

### Bit Manipulation
```c
n & (n-1)           // Clear rightmost set bit
n & -n              // Isolate rightmost set bit
n && !(n & (n-1))   // Check power of 2
```

### Pointers
```c
const int *p        // Pointer to const int
int * const p       // Const pointer to int
const int * const p // Const pointer to const int
```

### Linked Lists
```c
// Floyd's cycle detection
slow = slow->next;
fast = fast->next->next;
```

### Memory
```c
volatile uint32_t *reg = (volatile uint32_t *)0x40021000;
*reg |= (1 << 5);   // Set bit 5
```

## 📊 Progress Tracking

- [ ] Bit Manipulation (15/15) ✅
- [ ] Pointers (0/12)
- [ ] Linked Lists (0/15)
- [ ] Strings (0/10)
- [ ] Arrays (0/14)
- [ ] Recursion (0/7)
- [ ] Memory & Storage (0/14)
- [ ] OS/Kernel/Concurrency (0/20)
- [ ] Stack & Queue (0/7)
- [ ] Tree/Graph (0/7)
- [ ] Embedded Gotchas (0/10)
- [ ] Algorithms/DSA (0/5)
- [ ] Number/Math (0/7)

**Overall Progress: 15/143 (10.5%)**

## 🎯 Next Steps

1. Complete Pointers implementations
2. Implement Linked Lists (highest priority)
3. Create Top 20 must-prepare collection
4. Add more test cases
5. Create mock interview scenarios

---

*For Kernel/Embedded/Systems Engineering Interviews*
*Companies: Qualcomm, Intel, NVIDIA, Samsung, Broadcom, TI, etc.*

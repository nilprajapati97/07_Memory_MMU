# Common C Coding Questions for Kernel / Embedded / Systems Engineer Interviews

Below is a categorized list of C programs frequently asked in interviews for **Linux Kernel, Device Driver, Embedded, Firmware, and Systems Engineering** roles (companies like Qualcomm, Intel, NVIDIA, Samsung, Nvidia, Broadcom, Texas Instruments, Wipro, HCL, etc.).

---

## 1. Bit Manipulation (Very High Priority for Kernel/Embedded)

1. Set / Clear / Toggle / Check the **Nth bit** of a number
2. Count the number of **set bits** (Hamming weight) — Brian Kernighan's algorithm
3. Check if a number is a **power of 2**
4. Find the **only non-repeating** number where all others repeat twice (XOR)
5. Find **two non-repeating** numbers in an array
6. **Swap two numbers** without a temp variable (XOR swap)
7. **Reverse the bits** of an unsigned integer
8. Check if a machine is **little-endian or big-endian**
9. **Swap endianness** of a 32-bit integer (htonl-like)
10. **Rotate bits** left / right by N positions
11. Find **position of the only set bit**
12. Find the **log base 2** of an integer using bit ops
13. Multiply / divide by powers of 2 using shifts
14. Implement `setbit()`, `clearbit()`, `togglebit()` as **macros**
15. Find the **highest set bit** (MSB position)

---

## 2. Pointers (Extremely Important)

1. Difference between `const int *p`, `int * const p`, `const int * const p`
2. **Pointer to function** and array of function pointers
3. **Function returning a pointer** to a function
4. Declare: pointer to array of 10 integers vs array of 10 integer pointers
5. **Dangling, wild, void, NULL** pointer demonstrations
6. Implement `memcpy`, `memmove`, `memset`, `memcmp`
7. Implement `strcpy`, `strncpy`, `strlen`, `strcmp`, `strcat`, `strstr`, `strtok`
8. Implement `atoi`, `itoa`, `atof`
9. Pass a **2D array** to a function (multiple ways)
10. Dynamic allocation of **2D / 3D arrays** using `malloc`
11. Difference between `malloc`, `calloc`, `realloc`, `free`
12. Write your own **memory allocator** (simple `my_malloc`/`my_free`)

---

## 3. Linked Lists (Asked in ~80% of interviews)

1. **Singly linked list**: insert, delete, search, reverse, print
2. **Reverse a linked list** — iterative & recursive
3. **Detect a loop** (Floyd's algorithm) ✅ *(already done)*
4. **Find loop start** node and **remove the loop**
5. Find the **middle** node (slow/fast pointer)
6. Find **Nth node from end** in single pass
7. **Merge two sorted** linked lists
8. **Sort** a linked list (merge sort)
9. Check if a linked list is a **palindrome**
10. **Intersection point** of two linked lists
11. **Doubly linked list** operations
12. **Circular linked list** operations
13. Implement a **kernel-style linked list** (like Linux's `list_head` with `container_of`)
14. **Delete a node** given only a pointer to it (not head)
15. Swap nodes in **pairs**

---

## 4. Strings

1. Reverse a string (in-place, recursive, word-wise)
2. Check **palindrome**
3. Check **anagram** of two strings
4. Remove duplicate characters
5. First **non-repeating character**
6. Count vowels, consonants, words, lines
7. Print all **permutations** of a string
8. **Longest common substring / subsequence**
9. Pattern matching (KMP / naive)
10. Replace all spaces with `%20`

---

## 5. Arrays

1. Find the **largest / smallest / 2nd largest** element
2. **Reverse** an array in place
3. **Rotate** array by K positions
4. Find **missing number** from 1..N
5. Find **duplicate number** in O(1) space
6. **Majority element** (Boyer–Moore)
7. **Kadane's algorithm** — max subarray sum
8. **Merge two sorted arrays** in-place
9. **Remove duplicates** from sorted array
10. **Binary search** (iterative & recursive)
11. Find pair with given **sum**
12. **Matrix rotation** by 90°
13. **Spiral traversal** of a matrix
14. **Transpose** of a matrix

---

## 6. Recursion

1. Factorial, Fibonacci (with & without memoization)
2. **GCD / LCM** (Euclidean algorithm)
3. **Tower of Hanoi**
4. Power function `x^n` in O(log n)
5. Reverse a string / number using recursion
6. Print all **subsets** of a set
7. **N-Queens** problem

---

## 7. Memory, Storage Classes & Compilation

1. Difference between **stack and heap**
2. **Static, extern, auto, register, volatile, const** — use cases
3. Why **`volatile`** is critical in embedded code (registers, ISRs, multi-thread)
4. Memory layout of a C program — **text, data, bss, heap, stack**
5. `sizeof` of various data types, structs (with padding)
6. **Structure padding & packing** — `#pragma pack`, `__attribute__((packed))`
7. **Bit fields** in structures
8. **Union** vs structure — use cases (endianness checker via union)
9. Difference between `#define` and `const`
10. Difference between `typedef` and `#define`
11. **Inline functions** vs macros
12. **`static`** keyword inside function vs at file scope
13. **`extern`** variables across files
14. **`#pragma once`** vs include guards

---

## 8. OS / Kernel / Concurrency (Driver-specific)

1. Implement a **circular (ring) buffer**
2. Implement a **producer–consumer** using mutex + condition variable
3. Implement a **thread-safe queue / stack**
4. Implement a **spinlock** (test-and-set)
5. Implement a **semaphore** using mutex + condvar
6. **Reader-writer lock** implementation
7. Demonstrate **deadlock** and how to avoid it
8. Difference between **mutex, semaphore, spinlock**
9. **Atomic counter** using GCC atomics or `__sync_*`
10. Memory barriers — when needed
11. Simple **`malloc` / `free` implementation** using `sbrk`/`mmap`
12. Implement **`memcpy` aligned & optimized** (word-wise copy)
13. Build a **state machine** in C
14. Implement a simple **scheduler** (round-robin)
15. **`container_of()`** macro — explain & implement
16. **`offsetof()`** macro — implement
17. **Kernel linked list** (`struct list_head`) traversal & insertion
18. Implement a **timer wheel** / delayed work queue
19. **Reentrant** vs thread-safe functions
20. Signal handler — write an async-safe one

---

## 9. Stack & Queue

1. Stack using array / linked list
2. Queue using array / linked list
3. **Stack using two queues**, queue using two stacks
4. **Circular queue**
5. **Min stack** (O(1) get-min)
6. **Evaluate postfix** expression
7. **Balanced parentheses** check

---

## 10. Tree / Graph (Less in embedded, more in systems SW)

1. Binary tree traversals — inorder, preorder, postorder (recursive + iterative)
2. **Level order** (BFS) traversal
3. Height / depth of tree
4. **LCA** of two nodes
5. Check if a tree is a **BST**
6. **Mirror / invert** a binary tree
7. **DFS / BFS** on a graph

---

## 11. Classic Embedded / Kernel "Gotcha" Questions

1. Write a macro `MIN(a,b)` that's **side-effect safe**
2. `#define SECONDS_PER_YEAR` without overflow on 16-bit
3. **Infinite loop** in embedded — `while(1)` vs `for(;;)`
4. Read/write a **hardware register** at a fixed address using a pointer
   ```c
   *(volatile uint32_t *)0x40021000 = 0x1;
   ```
5. Declare:
   - An integer
   - A pointer to an integer
   - A pointer to a pointer to an integer
   - An array of 10 integers
   - An array of 10 pointers to integers
   - A pointer to an array of 10 integers
   - A pointer to a function that takes int and returns int
   - An array of 10 pointers to functions that take int and return int
6. What does `extern "C"` do?
7. Difference between **`a[i]` and `*(a+i)`** — are they always the same?
8. What is **strict aliasing**?
9. **Undefined behavior** examples (signed overflow, use-after-free, etc.)
10. Why never `return` a pointer to a **local variable**?

---

## 12. Algorithms / DSA Often Asked

1. All sorting algorithms — **bubble, selection, insertion, merge, quick, heap**
2. **Binary search** + variants (first/last occurrence, rotated array)
3. **Hash table** implementation with collision handling
4. **LRU cache** (hash map + doubly linked list) — very common at Intel/Qualcomm
5. **Trie** for string lookup

---

## 13. Number / Math Programs

1. Prime check, prime in range (Sieve of Eratosthenes)
2. Armstrong, perfect, palindrome number
3. Reverse digits of an integer (watch overflow)
4. Sum of digits, digital root
5. Decimal ↔ Binary / Hex / Octal conversion
6. Square root **without** using `sqrt()`
7. Print floating-point number without `printf("%f")`

---

## Top 20 "Must-Prepare" for Kernel/Driver Interviews

| # | Problem |
|---|---|
| 1 | Reverse a linked list (iterative + recursive) |
| 2 | Detect & remove loop in linked list |
| 3 | Implement `memcpy` / `memmove` (handle overlap) |
| 4 | Implement `strcpy` / `strlen` |
| 5 | Endianness detection & byte-swap |
| 6 | Count set bits |
| 7 | Reverse bits of an integer |
| 8 | Implement `container_of` and `offsetof` |
| 9 | Circular ring buffer |
| 10 | Producer-consumer with mutex/condvar |
| 11 | LRU cache |
| 12 | Read/write hardware register via `volatile` pointer |
| 13 | Function pointer & callback example |
| 14 | Bit field struct + packed struct |
| 15 | Custom `malloc`/`free` |
| 16 | Kadane's max subarray |
| 17 | Merge two sorted linked lists |
| 18 | Find middle of linked list |
| 19 | Detect palindrome linked list |
| 20 | Implement a state machine |

---

Would you like me to **create C source files for any specific topic** (e.g., all bit-manipulation programs, the Top-20 list, kernel-style linked list, ring buffer, custom `memcpy`)? I can scaffold them in your `Practice/` folder organized by category.
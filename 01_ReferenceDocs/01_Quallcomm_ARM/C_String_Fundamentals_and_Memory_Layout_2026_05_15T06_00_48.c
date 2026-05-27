
/**
 * =============================================================
 * C STRING FUNDAMENTALS AND MEMORY LAYOUT
 * =============================================================
 * 
 * Key Concepts:
 * 1. Strings in C are null-terminated character arrays ('\0')
 * 2. There is NO "string type" — only char arrays and char pointers
 * 3. The null terminator is what distinguishes a string from a raw char array
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 * 1. STRING DECLARATION METHODS & THEIR MEMORY LAYOUT
 * ============================================================ */

void string_declarations(void)
{
    /*
     * METHOD 1: String Literal (stored in .rodata / read-only section)
     * 
     * Memory Layout:
     *   ptr (stack) ──────► "Hello" (in .rodata segment)
     *   [8 bytes on 64-bit]   [H][e][l][l][o][\0]
     *
     * - ptr itself is on the stack (8 bytes on 64-bit)
     * - The string data is in READ-ONLY memory
     * - Modifying *ptr is UNDEFINED BEHAVIOR
     */
    char *ptr = "Hello";
    /* ptr[0] = 'h';  // ❌ UNDEFINED BEHAVIOR - segfault likely */

    /*
     * METHOD 2: Character Array (stored on stack)
     * 
     * Memory Layout (stack):
     *   [H][e][l][l][o][\0]
     *    ↑
     *   arr (address of first element)
     *
     * - Entire array is on the stack
     * - sizeof(arr) = 6 (5 chars + null terminator)
     * - arr is NOT a pointer — it's an array that decays to pointer
     * - Content IS modifiable
     */
    char arr[] = "Hello";
    arr[0] = 'h';  /* ✅ Valid — stack memory is writable */

    /*
     * METHOD 3: Fixed-size array
     * 
     * Memory Layout (stack):
     *   [H][e][l][l][o][\0][0][0][0][0]
     *    0   1   2   3   4   5  6  7  8  9
     *
     * - sizeof(buf) = 10
     * - strlen(buf) = 5
     * - Remaining bytes are zero-initialized (only if static/global)
     */
    char buf[10] = "Hello";

    /*
     * METHOD 4: Heap-allocated string
     * 
     * Memory Layout:
     *   heap_str (stack) ──────► [H][e][l][l][o][\0] (heap)
     *   [8 bytes]                 malloc'd region
     *
     * - Pointer on stack, data on heap
     * - Must be freed manually
     * - Modifiable
     */
    char *heap_str = malloc(6);
    strcpy(heap_str, "Hello");
    free(heap_str);

    printf("ptr:  sizeof=%zu, strlen=%zu
", sizeof(ptr), strlen(ptr));
    printf("arr:  sizeof=%zu, strlen=%zu
", sizeof(arr), strlen(arr));
    printf("buf:  sizeof=%zu, strlen=%zu
", sizeof(buf), strlen(buf));
}

/* ============================================================
 * 2. MEMORY SEGMENTS — WHERE STRINGS LIVE
 * ============================================================
 *
 * ┌─────────────────────────────────┐  High Address
 * │          STACK                   │  ← Local char arrays, pointers
 * │  (grows downward ↓)             │
 * ├─────────────────────────────────┤
 * │          HEAP                    │  ← malloc'd strings
 * │  (grows upward ↑)               │
 * ├─────────────────────────────────┤
 * │          .BSS                    │  ← Uninitialized global char arrays
 * ├─────────────────────────────────┤
 * │          .DATA                   │  ← Initialized global char arrays
 * ├─────────────────────────────────┤
 * │          .RODATA                 │  ← String literals ("Hello")
 * ├─────────────────────────────────┤
 * │          .TEXT                   │  ← Code
 * └─────────────────────────────────┘  Low Address
 */

/* .data segment — initialized, writable */
char global_arr[] = "Global";

/* .rodata segment — the literal; .data for the pointer */
char *global_ptr = "ReadOnly";

/* .bss segment — uninitialized, zero-filled */
char global_uninit[50];

/* ============================================================
 * 3. CRITICAL DIFFERENCE: ARRAY vs POINTER
 * ============================================================ */

void array_vs_pointer(void)
{
    char arr[] = "Hello";   /* Array: copies string to stack */
    char *ptr  = "Hello";   /* Pointer: points to .rodata */

    /*
     * Key Differences:
     * ┌──────────────────┬──────────────────────┬──────────────────────┐
     * │ Property         │ char arr[]           │ char *ptr            │
     * ├──────────────────┼──────────────────────┼──────────────────────┤
     * │ sizeof           │ 6 (array size)       │ 8 (pointer size)     │
     * │ Modifiable data  │ Yes                  │ No (UB)              │
     * │ Reassignable     │ No (arr = ... fails) │ Yes (ptr = ... ok)   │
     * │ Storage of data  │ Stack                │ .rodata              │
     * │ &arr == arr      │ Yes (same address)   │ No (different)       │
     * │ Decay to pointer │ In most expressions  │ Already a pointer    │
     * └──────────────────┴──────────────────────┴──────────────────────┘
     */

    /* arr = "World";  // ❌ Compile error: array not assignable */
    ptr = "World";     /* ✅ Valid: pointer reassignment */

    /* Address demonstration */
    printf("&arr = %p, arr = %p (same!)
", (void *)&arr, (void *)arr);
    printf("&ptr = %p, ptr = %p (different!)
", (void *)&ptr, (void *)ptr);
}

/* ============================================================
 * 4. STRING LITERAL POOLING & INTERNING
 * ============================================================ */

void string_literal_pooling(void)
{
    /*
     * Compilers MAY (not guaranteed) merge identical string literals
     * into a single copy in .rodata. This is called "string pooling."
     */
    char *s1 = "Hello";
    char *s2 = "Hello";

    /* May or may not be true — implementation-defined */
    if (s1 == s2)
        printf("Same address (pooled): %p
", (void *)s1);
    else
        printf("Different addresses: %p vs %p
", (void *)s1, (void *)s2);

    /* NEVER compare strings with == (compares addresses, not content) */
    /* Always use strcmp() for content comparison */
}

/* ============================================================
 * 5. NULL TERMINATOR PITFALLS
 * ============================================================ */

void null_terminator_issues(void)
{
    /* Pitfall 1: Forgetting null terminator */
    char bad[5] = {'H', 'e', 'l', 'l', 'o'};  /* NOT a valid string! */
    /* strlen(bad) → UNDEFINED BEHAVIOR (reads past array) */

    char good[6] = {'H', 'e', 'l', 'l', 'o', '\0'};  /* Valid string */

    /* Pitfall 2: Buffer overflow with strcpy */
    char small[4];
    /* strcpy(small, "Hello");  // ❌ Buffer overflow! Writes 6 bytes into 4 */

    /* Pitfall 3: Off-by-one with allocation */
    char *p = malloc(5);       /* ❌ Wrong: "Hello" needs 6 bytes */
    /* char *p = malloc(6);    // ✅ Correct: 5 chars + '\0' */

    /* Pitfall 4: Embedded null terminators */
    char tricky[] = "Hello\0World";
    printf("strlen = %zu
", strlen(tricky));   /* 5, not 11 */
    printf("sizeof = %zu
", sizeof(tricky));   /* 12 (includes both \0s) */

    free(p);
}

/* ============================================================
 * 6. STRING IN STRUCTS — PADDING & ALIGNMENT
 * ============================================================ */

struct StringInStruct {
    char name[7];    /* 7 bytes */
    int id;          /* 4 bytes, needs 4-byte alignment */
    char tag[3];     /* 3 bytes */
};
/*
 * Memory Layout (assuming 4-byte int alignment):
 * 
 * Offset: 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
 *        [n][a][m][e][\0][ ][ ][P][i][d ][  ][  ][t][a][g][P]
 *        |--- name[7] ---|pad|--- id (4) ---|--tag[3]--|pad
 * 
 * sizeof(struct StringInStruct) = 16 (not 14!)
 * Padding inserted for alignment
 */

/* ============================================================
 * 7. CONST CORRECTNESS WITH STRINGS
 * ============================================================ */

void const_correctness(void)
{
    /* Pointer to const char — cannot modify string through this pointer */
    const char *p1 = "Hello";
    /* p1[0] = 'h';   // ❌ Compile error */
    p1 = "World";      /* ✅ Can change what p1 points to */

    /* Const pointer to char — cannot change where pointer points */
    char arr[] = "Hello";
    char *const p2 = arr;
    p2[0] = 'h';      /* ✅ Can modify the string */
    /* p2 = "World";   // ❌ Compile error */

    /* Const pointer to const char — neither modifiable */
    const char *const p3 = "Hello";
    /* p3[0] = 'h';   // ❌ Compile error */
    /* p3 = "World";  // ❌ Compile error */

    /* Read right-to-left:
     * const char *p      → "p is a pointer to char that is const"
     * char *const p      → "p is a const pointer to char"
     * const char *const p → "p is a const pointer to const char"
     */
}

/* ============================================================
 * 8. COMMON INTERVIEW QUESTIONS — MEMORY LAYOUT
 * ============================================================ */

void interview_scenarios(void)
{
    /* Q: What's wrong here? */
    char *get_string(void) {
        char local[] = "Hello";  /* Stack allocated */
        return local;            /* ❌ Dangling pointer! */
    }
    /* A: local is destroyed when function returns.
     *    Fix: use static, malloc, or string literal */

    /* Q: What does this print? */
    char s[] = "Hello";
    printf("%zu %zu
", sizeof(s), sizeof(s+0));
    /* A: 6 (array size), 8 (pointer size on 64-bit)
     *    s+0 causes array-to-pointer decay */

    /* Q: Are these the same? */
    char a[] = "abc";
    char b[] = "abc";
    printf("%d
", a == b);  /* Always 0: different stack locations */
}

int main(void)
{
    printf("=== String Declarations ===
");
    string_declarations();

    printf("
=== Array vs Pointer ===
");
    array_vs_pointer();

    printf("
=== String Literal Pooling ===
");
    string_literal_pooling();

    printf("
=== Null Terminator Issues ===
");
    null_terminator_issues();

    printf("
=== Struct Size with Strings ===
");
    printf("sizeof(StringInStruct) = %zu
", sizeof(struct StringInStruct));

    printf("
=== Const Correctness ===
");
    const_correctness();

    return 0;
}


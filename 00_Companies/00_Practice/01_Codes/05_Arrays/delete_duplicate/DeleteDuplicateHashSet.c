/*
 * File: Delete_Duplicate_HashSet.c
 *
 * What we have done (All 3 Approaches Summary):
 * -----------------------------------------------
 *
 * Approach 1 — Delete_Duplicate.c          [O(n^2) Time | O(1) Space]
 *   Used a nested loop. For every element, scanned all previously
 *   kept elements to check for a duplicate. Simple but slow.
 *   Order preserved: YES
 *
 * Approach 2 — Delete_Duplicate_Optimized.c [O(n log n) Time | O(1) Space]
 *   Sorted the array with qsort(), then did a single-pass two-pointer
 *   scan to remove adjacent duplicates. Faster, but sorting changes
 *   the original order.
 *   Order preserved: NO
 *
 * Approach 3 — This file                   [O(n) avg Time | O(n) Space]
 *   Uses a custom Hash Set (open addressing with linear probing).
 *   Each element is hashed to a slot. Before inserting an element
 *   into the result, we check the hash set:
 *     - If NOT seen before -> add to result + mark in hash set.
 *     - If already seen    -> skip (it's a duplicate).
 *   This gives O(1) average lookup per element, so O(n) overall.
 *   Original insertion order is fully preserved.
 *   Order preserved: YES
 *
 * Hash Set Implementation Details:
 *   - Table size: next prime above 2*MAX_SIZE to reduce collisions.
 *   - Hash function: h(x) = ((x % TABLE_SIZE) + TABLE_SIZE) % TABLE_SIZE
 *     (handles negative integers correctly).
 *   - Collision resolution: linear probing (step +1 until empty slot).
 *   - Empty slot sentinel: INT_MIN (marks an unused bucket).
 *
 * Complexity:
 *   Time  -> O(n) average  [O(n^2) worst case — highly unlikely with prime table]
 *   Space -> O(n)          [hash table proportional to input size]
 *
 * Comparison Table:
 * -----------------------------------------------------------------
 *  File                          Time         Space  Order kept?
 * -----------------------------------------------------------------
 *  Delete_Duplicate.c            O(n^2)       O(1)   Yes
 *  Delete_Duplicate_Optimized.c  O(n log n)   O(1)   No
 *  Delete_Duplicate_HashSet.c    O(n) avg     O(n)   Yes   <-- Best
 * -----------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>  /* for INT_MIN sentinel */

#define MAX_SIZE   100
#define TABLE_SIZE 211   /* prime > 2*MAX_SIZE to keep load factor < 0.5 */

/* ----- Hash Set using open addressing (linear probing) ----- */

static int hashTable[TABLE_SIZE];  /* hash buckets */

/* Initialize all buckets to INT_MIN (empty sentinel) */
void initHashSet(void) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        hashTable[i] = INT_MIN;
    }
}

/*
 * hash: maps any integer (including negatives) to a valid table index.
 *   Using double-mod ensures the result is always non-negative.
 */
int hash(int value) {
    return ((value % TABLE_SIZE) + TABLE_SIZE) % TABLE_SIZE;
}

/*
 * contains: returns 1 if value is already in the hash set, 0 otherwise.
 *   Uses linear probing to handle collisions.
 */
int contains(int value) {
    int idx = hash(value);
    while (hashTable[idx] != INT_MIN) {     /* probe until empty slot */
        if (hashTable[idx] == value) return 1;  /* found */
        idx = (idx + 1) % TABLE_SIZE;           /* next slot (wrap around) */
    }
    return 0;  /* empty slot reached — value not present */
}

/*
 * insertSet: inserts value into the hash set.
 *   Assumes the table is not full (load < 0.5 guarantees fast probing).
 */
void insertSet(int value) {
    int idx = hash(value);
    while (hashTable[idx] != INT_MIN) {     /* find next empty slot */
        idx = (idx + 1) % TABLE_SIZE;
    }
    hashTable[idx] = value;  /* place the value */
}

/* ----- Main duplicate removal logic ----- */

/*
 * removeDuplicates:
 *   Iterates once through arr[]. For each element:
 *     - Check the hash set: if not seen, keep it and insert into set.
 *     - If already seen, skip (duplicate).
 *   Returns the new size. Original order is preserved.
 */
int removeDuplicates(int arr[], int n) {
    initHashSet();      /* clear the hash table before use */

    int k = 0;          /* index for next unique element */
    for (int i = 0; i < n; i++) {
        if (!contains(arr[i])) {    /* first time seeing this value? */
            insertSet(arr[i]);      /* mark it as seen */
            arr[k] = arr[i];        /* keep it in the result */
            k++;
        }
        /* else: duplicate — do nothing, effectively deleting it */
    }
    return k;
}

int main(void) {
    int arr[MAX_SIZE];
    int n;

    printf("Enter number of elements: ");
    if (scanf("%d", &n) != 1 || n <= 0 || n > MAX_SIZE) {
        printf("Invalid input.\n");
        return 1;
    }

    printf("Enter %d elements: ", n);
    for (int i = 0; i < n; i++) {
        scanf("%d", &arr[i]);
    }

    int newSize = removeDuplicates(arr, n);

    printf("Array after removing duplicates: ");
    for (int i = 0; i < newSize; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    return 0;
}

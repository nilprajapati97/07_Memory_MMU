# Strings - Complete Guide

## 📚 Topics Covered (10 Questions)

### 1. Reverse String
- **In-place** - O(n) time, O(1) space
- **Recursive** - O(n) time, O(n) space
- **Word-wise** - Reverse word order

### 2. Check Palindrome
- **Two Pointer** - O(n) time, O(1) space
- **Recursive** - O(n) time, O(n) space
- Case-insensitive, ignores non-alphanumeric

### 3. Check Anagram
- **Sorting** - O(n log n) time
- **Frequency Count** - O(n) time, O(1) space

### 4. Remove Duplicates
- **In-place** - O(n) time, O(1) space
- Uses hash set for tracking

### 5. First Non-Repeating Character
- **Frequency Count** - O(n) time, O(1) space
- Two passes: count, then find

### 6. Count Characters
- Vowels, consonants, words, lines
- Digits, spaces
- Single pass O(n)

### 7. String Permutations
- **Recursive Backtracking**
- O(n!) time complexity
- Generates all permutations

### 8. Longest Common Substring/Subsequence
- **Substring** - Dynamic Programming
- **Subsequence** - DP with backtracking
- O(m*n) time, O(m*n) space

### 9. Pattern Matching
- **Naive** - O(m*n) time
- **KMP** - O(m+n) time, O(m) space

### 10. Replace Spaces
- Replace with %20
- **Malloc version** - New string
- **In-place** - Requires extra space

## 🎯 Quick Reference

### Common Patterns

#### 1. Two Pointer
```c
int left = 0, right = strlen(str) - 1;
while (left < right) {
    // Process
    left++;
    right--;
}
```

#### 2. Frequency Count
```c
int count[256] = {0};
for (int i = 0; str[i]; i++)
    count[str[i]]++;
```

#### 3. In-place Modification
```c
int write_idx = 0;
for (int read_idx = 0; str[read_idx]; read_idx++) {
    if (condition)
        str[write_idx++] = str[read_idx];
}
str[write_idx] = '\0';
```

#### 4. Dynamic Programming
```c
int dp[m+1][n+1];
for (int i = 0; i <= m; i++) {
    for (int j = 0; j <= n; j++) {
        // Fill DP table
    }
}
```

## 📊 Complexity Analysis

| Operation | Time | Space | Notes |
|-----------|------|-------|-------|
| Reverse (in-place) | O(n) | O(1) | Best |
| Reverse (recursive) | O(n) | O(n) | Stack space |
| Palindrome | O(n) | O(1) | Two pointer |
| Anagram (sort) | O(n log n) | O(n) | Sorting |
| Anagram (count) | O(n) | O(1) | Hash array |
| Remove duplicates | O(n) | O(1) | In-place |
| First non-repeat | O(n) | O(1) | Two passes |
| Permutations | O(n!) | O(n) | Recursive |
| LCS (substring) | O(m*n) | O(m*n) | DP |
| LCS (subsequence) | O(m*n) | O(m*n) | DP |
| Pattern (naive) | O(m*n) | O(1) | Brute force |
| Pattern (KMP) | O(m+n) | O(m) | Optimal |

## 🔥 Interview Tips

### Most Frequently Asked
1. **Reverse string** (90% of interviews)
2. **Check palindrome** (85%)
3. **Anagram check** (75%)
4. **First non-repeating** (70%)
5. **Pattern matching** (60%)

### Common Mistakes
```c
// 1. Not checking NULL
while (*str)  // WRONG if str is NULL

// Correct:
if (!str) return;
while (*str) ...

// 2. Off-by-one in reverse
for (int i = 0; i < len; i++)  // WRONG

// Correct:
for (int i = 0; i < len/2; i++)

// 3. Forgetting null terminator
str[write_idx] = '\0';  // MUST add

// 4. Case sensitivity
if (s1[i] == s2[i])  // May be wrong

// Correct:
if (tolower(s1[i]) == tolower(s2[i]))
```

### Edge Cases to Test
- Empty string ("")
- Single character
- All same characters
- No matches
- Case sensitivity
- Special characters
- Spaces and punctuation

## 💡 Problem-Solving Strategies

### 1. Two Pointer Technique
**Use for**: Palindrome, reverse, remove duplicates
```c
int left = 0, right = len - 1;
while (left < right) {
    // Process both ends
}
```

### 2. Frequency Counting
**Use for**: Anagram, first non-repeating, duplicates
```c
int freq[256] = {0};
// Count phase
// Process phase
```

### 3. Dynamic Programming
**Use for**: LCS, edit distance, pattern matching
```c
int dp[m+1][n+1];
// Build table bottom-up
```

### 4. Sliding Window
**Use for**: Substring problems, pattern matching
```c
int window_start = 0;
for (int window_end = 0; ...) {
    // Expand window
    // Shrink if needed
}
```

## 🎓 Study Order

### Beginner
1. Reverse string (in-place)
2. Check palindrome
3. Count characters
4. Remove duplicates

### Intermediate
5. Anagram check
6. First non-repeating
7. Replace spaces
8. Pattern matching (naive)

### Advanced
9. String permutations
10. Longest common substring/subsequence
11. KMP algorithm

## 📝 Implementation Checklist

- [ ] Reverse (in-place, recursive, word-wise)
- [ ] Check palindrome (two pointer, recursive)
- [ ] Anagram (sorting, frequency)
- [ ] Remove duplicates
- [ ] First non-repeating character
- [ ] Count vowels, consonants, words
- [ ] String permutations
- [ ] Longest common substring
- [ ] Longest common subsequence
- [ ] Pattern matching (naive, KMP)
- [ ] Replace spaces

## 🚀 Quick Test

```bash
# Test reverse
cd 01_reverse_string
gcc approach1_inplace.c -o test && ./test

# Test palindrome
cd ../02_palindrome
gcc approach1_two_pointer.c -o test && ./test

# Test anagram
cd ../03_anagram
gcc approach1_sorting.c -o test && ./test

# Test KMP
cd ../09_pattern_matching
gcc approach2_kmp.c -o test && ./test
```

## 📚 Files Structure

```
04_Strings/
├── 01_reverse_string/
│   ├── approach1_inplace.c
│   ├── approach2_recursive.c
│   └── approach3_wordwise.c
├── 02_palindrome/
│   ├── approach1_two_pointer.c
│   └── approach2_recursive.c
├── 03_anagram/
│   ├── approach1_sorting.c
│   └── approach2_frequency.c
├── 04_remove_duplicates/
│   └── approach1_inplace.c
├── 05_first_non_repeating/
│   └── approach1_frequency.c
├── 06_count_chars/
│   └── approach1_complete.c
├── 07_permutations/
│   └── approach1_recursive.c
├── 08_longest_common/
│   ├── approach1_substring.c
│   └── approach2_subsequence.c
├── 09_pattern_matching/
│   ├── approach1_naive.c
│   └── approach2_kmp.c
└── 10_replace_spaces/
    └── approach1_malloc.c
```

## 🏆 Key Takeaways

1. **Master two-pointer** - Used in 40% of problems
2. **Understand frequency counting** - O(1) space solution
3. **Know when to use DP** - Substring/subsequence problems
4. **Practice KMP** - Optimal pattern matching
5. **Handle edge cases** - NULL, empty, single char

## 🔗 Related Topics

- **Arrays** - Similar techniques
- **Pointers** - String manipulation
- **Recursion** - Permutations, palindrome
- **Dynamic Programming** - LCS problems

---

**Status**: Complete ✅
**Questions**: 10/10
**Implementations**: 16 programs
**Interview Ready**: Yes

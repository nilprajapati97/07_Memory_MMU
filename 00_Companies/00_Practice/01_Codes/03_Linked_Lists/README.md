# Linked Lists - Complete Guide

## 📚 Topics Covered (15 Questions)

### 1. Singly Linked List
- Insert (front, end, position)
- Delete (by value, by position)
- Search
- Reverse
- Print

### 2. Reverse Linked List
- **Iterative** - O(n) time, O(1) space
- **Recursive** - O(n) time, O(n) space

### 3. Detect Loop
- **Floyd's Algorithm** (Tortoise and Hare)
- O(n) time, O(1) space

### 4. Find Loop Start & Remove
- Find meeting point
- Find loop start
- Remove loop

### 5. Find Middle Node
- **Slow/Fast Pointer**
- Single pass
- O(n) time, O(1) space

### 6. Nth Node from End
- **Two Pointer Technique**
- Single pass
- O(n) time, O(1) space

### 7. Merge Two Sorted Lists
- **Iterative** - Using dummy node
- **Recursive** - Elegant solution

### 8. Sort Linked List
- **Merge Sort** - O(n log n)
- Best for linked lists
- Stable sort

### 9. Check Palindrome
- **Using Stack** - O(n) space
- **Reverse Second Half** - O(1) space

### 10. Intersection Point
- **Length Difference Method**
- O(m + n) time, O(1) space

### 11. Doubly Linked List
- Insert (front, end)
- Delete
- Traverse (forward, backward)

### 12. Circular Linked List
- Insert (front, end)
- Delete
- Traverse

### 13. Kernel-Style List
- **Linux list_head**
- **container_of** macro
- Generic list implementation

### 14. Delete Node (No Head)
- Copy next node's data
- Delete next node
- O(1) time

### 15. Swap Nodes in Pairs
- **Iterative** - Using dummy node
- **Recursive** - Clean solution

## 🎯 Quick Reference

### Common Patterns

#### 1. Two Pointer (Slow/Fast)
```c
Node *slow = head, *fast = head;
while (fast && fast->next) {
    slow = slow->next;
    fast = fast->next->next;
}
// slow is at middle
```

#### 2. Dummy Node
```c
Node dummy = {0, NULL};
Node *tail = &dummy;
// ... build list ...
return dummy.next;
```

#### 3. Reverse
```c
Node *prev = NULL, *curr = head, *next;
while (curr) {
    next = curr->next;
    curr->next = prev;
    prev = curr;
    curr = next;
}
return prev;
```

#### 4. Find Nth from End
```c
// Move first n steps ahead
for (int i = 0; i < n; i++)
    first = first->next;

// Move both until first reaches end
while (first) {
    first = first->next;
    second = second->next;
}
```

## 📊 Complexity Analysis

| Operation | Time | Space | Notes |
|-----------|------|-------|-------|
| Insert (front) | O(1) | O(1) | Direct |
| Insert (end) | O(n) | O(1) | Need to traverse |
| Delete | O(n) | O(1) | Search + delete |
| Search | O(n) | O(1) | Linear scan |
| Reverse (iterative) | O(n) | O(1) | Best |
| Reverse (recursive) | O(n) | O(n) | Stack space |
| Detect loop | O(n) | O(1) | Floyd's |
| Find middle | O(n) | O(1) | Slow/fast |
| Merge sorted | O(m+n) | O(1) | Iterative |
| Sort (merge) | O(n log n) | O(log n) | Recursion stack |
| Palindrome | O(n) | O(1) | Reverse half |

## 🔥 Interview Tips

### Most Frequently Asked
1. **Reverse linked list** (95% of interviews)
2. **Detect loop** (Floyd's algorithm)
3. **Find middle** (slow/fast pointer)
4. **Merge two sorted lists**
5. **Check palindrome**

### Common Mistakes
```c
// 1. Not checking NULL
Node *curr = head;
while (curr->next)  // WRONG if head is NULL
    curr = curr->next;

// Correct:
while (curr && curr->next)
    curr = curr->next;

// 2. Memory leak
Node *temp = head;
head = head->next;
// Missing: free(temp);

// 3. Losing head pointer
while (head) {  // WRONG: loses original head
    head = head->next;
}

// Correct:
Node *curr = head;
while (curr) {
    curr = curr->next;
}
```

### Edge Cases to Test
- Empty list (NULL)
- Single node
- Two nodes
- Odd vs even length
- Loop at different positions
- Lists of different lengths

## 💡 Problem-Solving Strategies

### 1. Use Dummy Node
When building new list or modifying head:
```c
Node dummy = {0, NULL};
Node *tail = &dummy;
// ... operations ...
return dummy.next;
```

### 2. Two Pointer Technique
For middle, nth from end, cycle detection:
```c
Node *slow = head, *fast = head;
```

### 3. Recursion
For reverse, merge, palindrome:
```c
Node *reverse(Node *head) {
    if (!head || !head->next) return head;
    Node *new_head = reverse(head->next);
    head->next->next = head;
    head->next = NULL;
    return new_head;
}
```

### 4. Stack
For palindrome, reverse print:
```c
// Push first half to stack
// Compare with second half
```

## 🎓 Study Order

### Beginner
1. Singly linked list basics
2. Reverse (iterative)
3. Find middle
4. Nth from end

### Intermediate
5. Detect loop (Floyd's)
6. Merge two sorted
7. Check palindrome
8. Doubly linked list

### Advanced
9. Sort linked list
10. Find loop start
11. Intersection point
12. Kernel-style list
13. Swap pairs

## 📝 Implementation Checklist

- [ ] Basic operations (insert, delete, search)
- [ ] Reverse (iterative & recursive)
- [ ] Detect loop (Floyd's algorithm)
- [ ] Find middle (slow/fast pointer)
- [ ] Nth from end (two pointer)
- [ ] Merge sorted lists
- [ ] Sort list (merge sort)
- [ ] Check palindrome
- [ ] Find intersection
- [ ] Doubly linked list
- [ ] Circular linked list
- [ ] Kernel-style list
- [ ] Delete without head
- [ ] Swap pairs

## 🚀 Quick Test

```bash
# Test basic operations
cd 01_singly_linked_list
gcc approach1_basic.c -o test && ./test

# Test reverse
cd ../02_reverse_list
gcc approach1_iterative.c -o test && ./test

# Test Floyd's algorithm
cd ../03_detect_loop
gcc approach1_floyd.c -o test && ./test

# Test merge sort
cd ../08_sort_list
gcc approach1_merge_sort.c -o test && ./test
```

## 📚 Files Structure

```
03_Linked_Lists/
├── 01_singly_linked_list/
├── 02_reverse_list/
├── 03_detect_loop/
├── 04_find_remove_loop/
├── 05_find_middle/
├── 06_nth_from_end/
├── 07_merge_sorted/
├── 08_sort_list/
├── 09_palindrome_list/
├── 10_intersection_point/
├── 11_doubly_linked_list/
├── 12_circular_linked_list/
├── 13_kernel_style_list/
├── 14_delete_node/
└── 15_swap_pairs/
```

## 🏆 Key Takeaways

1. **Master two-pointer technique** - Used in 50% of problems
2. **Understand Floyd's algorithm** - Loop detection
3. **Practice with dummy nodes** - Simplifies edge cases
4. **Know when to use recursion** - Elegant but uses stack
5. **Handle NULL carefully** - Most common bug

## 🔗 Related Topics

- **Arrays** - Comparison with linked lists
- **Stacks/Queues** - Can be implemented with lists
- **Trees** - Extension of linked structures
- **Graphs** - Adjacency list representation

---

**Status**: Complete ✅
**Questions**: 15/15
**Implementations**: 18 programs
**Interview Ready**: Yes

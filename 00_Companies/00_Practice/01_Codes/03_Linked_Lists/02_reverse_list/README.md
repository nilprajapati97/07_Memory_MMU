# Reverse Linked List

## Two Approaches

### Approach 1: Iterative (Best)
**Time**: O(n)  
**Space**: O(1)

```c
Node *reverse(Node *head) {
    Node *prev = NULL, *curr = head, *next;
    while (curr) {
        next = curr->next;  // Save next
        curr->next = prev;  // Reverse link
        prev = curr;        // Move prev
        curr = next;        // Move curr
    }
    return prev;
}
```

**Pros**:
- O(1) space
- Faster in practice
- No stack overflow risk

**Cons**:
- More code
- Less intuitive

### Approach 2: Recursive (Elegant)
**Time**: O(n)  
**Space**: O(n) - recursion stack

```c
Node *reverse(Node *head) {
    if (!head || !head->next)
        return head;
    
    Node *new_head = reverse(head->next);
    head->next->next = head;
    head->next = NULL;
    
    return new_head;
}
```

**Pros**:
- Clean code
- Easy to understand
- Elegant solution

**Cons**:
- O(n) stack space
- Stack overflow for large lists
- Slower due to function calls

## Visualization

### Iterative Process
```
Initial:  1 -> 2 -> 3 -> 4 -> NULL
          ^
         head

Step 1:   NULL <- 1    2 -> 3 -> 4 -> NULL
                  ^    ^
                prev  curr

Step 2:   NULL <- 1 <- 2    3 -> 4 -> NULL
                       ^    ^
                     prev  curr

Step 3:   NULL <- 1 <- 2 <- 3    4 -> NULL
                            ^    ^
                          prev  curr

Step 4:   NULL <- 1 <- 2 <- 3 <- 4
                                 ^
                               prev (new head)
```

### Recursive Process
```
reverse(1->2->3->4->NULL)
  reverse(2->3->4->NULL)
    reverse(3->4->NULL)
      reverse(4->NULL)
        return 4
      4->next->next = 3  // 4->NULL becomes 4->3
      3->next = NULL
      return 4
    4->3->next->next = 2  // becomes 4->3->2
    2->next = NULL
    return 4
  4->3->2->next->next = 1  // becomes 4->3->2->1
  1->next = NULL
  return 4

Result: 4->3->2->1->NULL
```

## Interview Tips

### When to Use Iterative
- Large lists (avoid stack overflow)
- Memory constrained
- Performance critical
- **Recommended for interviews**

### When to Use Recursive
- Small lists
- Code clarity important
- Demonstrating recursion knowledge

### Common Questions
**Q: Can you reverse in-place?**  
A: Yes, both approaches reverse in-place (no new nodes created)

**Q: What's the space complexity?**  
A: Iterative O(1), Recursive O(n) due to call stack

**Q: How to reverse only part of list?**  
A: Modify to take start and end positions

## Variations

### Reverse in Groups
```c
// Reverse in groups of k
Node *reverse_k_group(Node *head, int k) {
    // Count nodes
    Node *curr = head;
    int count = 0;
    while (curr && count < k) {
        curr = curr->next;
        count++;
    }
    
    if (count == k) {
        // Reverse first k nodes
        curr = reverse_k_group(curr, k);
        
        while (count-- > 0) {
            Node *temp = head->next;
            head->next = curr;
            curr = head;
            head = temp;
        }
        head = curr;
    }
    
    return head;
}
```

### Reverse Between Positions
```c
// Reverse from position m to n
Node *reverse_between(Node *head, int m, int n) {
    if (m == n) return head;
    
    Node dummy = {0, head};
    Node *prev = &dummy;
    
    // Move to position m-1
    for (int i = 0; i < m - 1; i++)
        prev = prev->next;
    
    // Reverse from m to n
    Node *curr = prev->next;
    for (int i = 0; i < n - m; i++) {
        Node *temp = curr->next;
        curr->next = temp->next;
        temp->next = prev->next;
        prev->next = temp;
    }
    
    return dummy.next;
}
```

## Edge Cases

1. **Empty list**: Return NULL
2. **Single node**: Return as is
3. **Two nodes**: Simple swap
4. **Very long list**: Iterative preferred

## Testing

```c
// Test cases
test_reverse(NULL);           // Empty
test_reverse(single_node);    // One node
test_reverse(two_nodes);      // Two nodes
test_reverse(odd_length);     // 1->2->3
test_reverse(even_length);    // 1->2->3->4
```

## Complexity Comparison

| Approach | Time | Space | Stack Safe | Code Lines |
|----------|------|-------|------------|------------|
| Iterative | O(n) | O(1) | Yes | ~10 |
| Recursive | O(n) | O(n) | No | ~7 |

## Interview Answer Template

```
1. Clarify: In-place? Return new head?
2. Approach: Iterative (O(1) space)
3. Algorithm:
   - Use three pointers: prev, curr, next
   - Iterate and reverse links
   - Return prev as new head
4. Complexity: O(n) time, O(1) space
5. Edge cases: NULL, single node
```

# Detect Loop - Floyd's Algorithm

## Floyd's Cycle Detection (Tortoise and Hare)

### Algorithm
```c
int detect_loop(Node *head) {
    Node *slow = head;
    Node *fast = head;
    
    while (fast && fast->next) {
        slow = slow->next;        // Move 1 step
        fast = fast->next->next;  // Move 2 steps
        
        if (slow == fast)
            return 1;  // Loop detected
    }
    
    return 0;  // No loop
}
```

### Why It Works

**Key Insight**: If there's a loop, fast pointer will eventually catch up to slow pointer.

**Proof**:
1. Slow moves 1 step per iteration
2. Fast moves 2 steps per iteration
3. Relative speed = 1 step per iteration
4. In a loop, fast will catch slow in at most N iterations (N = loop size)

### Visualization

```
No Loop:
1 -> 2 -> 3 -> 4 -> 5 -> NULL
S    
F

1 -> 2 -> 3 -> 4 -> 5 -> NULL
     S         F

1 -> 2 -> 3 -> 4 -> 5 -> NULL
          S              F (NULL)

With Loop:
1 -> 2 -> 3 -> 4 -> 5
          ^         |
          |_________|

Initial:
S,F at 1

After 1 iteration:
S at 2, F at 3

After 2 iterations:
S at 3, F at 5

After 3 iterations:
S at 4, F at 4  // MEET!
```

## Complexity

**Time**: O(n)
- Without loop: Fast reaches end in n/2 steps
- With loop: They meet in at most n steps

**Space**: O(1)
- Only two pointers

## Finding Loop Start

Once loop is detected, find where it starts:

```c
Node *find_loop_start(Node *head) {
    Node *meeting = detect_loop(head);
    if (!meeting) return NULL;
    
    Node *ptr1 = head;
    Node *ptr2 = meeting;
    
    while (ptr1 != ptr2) {
        ptr1 = ptr1->next;
        ptr2 = ptr2->next;
    }
    
    return ptr1;  // Loop start
}
```

### Why This Works

**Mathematical Proof**:
- Let distance from head to loop start = x
- Let distance from loop start to meeting point = y
- Let loop size = L

When they meet:
- Slow traveled: x + y
- Fast traveled: x + y + kL (k loops)
- Fast = 2 * Slow
- x + y + kL = 2(x + y)
- kL = x + y
- x = kL - y

So moving from head and meeting point at same speed, they meet at loop start!

## Remove Loop

```c
void remove_loop(Node *head) {
    Node *loop_start = find_loop_start(head);
    if (!loop_start) return;
    
    Node *curr = loop_start;
    while (curr->next != loop_start)
        curr = curr->next;
    
    curr->next = NULL;
}
```

## Alternative Approaches

### 1. Hash Set (O(n) space)
```c
int detect_loop_hash(Node *head) {
    Node *visited[1000];
    int count = 0;
    
    while (head) {
        for (int i = 0; i < count; i++) {
            if (visited[i] == head)
                return 1;
        }
        visited[count++] = head;
        head = head->next;
    }
    return 0;
}
```

### 2. Modify Node (Destructive)
```c
int detect_loop_modify(Node *head) {
    while (head) {
        if (head->data == INT_MIN)
            return 1;
        head->data = INT_MIN;  // Mark visited
        head = head->next;
    }
    return 0;
}
```

## Comparison

| Method | Time | Space | Destructive |
|--------|------|-------|-------------|
| Floyd's | O(n) | O(1) | No |
| Hash Set | O(n) | O(n) | No |
| Modify | O(n) | O(1) | Yes |

## Interview Tips

### Questions to Ask
1. Can I modify the list?
2. What should I return? (boolean, node, etc.)
3. Should I find loop start?
4. Should I remove the loop?

### Common Follow-ups
1. Find loop start ✓
2. Find loop length
3. Remove loop ✓
4. Find meeting point distance

### Edge Cases
- Empty list
- Single node (no loop)
- Single node pointing to itself
- Loop at head
- Loop at tail
- No loop

## Loop Length

```c
int loop_length(Node *head) {
    Node *meeting = detect_loop(head);
    if (!meeting) return 0;
    
    int length = 1;
    Node *curr = meeting->next;
    
    while (curr != meeting) {
        length++;
        curr = curr->next;
    }
    
    return length;
}
```

## Interview Answer Template

```
1. Clarify: Return boolean? Find start? Remove?
2. Approach: Floyd's Cycle Detection
3. Algorithm:
   - Use slow (1 step) and fast (2 steps) pointers
   - If they meet, loop exists
   - To find start: reset one to head, move both 1 step
4. Complexity: O(n) time, O(1) space
5. Why it works: Mathematical proof with distances
```

## Real-World Applications

1. **Garbage Collection**: Detect circular references
2. **Network Routing**: Detect routing loops
3. **Deadlock Detection**: Circular wait conditions
4. **Graph Algorithms**: Cycle detection

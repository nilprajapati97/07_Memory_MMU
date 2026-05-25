/* Find Loop Start and Remove Loop */
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

Node *create_node(int data) {
    Node *node = (Node *)malloc(sizeof(Node));
    if (!node) return NULL;
    node->data = data;
    node->next = NULL;
    return node;
}

// Detect loop
Node *detect_loop(Node *head) {
    Node *slow = head, *fast = head;
    
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        
        if (slow == fast)
            return slow;  // Meeting point
    }
    
    return NULL;
}

// Find loop start
Node *find_loop_start(Node *head) {
    Node *meeting = detect_loop(head);
    if (!meeting) return NULL;
    
    // Move one pointer to head
    Node *ptr1 = head;
    Node *ptr2 = meeting;
    
    // Move both at same speed
    while (ptr1 != ptr2) {
        ptr1 = ptr1->next;
        ptr2 = ptr2->next;
    }
    
    return ptr1;  // Loop start
}

// Remove loop
void remove_loop(Node *head) {
    Node *loop_start = find_loop_start(head);
    if (!loop_start) return;
    
    // Find node before loop start
    Node *curr = loop_start;
    while (curr->next != loop_start)
        curr = curr->next;
    
    curr->next = NULL;  // Break loop
}

void print_list(Node *head, int max_nodes) {
    int count = 0;
    while (head && count < max_nodes) {
        printf("%d -> ", head->data);
        head = head->next;
        count++;
    }
    if (count == max_nodes)
        printf("...\n");
    else
        printf("NULL\n");
}

int main() {
    // Create list: 1 -> 2 -> 3 -> 4 -> 5
    Node *head = create_node(1);
    head->next = create_node(2);
    head->next->next = create_node(3);
    head->next->next->next = create_node(4);
    head->next->next->next->next = create_node(5);
    
    // Create loop: 5 -> 3
    Node *loop_node = head->next->next;
    head->next->next->next->next->next = loop_node;
    
    printf("List with loop (showing first 10 nodes):\n");
    print_list(head, 10);
    
    Node *start = find_loop_start(head);
    printf("Loop starts at node with data: %d\n", start->data);
    
    remove_loop(head);
    printf("\nAfter removing loop:\n");
    print_list(head, 10);
    
    return 0;
}

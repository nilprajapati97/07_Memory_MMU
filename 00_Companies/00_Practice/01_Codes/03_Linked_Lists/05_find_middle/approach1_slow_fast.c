/* Approach 1: Find Middle Node - Slow/Fast Pointer */
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

void insert_end(Node **head, int data) {
    Node *node = create_node(data);
    if (!*head) {
        *head = node;
        return;
    }
    Node *curr = *head;
    while (curr->next) curr = curr->next;
    curr->next = node;
}

// Find middle using slow/fast pointer
Node *find_middle(Node *head) {
    if (!head) return NULL;
    
    Node *slow = head;
    Node *fast = head;
    
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    
    return slow;  // Middle node
}

void print_list(Node *head) {
    while (head) {
        printf("%d -> ", head->data);
        head = head->next;
    }
    printf("NULL\n");
}

int main() {
    Node *head = NULL;
    
    // Odd number of nodes
    for (int i = 1; i <= 5; i++)
        insert_end(&head, i);
    
    printf("List (odd): ");
    print_list(head);
    Node *mid = find_middle(head);
    printf("Middle: %d\n\n", mid->data);
    
    // Even number of nodes
    insert_end(&head, 6);
    printf("List (even): ");
    print_list(head);
    mid = find_middle(head);
    printf("Middle: %d\n", mid->data);
    
    return 0;
}

/* Approach 2: Reverse Linked List - Recursive */
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

void print_list(Node *head) {
    while (head) {
        printf("%d -> ", head->data);
        head = head->next;
    }
    printf("NULL\n");
}

// Recursive reverse
Node *reverse_recursive(Node *head) {
    // Base case: empty or single node
    if (!head || !head->next)
        return head;
    
    // Reverse rest of list
    Node *new_head = reverse_recursive(head->next);
    
    // Fix links
    head->next->next = head;
    head->next = NULL;
    
    return new_head;
}

int main() {
    Node *head = NULL;
    
    for (int i = 1; i <= 5; i++)
        insert_end(&head, i);
    
    printf("Original: ");
    print_list(head);
    
    head = reverse_recursive(head);
    
    printf("Reversed: ");
    print_list(head);
    
    return 0;
}

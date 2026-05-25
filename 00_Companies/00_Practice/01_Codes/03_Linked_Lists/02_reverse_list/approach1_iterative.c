/* Approach 1: Reverse Linked List - Iterative */
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

// Iterative reverse
Node *reverse_iterative(Node *head) {
    Node *prev = NULL;
    Node *curr = head;
    Node *next = NULL;
    
    while (curr) {
        next = curr->next;  // Save next
        curr->next = prev;  // Reverse link
        prev = curr;        // Move prev forward
        curr = next;        // Move curr forward
    }
    
    return prev;  // New head
}

int main() {
    Node *head = NULL;
    
    for (int i = 1; i <= 5; i++)
        insert_end(&head, i);
    
    printf("Original: ");
    print_list(head);
    
    head = reverse_iterative(head);
    
    printf("Reversed: ");
    print_list(head);
    
    return 0;
}

/* Swap Nodes in Pairs */
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

// Swap nodes in pairs (iterative)
Node *swap_pairs(Node *head) {
    if (!head || !head->next)
        return head;
    
    Node dummy = {0, head};
    Node *prev = &dummy;
    Node *curr = head;
    
    while (curr && curr->next) {
        Node *first = curr;
        Node *second = curr->next;
        
        // Swap
        prev->next = second;
        first->next = second->next;
        second->next = first;
        
        // Move forward
        prev = first;
        curr = first->next;
    }
    
    return dummy.next;
}

// Swap pairs recursively
Node *swap_pairs_recursive(Node *head) {
    if (!head || !head->next)
        return head;
    
    Node *first = head;
    Node *second = head->next;
    
    first->next = swap_pairs_recursive(second->next);
    second->next = first;
    
    return second;
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
    
    for (int i = 1; i <= 6; i++)
        insert_end(&head, i);
    
    printf("Original: ");
    print_list(head);
    
    head = swap_pairs(head);
    printf("After swap (iterative): ");
    print_list(head);
    
    // Create new list for recursive
    Node *head2 = NULL;
    for (int i = 1; i <= 6; i++)
        insert_end(&head2, i);
    
    head2 = swap_pairs_recursive(head2);
    printf("After swap (recursive): ");
    print_list(head2);
    
    return 0;
}

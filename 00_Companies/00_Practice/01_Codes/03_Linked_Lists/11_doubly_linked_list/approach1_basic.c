/* Doubly Linked List Operations */
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node *prev;
    struct Node *next;
} Node;

Node *create_node(int data) {
    Node *node = (Node *)malloc(sizeof(Node));
    if (!node) return NULL;
    node->data = data;
    node->prev = NULL;
    node->next = NULL;
    return node;
}

// Insert at front
void insert_front(Node **head, int data) {
    Node *node = create_node(data);
    if (!node) return;
    
    if (*head) {
        node->next = *head;
        (*head)->prev = node;
    }
    *head = node;
}

// Insert at end
void insert_end(Node **head, int data) {
    Node *node = create_node(data);
    if (!node) return;
    
    if (!*head) {
        *head = node;
        return;
    }
    
    Node *curr = *head;
    while (curr->next)
        curr = curr->next;
    
    curr->next = node;
    node->prev = curr;
}

// Delete node
void delete_node(Node **head, Node *del) {
    if (!*head || !del) return;
    
    if (*head == del)
        *head = del->next;
    
    if (del->next)
        del->next->prev = del->prev;
    
    if (del->prev)
        del->prev->next = del->next;
    
    free(del);
}

// Print forward
void print_forward(Node *head) {
    printf("Forward: ");
    while (head) {
        printf("%d <-> ", head->data);
        head = head->next;
    }
    printf("NULL\n");
}

// Print backward
void print_backward(Node *head) {
    if (!head) return;
    
    // Go to end
    while (head->next)
        head = head->next;
    
    printf("Backward: ");
    while (head) {
        printf("%d <-> ", head->data);
        head = head->prev;
    }
    printf("NULL\n");
}

int main() {
    Node *head = NULL;
    
    insert_end(&head, 1);
    insert_end(&head, 2);
    insert_end(&head, 3);
    insert_front(&head, 0);
    
    print_forward(head);
    print_backward(head);
    
    // Delete middle node
    delete_node(&head, head->next->next);
    printf("\nAfter deleting node with data 2:\n");
    print_forward(head);
    
    return 0;
}

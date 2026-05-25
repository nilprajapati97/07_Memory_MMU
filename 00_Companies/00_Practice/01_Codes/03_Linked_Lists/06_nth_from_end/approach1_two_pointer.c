/* Approach 1: Nth Node from End - Two Pointer */
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

// Find nth node from end in single pass
Node *nth_from_end(Node *head, int n) {
    Node *first = head;
    Node *second = head;
    
    // Move first pointer n steps ahead
    for (int i = 0; i < n; i++) {
        if (!first) return NULL;  // n is larger than list
        first = first->next;
    }
    
    // Move both pointers until first reaches end
    while (first) {
        first = first->next;
        second = second->next;
    }
    
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
    
    for (int i = 1; i <= 5; i++)
        insert_end(&head, i);
    
    printf("List: ");
    print_list(head);
    
    for (int n = 1; n <= 5; n++) {
        Node *node = nth_from_end(head, n);
        if (node)
            printf("%dth from end: %d\n", n, node->data);
    }
    
    return 0;
}

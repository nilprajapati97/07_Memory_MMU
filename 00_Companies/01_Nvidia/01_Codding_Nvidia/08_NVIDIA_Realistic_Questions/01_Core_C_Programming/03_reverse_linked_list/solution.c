// Reverse a singly linked list (iterative and recursive)
#include <stdio.h>
#include <stdlib.h>

struct Node {
    int data;
    struct Node* next;
};

// Iterative
struct Node* reverse_iterative(struct Node* head) {
    struct Node* prev = NULL;
    struct Node* curr = head;
    while (curr) {
        struct Node* next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}

// Recursive
struct Node* reverse_recursive(struct Node* head) {
    if (!head || !head->next) return head;
    struct Node* rest = reverse_recursive(head->next);
    head->next->next = head;
    head->next = NULL;
    return rest;
}

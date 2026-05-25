/* Approach 2: Merge Two Sorted Lists - Recursive */
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

// Merge two sorted lists recursively
Node *merge_sorted_recursive(Node *l1, Node *l2) {
    if (!l1) return l2;
    if (!l2) return l1;
    
    if (l1->data <= l2->data) {
        l1->next = merge_sorted_recursive(l1->next, l2);
        return l1;
    } else {
        l2->next = merge_sorted_recursive(l1, l2->next);
        return l2;
    }
}

void print_list(Node *head) {
    while (head) {
        printf("%d -> ", head->data);
        head = head->next;
    }
    printf("NULL\n");
}

int main() {
    Node *l1 = NULL, *l2 = NULL;
    
    insert_end(&l1, 1);
    insert_end(&l1, 3);
    insert_end(&l1, 5);
    
    insert_end(&l2, 2);
    insert_end(&l2, 4);
    insert_end(&l2, 6);
    
    printf("List 1: ");
    print_list(l1);
    printf("List 2: ");
    print_list(l2);
    
    Node *merged = merge_sorted_recursive(l1, l2);
    printf("Merged: ");
    print_list(merged);
    
    return 0;
}

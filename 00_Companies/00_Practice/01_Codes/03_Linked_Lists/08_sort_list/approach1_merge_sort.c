/* Sort Linked List - Merge Sort */
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

// Find middle
Node *get_middle(Node *head) {
    if (!head) return NULL;
    Node *slow = head, *fast = head->next;
    
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    
    return slow;
}

// Merge two sorted lists
Node *merge(Node *l1, Node *l2) {
    if (!l1) return l2;
    if (!l2) return l1;
    
    if (l1->data <= l2->data) {
        l1->next = merge(l1->next, l2);
        return l1;
    } else {
        l2->next = merge(l1, l2->next);
        return l2;
    }
}

// Merge sort
Node *merge_sort(Node *head) {
    if (!head || !head->next)
        return head;
    
    // Split list
    Node *middle = get_middle(head);
    Node *next_to_middle = middle->next;
    middle->next = NULL;
    
    // Sort both halves
    Node *left = merge_sort(head);
    Node *right = merge_sort(next_to_middle);
    
    // Merge sorted halves
    return merge(left, right);
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
    int arr[] = {5, 2, 8, 1, 9, 3};
    
    for (int i = 0; i < 6; i++)
        insert_end(&head, arr[i]);
    
    printf("Original: ");
    print_list(head);
    
    head = merge_sort(head);
    
    printf("Sorted:   ");
    print_list(head);
    
    return 0;
}

/* Approach 1: Singly Linked List - Basic Operations */
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

// Insert at beginning
void insert_front(Node **head, int data) {
    Node *node = create_node(data);
    if (!node) return;
    node->next = *head;
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
}

// Insert at position
void insert_at(Node **head, int data, int pos) {
    if (pos == 0) {
        insert_front(head, data);
        return;
    }
    
    Node *curr = *head;
    for (int i = 0; i < pos - 1 && curr; i++)
        curr = curr->next;
    
    if (!curr) return;
    
    Node *node = create_node(data);
    if (!node) return;
    node->next = curr->next;
    curr->next = node;
}

// Delete by value
void delete_value(Node **head, int data) {
    if (!*head) return;
    
    if ((*head)->data == data) {
        Node *temp = *head;
        *head = (*head)->next;
        free(temp);
        return;
    }
    
    Node *curr = *head;
    while (curr->next && curr->next->data != data)
        curr = curr->next;
    
    if (curr->next) {
        Node *temp = curr->next;
        curr->next = curr->next->next;
        free(temp);
    }
}

// Search
Node *search(Node *head, int data) {
    while (head) {
        if (head->data == data)
            return head;
        head = head->next;
    }
    return NULL;
}

// Print
void print_list(Node *head) {
    while (head) {
        printf("%d -> ", head->data);
        head = head->next;
    }
    printf("NULL\n");
}

// Reverse
void reverse(Node **head) {
    Node *prev = NULL, *curr = *head, *next = NULL;
    
    while (curr) {
        next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    
    *head = prev;
}

// Free list
void free_list(Node **head) {
    Node *curr = *head;
    while (curr) {
        Node *temp = curr;
        curr = curr->next;
        free(temp);
    }
    *head = NULL;
}

int main() {
    Node *head = NULL;
    
    printf("Insert at front: 3, 2, 1\n");
    insert_front(&head, 3);
    insert_front(&head, 2);
    insert_front(&head, 1);
    print_list(head);
    
    printf("\nInsert at end: 4, 5\n");
    insert_end(&head, 4);
    insert_end(&head, 5);
    print_list(head);
    
    printf("\nInsert 10 at position 2\n");
    insert_at(&head, 10, 2);
    print_list(head);
    
    printf("\nSearch for 10: %s\n", search(head, 10) ? "Found" : "Not found");
    
    printf("\nDelete 10\n");
    delete_value(&head, 10);
    print_list(head);
    
    printf("\nReverse list\n");
    reverse(&head);
    print_list(head);
    
    free_list(&head);
    return 0;
}

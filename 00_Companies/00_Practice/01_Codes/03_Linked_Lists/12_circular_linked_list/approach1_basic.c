/* Circular Linked List Operations */
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

// Insert at end
void insert_end(Node **head, int data) {
    Node *node = create_node(data);
    if (!node) return;
    
    if (!*head) {
        *head = node;
        node->next = node;  // Point to itself
        return;
    }
    
    Node *curr = *head;
    while (curr->next != *head)
        curr = curr->next;
    
    curr->next = node;
    node->next = *head;
}

// Insert at front
void insert_front(Node **head, int data) {
    Node *node = create_node(data);
    if (!node) return;
    
    if (!*head) {
        *head = node;
        node->next = node;
        return;
    }
    
    Node *curr = *head;
    while (curr->next != *head)
        curr = curr->next;
    
    node->next = *head;
    curr->next = node;
    *head = node;
}

// Delete node
void delete_node(Node **head, int key) {
    if (!*head) return;
    
    Node *curr = *head, *prev = NULL;
    
    // If head is to be deleted
    if (curr->data == key) {
        // Find last node
        while (curr->next != *head)
            curr = curr->next;
        
        if (*head == (*head)->next) {
            // Only one node
            free(*head);
            *head = NULL;
        } else {
            curr->next = (*head)->next;
            Node *temp = *head;
            *head = (*head)->next;
            free(temp);
        }
        return;
    }
    
    // Search for node
    prev = *head;
    curr = (*head)->next;
    while (curr != *head && curr->data != key) {
        prev = curr;
        curr = curr->next;
    }
    
    if (curr->data == key) {
        prev->next = curr->next;
        free(curr);
    }
}

// Print circular list
void print_list(Node *head) {
    if (!head) {
        printf("Empty list\n");
        return;
    }
    
    Node *curr = head;
    do {
        printf("%d -> ", curr->data);
        curr = curr->next;
    } while (curr != head);
    printf("(back to %d)\n", head->data);
}

int main() {
    Node *head = NULL;
    
    insert_end(&head, 1);
    insert_end(&head, 2);
    insert_end(&head, 3);
    insert_front(&head, 0);
    
    printf("Circular list: ");
    print_list(head);
    
    delete_node(&head, 2);
    printf("After deleting 2: ");
    print_list(head);
    
    return 0;
}

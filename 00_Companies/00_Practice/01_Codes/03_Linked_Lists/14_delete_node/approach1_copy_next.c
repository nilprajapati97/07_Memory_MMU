/* Delete Node Given Only Pointer to It */
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

// Delete node without head pointer
// Copy next node's data and delete next node
void delete_node(Node *node) {
    if (!node || !node->next) {
        // Cannot delete last node or NULL
        return;
    }
    
    // Copy next node's data
    Node *temp = node->next;
    node->data = temp->data;
    node->next = temp->next;
    
    free(temp);
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
    
    printf("Original: ");
    print_list(head);
    
    // Delete node with data 3 (without head pointer)
    Node *node_to_delete = head->next->next;  // Node with data 3
    printf("Deleting node with data: %d\n", node_to_delete->data);
    delete_node(node_to_delete);
    
    printf("After deletion: ");
    print_list(head);
    
    return 0;
}

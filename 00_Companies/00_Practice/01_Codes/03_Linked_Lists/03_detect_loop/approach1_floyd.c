/* Approach 1: Detect Loop - Floyd's Algorithm */
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

// Floyd's Cycle Detection (Tortoise and Hare)
int detect_loop(Node *head) {
    Node *slow = head;
    Node *fast = head;
    
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        
        if (slow == fast)
            return 1;  // Loop detected
    }
    
    return 0;  // No loop
}

int main() {
    // Create list: 1 -> 2 -> 3 -> 4 -> 5
    Node *head = create_node(1);
    head->next = create_node(2);
    head->next->next = create_node(3);
    head->next->next->next = create_node(4);
    head->next->next->next->next = create_node(5);
    
    printf("List without loop: %s\n", 
           detect_loop(head) ? "Loop detected" : "No loop");
    
    // Create loop: 5 -> 3
    head->next->next->next->next->next = head->next->next;
    
    printf("List with loop: %s\n", 
           detect_loop(head) ? "Loop detected" : "No loop");
    
    return 0;
}

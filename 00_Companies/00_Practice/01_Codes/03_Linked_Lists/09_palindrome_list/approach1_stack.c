/* Approach 1: Check Palindrome - Using Stack */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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

// Check palindrome using stack
bool is_palindrome(Node *head) {
    if (!head || !head->next) return true;
    
    // Find middle
    Node *slow = head, *fast = head;
    int stack[1000], top = 0;
    
    // Push first half to stack
    while (fast && fast->next) {
        stack[top++] = slow->data;
        slow = slow->next;
        fast = fast->next->next;
    }
    
    // Skip middle for odd length
    if (fast) slow = slow->next;
    
    // Compare second half with stack
    while (slow) {
        if (stack[--top] != slow->data)
            return false;
        slow = slow->next;
    }
    
    return true;
}

void print_list(Node *head) {
    while (head) {
        printf("%d -> ", head->data);
        head = head->next;
    }
    printf("NULL\n");
}

int main() {
    Node *head1 = NULL;
    int arr1[] = {1, 2, 3, 2, 1};
    for (int i = 0; i < 5; i++)
        insert_end(&head1, arr1[i]);
    
    printf("List 1: ");
    print_list(head1);
    printf("Is palindrome: %s\n\n", is_palindrome(head1) ? "Yes" : "No");
    
    Node *head2 = NULL;
    int arr2[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++)
        insert_end(&head2, arr2[i]);
    
    printf("List 2: ");
    print_list(head2);
    printf("Is palindrome: %s\n", is_palindrome(head2) ? "Yes" : "No");
    
    return 0;
}

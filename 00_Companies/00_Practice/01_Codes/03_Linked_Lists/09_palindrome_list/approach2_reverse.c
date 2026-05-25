/* Approach 2: Check Palindrome - Reverse Second Half */
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

Node *reverse(Node *head) {
    Node *prev = NULL, *curr = head, *next;
    while (curr) {
        next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    return prev;
}

// Check palindrome by reversing second half
bool is_palindrome(Node *head) {
    if (!head || !head->next) return true;
    
    // Find middle
    Node *slow = head, *fast = head;
    while (fast->next && fast->next->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    
    // Reverse second half
    Node *second_half = reverse(slow->next);
    Node *first_half = head;
    
    // Compare
    bool result = true;
    Node *temp = second_half;
    while (temp) {
        if (first_half->data != temp->data) {
            result = false;
            break;
        }
        first_half = first_half->next;
        temp = temp->next;
    }
    
    // Restore list (optional)
    slow->next = reverse(second_half);
    
    return result;
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
    int arr[] = {1, 2, 3, 2, 1};
    
    for (int i = 0; i < 5; i++)
        insert_end(&head, arr[i]);
    
    printf("List: ");
    print_list(head);
    printf("Is palindrome: %s\n", is_palindrome(head) ? "Yes" : "No");
    printf("After check: ");
    print_list(head);
    
    return 0;
}

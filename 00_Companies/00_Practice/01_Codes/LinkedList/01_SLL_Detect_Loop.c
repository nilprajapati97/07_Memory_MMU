#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Node definition */
struct Node {
    int data;
    struct Node *next;
};

/* Create a new node */
struct Node* newNode(int data) {
    struct Node *node = (struct Node*)malloc(sizeof(struct Node));
    if (!node) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    node->data = data;
    node->next = NULL;
    return node;
}

/* Floyd's Cycle Detection Algorithm
 * Returns true if a loop exists, false otherwise.
 */
bool detectLoop(struct Node *head) {
    struct Node *slow = head;
    struct Node *fast = head;

    while (fast != NULL && fast->next != NULL) {
        slow = slow->next;        /* move 1 step */
        fast = fast->next->next;  /* move 2 steps */

        if (slow == fast) {
            return true;          /* loop found */
        }
    }
    return false;                 /* reached end -> no loop */
}

/* Optional: find the node where the loop starts */
struct Node* findLoopStart(struct Node *head) {
    struct Node *slow = head, *fast = head;

    while (fast != NULL && fast->next != NULL) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) break;
    }
    if (fast == NULL || fast->next == NULL) return NULL;

    slow = head;
    while (slow != fast) {
        slow = slow->next;
        fast = fast->next;
    }
    return slow; /* loop start node */
}

/* Driver */
int main(void) {
    /* Build list: 1 -> 2 -> 3 -> 4 -> 5 */
    struct Node *head = newNode(1);
    head->next        = newNode(2);
    head->next->next  = newNode(3);
    head->next->next->next       = newNode(4);
    head->next->next->next->next = newNode(5);

    /* Create a loop: 5 -> 3 (comment out to test no-loop case) */
    head->next->next->next->next->next = head->next->next;

    if (detectLoop(head)) {
        struct Node *start = findLoopStart(head);
        printf("Loop detected. Loop starts at node with data = %d\n",
               start ? start->data : -1);
    } else {
        printf("No loop in the linked list.\n");
    }

    /* NOTE: freeing nodes is skipped here because the list is cyclic.
       In production code, break the loop first before freeing. */
    return 0;
}
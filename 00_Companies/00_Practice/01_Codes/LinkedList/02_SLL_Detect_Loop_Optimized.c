/*
 * SLL_Detect_Loop_Optimized.c
 * ---------------------------------------------------------------
 * Optimized loop detection in a singly linked list using
 * Floyd's Tortoise & Hare algorithm.
 *
 * Features:
 *   1. detectLoop()       - O(n) time, O(1) space loop detection
 *   2. findLoopStart()    - locate the first node of the loop
 *   3. countLoopLength()  - number of nodes in the loop
 *   4. removeLoop()       - safely break the loop so the list
 *                           can be traversed/freed normally
 *   5. freeList()         - frees all nodes (safe after removeLoop)
 *
 * Compile : gcc SLL_Detect_Loop_Optimized.c -o loop
 * Run     : ./loop
 * ---------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct Node {
    int           data;
    struct Node  *next;
} Node;

/* -------- Utility: create a new node -------------------------- */
static Node *createNode(int data)
{
    Node *n = (Node *)malloc(sizeof(Node));
    if (!n) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }
    n->data = data;
    n->next = NULL;
    return n;
}

/* -------- Append at tail (helper for building demo list) ------ */
static void append(Node **head, int data)
{
    Node *n = createNode(data);
    if (*head == NULL) { *head = n; return; }

    Node *cur = *head;
    while (cur->next) cur = cur->next;
    cur->next = n;
}

/* ---------------------------------------------------------------
 * Floyd's algorithm - returns the meeting point inside the loop,
 * or NULL if no loop exists. Single pass, O(n) time, O(1) space.
 * ------------------------------------------------------------- */
static Node *floydMeetingPoint(Node *head)
{
    Node *slow = head, *fast = head;

    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) return slow;   /* loop detected */
    }
    return NULL;                         /* no loop */
}

/* -------- Public: detect loop --------------------------------- */
bool detectLoop(Node *head)
{
    return floydMeetingPoint(head) != NULL;
}

/* -------- Public: find the node where the loop begins --------- *
 * Math: distance(head -> loopStart) == distance(meet -> loopStart)
 * Move one pointer from head, one from meet, both 1 step at a
 * time; they collide at the loop's start.
 * ------------------------------------------------------------- */
Node *findLoopStart(Node *head)
{
    Node *meet = floydMeetingPoint(head);
    if (!meet) return NULL;

    Node *p = head;
    while (p != meet) {
        p    = p->next;
        meet = meet->next;
    }
    return p;
}

/* -------- Public: length of the loop -------------------------- */
int countLoopLength(Node *head)
{
    Node *meet = floydMeetingPoint(head);
    if (!meet) return 0;

    int   len = 1;
    Node *cur = meet->next;
    while (cur != meet) {
        cur = cur->next;
        len++;
    }
    return len;
}

/* -------- Public: remove the loop in O(n) / O(1) -------------- *
 * Walk one pointer from head and another from the loop start;
 * the node whose ->next equals the loop start is the tail.
 * Setting tail->next = NULL safely breaks the cycle.
 * ------------------------------------------------------------- */
bool removeLoop(Node *head)
{
    Node *start = findLoopStart(head);
    if (!start) return false;

    /* Special case: loop start is head itself */
    Node *tail = start;
    while (tail->next != start) tail = tail->next;

    tail->next = NULL;
    return true;
}

/* -------- Print first `limit` nodes (safe for cyclic lists) --- */
static void printList(Node *head, int limit)
{
    Node *cur = head;
    int   i   = 0;
    while (cur && i < limit) {
        printf("%d -> ", cur->data);
        cur = cur->next;
        i++;
    }
    printf(cur ? "...\n" : "NULL\n");
}

/* -------- Free a non-cyclic list ------------------------------ */
static void freeList(Node *head)
{
    while (head) {
        Node *tmp = head;
        head = head->next;
        free(tmp);
    }
}

/* =============================================================
 * Demo / Driver
 * ============================================================= */
int main(void)
{
    /* Build: 1 -> 2 -> 3 -> 4 -> 5 -> 6 */
    Node *head = NULL;
    for (int i = 1; i <= 6; ++i) append(&head, i);

    /* Introduce a loop: 6 -> 3 */
    Node *loopAt = head->next->next;          /* node with data 3 */
    Node *tail   = head;
    while (tail->next) tail = tail->next;     /* node with data 6 */
    tail->next = loopAt;

    printf("List (first 10 nodes): ");
    printList(head, 10);

    if (detectLoop(head)) {
        Node *s = findLoopStart(head);
        printf("Loop detected.\n");
        printf("  Loop starts at node : %d\n", s->data);
        printf("  Loop length         : %d\n", countLoopLength(head));

        if (removeLoop(head)) {
            printf("Loop removed successfully.\n");
        }
    } else {
        printf("No loop in the list.\n");
    }

    printf("List after fix         : ");
    printList(head, 20);

    freeList(head);
    return 0;
}

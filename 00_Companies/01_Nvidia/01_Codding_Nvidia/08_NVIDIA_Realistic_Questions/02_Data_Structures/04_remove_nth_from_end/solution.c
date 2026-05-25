// Remove nth node from end of list
struct ListNode {
    int val;
    struct ListNode *next;
};

struct ListNode* removeNthFromEnd(struct ListNode* head, int n) {
    struct ListNode dummy = {0, head};
    struct ListNode *first = &dummy, *second = &dummy;
    for (int i = 0; i <= n; ++i)
        first = first->next;
    while (first) {
        first = first->next;
        second = second->next;
    }
    struct ListNode *to_delete = second->next;
    second->next = to_delete->next;
    // free(to_delete); // Uncomment if using malloc
    return dummy.next;
}

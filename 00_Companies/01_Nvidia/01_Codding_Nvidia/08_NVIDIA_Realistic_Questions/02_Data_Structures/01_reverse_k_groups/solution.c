// Reverse nodes in k-groups in a linked list
#include <stdlib.h>
struct ListNode {
    int val;
    struct ListNode *next;
};

struct ListNode* reverseKGroup(struct ListNode* head, int k) {
    struct ListNode *curr = head;
    int count = 0;
    while (curr && count < k) {
        curr = curr->next;
        count++;
    }
    if (count == k) {
        curr = reverseKGroup(curr, k);
        while (count--) {
            struct ListNode *tmp = head->next;
            head->next = curr;
            curr = head;
            head = tmp;
        }
        head = curr;
    }
    return head;
}

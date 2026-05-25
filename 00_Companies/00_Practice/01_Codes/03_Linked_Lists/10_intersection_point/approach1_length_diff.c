/* Find Intersection Point of Two Lists */
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

int get_length(Node *head) {
    int len = 0;
    while (head) {
        len++;
        head = head->next;
    }
    return len;
}

// Find intersection point
Node *find_intersection(Node *head1, Node *head2) {
    int len1 = get_length(head1);
    int len2 = get_length(head2);
    
    // Advance longer list
    int diff = abs(len1 - len2);
    Node *longer = (len1 > len2) ? head1 : head2;
    Node *shorter = (len1 > len2) ? head2 : head1;
    
    for (int i = 0; i < diff; i++)
        longer = longer->next;
    
    // Find intersection
    while (longer && shorter) {
        if (longer == shorter)
            return longer;
        longer = longer->next;
        shorter = shorter->next;
    }
    
    return NULL;
}

void print_list(Node *head) {
    while (head) {
        printf("%d -> ", head->data);
        head = head->next;
    }
    printf("NULL\n");
}

int main() {
    // Create first list: 1 -> 2 -> 3
    Node *head1 = create_node(1);
    head1->next = create_node(2);
    head1->next->next = create_node(3);
    
    // Create second list: 4 -> 5
    Node *head2 = create_node(4);
    head2->next = create_node(5);
    
    // Create common part: 6 -> 7 -> 8
    Node *common = create_node(6);
    common->next = create_node(7);
    common->next->next = create_node(8);
    
    // Connect both lists to common part
    head1->next->next->next = common;
    head2->next->next = common;
    
    printf("List 1: ");
    print_list(head1);
    printf("List 2: ");
    print_list(head2);
    
    Node *intersection = find_intersection(head1, head2);
    if (intersection)
        printf("Intersection at node with data: %d\n", intersection->data);
    else
        printf("No intersection\n");
    
    return 0;
}

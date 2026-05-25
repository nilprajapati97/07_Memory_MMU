// Doubly linked list implementation like Linux list_head
#include <stdio.h>
#include <stdlib.h>

struct list_head {
    struct list_head *next, *prev;
};

#define INIT_LIST_HEAD(ptr) do { \
    (ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head) {
    __list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head) {
    __list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head * prev, struct list_head * next) {
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry) {
    __list_del(entry->prev, entry->next);
    entry->next = entry->prev = NULL;
}

// Example usage
struct my_node {
    int data;
    struct list_head list;
};

int main() {
    struct list_head head;
    INIT_LIST_HEAD(&head);
    struct my_node n1 = { .data = 1 }, n2 = { .data = 2 };
    list_add(&n1.list, &head);
    list_add_tail(&n2.list, &head);
    // Traverse
    struct list_head *pos;
    for (pos = head.next; pos != &head; pos = pos->next) {
        struct my_node *node = (struct my_node *)((char *)pos - offsetof(struct my_node, list));
        printf("%d\n", node->data);
    }
    return 0;
}

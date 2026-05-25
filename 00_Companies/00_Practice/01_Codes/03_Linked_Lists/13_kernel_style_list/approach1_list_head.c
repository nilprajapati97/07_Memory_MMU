/* Kernel-Style Linked List (Linux list_head) */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

// Generic list head (like Linux kernel)
typedef struct list_head {
    struct list_head *next;
    struct list_head *prev;
} list_head_t;

// Initialize list
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) list_head_t name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(list_head_t *list) {
    list->next = list;
    list->prev = list;
}

// Add node
static inline void list_add(list_head_t *new_node, list_head_t *head) {
    new_node->next = head->next;
    new_node->prev = head;
    head->next->prev = new_node;
    head->next = new_node;
}

// Delete node
static inline void list_del(list_head_t *entry) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->next = entry->prev = NULL;
}

// container_of macro
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// list_entry macro
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

// Iterate over list
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

// Example structure using list_head
typedef struct person {
    char name[50];
    int age;
    list_head_t list;  // Embedded list node
} person_t;

int main() {
    LIST_HEAD(person_list);
    
    // Create persons
    person_t *p1 = malloc(sizeof(person_t));
    sprintf(p1->name, "Alice");
    p1->age = 25;
    INIT_LIST_HEAD(&p1->list);
    
    person_t *p2 = malloc(sizeof(person_t));
    sprintf(p2->name, "Bob");
    p2->age = 30;
    INIT_LIST_HEAD(&p2->list);
    
    person_t *p3 = malloc(sizeof(person_t));
    sprintf(p3->name, "Charlie");
    p3->age = 35;
    INIT_LIST_HEAD(&p3->list);
    
    // Add to list
    list_add(&p1->list, &person_list);
    list_add(&p2->list, &person_list);
    list_add(&p3->list, &person_list);
    
    // Iterate and print
    printf("Person list:\n");
    list_head_t *pos;
    list_for_each(pos, &person_list) {
        person_t *p = list_entry(pos, person_t, list);
        printf("  %s, age %d\n", p->name, p->age);
    }
    
    // Delete Bob
    list_del(&p2->list);
    free(p2);
    
    printf("\nAfter deleting Bob:\n");
    list_for_each(pos, &person_list) {
        person_t *p = list_entry(pos, person_t, list);
        printf("  %s, age %d\n", p->name, p->age);
    }
    
    return 0;
}

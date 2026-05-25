// Lock-free stack with ABA problem (simplified)
#include <stdatomic.h>
#include <stdint.h>
typedef struct node {
    int val;
    struct node *next;
} node_t;

typedef struct {
    _Atomic(node_t*) head;
} stack_t;

void stack_init(stack_t *s) { atomic_store(&s->head, NULL); }

void stack_push(stack_t *s, node_t *n) {
    node_t *old_head;
    do {
        old_head = atomic_load(&s->head);
        n->next = old_head;
    } while (!atomic_compare_exchange_weak(&s->head, &old_head, n));
}

node_t *stack_pop(stack_t *s) {
    node_t *old_head, *next;
    do {
        old_head = atomic_load(&s->head);
        if (!old_head) return NULL;
        next = old_head->next;
    } while (!atomic_compare_exchange_weak(&s->head, &old_head, next));
    return old_head;
}

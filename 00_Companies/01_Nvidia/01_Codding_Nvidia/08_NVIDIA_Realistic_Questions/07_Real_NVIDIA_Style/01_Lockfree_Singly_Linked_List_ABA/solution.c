// Lock-free singly linked list for Linux kernel (ABA-aware)
// See README.md for full explanation
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/spinlock.h>

struct lf_node {
    int data;
    struct lf_node *next;
};

struct lf_list {
    atomic_long_t head; // Use tagged pointer for ABA avoidance
};

// Helper to pack pointer and tag into a single long
#define LF_TAG_MASK 0xFFFF
#define LF_PTR_MASK (~LF_TAG_MASK)
static inline long lf_pack(struct lf_node *ptr, uint16_t tag) {
    return ((long)ptr & LF_PTR_MASK) | (tag & LF_TAG_MASK);
}
static inline struct lf_node *lf_unpack_ptr(long packed) {
    return (struct lf_node *)(packed & LF_PTR_MASK);
}
static inline uint16_t lf_unpack_tag(long packed) {
    return (uint16_t)(packed & LF_TAG_MASK);
}

void lf_list_init(struct lf_list *list) {
    atomic_long_set(&list->head, lf_pack(NULL, 0));
}

// Lock-free push (ABA-aware)
void lf_list_push(struct lf_list *list, int value) {
    struct lf_node *node = kmalloc(sizeof(*node), GFP_KERNEL);
    node->data = value;
    long old_head, new_head;
    struct lf_node *old_ptr;
    uint16_t old_tag;
    do {
        old_head = atomic_long_read(&list->head);
        old_ptr = lf_unpack_ptr(old_head);
        old_tag = lf_unpack_tag(old_head);
        node->next = old_ptr;
        new_head = lf_pack(node, old_tag + 1);
    } while (atomic_long_cmpxchg(&list->head, old_head, new_head) != old_head);
}

// Lock-free pop (ABA-aware)
int lf_list_pop(struct lf_list *list, int *out) {
    long old_head, new_head;
    struct lf_node *old_ptr, *next_ptr;
    uint16_t old_tag;
    do {
        old_head = atomic_long_read(&list->head);
        old_ptr = lf_unpack_ptr(old_head);
        old_tag = lf_unpack_tag(old_head);
        if (!old_ptr) return -1; // Empty
        next_ptr = old_ptr->next;
        new_head = lf_pack(next_ptr, old_tag + 1);
    } while (atomic_long_cmpxchg(&list->head, old_head, new_head) != old_head);
    *out = old_ptr->data;
    kfree(old_ptr);
    return 0;
}

MODULE_LICENSE("GPL");

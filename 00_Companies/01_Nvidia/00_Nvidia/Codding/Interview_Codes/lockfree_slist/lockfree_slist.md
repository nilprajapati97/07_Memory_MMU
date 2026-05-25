# Lock-Free Singly Linked List (Linux Kernel Style)

## Source Code
```c
/*
 * lockfree_slist.c
 * Lock-free singly linked list for Linux kernel (conceptual, not for direct use)
 * ABA problem handled using pointer tagging (version counters)
 *
 * Note: This is a simplified demonstration. In real kernel code, use kernel primitives.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/types.h>

struct lf_node {
    int data;
    struct lf_node *next;
};

struct lf_head {
    atomic64_t ptr_ver; // Lower 48 bits: pointer, upper 16 bits: version
};

#define PTR_MASK 0x0000FFFFFFFFFFFFULL
#define VER_SHIFT 48

static inline struct lf_node *lf_get_ptr(u64 ptr_ver) {
    return (struct lf_node *)(ptr_ver & PTR_MASK);
}

static inline u16 lf_get_ver(u64 ptr_ver) {
    return (u16)(ptr_ver >> VER_SHIFT);
}

static inline u64 lf_make_ptr_ver(struct lf_node *ptr, u16 ver) {
    return ((u64)ver << VER_SHIFT) | ((u64)ptr & PTR_MASK);
}

void lf_list_init(struct lf_head *head) {
    atomic64_set(&head->ptr_ver, lf_make_ptr_ver(NULL, 0));
}

bool lf_list_push(struct lf_head *head, int data) {
    struct lf_node *new_node = kmalloc(sizeof(*new_node), GFP_ATOMIC);
    if (!new_node)
        return false;
    new_node->data = data;
    while (1) {
        u64 old = atomic64_read(&head->ptr_ver);
        struct lf_node *old_ptr = lf_get_ptr(old);
        u16 old_ver = lf_get_ver(old);
        new_node->next = old_ptr;
        u64 new = lf_make_ptr_ver(new_node, old_ver + 1);
        if (atomic64_cmpxchg(&head->ptr_ver, old, new) == old)
            return true;
    }
}

bool lf_list_pop(struct lf_head *head, int *data) {
    while (1) {
        u64 old = atomic64_read(&head->ptr_ver);
        struct lf_node *old_ptr = lf_get_ptr(old);
        u16 old_ver = lf_get_ver(old);
        if (!old_ptr)
            return false;
        struct lf_node *next = old_ptr->next;
        u64 new = lf_make_ptr_ver(next, old_ver + 1);
        if (atomic64_cmpxchg(&head->ptr_ver, old, new) == old) {
            *data = old_ptr->data;
            kfree(old_ptr);
            return true;
        }
    }
}

void lf_list_destroy(struct lf_head *head) {
    int tmp;
    while (lf_list_pop(head, &tmp))
        ;
}

MODULE_LICENSE("GPL");
```

## ABA Problem Handling
The ABA problem occurs when a memory location is changed from value A to B and back to A, making it appear unchanged to a thread. To prevent this, we use a version counter (tag) alongside the pointer. Every update increments the version, so even if the pointer is the same, the version will differ if an intermediate change occurred.

## Key Concepts
- **atomic64_t**: Used to store both the pointer and a version counter.
- **Pointer Tagging**: Lower bits store the pointer, upper bits store the version.
- **atomic64_cmpxchg**: Ensures lock-free, atomic updates.

## Usage
This code is for educational purposes and is not production-ready. In real kernel code, use kernel-provided lock-free primitives and memory barriers as appropriate.

## References
- [Linux Kernel Documentation: Lockless Algorithms](https://www.kernel.org/doc/html/latest/locking/lockless-design.html)
- [The ABA Problem](https://en.wikipedia.org/wiki/ABA_problem)

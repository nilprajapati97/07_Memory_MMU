# Q04: Kernel Linked List with Safe Deletion During Iteration

**Section:** Linux Kernel Internals | **Difficulty:** Easy-Medium | **Topics:** `list_head`, `list_for_each_entry_safe`, intrusive linked list, kernel data structures

---

## Question

Implement a kernel linked list using `list_head` and iterate safely while deleting.

---

## Answer

```c
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/module.h>

struct my_node {
    int              data;
    struct list_head list;   /* embedded list node — intrusive design */
};

/* Static list head (sentinel node) */
static LIST_HEAD(my_list);

/* Add a node to the tail of the list */
void add_node(int val)
{
    struct my_node *node = kmalloc(sizeof(*node), GFP_KERNEL);
    if (!node)
        return;
    node->data = val;
    list_add_tail(&node->list, &my_list);
}

/* Safe iteration: delete nodes where data is even */
void delete_even_nodes(void)
{
    struct my_node *node, *tmp;

    /*
     * list_for_each_entry_safe pre-fetches the NEXT pointer before
     * entering the loop body, so deleting 'node' is safe.
     * Plain list_for_each_entry would leave a dangling 'pos->list.next'
     * after deletion — undefined behaviour.
     */
    list_for_each_entry_safe(node, tmp, &my_list, list) {
        if (node->data % 2 == 0) {
            list_del(&node->list);
            kfree(node);
        }
    }
}

/* Print all nodes */
void print_list(void)
{
    struct my_node *node;
    list_for_each_entry(node, &my_list, list)
        pr_info("  node->data = %d\n", node->data);
}

/* Free entire list */
void free_list(void)
{
    struct my_node *node, *tmp;
    list_for_each_entry_safe(node, tmp, &my_list, list) {
        list_del(&node->list);
        kfree(node);
    }
}

static int __init list_demo_init(void)
{
    int i;
    for (i = 1; i <= 6; i++)
        add_node(i);

    pr_info("Before deletion:\n");
    print_list();

    delete_even_nodes();

    pr_info("After deleting evens:\n");
    print_list();

    free_list();
    return 0;
}

static void __exit list_demo_exit(void)
{
    free_list();
}

module_init(list_demo_init);
module_exit(list_demo_exit);
MODULE_LICENSE("GPL");
```

---

## Explanation

### Core Concept

The Linux kernel uses an **intrusive doubly-linked list** — instead of the list node containing a pointer to your data, your data structure embeds a `struct list_head`. The key macro `container_of` (used internally by `list_entry` and `list_for_each_entry`) recovers the outer struct pointer from the embedded `list_head` pointer:

```c
/* container_of recovers "struct my_node *" from "struct list_head *" */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)
```

This design allows one struct to be a member of multiple lists simultaneously (by embedding multiple `list_head` fields), and the list implementation has zero overhead for type casting.

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `LIST_HEAD(name)` | Declare and initialize a list head (sentinel) |
| `INIT_LIST_HEAD(ptr)` | Initialize a `list_head` at runtime |
| `list_add(&new, &head)` | Add after head (LIFO / stack) |
| `list_add_tail(&new, &head)` | Add before head = append to tail (FIFO / queue) |
| `list_del(&entry)` | Remove entry from list (poisons next/prev in debug builds) |
| `list_for_each_entry(pos, head, member)` | Iterate — NOT safe for deletion |
| `list_for_each_entry_safe(pos, tmp, head, member)` | Iterate — safe for deletion |
| `list_empty(&head)` | Returns true if list has no entries |
| `list_move(&entry, &head)` | Move entry to another list |
| `list_splice_init(&list, &head)` | Move all entries from `list` into `head`, reset `list` |

### Trade-offs & Pitfalls

- **`list_for_each_entry` for deletion = BUG.** After `list_del(node)`, `node->list.next` is poisoned (`LIST_POISON1`). The next iteration tries to follow it — crash.
- **Lock protection:** `list_head` provides no thread safety. Always protect with a spinlock (short operations) or mutex (if sleeping is acceptable).
- **`list_del_init` vs `list_del`:** `list_del_init` reinitializes the entry to an empty list after deletion. Useful when the struct is later re-added to a list. `list_del` leaves poisoned pointers (better for catching use-after-free).
- **`list_splice_init`:** Atomic list hand-off pattern — used in workqueue processing to move a shared list to a local list under a spinlock, then process locally without holding the lock.

### NVIDIA / GPU Context

NVIDIA GPU drivers use `list_head` for:
- **Per-context command queue:** pending GPU commands linked in submission order
- **DMA buffer tracking:** all mapped buffers for a process context in a list, freed on `close()`
- **Fence waiters:** list of tasks waiting for a GPU fence, woken by the IRQ handler
- **`list_splice_init` pattern** in IRQ handlers — quickly transfer the pending work list to a local list, release the spinlock, then process without holding it (reduces lock contention)

---

## Cross Questions & Answers

**CQ1: What is `container_of` and how does it work?**
> `container_of(ptr, type, member)` computes the address of the enclosing struct from a pointer to an embedded member. It subtracts `offsetof(type, member)` from `ptr`. For example, if `ptr` points to `node->list` and you know `list` is at offset 8 bytes into `struct my_node`, then `container_of` returns `ptr - 8`, giving you the `struct my_node *`. No runtime cost — pure pointer arithmetic.

**CQ2: How do you make a struct a member of two different lists simultaneously?**
> Embed two `list_head` fields in the struct:
> ```c
> struct gpu_buffer {
>     struct list_head lru_node;   /* for LRU eviction list */
>     struct list_head ctx_node;   /* for per-context tracking list */
> };
> ```
> Each list only touches its corresponding `list_head`. The same buffer can be in both lists independently.

**CQ3: What does `list_del` do in a debug kernel (`CONFIG_DEBUG_LIST`)?**
> In a debug build, `list_del` sets `entry->next = LIST_POISON1` (`0xDEAD000000000100`) and `entry->prev = LIST_POISON2` (`0xDEAD000000000200`). Any subsequent access to these pointers will immediately fault with an obvious address, making use-after-free bugs easy to diagnose. In a release kernel, it performs a minimal cleanup.

**CQ4: How does `list_splice_init` enable low-latency interrupt handlers?**
> In an interrupt handler, you often want to move pending work off a shared list quickly without holding the lock during processing. Pattern:
> ```c
> spin_lock(&shared_lock);
> list_splice_init(&shared_list, &local_list);  /* O(1) */
> spin_unlock(&shared_lock);
> /* Process local_list without any lock */
> list_for_each_entry_safe(...) { ... }
> ```
> This minimizes the lock hold time to a single pointer swap, not the entire processing loop.

**CQ5: How would you implement an O(1) move-to-front (LRU promotion) using `list_head`?**
> Use `list_move(&entry->lru, &lru_head)`. `list_move` is equivalent to `list_del` + `list_add` but is implemented as a single linked-list manipulation without dropping and re-acquiring the lock. This is exactly how the Linux page cache LRU works — a cache hit calls `list_move` to promote the page to the MRU end.

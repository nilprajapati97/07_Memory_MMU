# 17 — Kernel-Style Linked List (`list_head`)

## Problem
Implement a generic doubly-linked list where the **node** is embedded inside the object, not separately allocated. Used throughout the Linux kernel.

```c
struct list_head { struct list_head *next, *prev; };

struct task {
    int pid;
    struct list_head run_list;     // node embedded in the object
};
```

## Why It Matters
Intrusive lists give you one allocation per object (not two), type-flexible iteration, O(1) splice/remove, and no `void*` casts. Pattern repeats across kernels, network stacks, and high-perf user code.

## Approaches

### Approach 1 — Classic Non-Intrusive List (`struct node { T data; node *next; }`)
- One node + one object → two allocations, two cache lines.
- `void*` data → no type safety, casts everywhere.
- Same object can't appear on two lists without two separate node allocations.

### Approach 2 — Intrusive Singly-Linked
Embed `next` pointer inside object.
- Half the memory of doubly-linked.
- O(1) push/pop at head; O(n) middle removal.
- Used in slab freelists, simple stacks.

### Approach 3 — Intrusive Doubly-Linked — `list_head` (Linux Style)
Embed `struct list_head { next, prev }` in object. Operations work on `list_head*`; recover the object with `container_of`.
```text
LIST_HEAD_INIT(head)                  head.next = head.prev = &head
list_add(new, head)                   insert after head (push to front)
list_add_tail(new, head)              insert before head (push to back)
list_del(node)                        unlink (next/prev pointers updated)
list_for_each_entry(p, head, member)  iterate, with container_of
```
- Circular doubly-linked with sentinel `head`: empty list ⇔ `head.next == &head`. No NULL checks in the inner loop — a huge readability win.
- Same object can sit on N lists by embedding N `list_head` members.
- O(1) insert, delete, splice.

### Approach 4 — `hlist` (Hash Bucket Variant)
Doubly-linked but `prev` is a `**` pointing to the prior node's `next` field, saving one pointer per bucket head.
- Used for hash tables; halves head storage.
- Same O(1) operations.

### Approach 5 — RCU List
Same structure, but `next` updates use `rcu_assign_pointer` and reads use `rcu_dereference`. Readers walk locklessly; writers must wait for a grace period before freeing removed nodes.
- Read-side cost: one ordered load per step. Essentially free.
- Write-side cost: deferred reclamation.
- Linux's standard read-mostly list.

### Approach 6 — Lock-Free Doubly-Linked
Doubly-linked lock-free is hard (need double-CAS for both pointers atomically). Singly-linked lock-free (Harris) is feasible. Most production systems use RCU instead.

## ASCII — Empty vs Non-Empty
```
empty:   head ─┐
              v
              head  (next=prev=&head)

with A, B:
              head ⇄ A ⇄ B ⇄ head    (circular)
```

## Core Operations
```text
__list_add(new, prev, next):
    next->prev = new
    new->next  = next
    new->prev  = prev
    prev->next = new

list_del(entry):
    entry->prev->next = entry->next
    entry->next->prev = entry->prev
    entry->next = LIST_POISON1
    entry->prev = LIST_POISON2
```

## Comparison
| List | Memory/obj | Removal | Type safety | Multi-list | Concurrent reads |
|---|---|---|---|---|---|
| Non-intrusive SLL | data+ptr | O(n) | void* | extra alloc | needs lock |
| Intrusive SLL | one ptr | O(n) | typed | per member | needs lock |
| `list_head` DLL | two ptrs | O(1) | typed via container_of | per member | needs lock |
| `hlist` | one ptr (head saves one) | O(1) | typed | per member | needs lock |
| RCU `list_head` | two ptrs | O(1) | typed | per member | **lockless** |

## Key Insight
- "**Container-of + circular sentinel**" is the trick that makes intrusive lists ergonomic: no NULL checks, no type erasure, O(1) ops, and a single allocation per object.
- One object on multiple lists is just "embed more `list_head` members". This composability is why Linux uses it everywhere (per-CPU run queue, address space list, signal pending list, etc., on the same `task_struct`).
- RCU upgrades the same data structure to lock-free reads without changing the API.

## Pitfalls
- Forgetting to call `INIT_LIST_HEAD` → list iteration walks into garbage
- `list_del` then dereferencing the freed object → use `list_del_init` or `list_del_rcu` properly
- Iterating with `list_for_each_entry` and freeing entries inside → use `list_for_each_entry_safe` (caches `next` before body)
- Two `list_head` members in one struct → distinguish carefully; container_of must use the right `member`
- Concurrent modification without lock → list corruption; locks or RCU required
- RCU readers freeing memory directly → must use `call_rcu`/`synchronize_rcu`
- Confusing `hlist` (single-pointer head) with `list_head` (two-pointer head) — different APIs

## Interview Tips
1. Sketch the circular-sentinel diagram; explain why no NULL checks.
2. Show `container_of` + `list_for_each_entry`.
3. Mention that the same object can be on multiple lists via multiple member slots.
4. Bring up RCU for read-mostly workloads — that's senior-level depth.
5. Cite `list_for_each_entry_safe` for delete-during-iteration.

## Related / Follow-ups
- [15_container_of](../15_container_of/), [16_offsetof](../16_offsetof/)
- [06_reader_writer_lock](../06_reader_writer_lock/) (RCU section)
- Linux `include/linux/list.h`
- Boost.Intrusive (C++ equivalent)
- Harris lock-free linked list paper

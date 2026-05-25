# 15 — `container_of` Macro

## Problem
Given a pointer to a struct member, recover a pointer to the containing struct.
```c
struct work { int prio; struct list_head node; void (*fn)(void); };

void handler(struct list_head *n) {
    struct work *w = container_of(n, struct work, node);
    w->fn();
}
```

## Why It Matters
Foundation of intrusive data structures throughout the Linux kernel (`list_head`, `rb_node`, `hlist_node`), embedded systems, and any C code that wants generic containers without `void*` casts and a separate allocation.

## The Approaches

### Approach 1 — Classic Linux Macro
```c
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
```
Logic: subtract the byte offset of `member` within `type` from the member's address → start of the containing struct.

### Approach 2 — Type-Safe Version with `typeof` (GCC)
Linux actually uses:
```c
#define container_of(ptr, type, member) ({                       \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);         \
    (type *)( (char *)__mptr - offsetof(type, member) ); })
```
The temporary `__mptr` triggers a compile-time **type check**: passing the wrong member type → warning. The "statement-expression" `({ ... })` is a GCC extension.

### Approach 3 — Standard C Portable (No `typeof`)
```c
#define container_of(ptr, type, member) \
    ((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))
```
The `1 ? ptr : &((type*)0)->member` trick uses the conditional-operator type rules to verify `ptr` is compatible with `member`. Works in ISO C, no GCC extensions.

### Approach 4 — Hand Without the Macro
Conceptually: 
```text
member_addr - offsetof(type, member) → struct_start
```
You can write this directly; the macro just packages it safely.

## Why `offsetof`?
`offsetof(type, member)` from `<stddef.h>` returns the byte distance from the start of `type` to `member`, accounting for padding. Don't compute it by hand — packing/alignment differ across compilers.

## ASCII Layout
```
struct work {
    int                prio;     // offset 0,  size 4
    /* padding */                // offset 4,  size 4 (on 64-bit)
    struct list_head   node;     // offset 8,  size 16
    void (*fn)(void);            // offset 24
};

ptr to node ──────► |  |  |  |  |  |  |  |  | node |  |
                    ^                          ^
                    └──── offsetof(work,node) ─┘
container_of: node_ptr - 8  → start of work
```

## Comparison
| Variant | Type-check | Portability | Notes |
|---|---|---|---|
| Plain cast | none | ISO C | classic K&R style |
| `typeof` ({ }) | yes | GCC/Clang | Linux kernel default |
| Conditional trick | yes | ISO C | portable type-safety |

## Key Insight
- A struct's fields are at **fixed offsets** known at compile time → walking up from a member to the containing struct is just a constant subtract.
- This enables **intrusive containers**: the linked-list node lives **inside** your object; one allocation, no separate "node holding pointer to data".
- Combined with `typeof`, you get C++-template-like type safety without templates.

## Pitfalls
- Computing `offsetof` by hand → wrong on padded structs
- Casting `ptr` to wrong `type` → compiles, returns garbage; the `typeof` version catches this
- Member must be a real subobject (not a bit field — `offsetof` on bit fields is UB)
- Aliasing: if you have two structs both starting with `int x;`, recovering "the wrong type" gives UB even though offsets match
- `((type*)0)->member` is technically UB by the strictest reading — but `offsetof` macro is the standard sanctioned form; all compilers support it
- Doesn't help when the same `list_head` is on multiple lists belonging to different parents — keep separate node members per list

## Common Patterns
**Intrusive linked list (Linux kernel)**:
```c
struct list_head { struct list_head *next, *prev; };

#define list_for_each_entry(pos, head, member)              \
    for (pos = container_of((head)->next, typeof(*pos), member); \
         &pos->member != (head);                            \
         pos = container_of(pos->member.next, typeof(*pos), member))
```

**Callback recovery**:
```c
void timer_cb(struct timer *t) {
    struct device *d = container_of(t, struct device, watchdog);
    /* ... */
}
```

## Interview Tips
1. Write the simple version first; explain `offsetof` semantics.
2. Show the `typeof` version and explain why the type check matters.
3. Cite Linux `list_head` as the canonical user.
4. Mention bit-field exclusion and "wrong type" as the two common gotchas.

## Related / Follow-ups
- [16_offsetof](../16_offsetof/)
- [17_kernel_linked_list](../17_kernel_linked_list/)
- Linux `include/linux/container_of.h`, `list.h`
- Intrusive vs non-intrusive containers
- Generic programming in C without `void*`

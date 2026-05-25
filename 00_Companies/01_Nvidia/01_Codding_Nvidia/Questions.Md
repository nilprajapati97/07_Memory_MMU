Below are **Top 10 MUST practice coding problems with full C code** for an NVIDIA/Linux Kernel Engineer round. These match the common areas in your uploaded NVIDIA prep: `memcpy`, `memmove`, linked list, circular buffer, producer-consumer, refcounting, custom malloc, slab allocator, wait/timeout, and LRU-style design

---

# 1. Implement `memcpy`

```c
#include <stddef.h>
#include <stdint.h>

void *my_memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    while (n--)
        *d++ = *s++;

    return dst;
}
```

Follow-up: `memcpy` does **not** handle overlap. Use `memmove`.

---

# 2. Implement `memmove`

```c
#include <stddef.h>

void *my_memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    if (d == s || n == 0)
        return dst;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }

    return dst;
}
```

---

# 3. Reverse Linked List

```c
struct node {
    int data;
    struct node *next;
};

struct node *reverse_list(struct node *head)
{
    struct node *prev = 0;
    struct node *cur = head;
    struct node *next;

    while (cur) {
        next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }

    return prev;
}
```

---

# 4. Detect Cycle in Linked List

```c
int has_cycle(struct node *head)
{
    struct node *slow = head;
    struct node *fast = head;

    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;

        if (slow == fast)
            return 1;
    }

    return 0;
}
```

---

# 5. Circular Ring Buffer

```c
#define RING_SIZE 1024

struct ring {
    int buf[RING_SIZE];
    unsigned int head;
    unsigned int tail;
};

int ring_empty(struct ring *r)
{
    return r->head == r->tail;
}

int ring_full(struct ring *r)
{
    return ((r->head + 1) % RING_SIZE) == r->tail;
}

int ring_push(struct ring *r, int val)
{
    if (ring_full(r))
        return -1;

    r->buf[r->head] = val;
    r->head = (r->head + 1) % RING_SIZE;
    return 0;
}

int ring_pop(struct ring *r, int *val)
{
    if (ring_empty(r))
        return -1;

    *val = r->buf[r->tail];
    r->tail = (r->tail + 1) % RING_SIZE;
    return 0;
}
```

Kernel follow-up: protect with spinlock/mutex or use atomic head/tail for SPSC.

---

# 6. Lock-Free SPSC Queue

```c
#include <stdatomic.h>

#define QSIZE 1024

struct spsc_queue {
    int buf[QSIZE];
    atomic_uint head;
    atomic_uint tail;
};

int spsc_push(struct spsc_queue *q, int val)
{
    unsigned int head = atomic_load_explicit(&q->head, memory_order_relaxed);
    unsigned int next = (head + 1) % QSIZE;
    unsigned int tail = atomic_load_explicit(&q->tail, memory_order_acquire);

    if (next == tail)
        return -1;

    q->buf[head] = val;

    atomic_store_explicit(&q->head, next, memory_order_release);
    return 0;
}

int spsc_pop(struct spsc_queue *q, int *val)
{
    unsigned int tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    unsigned int head = atomic_load_explicit(&q->head, memory_order_acquire);

    if (tail == head)
        return -1;

    *val = q->buf[tail];

    atomic_store_explicit(&q->tail, (tail + 1) % QSIZE, memory_order_release);
    return 0;
}
```

Follow-up: explain `release/acquire` memory ordering.

---

# 7. Reference Counting

```c
#include <stdatomic.h>
#include <stdlib.h>

struct object {
    atomic_int refcnt;
    int data;
};

struct object *obj_create(int data)
{
    struct object *obj = malloc(sizeof(*obj));
    if (!obj)
        return 0;

    atomic_init(&obj->refcnt, 1);
    obj->data = data;
    return obj;
}

void obj_get(struct object *obj)
{
    atomic_fetch_add_explicit(&obj->refcnt, 1, memory_order_relaxed);
}

void obj_put(struct object *obj)
{
    if (atomic_fetch_sub_explicit(&obj->refcnt, 1, memory_order_acq_rel) == 1)
        free(obj);
}
```

Kernel answer: use `kref` or `refcount_t`, not raw `atomic_t`.

---

# 8. Custom `malloc/free` Using `sbrk`

This is a very common memory-manager question. Interviewers usually ask about **splitting and coalescing** for fragmentation handling

```c
#include <unistd.h>
#include <stddef.h>

typedef struct block {
    size_t size;
    int free;
    struct block *next;
} block_t;

#define ALIGN8(x) (((x) + 7) & ~7)
#define META_SIZE sizeof(block_t)

static block_t *heap_head = 0;

static block_t *find_free_block(size_t size)
{
    block_t *cur = heap_head;

    while (cur) {
        if (cur->free && cur->size >= size)
            return cur;
        cur = cur->next;
    }

    return 0;
}

static block_t *request_space(size_t size)
{
    void *ptr = sbrk(0);

    if (sbrk(META_SIZE + size) == (void *)-1)
        return 0;

    block_t *block = ptr;
    block->size = size;
    block->free = 0;
    block->next = 0;

    if (!heap_head) {
        heap_head = block;
    } else {
        block_t *cur = heap_head;
        while (cur->next)
            cur = cur->next;
        cur->next = block;
    }

    return block;
}

static void split_block(block_t *block, size_t size)
{
    if (block->size >= size + META_SIZE + 8) {
        block_t *new_block = (block_t *)((char *)(block + 1) + size);

        new_block->size = block->size - size - META_SIZE;
        new_block->free = 1;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }
}

void *my_malloc(size_t size)
{
    block_t *block;

    if (size == 0)
        return 0;

    size = ALIGN8(size);

    block = find_free_block(size);
    if (block) {
        block->free = 0;
        split_block(block, size);
        return block + 1;
    }

    block = request_space(size);
    if (!block)
        return 0;

    return block + 1;
}

static void coalesce(void)
{
    block_t *cur = heap_head;

    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += META_SIZE + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void my_free(void *ptr)
{
    block_t *block;

    if (!ptr)
        return;

    block = ((block_t *)ptr) - 1;
    block->free = 1;

    coalesce();
}
```

---

# 9. Fixed-Size Slab Allocator

Fixed-size slab allocation is especially relevant for GPU buffer objects, because it reduces general allocator overhead and avoids most external fragmentation

```c
#include <stdlib.h>
#include <stdint.h>

#define OBJ_SIZE 256
#define SLAB_SIZE 4096
#define OBJS_PER_SLAB (SLAB_SIZE / OBJ_SIZE)

struct slab {
    unsigned char *mem;
    uint32_t bitmap;
    int free_count;
    struct slab *next;
};

static struct slab *slab_list;

static struct slab *new_slab(void)
{
    struct slab *s = malloc(sizeof(*s));
    if (!s)
        return 0;

    s->mem = malloc(SLAB_SIZE);
    if (!s->mem) {
        free(s);
        return 0;
    }

    s->bitmap = 0;
    s->free_count = OBJS_PER_SLAB;
    s->next = slab_list;
    slab_list = s;

    return s;
}

void *slab_alloc(void)
{
    struct slab *s;
    int i;

    for (s = slab_list; s; s = s->next) {
        if (s->free_count > 0)
            break;
    }

    if (!s)
        s = new_slab();

    if (!s)
        return 0;

    for (i = 0; i < OBJS_PER_SLAB; i++) {
        if (!(s->bitmap & (1U << i))) {
            s->bitmap |= (1U << i);
            s->free_count--;
            return s->mem + i * OBJ_SIZE;
        }
    }

    return 0;
}

void slab_free(void *ptr)
{
    struct slab *s;

    for (s = slab_list; s; s = s->next) {
        if ((unsigned char *)ptr >= s->mem &&
            (unsigned char *)ptr < s->mem + SLAB_SIZE) {

            int idx = ((unsigned char *)ptr - s->mem) / OBJ_SIZE;
            s->bitmap &= ~(1U << idx);
            s->free_count++;
            return;
        }
    }
}
```

Follow-up: add partial/full/empty lists, cache coloring, per-CPU/per-NUMA caches.

---

# 10. LRU Cache

```c
#include <stdlib.h>

#define HASH_SIZE 128

struct lru_node {
    int key;
    int value;
    struct lru_node *prev;
    struct lru_node *next;
    struct lru_node *hnext;
};

struct lru_cache {
    int capacity;
    int size;
    struct lru_node *head;
    struct lru_node *tail;
    struct lru_node *hash[HASH_SIZE];
};

static int hash_key(int key)
{
    return key % HASH_SIZE;
}

static void remove_from_list(struct lru_cache *c, struct lru_node *n)
{
    if (n->prev)
        n->prev->next = n->next;
    else
        c->head = n->next;

    if (n->next)
        n->next->prev = n->prev;
    else
        c->tail = n->prev;
}

static void add_front(struct lru_cache *c, struct lru_node *n)
{
    n->prev = 0;
    n->next = c->head;

    if (c->head)
        c->head->prev = n;

    c->head = n;

    if (!c->tail)
        c->tail = n;
}

static struct lru_node *find_node(struct lru_cache *c, int key)
{
    int h = hash_key(key);
    struct lru_node *n = c->hash[h];

    while (n) {
        if (n->key == key)
            return n;
        n = n->hnext;
    }

    return 0;
}

int lru_get(struct lru_cache *c, int key, int *value)
{
    struct lru_node *n = find_node(c, key);

    if (!n)
        return -1;

    remove_from_list(c, n);
    add_front(c, n);

    *value = n->value;
    return 0;
}

void lru_put(struct lru_cache *c, int key, int value)
{
    struct lru_node *n = find_node(c, key);

    if (n) {
        n->value = value;
        remove_from_list(c, n);
        add_front(c, n);
        return;
    }

    n = malloc(sizeof(*n));
    n->key = key;
    n->value = value;

    int h = hash_key(key);
    n->hnext = c->hash[h];
    c->hash[h] = n;

    add_front(c, n);
    c->size++;

    if (c->size > c->capacity) {
        struct lru_node *victim = c->tail;
        remove_from_list(c, victim);
        free(victim);
        c->size--;
    }
}
```

For NVIDIA/Linux kernel, practice these in this order:

```text
1. memmove
2. linked list reverse + cycle
3. ring buffer
4. lock-free SPSC queue
5. refcounting
6. custom malloc/free
7. slab allocator
8. LRU cache
9. wait queue / timeout logic
10. producer-consumer with locking
```

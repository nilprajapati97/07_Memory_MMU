// Custom slab allocator for fixed-size GPU buffer objects
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#define SLAB_OBJ_SIZE 4096 // 4KB GPU buffer
#define SLAB_OBJ_COUNT 128

struct slab_obj {
    struct list_head list;
    char data[SLAB_OBJ_SIZE];
    int color; // For cache coloring
};

struct slab_cache {
    struct list_head free_list;
    spinlock_t lock;
    int color_next;
};

static struct slab_cache *slab_create(void) {
    struct slab_cache *cache = kmalloc(sizeof(*cache), GFP_KERNEL);
    INIT_LIST_HEAD(&cache->free_list);
    spin_lock_init(&cache->lock);
    cache->color_next = 0;
    for (int i = 0; i < SLAB_OBJ_COUNT; ++i) {
        struct slab_obj *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
        obj->color = i % 8; // 8 color bins for cache coloring
        list_add(&obj->list, &cache->free_list);
    }
    return cache;
}

static void *slab_alloc(struct slab_cache *cache) {
    struct slab_obj *obj = NULL;
    spin_lock(&cache->lock);
    if (!list_empty(&cache->free_list)) {
        obj = list_first_entry(&cache->free_list, struct slab_obj, list);
        list_del(&obj->list);
    }
    spin_unlock(&cache->lock);
    return obj ? obj->data : NULL;
}

static void slab_free(struct slab_cache *cache, void *ptr) {
    struct slab_obj *obj = container_of(ptr, struct slab_obj, data);
    spin_lock(&cache->lock);
    list_add(&obj->list, &cache->free_list);
    spin_unlock(&cache->lock);
}

static void slab_destroy(struct slab_cache *cache) {
    struct slab_obj *obj, *tmp;
    list_for_each_entry_safe(obj, tmp, &cache->free_list, list) {
        kfree(obj);
    }
    kfree(cache);
}

MODULE_LICENSE("GPL");

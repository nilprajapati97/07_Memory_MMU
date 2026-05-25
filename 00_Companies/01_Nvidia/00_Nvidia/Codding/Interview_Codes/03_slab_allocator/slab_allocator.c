/*
 * Custom Slab Allocator for Fixed-Size GPU Buffer Objects
 * Handles cache coloring and fragmentation
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/mm.h>

#define SLAB_OBJ_SIZE   256    // Example fixed object size (bytes)
#define SLAB_PAGE_ORDER 0      // Order 0 = 1 page per slab
#define SLAB_COLOR_NUM  8      // Number of cache colors

struct slab_obj {
    struct list_head list;
    void *addr;
};

struct slab_page {
    struct list_head list;
    struct list_head free_objs;
    int free_count;
    int color;
};

struct slab_cache {
    struct list_head slabs_full;
    struct list_head slabs_partial;
    struct list_head slabs_free;
    spinlock_t lock;
};

static struct slab_cache gpu_slab_cache;

static void slab_cache_init(struct slab_cache *cache) {
    INIT_LIST_HEAD(&cache->slabs_full);
    INIT_LIST_HEAD(&cache->slabs_partial);
    INIT_LIST_HEAD(&cache->slabs_free);
    spin_lock_init(&cache->lock);
}

static struct slab_page *slab_page_create(int color) {
    struct slab_page *page;
    void *mem;
    int i, num_objs = PAGE_SIZE / SLAB_OBJ_SIZE;
    struct slab_obj *obj;

    page = kmalloc(sizeof(*page), GFP_KERNEL);
    if (!page)
        return NULL;
    mem = (void *)__get_free_pages(GFP_KERNEL, SLAB_PAGE_ORDER);
    if (!mem) {
        kfree(page);
        return NULL;
    }
    INIT_LIST_HEAD(&page->list);
    INIT_LIST_HEAD(&page->free_objs);
    page->free_count = num_objs;
    page->color = color;
    for (i = 0; i < num_objs; ++i) {
        obj = kmalloc(sizeof(*obj), GFP_KERNEL);
        obj->addr = mem + i * SLAB_OBJ_SIZE + color * (PAGE_SIZE / SLAB_COLOR_NUM);
        list_add(&obj->list, &page->free_objs);
    }
    return page;
}

void *gpu_slab_alloc(void) {
    struct slab_page *page;
    struct slab_obj *obj;
    void *addr = NULL;
    unsigned long flags;
    int color = get_cpu() % SLAB_COLOR_NUM;

    spin_lock_irqsave(&gpu_slab_cache.lock, flags);
    list_for_each_entry(page, &gpu_slab_cache.slabs_partial, list) {
        if (page->color == color && page->free_count > 0) {
            obj = list_first_entry(&page->free_objs, struct slab_obj, list);
            list_del(&obj->list);
            addr = obj->addr;
            kfree(obj);
            page->free_count--;
            if (page->free_count == 0) {
                list_move(&page->list, &gpu_slab_cache.slabs_full);
            }
            spin_unlock_irqrestore(&gpu_slab_cache.lock, flags);
            return addr;
        }
    }
    // No partial slab, create new
    page = slab_page_create(color);
    if (!page) {
        spin_unlock_irqrestore(&gpu_slab_cache.lock, flags);
        return NULL;
    }
    list_add(&page->list, &gpu_slab_cache.slabs_partial);
    obj = list_first_entry(&page->free_objs, struct slab_obj, list);
    list_del(&obj->list);
    addr = obj->addr;
    kfree(obj);
    page->free_count--;
    spin_unlock_irqrestore(&gpu_slab_cache.lock, flags);
    return addr;
}

void gpu_slab_free(void *addr) {
    struct slab_page *page;
    struct slab_obj *obj;
    unsigned long flags;
    spin_lock_irqsave(&gpu_slab_cache.lock, flags);
    list_for_each_entry(page, &gpu_slab_cache.slabs_partial, list) {
        // In real code, track which page owns addr
        // Here, we assume all pages are checked
        // (For demo, not production)
        if (addr >= (void *)page && addr < (void *)page + PAGE_SIZE) {
            obj = kmalloc(sizeof(*obj), GFP_ATOMIC);
            obj->addr = addr;
            list_add(&obj->list, &page->free_objs);
            page->free_count++;
            if (page->free_count == (PAGE_SIZE / SLAB_OBJ_SIZE)) {
                list_move(&page->list, &gpu_slab_cache.slabs_free);
            }
            break;
        }
    }
    spin_unlock_irqrestore(&gpu_slab_cache.lock, flags);
}

static int __init slab_allocator_init(void) {
    slab_cache_init(&gpu_slab_cache);
    pr_info("Custom GPU slab allocator loaded\n");
    return 0;
}

static void __exit slab_allocator_exit(void) {
    pr_info("Custom GPU slab allocator unloaded\n");
}

module_init(slab_allocator_init);
module_exit(slab_allocator_exit);
MODULE_LICENSE("GPL");

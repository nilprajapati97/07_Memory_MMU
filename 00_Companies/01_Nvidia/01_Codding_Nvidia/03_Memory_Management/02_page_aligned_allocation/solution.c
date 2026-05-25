// Page-aligned allocation logic
#include <linux/slab.h>
#include <linux/mm.h>

void *alloc_page_aligned(size_t size) {
    return kmalloc(PAGE_ALIGN(size), GFP_KERNEL);
}

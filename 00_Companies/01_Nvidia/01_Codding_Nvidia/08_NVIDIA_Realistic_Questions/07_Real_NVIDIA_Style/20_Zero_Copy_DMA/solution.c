// Zero-copy DMA buffer mapping in the Linux kernel
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>

#define DMA_BUF_SIZE 4096

static int map_user_dma(struct device *dev, unsigned long user_addr) {
    struct page *pages[1];
    int ret = get_user_pages_fast(user_addr, 1, 1, pages);
    if (ret < 1) {
        pr_err("Failed to pin user page\n");
        return -EFAULT;
    }
    dma_addr_t dma_handle = dma_map_page(dev, pages[0], 0, DMA_BUF_SIZE, DMA_BIDIRECTIONAL);
    if (dma_mapping_error(dev, dma_handle)) {
        pr_err("DMA mapping failed\n");
        put_page(pages[0]);
        return -EIO;
    }
    // Use dma_handle for device DMA
    dma_unmap_page(dev, dma_handle, DMA_BUF_SIZE, DMA_BIDIRECTIONAL);
    put_page(pages[0]);
    return 0;
}

MODULE_LICENSE("GPL");

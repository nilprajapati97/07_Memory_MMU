// Allocate and use a DMA-coherent buffer in a Linux device driver
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#define DMA_BUF_SIZE 4096

static void *dma_buf;
static dma_addr_t dma_handle;
static struct device *dev;

static int __init dma_example_init(void) {
    // Assume dev is initialized elsewhere (e.g., platform device)
    dma_buf = dma_alloc_coherent(dev, DMA_BUF_SIZE, &dma_handle, GFP_KERNEL);
    if (!dma_buf) {
        pr_err("Failed to allocate DMA-coherent buffer\n");
        return -ENOMEM;
    }
    pr_info("DMA buffer allocated: virt=%p, phys=%pad\n", dma_buf, &dma_handle);
    // Use dma_buf for device operations
    return 0;
}

static void __exit dma_example_exit(void) {
    if (dma_buf)
        dma_free_coherent(dev, DMA_BUF_SIZE, dma_buf, dma_handle);
    pr_info("DMA buffer freed\n");
}

module_init(dma_example_init);
module_exit(dma_example_exit);
MODULE_LICENSE("GPL");

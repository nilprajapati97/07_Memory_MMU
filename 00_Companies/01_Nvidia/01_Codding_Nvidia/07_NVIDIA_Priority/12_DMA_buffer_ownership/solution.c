// DMA buffer ownership transfer (pseudo-code)
#include <linux/dma-mapping.h>
#include <linux/device.h>

void transfer_dma_ownership(struct device *dev, void *buf, size_t size) {
    dma_sync_single_for_device(dev, virt_to_phys(buf), size, DMA_TO_DEVICE);
    // Device can now access the buffer
    // ...
    dma_sync_single_for_cpu(dev, virt_to_phys(buf), size, DMA_FROM_DEVICE);
    // CPU can now access the buffer
}

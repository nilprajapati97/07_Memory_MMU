# Q36: Explain IOMMU and DMA address translation

## In-depth Explanation (Nvidia Interview Style)

- IOMMU (Input-Output Memory Management Unit) translates device-visible addresses to physical memory addresses, enabling device isolation and security.
- DMA (Direct Memory Access) allows devices to read/write memory directly, using addresses translated by the IOMMU.

### Why is this important for Nvidia/Linux Kernel?
- Critical for GPU, PCIe, and high-performance device drivers.
- Enables features like device passthrough and secure memory sharing.

### Interview Tips
- Be ready to discuss scatter-gather, address translation, and security implications.

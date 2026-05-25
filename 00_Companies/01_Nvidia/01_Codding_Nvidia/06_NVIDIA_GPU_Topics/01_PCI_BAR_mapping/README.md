# Q37: How does PCI BAR mapping work?

## In-depth Explanation (Nvidia Interview Style)

- PCI Base Address Registers (BARs) define the memory or I/O regions used by PCI devices.
- The kernel reads BARs to map device memory into the system address space.

### Why is this important for Nvidia/Linux Kernel?
- Essential for device driver initialization and MMIO access.

### Interview Tips
- Be ready to discuss BAR types, resource allocation, and mapping APIs.

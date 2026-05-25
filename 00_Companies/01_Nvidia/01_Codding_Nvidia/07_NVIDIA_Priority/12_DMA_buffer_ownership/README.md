# DMA Buffer Ownership

## In-depth Explanation (Nvidia Interview Style)
- DMA buffer ownership must be transferred between CPU and device to avoid cache coherency bugs.
- Use proper DMA APIs for synchronization.

### Interview Tips
- Discuss cache management, DMA mapping APIs, and real-world driver bugs.

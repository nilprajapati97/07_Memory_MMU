# Q38: Explain ioremap() and MMIO access

## In-depth Explanation (Nvidia Interview Style)

- `ioremap()` maps physical device memory into kernel virtual address space for MMIO (Memory-Mapped I/O).
- Used to safely access device registers from kernel code.

### Interview Tips
- Be ready to discuss unmapping, cache attributes, and safe register access.

# Linux Kernel Memory APIs

---

## 1. Basic Allocation (Most Common)

```c
kmalloc()
kzalloc()
kfree()
```

---

## 2. Large Memory Allocation

```c
vmalloc()
vfree()
```

---

## 3. Slab Allocator (Efficient Object Allocation)

```c
kmem_cache_create()
kmem_cache_alloc()
kmem_cache_free()
kmem_cache_destroy()
```

---

## 4. Page-Level Allocation

```c
alloc_pages()
__get_free_pages()
free_pages()
```

---

## 5. User Space ↔ Kernel Space

```c
copy_to_user()
copy_from_user()
```

---

## 6. Memory Mapping

```c
mmap()           /* via file operations */
remap_pfn_range()
```

---

## 7. DMA Memory Allocation

```c
dma_alloc_coherent()
dma_free_coherent()
```


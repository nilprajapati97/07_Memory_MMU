# NVIDIA Coding Round — Linux Kernel (9 Years Experience)
## 50 Questions & Answers

---

## Section 1: Linux Kernel Internals (Q1–Q10)

---

**Q1. Implement a custom kernel module that uses a spinlock to protect a shared counter incremented by multiple CPUs.**

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/spinlock.h>

static spinlock_t counter_lock;
static atomic64_t shared_counter;

static void increment_counter(void *info)
{
    unsigned long flags;
    spin_lock_irqsave(&counter_lock, flags);
    atomic64_inc(&shared_counter);
    spin_unlock_irqrestore(&counter_lock, flags);
}

static int __init counter_init(void)
{
    spin_lock_init(&counter_lock);
    atomic64_set(&shared_counter, 0);
    on_each_cpu(increment_counter, NULL, 1);
    pr_info("Counter value: %lld\n", atomic64_read(&shared_counter));
    return 0;
}

static void __exit counter_exit(void)
{
    pr_info("Module removed\n");
}

module_init(counter_init);
module_exit(counter_exit);
MODULE_LICENSE("GPL");
```

**Key Concepts:** `spin_lock_irqsave` disables IRQs and saves flags — mandatory in interrupt context. Use `atomic64_t` when possible to avoid spinlocks for simple counters.

---

**Q2. Design and implement a lock-free ring buffer (SPSC) for kernel use.**

```c
#include <linux/slab.h>
#include <linux/atomic.h>

#define RING_SIZE 1024  /* Must be power of 2 */
#define RING_MASK (RING_SIZE - 1)

struct spsc_ring {
    void        *buf[RING_SIZE];
    atomic_t     head;  /* producer */
    atomic_t     tail;  /* consumer */
};

struct spsc_ring *ring_alloc(void)
{
    struct spsc_ring *r = kzalloc(sizeof(*r), GFP_KERNEL);
    if (!r)
        return NULL;
    atomic_set(&r->head, 0);
    atomic_set(&r->tail, 0);
    return r;
}

/* Producer: returns 0 on success, -ENOSPC if full */
int ring_push(struct spsc_ring *r, void *item)
{
    int head = atomic_read(&r->head);
    int next = (head + 1) & RING_MASK;

    if (next == atomic_read(&r->tail))
        return -ENOSPC;

    r->buf[head] = item;
    smp_store_release(&r->head.counter, next); /* release barrier */
    return 0;
}

/* Consumer: returns NULL if empty */
void *ring_pop(struct spsc_ring *r)
{
    int tail = atomic_read(&r->tail);
    void *item;

    if (tail == smp_load_acquire(&r->head.counter))
        return NULL;

    item = r->buf[tail];
    atomic_set(&r->tail, (tail + 1) & RING_MASK);
    return item;
}
```

**Key Concepts:** `smp_store_release` / `smp_load_acquire` provide memory ordering without full barriers. SPSC requires no locks — safe with one producer and one consumer thread.

---

**Q3. What is the difference between `kmalloc`, `vmalloc`, and `get_free_pages`? When do you use each?**

```c
/* kmalloc — physically contiguous, < 4MB, for DMA/hardware */
void *buf = kmalloc(256, GFP_KERNEL);

/* vmalloc — virtually contiguous, large allocations, NOT for DMA */
void *vbuf = vmalloc(10 * 1024 * 1024); /* 10MB */

/* get_free_pages — raw page allocator, order = log2(pages) */
unsigned long page = __get_free_pages(GFP_KERNEL, 2); /* 4 pages = 16KB */
free_pages(page, 2);
```

| API | Physical Contiguous | Max Size | DMA Safe | Overhead |
|-----|--------------------|---------|---------| ---------|
| `kmalloc` | Yes | ~4MB | Yes | Low |
| `vmalloc` | No | GB | No | High (page table) |
| `get_free_pages` | Yes | ~4MB | Yes | Medium |

**NVIDIA context:** DMA allocations to GPU require `dma_alloc_coherent` for proper IOMMU mapping.

---

**Q4. Implement a kernel linked list using `list_head` and iterate safely while deleting.**

```c
#include <linux/list.h>
#include <linux/slab.h>

struct my_node {
    int           data;
    struct list_head list;
};

static LIST_HEAD(my_list);

void add_node(int val)
{
    struct my_node *node = kmalloc(sizeof(*node), GFP_KERNEL);
    node->data = val;
    list_add_tail(&node->list, &my_list);
}

/* Safe deletion during iteration using list_for_each_entry_safe */
void delete_even_nodes(void)
{
    struct my_node *node, *tmp;

    list_for_each_entry_safe(node, tmp, &my_list, list) {
        if (node->data % 2 == 0) {
            list_del(&node->list);
            kfree(node);
        }
    }
}
```

**Key Concept:** `list_for_each_entry` is NOT safe for deletion. Always use `list_for_each_entry_safe` which pre-fetches the next pointer.

---

**Q5. Explain and implement RCU (Read-Copy-Update) for a read-heavy data structure.**

```c
#include <linux/rcupdate.h>
#include <linux/slab.h>

struct config {
    int  threshold;
    char name[64];
    struct rcu_head rcu; /* for deferred free */
};

static struct config __rcu *global_cfg;

/* Reader — no locks, very fast */
int read_threshold(void)
{
    struct config *cfg;
    int val;

    rcu_read_lock();
    cfg = rcu_dereference(global_cfg);
    val = cfg ? cfg->threshold : 0;
    rcu_read_unlock();
    return val;
}

/* Writer — allocate new, swap pointer, free old after grace period */
int update_threshold(int new_val)
{
    struct config *new_cfg, *old_cfg;

    new_cfg = kmalloc(sizeof(*new_cfg), GFP_KERNEL);
    if (!new_cfg)
        return -ENOMEM;

    /* Copy old data */
    rcu_read_lock();
    old_cfg = rcu_dereference(global_cfg);
    if (old_cfg)
        memcpy(new_cfg, old_cfg, sizeof(*new_cfg));
    rcu_read_unlock();

    new_cfg->threshold = new_val;

    old_cfg = rcu_replace_pointer(global_cfg, new_cfg,
                                   lockdep_is_held(&update_lock));
    if (old_cfg)
        kfree_rcu(old_cfg, rcu); /* free after all readers finish */
    return 0;
}
```

**Key Concept:** RCU allows unlimited concurrent readers with zero locking. Writers pay the cost. `kfree_rcu` waits for a grace period before freeing.

---

**Q6. Implement a kernel work queue that batches GPU command completions.**

```c
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/spinlock.h>

struct gpu_completion {
    u64              fence_id;
    struct list_head list;
};

static struct workqueue_struct *gpu_wq;
static struct work_struct       completion_work;
static LIST_HEAD(pending_completions);
static DEFINE_SPINLOCK(comp_lock);

static void process_completions(struct work_struct *work)
{
    struct gpu_completion *comp, *tmp;
    LIST_HEAD(local_list);

    spin_lock(&comp_lock);
    list_splice_init(&pending_completions, &local_list);
    spin_unlock(&comp_lock);

    list_for_each_entry_safe(comp, tmp, &local_list, list) {
        pr_info("Fence %llu completed\n", comp->fence_id);
        list_del(&comp->list);
        kfree(comp);
    }
}

/* Called from interrupt context */
irqreturn_t gpu_irq_handler(int irq, void *dev)
{
    struct gpu_completion *comp = kmalloc(sizeof(*comp), GFP_ATOMIC);
    comp->fence_id = read_fence_register();

    spin_lock(&comp_lock);
    list_add_tail(&comp->list, &pending_completions);
    spin_unlock(&comp_lock);

    queue_work(gpu_wq, &completion_work);
    return IRQ_HANDLED;
}

static int __init gpu_init(void)
{
    gpu_wq = alloc_workqueue("gpu_wq", WQ_UNBOUND | WQ_HIGHPRI, 0);
    INIT_WORK(&completion_work, process_completions);
    return 0;
}
```

---

**Q7. What is the difference between `mutex`, `semaphore`, `spinlock`, and `rwlock`? Write kernel code showing proper usage.**

```c
/* SPINLOCK: Short critical sections, can be used in IRQ context */
static DEFINE_SPINLOCK(hw_lock);
void update_hw_reg(u32 val)
{
    unsigned long flags;
    spin_lock_irqsave(&hw_lock, flags);
    writel(val, reg_base);
    spin_unlock_irqrestore(&hw_lock, flags);
}

/* MUTEX: Sleepable lock, process context only, supports priority inheritance */
static DEFINE_MUTEX(dev_mutex);
int open_device(void)
{
    if (mutex_lock_interruptible(&dev_mutex))
        return -ERESTARTSYS;
    /* ... */
    mutex_unlock(&dev_mutex);
    return 0;
}

/* RWLOCK: Multiple readers OR one writer */
static DEFINE_RWLOCK(map_lock);
u64 read_map(int key)
{
    unsigned long flags;
    u64 val;
    read_lock_irqsave(&map_lock, flags);
    val = lookup(key);
    read_unlock_irqrestore(&map_lock, flags);
    return val;
}

/* SEMAPHORE: Counting, use for resource pools */
static DEFINE_SEMAPHORE(gpu_slots, 4); /* 4 DMA channels */
int acquire_dma_channel(void)
{
    return down_interruptible(&gpu_slots);
}
```

---

**Q8. Implement a character device driver with `read`, `write`, and `ioctl` support.**

```c
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "nvidia_sample"
#define BUF_SIZE    4096
#define IOC_RESET   _IO('N', 1)
#define IOC_GETSTAT _IOR('N', 2, u64)

static char   dev_buf[BUF_SIZE];
static size_t buf_len;
static dev_t  dev_num;
static struct cdev my_cdev;

static ssize_t my_read(struct file *f, char __user *ubuf,
                        size_t len, loff_t *off)
{
    size_t to_copy = min(len, buf_len - (size_t)*off);
    if (copy_to_user(ubuf, dev_buf + *off, to_copy))
        return -EFAULT;
    *off += to_copy;
    return to_copy;
}

static ssize_t my_write(struct file *f, const char __user *ubuf,
                         size_t len, loff_t *off)
{
    size_t to_copy = min(len, BUF_SIZE - (size_t)*off);
    if (copy_from_user(dev_buf + *off, ubuf, to_copy))
        return -EFAULT;
    buf_len = *off + to_copy;
    *off += to_copy;
    return to_copy;
}

static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    u64 stat;
    switch (cmd) {
    case IOC_RESET:
        memset(dev_buf, 0, BUF_SIZE);
        buf_len = 0;
        return 0;
    case IOC_GETSTAT:
        stat = buf_len;
        if (copy_to_user((void __user *)arg, &stat, sizeof(stat)))
            return -EFAULT;
        return 0;
    default:
        return -ENOTTY;
    }
}

static const struct file_operations my_fops = {
    .owner          = THIS_MODULE,
    .read           = my_read,
    .write          = my_write,
    .unlocked_ioctl = my_ioctl,
};
```

**Security Note:** Always use `copy_from_user` / `copy_to_user` — never dereference user pointers directly. Validate `ioctl` commands to prevent TOCTOU attacks.

---

**Q9. Explain interrupt handling — top half vs bottom half. Implement a tasklet.**

```c
#include <linux/interrupt.h>

static struct tasklet_struct gpu_tasklet;
static u32 saved_status;

/* Bottom half — runs in softirq context, non-preemptible */
static void gpu_tasklet_fn(unsigned long data)
{
    u32 status = (u32)data;
    if (status & GPU_FRAME_DONE)
        signal_frame_complete();
    if (status & GPU_DMA_DONE)
        unmap_dma_buffer();
}

/* Top half — runs in hard IRQ context, must be FAST */
static irqreturn_t gpu_irq_top(int irq, void *dev_id)
{
    u32 status = readl(dev_id + IRQ_STATUS_REG);
    writel(status, dev_id + IRQ_CLEAR_REG); /* ACK immediately */

    tasklet_schedule(&gpu_tasklet); /* defer heavy work */
    return IRQ_HANDLED;
}

static int __init gpu_irq_init(void)
{
    tasklet_init(&gpu_tasklet, gpu_tasklet_fn, 0);
    return request_irq(GPU_IRQ, gpu_irq_top,
                        IRQF_SHARED, "gpu", reg_base);
}
```

| | Top Half | Bottom Half (Tasklet/Workqueue) |
|--|----------|-------------------------------|
| Context | Hard IRQ | Softirq / Process |
| Can sleep | No | Workqueue only |
| Latency | Microseconds | Milliseconds OK |

---

**Q10. Implement kernel memory-mapped I/O with `ioremap` and barrier usage.**

```c
#include <linux/io.h>

#define GPU_BAR0_BASE  0xF0000000ULL
#define GPU_BAR0_SIZE  (16 * 1024 * 1024)
#define REG_CTRL       0x000
#define REG_STATUS     0x004
#define REG_DMA_ADDR   0x100

void __iomem *bar0;

int gpu_mmio_init(void)
{
    bar0 = ioremap(GPU_BAR0_BASE, GPU_BAR0_SIZE);
    if (!bar0)
        return -ENOMEM;
    return 0;
}

void gpu_start_dma(dma_addr_t addr, u32 len)
{
    writel((u32)(addr & 0xFFFFFFFF), bar0 + REG_DMA_ADDR);
    writel((u32)(addr >> 32),        bar0 + REG_DMA_ADDR + 4);
    writel(len,                      bar0 + REG_DMA_ADDR + 8);
    /* mmiowb() ensures write ordering on weakly-ordered archs */
    wmb();
    writel(CTRL_DMA_START, bar0 + REG_CTRL);
}

bool gpu_is_ready(void)
{
    return (readl(bar0 + REG_STATUS) & STATUS_READY) != 0;
}

void gpu_mmio_exit(void)
{
    iounmap(bar0);
}
```

---

## Section 2: Memory Management (Q11–Q18)

---

**Q11. Explain the Linux page allocator — Buddy system. Implement a page cache with LRU eviction.**

```c
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/slab.h>

#define CACHE_BITS  8
#define MAX_ENTRIES 128

struct cache_entry {
    u64              key;
    struct page      *page;
    struct list_head lru;
    struct hlist_node hash;
};

static DEFINE_HASHTABLE(cache_ht, CACHE_BITS);
static LIST_HEAD(lru_list);
static int cache_count;
static DEFINE_MUTEX(cache_lock);

static void evict_lru(void)
{
    struct cache_entry *victim;
    victim = list_last_entry(&lru_list, struct cache_entry, lru);
    hash_del(&victim->hash);
    list_del(&victim->lru);
    __free_page(victim->page);
    kfree(victim);
    cache_count--;
}

struct page *cache_get(u64 key)
{
    struct cache_entry *e;

    mutex_lock(&cache_lock);
    hash_for_each_possible(cache_ht, e, hash, key) {
        if (e->key == key) {
            list_move(&e->lru, &lru_list); /* promote to MRU */
            mutex_unlock(&cache_lock);
            return e->page;
        }
    }
    mutex_unlock(&cache_lock);
    return NULL;
}

int cache_insert(u64 key, struct page *page)
{
    struct cache_entry *e = kmalloc(sizeof(*e), GFP_KERNEL);
    if (!e)
        return -ENOMEM;

    e->key  = key;
    e->page = page;

    mutex_lock(&cache_lock);
    if (cache_count >= MAX_ENTRIES)
        evict_lru();
    hash_add(cache_ht, &e->hash, key);
    list_add(&e->lru, &lru_list);
    cache_count++;
    mutex_unlock(&cache_lock);
    return 0;
}
```

---

**Q12. What is huge pages / THP in Linux? Write kernel code to allocate a 2MB huge page.**

```c
#include <linux/mm.h>
#include <linux/huge_mm.h>

/* Allocate a 2MB THP-backed anonymous region from kernel */
struct page *alloc_huge_page_kernel(void)
{
    /* order 9 = 512 pages = 2MB on x86_64 */
    struct page *page = alloc_pages(GFP_KERNEL | __GFP_COMP, 
                                     HPAGE_PMD_ORDER);
    if (!page)
        return NULL;

    pr_info("Huge page PA: %llx, size: %lu KB\n",
            (u64)page_to_phys(page),
            PAGE_SIZE << HPAGE_PMD_ORDER >> 10);
    return page;
}

void free_huge_page_kernel(struct page *page)
{
    __free_pages(page, HPAGE_PMD_ORDER);
}

/* For userspace driver (NVIDIA GPU typically uses this path) */
static vm_fault_t gpu_vm_fault(struct vm_fault *vmf)
{
    struct page *page = alloc_pages(GFP_KERNEL, 0);
    if (!page)
        return VM_FAULT_OOM;
    get_page(page);
    vmf->page = page;
    return 0;
}
```

**NVIDIA Context:** CUDA uses huge pages extensively for GPU memory mappings to reduce TLB pressure and improve bandwidth.

---

**Q13. Implement a slab cache for fixed-size GPU descriptor objects.**

```c
#include <linux/slab.h>

struct gpu_descriptor {
    u64  va;          /* virtual address */
    u64  pa;          /* physical address */
    u32  size;
    u32  flags;
    u8   reserved[48];
};

static struct kmem_cache *desc_cache;

int descriptor_cache_init(void)
{
    desc_cache = kmem_cache_create(
        "gpu_descriptor",
        sizeof(struct gpu_descriptor),
        __alignof__(struct gpu_descriptor),
        SLAB_HWCACHE_ALIGN | SLAB_POISON, /* poison detects use-after-free */
        NULL /* constructor */
    );
    return desc_cache ? 0 : -ENOMEM;
}

struct gpu_descriptor *descriptor_alloc(void)
{
    return kmem_cache_zalloc(desc_cache, GFP_KERNEL);
}

void descriptor_free(struct gpu_descriptor *desc)
{
    kmem_cache_free(desc_cache, desc);
}

void descriptor_cache_destroy(void)
{
    kmem_cache_destroy(desc_cache);
}
```

**Advantage over `kmalloc`:** Eliminates fragmentation, provides better cache locality, `SLAB_POISON` aids debugging.

---

**Q14. Implement CMA (Contiguous Memory Allocator) usage for GPU DMA.**

```c
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>

#define DMA_BUF_SIZE (64 * 1024 * 1024) /* 64MB */

struct gpu_dma_buf {
    void        *vaddr;
    dma_addr_t   dma_handle;
    size_t       size;
};

int gpu_dma_alloc(struct device *dev, struct gpu_dma_buf *buf)
{
    buf->size   = DMA_BUF_SIZE;
    buf->vaddr  = dma_alloc_coherent(dev, buf->size,
                                      &buf->dma_handle, GFP_KERNEL);
    if (!buf->vaddr)
        return -ENOMEM;

    pr_info("DMA buf: vaddr=%p, dma_addr=%pad, size=%zuMB\n",
            buf->vaddr, &buf->dma_handle, buf->size >> 20);
    return 0;
}

/* Streaming DMA — for non-coherent architectures */
int gpu_dma_map_streaming(struct device *dev, void *vaddr, size_t size,
                           dma_addr_t *dma_addr)
{
    *dma_addr = dma_map_single(dev, vaddr, size, DMA_TO_DEVICE);
    if (dma_mapping_error(dev, *dma_addr))
        return -EIO;
    return 0;
}

void gpu_dma_free(struct device *dev, struct gpu_dma_buf *buf)
{
    dma_free_coherent(dev, buf->size, buf->vaddr, buf->dma_handle);
}
```

---

**Q15. What is IOMMU and how does it affect GPU DMA? Implement IOMMU group management.**

```c
#include <linux/iommu.h>
#include <linux/pci.h>

/* Attach a PCI device to a custom IOMMU domain */
int setup_iommu_domain(struct pci_dev *pdev)
{
    struct iommu_domain *domain;
    int ret;

    domain = iommu_domain_alloc(pdev->dev.bus);
    if (!domain)
        return -ENOMEM;

    ret = iommu_attach_device(domain, &pdev->dev);
    if (ret) {
        iommu_domain_free(domain);
        return ret;
    }

    /* Map GPU VRAM into IOVA space */
    ret = iommu_map(domain,
                    0x100000000ULL,  /* IOVA */
                    0xC0000000ULL,   /* PA of GPU VRAM */
                    256 * 1024 * 1024,
                    IOMMU_READ | IOMMU_WRITE,
                    GFP_KERNEL);
    if (ret) {
        iommu_detach_device(domain, &pdev->dev);
        iommu_domain_free(domain);
        return ret;
    }
    return 0;
}
```

**Key Point:** IOMMU isolates DMA — a buggy/malicious PCIe device cannot access arbitrary system memory. Critical for GPU virtualization (SR-IOV, vGPU).

---

**Q16. Implement a memory pressure notifier to reclaim GPU memory under OOM.**

```c
#include <linux/shrinker.h>

static unsigned long gpu_shrink_count(struct shrinker *s,
                                       struct shrink_control *sc)
{
    return atomic_long_read(&gpu_cached_pages);
}

static unsigned long gpu_shrink_scan(struct shrinker *s,
                                      struct shrink_control *sc)
{
    unsigned long freed = 0;
    struct gpu_buffer *buf, *tmp;

    mutex_lock(&gpu_buf_lock);
    list_for_each_entry_safe(buf, tmp, &gpu_lru, lru) {
        if (freed >= sc->nr_to_scan)
            break;
        if (buf->refcount == 0) {
            gpu_free_buffer(buf);
            freed++;
        }
    }
    mutex_unlock(&gpu_buf_lock);
    return freed;
}

static struct shrinker gpu_shrinker = {
    .count_objects = gpu_shrink_count,
    .scan_objects  = gpu_shrink_scan,
    .seeks         = DEFAULT_SEEKS,
};

static int __init gpu_shrinker_init(void)
{
    return register_shrinker(&gpu_shrinker, "nvidia-gpu");
}
```

---

**Q17. Explain the difference between `GFP_KERNEL`, `GFP_ATOMIC`, `GFP_DMA`, `GFP_NOWAIT`.**

```c
/*
 * GFP_KERNEL   — Normal allocation. Can sleep. Used in process context.
 * GFP_ATOMIC   — Cannot sleep. Used in IRQ/softirq context.
 * GFP_DMA      — Allocate from DMA zone (0-16MB on x86). For old ISA DMA.
 * GFP_DMA32    — Allocate below 4GB. For 32-bit DMA devices.
 * GFP_NOWAIT   — Like GFP_ATOMIC but without high-memory fallback.
 * GFP_HIGHUSER — Allocate from high memory for user pages.
 */

irqreturn_t irq_handler(int irq, void *dev)
{
    /* WRONG: GFP_KERNEL can sleep, forbidden in IRQ context */
    // void *p = kmalloc(64, GFP_KERNEL); /* BUG! */

    /* CORRECT: GFP_ATOMIC never sleeps */
    void *p = kmalloc(64, GFP_ATOMIC);
    return IRQ_HANDLED;
}

int process_context_alloc(void)
{
    /* Prefer GFP_KERNEL — allows reclaim, compaction */
    void *p = kmalloc(1024, GFP_KERNEL);
    return p ? 0 : -ENOMEM;
}
```

---

**Q18. Implement a custom mmap handler for exposing GPU framebuffer to userspace.**

```c
#include <linux/mm.h>

static int gpu_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size     = vma->vm_end - vma->vm_start;
    unsigned long pfn_base = GPU_FRAMEBUF_PA >> PAGE_SHIFT;

    if (size > GPU_FRAMEBUF_SIZE)
        return -EINVAL;

    /* Non-cacheable, write-combining for GPU MMIO regions */
    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
    vma->vm_flags    |= VM_IO | VM_PFNMAP | VM_DONTEXPAND;

    if (remap_pfn_range(vma, vma->vm_start,
                         pfn_base + vma->vm_pgoff,
                         size, vma->vm_page_prot))
        return -EAGAIN;

    return 0;
}

static const struct file_operations gpu_fops = {
    .mmap = gpu_mmap,
};
```

---

## Section 3: Concurrency & Synchronization (Q19–Q25)

---

**Q19. Implement a wait queue for GPU fence completion.**

```c
#include <linux/wait.h>
#include <linux/atomic.h>

struct gpu_fence {
    atomic64_t       seqno;
    wait_queue_head_t wq;
};

void gpu_fence_init(struct gpu_fence *fence)
{
    atomic64_set(&fence->seqno, 0);
    init_waitqueue_head(&fence->wq);
}

/* Called from IRQ when GPU signals fence */
void gpu_fence_signal(struct gpu_fence *fence, u64 val)
{
    atomic64_set(&fence->seqno, val);
    wake_up_all(&fence->wq);
}

/* Wait for fence with timeout (jiffies) */
int gpu_fence_wait(struct gpu_fence *fence, u64 target, long timeout_ms)
{
    long ret = wait_event_interruptible_timeout(
        fence->wq,
        atomic64_read(&fence->seqno) >= target,
        msecs_to_jiffies(timeout_ms)
    );

    if (ret == 0) return -ETIMEDOUT;
    if (ret < 0)  return -ERESTARTSYS;
    return 0;
}
```

---

**Q20. Implement a ticket spinlock to guarantee FIFO ordering.**

```c
#include <linux/atomic.h>
#include <linux/processor.h>

struct ticket_lock {
    atomic_t next;   /* next ticket to issue */
    atomic_t serving; /* ticket currently being served */
};

#define TICKET_LOCK_INIT { ATOMIC_INIT(0), ATOMIC_INIT(0) }

void ticket_lock(struct ticket_lock *tl)
{
    int my_ticket = atomic_fetch_add(1, &tl->next);
    while (atomic_read(&tl->serving) != my_ticket)
        cpu_relax(); /* spin without polluting cache */
}

void ticket_unlock(struct ticket_lock *tl)
{
    atomic_inc(&tl->serving);
}
```

**Why FIFO matters:** Prevents starvation. Standard spinlocks can cause some CPUs to wait indefinitely on NUMA systems.

---

**Q21. Implement a per-CPU counter to avoid false sharing.**

```c
#include <linux/percpu.h>
#include <linux/cpumask.h>

static DEFINE_PER_CPU(u64, gpu_submit_count);

void gpu_submit_inc(void)
{
    /* Disable preemption to prevent migration between CPUs */
    this_cpu_inc(gpu_submit_count);
}

u64 gpu_submit_total(void)
{
    u64 total = 0;
    int cpu;

    for_each_possible_cpu(cpu)
        total += per_cpu(gpu_submit_count, cpu);
    return total;
}

void gpu_submit_reset(void)
{
    int cpu;
    for_each_possible_cpu(cpu)
        per_cpu(gpu_submit_count, cpu) = 0;
}
```

**Key Concept:** Each CPU has its own cacheline — zero contention. Used extensively in kernel networking (`net_device` stats), memory management, and scheduler.

---

**Q22. Implement a completion variable for module initialization synchronization.**

```c
#include <linux/completion.h>
#include <linux/kthread.h>

static DECLARE_COMPLETION(hw_init_done);
static struct task_struct *init_thread;

static int gpu_hw_init_thread(void *data)
{
    /* Simulate long hardware initialization */
    msleep(500);
    init_gpu_registers();
    complete(&hw_init_done); /* signal completion */
    return 0;
}

static int __init gpu_module_init(void)
{
    init_thread = kthread_run(gpu_hw_init_thread, NULL, "gpu_init");
    if (IS_ERR(init_thread))
        return PTR_ERR(init_thread);

    /* Block until HW init completes (or timeout) */
    if (!wait_for_completion_timeout(&hw_init_done,
                                      msecs_to_jiffies(2000))) {
        pr_err("GPU hardware init timed out\n");
        return -ETIMEDOUT;
    }
    pr_info("GPU initialized\n");
    return 0;
}
```

---

**Q23. What is a seqlock and when should it be used over RCU?**

```c
#include <linux/seqlock.h>

static seqlock_t clock_lock;
static u64 clock_base;
static u32 clock_mult;

/* Writer — increments sequence, writes, increments again */
void update_clock_params(u64 base, u32 mult)
{
    write_seqlock(&clock_lock);
    clock_base = base;
    clock_mult = mult;
    write_sequnlock(&clock_lock);
}

/* Reader — retry if sequence changed during read */
u64 read_clock_ns(void)
{
    u64 base;
    u32 mult;
    unsigned seq;

    do {
        seq  = read_seqbegin(&clock_lock);
        base = clock_base;
        mult = clock_mult;
    } while (read_seqretry(&clock_lock, seq));

    return base * mult;
}
```

**Seqlock vs RCU:**
- **Seqlock:** Best for small, integer-like data. Reader may retry. No pointer indirection needed.
- **RCU:** Best for pointer-based data structures. No reader retry, but requires grace period for writers.

---

**Q24. Detect and resolve a kernel deadlock scenario.**

```c
/*
 * DEADLOCK SCENARIO:
 * Thread A: lock(L1) -> lock(L2)
 * Thread B: lock(L2) -> lock(L1)   <-- DEADLOCK
 */

/* WRONG */
void thread_a(void) {
    mutex_lock(&L1);
    mutex_lock(&L2); /* may block waiting for B */
    // work
    mutex_unlock(&L2);
    mutex_unlock(&L1);
}

void thread_b(void) {
    mutex_lock(&L2);
    mutex_lock(&L1); /* DEADLOCK if A holds L1 */
    // work
    mutex_unlock(&L1);
    mutex_unlock(&L2);
}

/* FIX: Establish a consistent lock ordering (L1 always before L2) */
void thread_b_fixed(void) {
    mutex_lock(&L1); /* always acquire in same order as A */
    mutex_lock(&L2);
    // work
    mutex_unlock(&L2);
    mutex_unlock(&L1);
}

/* Alternative: Use trylock with backoff */
int thread_b_trylock(void) {
    mutex_lock(&L2);
    if (!mutex_trylock(&L1)) {
        mutex_unlock(&L2);
        cond_resched();
        return -EAGAIN; /* retry */
    }
    // work
    mutex_unlock(&L1);
    mutex_unlock(&L2);
    return 0;
}
```

**Detection:** Enable `CONFIG_LOCKDEP` — kernel lockdep tracks lock ordering and reports cycles.

---

**Q25. Implement an atomic state machine for GPU power states.**

```c
#include <linux/atomic.h>

enum gpu_power_state {
    GPU_OFF     = 0,
    GPU_SUSPEND = 1,
    GPU_IDLE    = 2,
    GPU_ACTIVE  = 3,
};

static atomic_t gpu_state = ATOMIC_INIT(GPU_OFF);

/* Transition: only allow OFF -> IDLE */
int gpu_power_on(void)
{
    int old = atomic_cmpxchg(&gpu_state, GPU_OFF, GPU_IDLE);
    if (old != GPU_OFF)
        return -EBUSY;
    enable_gpu_clocks();
    return 0;
}

/* Transition: IDLE -> ACTIVE */
int gpu_start_work(void)
{
    int old = atomic_cmpxchg(&gpu_state, GPU_IDLE, GPU_ACTIVE);
    return (old == GPU_IDLE) ? 0 : -EINVAL;
}

/* Transition: ACTIVE -> IDLE */
void gpu_finish_work(void)
{
    atomic_cmpxchg(&gpu_state, GPU_ACTIVE, GPU_IDLE);
}

/* Safe read */
enum gpu_power_state gpu_get_state(void)
{
    return (enum gpu_power_state)atomic_read(&gpu_state);
}
```

---

## Section 4: System Design (Q26–Q37)

---

**Q26. Design a GPU Command Scheduler — multi-context, multi-priority.**

```
┌─────────────────────────────────────────────────────┐
│                  GPU Scheduler                       │
│                                                      │
│  Priority Queues         Runlist Builder             │
│  ┌──────────────┐        ┌──────────────────────┐   │
│  │ HIGH  (RT)   │──────► │ Context A cmds       │   │
│  │ NORMAL       │──────► │ Context B cmds       │   │  ──► GPU HW
│  │ LOW   (BG)   │──────► │ Context C cmds       │   │
│  └──────────────┘        └──────────────────────┘   │
│         ▲                         │                  │
│         │                         ▼                  │
│  Context Table            Fence Tracking             │
│  (per-process)            (seqno-based)              │
└─────────────────────────────────────────────────────┘
```

```c
struct gpu_context {
    u32                  ctx_id;
    int                  priority;   /* -20 to +19, like nice values */
    struct list_head     cmd_queue;
    spinlock_t           queue_lock;
    atomic64_t           submitted_seqno;
    atomic64_t           completed_seqno;
    struct rb_node       sched_node; /* in priority RB tree */
};

struct gpu_scheduler {
    struct rb_root        ctx_tree;    /* ordered by priority */
    struct workqueue_struct *wq;
    struct work_struct    sched_work;
    spinlock_t            lock;
};

/* Pick highest priority context with pending work */
static struct gpu_context *pick_next_context(struct gpu_scheduler *s)
{
    struct rb_node *node = rb_first(&s->ctx_tree);
    while (node) {
        struct gpu_context *ctx = rb_entry(node, struct gpu_context,
                                             sched_node);
        if (!list_empty(&ctx->cmd_queue))
            return ctx;
        node = rb_next(node);
    }
    return NULL;
}
```

**Design Decisions:**
- RB tree for O(log n) priority-ordered context selection
- Per-context spinlock for lockless multi-producer submission
- Fence seqno for zero-copy synchronization

---

**Q27. Design a Zero-Copy DMA Transfer Engine between CPU and GPU.**

```
CPU Memory          PCIe Bus            GPU Memory
┌──────────┐       ┌──────────┐        ┌──────────┐
│ Pinned   │──────►│ DMA Eng  │───────►│ VRAM     │
│ User Buf │◄──────│ (scatter │◄───────│ Buffer   │
│ (sg list)│       │  gather) │        │          │
└──────────┘       └──────────┘        └──────────┘
      │                  │                   │
      └──── Fence ────────────── Fence ───────┘
           Signal                 Signal
```

```c
struct zero_copy_transfer {
    struct sg_table     sgt;         /* scatter-gather table */
    dma_addr_t          *dma_addrs;
    struct page         **pages;
    unsigned int        num_pages;
    struct gpu_fence    completion;
};

int setup_zero_copy(struct device *dev, void __user *uaddr,
                     size_t size, struct zero_copy_transfer *xfer)
{
    int ret;

    xfer->num_pages = DIV_ROUND_UP(size, PAGE_SIZE);
    xfer->pages = kvmalloc_array(xfer->num_pages,
                                  sizeof(*xfer->pages), GFP_KERNEL);
    if (!xfer->pages)
        return -ENOMEM;

    /* Pin user pages — no copy! */
    ret = pin_user_pages_fast((unsigned long)uaddr, xfer->num_pages,
                               FOLL_WRITE, xfer->pages);
    if (ret < 0)
        goto err_pages;

    /* Build scatter-gather list for DMA engine */
    ret = sg_alloc_table_from_pages(&xfer->sgt, xfer->pages,
                                     xfer->num_pages, 0, size,
                                     GFP_KERNEL);
    if (ret)
        goto err_sg;

    /* Map for DMA */
    ret = dma_map_sg(dev, xfer->sgt.sgl, xfer->sgt.nents,
                      DMA_TO_DEVICE);
    if (!ret) {
        ret = -EIO;
        goto err_map;
    }

    gpu_fence_init(&xfer->completion);
    return 0;

err_map:
    sg_free_table(&xfer->sgt);
err_sg:
    unpin_user_pages(xfer->pages, xfer->num_pages);
err_pages:
    kvfree(xfer->pages);
    return ret;
}
```

---

**Q28. Design a GPU Virtual Memory Manager (GMMU).**

```
Process VA Space (48-bit)
┌─────────────────────────────────────────────────────┐
│ 0x0000_0000 ─────────────────── 0xFFFF_FFFF_FFFF   │
│                                                      │
│  Code/Data (CPU)    GPU Mappings    Kernel Mappings  │
│  ┌──────────┐       ┌──────────┐   ┌──────────────┐ │
│  │ ELF segs │       │ VRAM buf │   │ GPU registers│ │
│  │ heap/stk │       │ shared   │   │ DMA buffers  │ │
│  └──────────┘       └──────────┘   └──────────────┘ │
└─────────────────────────────────────────────────────┘
         │                    │
         ▼                    ▼
    CPU Page Tables       GPU Page Tables
    (CR3 / TTBR0)        (GMMU PDB register)
```

```c
struct gpu_va_space {
    struct rb_root   va_tree;       /* VA range allocations */
    spinlock_t       lock;
    u64              start;
    u64              end;
    struct gpu_pte   *page_tables;  /* 4-level PT, like CPU */
};

struct gpu_va_range {
    u64              va_start;
    u64              va_end;
    u32              flags;         /* READ|WRITE|EXEC|CACHED */
    struct rb_node   node;
};

/* Allocate a VA range using best-fit */
int gpu_va_alloc(struct gpu_va_space *vas, u64 size,
                  u64 align, u64 *va_out)
{
    struct rb_node *node = rb_first(&vas->va_tree);
    u64 prev_end = vas->start;
    u64 aligned_start;

    spin_lock(&vas->lock);
    while (node) {
        struct gpu_va_range *r = rb_entry(node, struct gpu_va_range, node);
        aligned_start = ALIGN(prev_end, align);
        if (aligned_start + size <= r->va_start) {
            *va_out = aligned_start;
            spin_unlock(&vas->lock);
            return 0; /* found a hole */
        }
        prev_end = r->va_end;
        node = rb_next(node);
    }
    /* Try end of range */
    aligned_start = ALIGN(prev_end, align);
    if (aligned_start + size <= vas->end) {
        *va_out = aligned_start;
        spin_unlock(&vas->lock);
        return 0;
    }
    spin_unlock(&vas->lock);
    return -ENOMEM;
}
```

---

**Q29. Design a kernel-mode GPU fault handler (page fault replay).**

```c
/*
 * When GPU accesses unmapped VA:
 * 1. GPU MMU raises fault interrupt
 * 2. Driver fault handler runs
 * 3. Page is allocated and mapped
 * 4. Faulting instruction is replayed
 */

struct gpu_fault_info {
    u64  fault_addr;
    u32  fault_type;   /* READ, WRITE, ATOMIC */
    u32  engine_id;
    u32  context_id;
};

static irqreturn_t gpu_fault_isr(int irq, void *dev_id)
{
    struct gpu_device *gdev = dev_id;
    struct gpu_fault_info fault;

    /* Read fault info from GPU MMU registers */
    fault.fault_addr  = readq(gdev->mmio + FAULT_ADDR_REG);
    fault.fault_type  = readl(gdev->mmio + FAULT_TYPE_REG);
    fault.context_id  = readl(gdev->mmio + FAULT_CTX_REG);

    /* Defer actual mapping to a work queue (can sleep) */
    queue_work(gdev->fault_wq, &gdev->fault_work);
    return IRQ_HANDLED;
}

static void gpu_fault_worker(struct work_struct *work)
{
    struct gpu_device *gdev = container_of(work, struct gpu_device,
                                             fault_work);
    struct gpu_fault_info *fault = &gdev->pending_fault;
    struct page *page;

    /* Allocate page and map it in GMMU */
    page = alloc_page(GFP_KERNEL);
    if (!page) {
        gpu_context_kill(gdev, fault->context_id);
        return;
    }
    gpu_gmmu_map(gdev, fault->context_id,
                  fault->fault_addr & PAGE_MASK,
                  page_to_phys(page), PAGE_SIZE,
                  GPU_PTE_READ | GPU_PTE_WRITE);

    /* Signal GPU to replay the faulting access */
    writel(FAULT_REPLAY, gdev->mmio + FAULT_CTRL_REG);
}
```

---

**Q30. Design a distributed GPU job scheduler across multiple nodes.**

```
                    ┌─────────────────┐
                    │   Job Broker    │
                    │  (etcd-backed)  │
                    └────────┬────────┘
                             │ gRPC
              ┌──────────────┼──────────────┐
              ▼              ▼              ▼
        ┌──────────┐  ┌──────────┐  ┌──────────┐
        │  Node 0  │  │  Node 1  │  │  Node 2  │
        │ 8x A100  │  │ 8x A100  │  │ 8x H100  │
        │ NVLink   │  │ NVLink   │  │ NVLink   │
        └──────────┘  └──────────┘  └──────────┘
              │              │              │
              └──────────────┴──────────────┘
                         NVSwitch / IB
```

```c
/* Node-local scheduler data structure */
struct node_scheduler {
    struct list_head    job_queue;       /* pending jobs */
    struct rb_root      running_jobs;    /* by job_id */
    atomic_t            free_gpus;       /* bitmask */
    spinlock_t          lock;
    struct workqueue_struct *exec_wq;
};

/* Job represents a CUDA kernel batch */
struct gpu_job {
    u64                  job_id;
    int                  gpu_mask;       /* which GPUs to use */
    int                  priority;
    size_t               input_size;
    dma_addr_t           input_dma;
    struct gpu_fence     done_fence;
    struct list_head     list;
    struct rb_node       rb_node;
    void (*callback)(struct gpu_job *, int status);
};

/*
 * Scheduling policy: FIFO within priority class.
 * Gang scheduling: allocate all required GPUs atomically.
 */
static bool try_schedule_job(struct node_scheduler *sched,
                               struct gpu_job *job)
{
    int current_mask, new_mask;

    do {
        current_mask = atomic_read(&sched->free_gpus);
        if ((current_mask & job->gpu_mask) != job->gpu_mask)
            return false; /* not enough GPUs */
        new_mask = current_mask & ~job->gpu_mask;
    } while (!atomic_cmpxchg(&sched->free_gpus, current_mask, new_mask));

    queue_work(sched->exec_wq, &job->exec_work);
    return true;
}
```

---

**Q31. Design an NVIDIA GPU kernel driver architecture for PCIe device.**

```
┌───────────────────────────────────────────────────┐
│                 NVIDIA Kernel Driver               │
│                                                    │
│  ┌──────────────────────────────────────────────┐ │
│  │            Character Device Layer             │ │
│  │   open/close/ioctl/mmap/poll                  │ │
│  └──────────────────────────────────────────────┘ │
│                         │                          │
│  ┌───────────┐  ┌───────────────┐  ┌───────────┐  │
│  │ GPU VASpace│  │  RM (Resource │  │  DMA Eng  │  │
│  │ Manager   │  │   Manager)    │  │  Subsys   │  │
│  └───────────┘  └───────────────┘  └───────────┘  │
│                         │                          │
│  ┌──────────────────────────────────────────────┐ │
│  │            HAL (Hardware Abstraction)         │ │
│  │   per-GPU-arch implementations                │ │
│  └──────────────────────────────────────────────┘ │
│                         │                          │
│  ┌──────────────────────────────────────────────┐ │
│  │           PCIe / Platform Bus Layer           │ │
│  └──────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────┘
```

**Component Responsibilities:**

| Component | Responsibility |
|-----------|---------------|
| Char device | User/kernel interface |
| VASpace Manager | GMMU page tables, VA allocation |
| Resource Manager | Object lifecycle, handles |
| DMA Engine | H2D/D2H transfers, CE management |
| HAL | GPU-specific register programming |

---

**Q32. Design a GPU memory allocator supporting VRAM + System memory.**

```c
enum mem_location {
    MEM_LOC_VIDMEM  = 0,  /* GPU VRAM — fastest for GPU */
    MEM_LOC_SYSMEM  = 1,  /* System RAM — accessible by CPU */
    MEM_LOC_UNIFIED = 2,  /* Managed memory — auto-migrate */
};

struct gpu_mem_descriptor {
    enum mem_location  location;
    u64                gpu_va;      /* GPU virtual address */
    u64                cpu_va;      /* CPU virtual address (if mapped) */
    size_t             size;
    u32                flags;
    struct list_head   node;
    atomic_t           refcount;
    struct drm_gem_object *gem;     /* for GEM integration */
};

struct gpu_mem_pool {
    struct gen_pool   *vram_pool;   /* VRAM allocator */
    struct gen_pool   *sysmem_pool;
    spinlock_t         lock;
    size_t             vram_total;
    size_t             vram_free;
    atomic64_t         alloc_count;
};

struct gpu_mem_descriptor *gpu_mem_alloc(struct gpu_mem_pool *pool,
                                           size_t size,
                                           enum mem_location loc,
                                           u32 flags)
{
    struct gpu_mem_descriptor *desc;
    unsigned long addr;

    desc = kzalloc(sizeof(*desc), GFP_KERNEL);
    if (!desc)
        return ERR_PTR(-ENOMEM);

    size = ALIGN(size, PAGE_SIZE);

    switch (loc) {
    case MEM_LOC_VIDMEM:
        addr = gen_pool_alloc(pool->vram_pool, size);
        if (!addr) {
            kfree(desc);
            return ERR_PTR(-ENOMEM);
        }
        desc->gpu_va = addr;
        break;
    case MEM_LOC_SYSMEM:
        desc->cpu_va = (u64)(uintptr_t)vmalloc(size);
        if (!desc->cpu_va) {
            kfree(desc);
            return ERR_PTR(-ENOMEM);
        }
        break;
    default:
        kfree(desc);
        return ERR_PTR(-EINVAL);
    }

    desc->location = loc;
    desc->size     = size;
    desc->flags    = flags;
    atomic_set(&desc->refcount, 1);
    atomic64_inc(&pool->alloc_count);
    return desc;
}
```

---

**Q33. Design a fault-tolerant GPU cluster health monitoring system.**

```
┌─────────────────────────────────────────────────────────┐
│                  Health Monitor Service                  │
│                                                          │
│  ┌──────────────┐    ┌──────────────┐   ┌─────────────┐ │
│  │  Heartbeat   │    │  ECC Error   │   │  Thermal    │ │
│  │  Collector   │    │  Tracker     │   │  Monitor    │ │
│  └──────┬───────┘    └──────┬───────┘   └──────┬──────┘ │
│         └──────────────┬────┘──────────────────┘         │
│                        ▼                                 │
│              ┌──────────────────┐                        │
│              │  Alert Aggregator│                        │
│              │  (ring buffer)   │                        │
│              └────────┬─────────┘                        │
│                       ▼                                  │
│           ┌───────────────────────┐                      │
│           │  Remediation Engine   │                      │
│           │  remap / retire page  │                      │
│           │  drain context        │                      │
│           │  trigger failover     │                      │
│           └───────────────────────┘                      │
└─────────────────────────────────────────────────────────┘
```

```c
struct gpu_health_metrics {
    u32   ecc_single_bit_errors;
    u32   ecc_double_bit_errors;
    u32   temperature_celsius;
    u32   power_watts;
    u32   pcie_replay_count;
    u64   timestamp_ns;
};

struct health_monitor {
    struct delayed_work    poll_work;
    struct workqueue_struct *wq;
    struct gpu_health_metrics metrics[MAX_GPUS];
    atomic_t               alert_count;
    spinlock_t             lock;
    unsigned long          poll_interval_ms;
};

static void health_poll_fn(struct work_struct *work)
{
    struct health_monitor *hm = container_of(to_delayed_work(work),
                                              struct health_monitor,
                                              poll_work);
    int gpu;
    for (gpu = 0; gpu < MAX_GPUS; gpu++) {
        struct gpu_health_metrics m;
        gpu_read_health_regs(gpu, &m);

        if (m.ecc_double_bit_errors > 0)
            gpu_retire_bad_pages(gpu);

        if (m.temperature_celsius > GPU_TEMP_CRITICAL)
            gpu_throttle(gpu);
    }
    queue_delayed_work(hm->wq, &hm->poll_work,
                        msecs_to_jiffies(hm->poll_interval_ms));
}
```

---

**Q34. Design a GPU profiling and tracing infrastructure.**

```c
#include <linux/tracepoint.h>

/* Define custom tracepoints */
TRACE_EVENT(gpu_kernel_launch,
    TP_PROTO(u64 ctx_id, u64 func_addr, u32 grid_x, u32 grid_y),
    TP_ARGS(ctx_id, func_addr, grid_x, grid_y),
    TP_STRUCT__entry(
        __field(u64, ctx_id)
        __field(u64, func_addr)
        __field(u32, grid_x)
        __field(u32, grid_y)
        __field(u64, ts)
    ),
    TP_fast_assign(
        __entry->ctx_id    = ctx_id;
        __entry->func_addr = func_addr;
        __entry->grid_x    = grid_x;
        __entry->grid_y    = grid_y;
        __entry->ts        = ktime_get_ns();
    ),
    TP_printk("ctx=%llu func=0x%llx grid=(%u,%u) ts=%llu",
               __entry->ctx_id, __entry->func_addr,
               __entry->grid_x, __entry->grid_y, __entry->ts)
);

/* Perf counter ring buffer for GPU hardware counters */
struct gpu_perf_buf {
    u64 sm_active_cycles;
    u64 dram_read_bytes;
    u64 dram_write_bytes;
    u64 l2_hit_count;
    u64 pcie_tx_bytes;
    u64 pcie_rx_bytes;
    u32 sample_id;
    u32 gpu_id;
} __packed;

/* Ring buffer with lock-free producer */
DEFINE_PER_CPU(struct gpu_perf_buf[256], perf_ring);
DEFINE_PER_CPU(unsigned int, perf_head);

static void record_perf_sample(struct gpu_perf_buf *sample)
{
    struct gpu_perf_buf *ring = this_cpu_ptr(perf_ring);
    unsigned int head = this_cpu_read(perf_head);

    memcpy(&ring[head & 255], sample, sizeof(*sample));
    this_cpu_write(perf_head, head + 1);
}
```

---

**Q35. Design PCIe peer-to-peer (P2P) DMA between two GPUs.**

```c
#include <linux/pci-p2pdma.h>

/*
 * GPU-to-GPU DMA without CPU involvement:
 *
 *   GPU0 VRAM ──────────────────────► GPU1 VRAM
 *                  PCIe P2P DMA
 *                  (no main memory)
 */

int setup_gpu_p2p(struct pci_dev *gpu0, struct pci_dev *gpu1)
{
    /* Check if P2P is supported between these two devices */
    if (!pci_p2pdma_distance(gpu0, gpu1, true)) {
        pr_err("P2P DMA not supported between GPUs\n");
        return -ENODEV;
    }

    /* Publish GPU0's BAR2 (VRAM) as P2P resource */
    if (pci_p2pdma_add_resource(gpu0, 2, 0, 0)) {
        pr_err("Failed to add P2P resource\n");
        return -ENOMEM;
    }

    pci_p2pdma_whitelist_add(gpu1);
    return 0;
}

int gpu_p2p_copy(struct pci_dev *src, u64 src_offset,
                  struct pci_dev *dst, u64 dst_offset,
                  size_t size)
{
    struct scatterlist sg;
    void *p2p_mem;
    dma_addr_t dma_addr;

    /* Map source GPU memory as P2P DMA buffer */
    p2p_mem = pci_alloc_p2pmem(src, size);
    if (!p2p_mem)
        return -ENOMEM;

    dma_addr = pci_p2pmem_virt_to_bus(src, p2p_mem);

    /* Program destination GPU's DMA engine */
    gpu_program_dma(dst, dma_addr, dst_offset, size, DMA_FROM_DEVICE);

    pci_free_p2pmem(src, p2p_mem, size);
    return 0;
}
```

---

**Q36. Design a GPU SR-IOV virtual function manager.**

```
Physical Function (PF) Driver
┌────────────────────────────────────┐
│  VF Manager                        │
│  ┌────────┐ ┌────────┐ ┌────────┐  │
│  │  VF 0  │ │  VF 1  │ │  VF N  │  │
│  │ vGPU 0 │ │ vGPU 1 │ │ vGPU N │  │
│  └────────┘ └────────┘ └────────┘  │
│                                    │
│  Resource Partitioner               │
│  - VRAM partitions                  │
│  - Compute engine time slices       │
│  - PCIe bandwidth limits            │
└────────────────────────────────────┘
```

```c
struct vf_config {
    u32   vf_id;
    u32   vram_mb;           /* VRAM quota */
    u32   compute_percent;   /* SM utilization cap */
    u32   max_contexts;
    u32   pcie_bw_mbps;
};

int pf_enable_sriov(struct pci_dev *pdev, int num_vfs)
{
    int ret;

    /* Verify hardware supports requested VF count */
    if (num_vfs > pci_sriov_get_totalvfs(pdev))
        return -EINVAL;

    /* Setup resource partitions before enabling VFs */
    for (int i = 0; i < num_vfs; i++) {
        struct vf_config cfg = {
            .vf_id           = i,
            .vram_mb         = GPU_TOTAL_VRAM_MB / num_vfs,
            .compute_percent = 100 / num_vfs,
            .max_contexts    = MAX_CONTEXTS / num_vfs,
        };
        gpu_partition_resources(pdev, &cfg);
    }

    ret = pci_enable_sriov(pdev, num_vfs);
    if (ret)
        pr_err("SR-IOV enable failed: %d\n", ret);
    return ret;
}

/* VF probe — called for each virtual function */
static int vf_probe(struct pci_dev *pdev,
                     const struct pci_device_id *id)
{
    /* VF only gets access to its own partitioned resources */
    struct vf_device *vf = devm_kzalloc(&pdev->dev,
                                          sizeof(*vf), GFP_KERNEL);
    vf->mmio = pci_ioremap_bar(pdev, 0);
    vf->vram_base = readq(vf->mmio + VF_VRAM_BASE_REG);
    vf->vram_size = readq(vf->mmio + VF_VRAM_SIZE_REG);
    return 0;
}
```

---

**Q37. Design a NUMA-aware GPU memory allocation policy.**

```c
#include <linux/topology.h>
#include <linux/cpuset.h>

struct numa_gpu_affinity {
    int   gpu_node;          /* NUMA node closest to GPU */
    int   cpu_node;          /* preferred CPU NUMA node */
    u64   local_bw_gbps;     /* PCIe/NVLink bandwidth to local node */
    u64   remote_bw_gbps;
};

/* Find NUMA node closest to PCIe device */
int gpu_get_numa_node(struct pci_dev *pdev)
{
    int node = dev_to_node(&pdev->dev);
    if (node == NUMA_NO_NODE)
        node = pci_dev_to_node(pdev);
    return node;
}

/* Allocate memory local to GPU for best PCIe bandwidth */
void *gpu_numa_alloc(struct pci_dev *pdev, size_t size)
{
    int node = gpu_get_numa_node(pdev);
    void *mem;

    /* Prefer local NUMA node allocation */
    mem = kmalloc_node(size, GFP_KERNEL, node);
    if (!mem)
        mem = kmalloc(size, GFP_KERNEL); /* fallback */

    pr_info("GPU NUMA alloc: node=%d, addr=%p\n", node, mem);
    return mem;
}

/* For large allocations, use CMA on correct NUMA node */
struct page *gpu_alloc_pages_numa(struct pci_dev *pdev,
                                    int order)
{
    int node = gpu_get_numa_node(pdev);
    return alloc_pages_node(node,
                             GFP_KERNEL | __GFP_THISNODE,
                             order);
}
```

---

## Section 5: Performance, Debugging & Algorithms (Q38–50)

---

**Q38. Implement a red-black tree for O(log n) VA range lookups.**

```c
#include <linux/rbtree.h>

struct va_range {
    u64            start;
    u64            end;
    u32            flags;
    struct rb_node node;
};

struct va_space {
    struct rb_root root;
};

static void rb_insert_va(struct va_space *vas, struct va_range *new)
{
    struct rb_node **link = &vas->root.rb_node;
    struct rb_node  *parent = NULL;

    while (*link) {
        struct va_range *r = rb_entry(*link, struct va_range, node);
        parent = *link;
        if (new->start < r->start)
            link = &(*link)->rb_left;
        else if (new->start >= r->end)
            link = &(*link)->rb_right;
        else
            return; /* overlapping range — error */
    }
    rb_link_node(&new->node, parent, link);
    rb_insert_color(&new->node, &vas->root);
}

/* Find VA range containing addr */
struct va_range *rb_find_va(struct va_space *vas, u64 addr)
{
    struct rb_node *node = vas->root.rb_node;

    while (node) {
        struct va_range *r = rb_entry(node, struct va_range, node);
        if (addr < r->start)
            node = node->rb_left;
        else if (addr >= r->end)
            node = node->rb_right;
        else
            return r; /* found */
    }
    return NULL;
}
```

---

**Q39. Implement a kernel hash table for GPU context lookups.**

```c
#include <linux/hashtable.h>

#define CTX_HASH_BITS 10  /* 1024 buckets */

struct gpu_context_entry {
    u32                ctx_id;
    pid_t              pid;
    struct gpu_context *ctx;
    struct hlist_node  hash_node;
};

static DEFINE_HASHTABLE(ctx_table, CTX_HASH_BITS);
static DEFINE_SPINLOCK(ctx_table_lock);

void ctx_table_insert(struct gpu_context_entry *entry)
{
    spin_lock(&ctx_table_lock);
    hash_add(ctx_table, &entry->hash_node, entry->ctx_id);
    spin_unlock(&ctx_table_lock);
}

struct gpu_context *ctx_table_lookup(u32 ctx_id)
{
    struct gpu_context_entry *entry;

    rcu_read_lock();
    hash_for_each_possible_rcu(ctx_table, entry, hash_node, ctx_id) {
        if (entry->ctx_id == ctx_id) {
            rcu_read_unlock();
            return entry->ctx;
        }
    }
    rcu_read_unlock();
    return NULL;
}
```

---

**Q40. Implement a lock-free stack using compare-and-swap.**

```c
#include <linux/atomic.h>

struct lf_node {
    struct lf_node __rcu *next;
    void                  *data;
};

struct lf_stack {
    struct lf_node __rcu *top;
};

void lf_stack_push(struct lf_stack *s, struct lf_node *node)
{
    struct lf_node *old_top;

    do {
        old_top = rcu_dereference_raw(s->top);
        rcu_assign_pointer(node->next, old_top);
    } while (cmpxchg(&s->top, old_top, node) != old_top);
}

struct lf_node *lf_stack_pop(struct lf_stack *s)
{
    struct lf_node *old_top, *new_top;

    do {
        old_top = rcu_dereference_raw(s->top);
        if (!old_top)
            return NULL;
        new_top = rcu_dereference_raw(old_top->next);
    } while (cmpxchg(&s->top, old_top, new_top) != old_top);

    return old_top;
}
```

**ABA Problem Note:** `cmpxchg` alone is vulnerable to ABA. Use `cmpxchg128` (pointer + version counter) or RCU for production code.

---

**Q41. How do you debug a kernel NULL pointer dereference? Decode an oops message.**

```
# Typical kernel oops output:
BUG: kernel NULL pointer dereference, address: 0000000000000018
RIP: 0010:gpu_submit_cmd+0x4f/0x120 [nvidia]
Call Trace:
 ioctl_gpu_submit+0x8c/0x100 [nvidia]
 __x64_sys_ioctl+0x97/0xd0
 do_syscall_64+0x5c/0x90

# Decode with addr2line:
$ addr2line -ifCe nvidia.ko 0x4f
# Or with objdump:
$ objdump -d nvidia.ko | grep -A 20 'gpu_submit_cmd'
```

```c
/*
 * Root cause: accessing ctx->queue when ctx is NULL
 * Fix: validate pointer before dereference
 */
int gpu_submit_cmd_fixed(struct gpu_context *ctx,
                          struct gpu_cmd *cmd)
{
    if (unlikely(!ctx)) {
        pr_err_ratelimited("NULL context in submit\n");
        return -EINVAL;
    }
    if (unlikely(!ctx->queue)) {
        pr_err("Context %u has no queue\n", ctx->ctx_id);
        return -ENXIO;
    }
    return enqueue_cmd(ctx->queue, cmd);
}
```

**Tools:** `crash`, `kgdb`, `KASAN`, `UBSAN`, `ftrace`, `/proc/sys/kernel/panic_on_oops`

---

**Q42. Implement KASAN-friendly memory debugging for a GPU buffer allocator.**

```c
#include <linux/kasan.h>

struct gpu_buffer {
    void    *data;
    size_t   size;
    u32      magic;  /* for use-after-free detection */
};

#define GPU_BUF_MAGIC  0xDEADBEEF
#define GPU_BUF_FREED  0xFEEDFACE

struct gpu_buffer *gpu_buf_alloc(size_t size)
{
    struct gpu_buffer *buf = kmalloc(sizeof(*buf), GFP_KERNEL);
    if (!buf)
        return NULL;

    buf->data  = kmalloc(size, GFP_KERNEL);
    if (!buf->data) {
        kfree(buf);
        return NULL;
    }
    buf->size  = size;
    buf->magic = GPU_BUF_MAGIC;

    /* KASAN: poison the padding bytes for OOB detection */
    kasan_check_write(buf->data, size);
    return buf;
}

void gpu_buf_free(struct gpu_buffer *buf)
{
    if (WARN_ON(!buf))
        return;
    if (WARN_ON(buf->magic == GPU_BUF_FREED)) {
        /* Double-free detected */
        return;
    }
    if (WARN_ON(buf->magic != GPU_BUF_MAGIC)) {
        /* Buffer corruption */
        return;
    }
    buf->magic = GPU_BUF_FREED;
    kfree(buf->data);
    buf->data = NULL; /* prevent dangling pointer use */
    kfree(buf);
}
```

---

**Q43. Implement ftrace-based latency measurement for GPU command submission.**

```c
#include <linux/ftrace.h>
#include <linux/ktime.h>

struct latency_stat {
    u64 min_ns;
    u64 max_ns;
    u64 sum_ns;
    u64 count;
};

static DEFINE_PER_CPU(struct latency_stat, submit_latency);

void record_submit_latency(ktime_t start, ktime_t end)
{
    u64 ns = ktime_to_ns(ktime_sub(end, start));
    struct latency_stat *stat = this_cpu_ptr(&submit_latency);

    if (ns < stat->min_ns || stat->count == 0)
        stat->min_ns = ns;
    if (ns > stat->max_ns)
        stat->max_ns = ns;
    stat->sum_ns += ns;
    stat->count++;
}

int gpu_submit_with_trace(struct gpu_context *ctx,
                           struct gpu_cmd *cmd)
{
    ktime_t t_start = ktime_get();
    int ret;

    trace_gpu_kernel_launch(ctx->ctx_id, cmd->func_addr,
                             cmd->grid_x, cmd->grid_y);
    ret = gpu_submit_cmd(ctx, cmd);

    record_submit_latency(t_start, ktime_get());
    return ret;
}

/* Export via debugfs */
static int latency_show(struct seq_file *s, void *unused)
{
    int cpu;
    u64 total_min = U64_MAX, total_max = 0, total_sum = 0, total_cnt = 0;

    for_each_possible_cpu(cpu) {
        struct latency_stat *st = per_cpu_ptr(&submit_latency, cpu);
        total_min = min(total_min, st->min_ns);
        total_max = max(total_max, st->max_ns);
        total_sum += st->sum_ns;
        total_cnt += st->count;
    }
    seq_printf(s, "min=%llu max=%llu avg=%llu ns count=%llu\n",
               total_min, total_max,
               total_cnt ? total_sum / total_cnt : 0,
               total_cnt);
    return 0;
}
```

---

**Q44. Implement a kernel interval timer for GPU watchdog.**

```c
#include <linux/hrtimer.h>

#define GPU_WATCHDOG_MS 5000

struct gpu_watchdog {
    struct hrtimer  timer;
    atomic64_t      last_seqno;
    atomic64_t      gpu_seqno;
    struct work_struct recovery_work;
};

static enum hrtimer_restart watchdog_fn(struct hrtimer *timer)
{
    struct gpu_watchdog *wd = container_of(timer,
                                            struct gpu_watchdog, timer);
    u64 last = atomic64_read(&wd->last_seqno);
    u64 curr = atomic64_read(&wd->gpu_seqno);

    if (last == curr) {
        /* GPU hasn't made progress — schedule recovery */
        pr_err("GPU watchdog: seqno stuck at %llu\n", curr);
        schedule_work(&wd->recovery_work);
    }
    atomic64_set(&wd->last_seqno, curr);

    hrtimer_forward_now(timer, ms_to_ktime(GPU_WATCHDOG_MS));
    return HRTIMER_RESTART;
}

int watchdog_init(struct gpu_watchdog *wd)
{
    INIT_WORK(&wd->recovery_work, gpu_do_recovery);
    hrtimer_init(&wd->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    wd->timer.function = watchdog_fn;
    hrtimer_start(&wd->timer, ms_to_ktime(GPU_WATCHDOG_MS),
                   HRTIMER_MODE_REL);
    return 0;
}
```

---

**Q45. Find the longest increasing subsequence — O(n log n) — applicable to GPU timeline ordering.**

```c
#include <linux/slab.h>

/* Returns LIS length. Used in GPU fence dependency resolution. */
int lis_length(u64 *seqnos, int n)
{
    u64 *tails;  /* tails[i] = smallest tail of IS of length i+1 */
    int  len = 0;
    int  i, lo, hi, mid;

    tails = kmalloc_array(n, sizeof(*tails), GFP_KERNEL);
    if (!tails)
        return -ENOMEM;

    for (i = 0; i < n; i++) {
        /* Binary search for position to update */
        lo = 0; hi = len;
        while (lo < hi) {
            mid = (lo + hi) / 2;
            if (tails[mid] < seqnos[i])
                lo = mid + 1;
            else
                hi = mid;
        }
        tails[lo] = seqnos[i];
        if (lo == len)
            len++;
    }
    kfree(tails);
    return len;
}
```

---

**Q46. Implement segment tree for range queries on GPU job intervals.**

```c
#define MAX_N 1024

struct seg_tree {
    u64 tree[4 * MAX_N]; /* sum tree */
    int n;
};

void seg_build(struct seg_tree *st, u64 *arr, int node,
               int start, int end)
{
    if (start == end) {
        st->tree[node] = arr[start];
        return;
    }
    int mid = (start + end) / 2;
    seg_build(st, arr, 2*node,   start, mid);
    seg_build(st, arr, 2*node+1, mid+1, end);
    st->tree[node] = st->tree[2*node] + st->tree[2*node+1];
}

u64 seg_query(struct seg_tree *st, int node,
              int start, int end, int l, int r)
{
    if (r < start || end < l)
        return 0;
    if (l <= start && end <= r)
        return st->tree[node];
    int mid = (start + end) / 2;
    return seg_query(st, 2*node,   start, mid, l, r)
         + seg_query(st, 2*node+1, mid+1, end, l, r);
}

void seg_update(struct seg_tree *st, int node,
                int start, int end, int idx, u64 val)
{
    if (start == end) {
        st->tree[node] = val;
        return;
    }
    int mid = (start + end) / 2;
    if (idx <= mid)
        seg_update(st, 2*node,   start, mid, idx, val);
    else
        seg_update(st, 2*node+1, mid+1, end, idx, val);
    st->tree[node] = st->tree[2*node] + st->tree[2*node+1];
}
```

**Application:** Track total GPU execution time across a range of job IDs. O(log n) updates and queries.

---

**Q47. Implement Dijkstra's algorithm for PCIe topology path finding.**

```c
#define MAX_NODES 64
#define INF       ULLONG_MAX

struct pcie_topology {
    u64  dist[MAX_NODES][MAX_NODES]; /* bandwidth cost matrix */
    int  n;
};

/* Returns min-latency path between GPU src and dst */
void dijkstra(struct pcie_topology *topo, int src,
              u64 *dist_out, int *prev)
{
    bool visited[MAX_NODES] = {};
    int n = topo->n;
    int i, u, v;
    u64 d;

    for (i = 0; i < n; i++) {
        dist_out[i] = INF;
        prev[i]     = -1;
    }
    dist_out[src] = 0;

    for (i = 0; i < n; i++) {
        /* Find unvisited node with min distance */
        u = -1;
        for (v = 0; v < n; v++) {
            if (!visited[v] &&
                (u == -1 || dist_out[v] < dist_out[u]))
                u = v;
        }
        if (u == -1 || dist_out[u] == INF)
            break;
        visited[u] = true;

        /* Relax edges */
        for (v = 0; v < n; v++) {
            if (topo->dist[u][v] == INF) continue;
            d = dist_out[u] + topo->dist[u][v];
            if (d < dist_out[v]) {
                dist_out[v] = d;
                prev[v]     = u;
            }
        }
    }
}
```

**Application:** NVLink / PCIe switch topology routing. Choose lowest-latency path for P2P copies.

---

**Q48. Implement a trie for kernel module parameter namespace lookup.**

```c
#define TRIE_ALPHA 128

struct trie_node {
    struct trie_node *children[TRIE_ALPHA];
    void             *value;    /* non-NULL if terminal */
};

struct trie_node *trie_alloc_node(void)
{
    return kzalloc(sizeof(struct trie_node), GFP_KERNEL);
}

int trie_insert(struct trie_node *root, const char *key, void *val)
{
    struct trie_node *cur = root;
    while (*key) {
        unsigned char c = (unsigned char)*key++;
        if (!cur->children[c]) {
            cur->children[c] = trie_alloc_node();
            if (!cur->children[c])
                return -ENOMEM;
        }
        cur = cur->children[c];
    }
    cur->value = val;
    return 0;
}

void *trie_lookup(struct trie_node *root, const char *key)
{
    struct trie_node *cur = root;
    while (*key) {
        unsigned char c = (unsigned char)*key++;
        if (!cur->children[c])
            return NULL;
        cur = cur->children[c];
    }
    return cur->value;
}
```

---

**Q49. Implement a GPU command ring buffer with hardware doorbell.**

```c
#define RING_ENTRIES  4096
#define RING_SIZE_BYTES (RING_ENTRIES * sizeof(u64))

struct gpu_ring {
    u64 __iomem   *hw_ring;     /* mapped to GPU MMIO */
    u32 __iomem   *doorbell;    /* write here to notify GPU */
    u32            head;         /* CPU write pointer */
    u32            tail;         /* GPU read pointer (polled) */
    spinlock_t     lock;
};

/* Submit a 64-bit command to the ring */
int ring_submit(struct gpu_ring *ring, u64 cmd)
{
    u32 next;
    unsigned long flags;

    spin_lock_irqsave(&ring->lock, flags);

    next = (ring->head + 1) & (RING_ENTRIES - 1);
    if (next == READ_ONCE(ring->tail)) {
        spin_unlock_irqrestore(&ring->lock, flags);
        return -ENOSPC; /* ring full */
    }

    /* Write command to MMIO ring */
    writeq(cmd, &ring->hw_ring[ring->head]);
    wmb(); /* ensure write is visible before doorbell */

    ring->head = next;

    /* Ring the doorbell — GPU picks up new commands */
    writel(ring->head, ring->doorbell);

    spin_unlock_irqrestore(&ring->lock, flags);
    return 0;
}

/* Poll for ring space (called before batch submit) */
int ring_wait_space(struct gpu_ring *ring, u32 needed,
                     unsigned long timeout_jiffies)
{
    unsigned long deadline = jiffies + timeout_jiffies;

    while (((ring->head - READ_ONCE(ring->tail)) & (RING_ENTRIES-1))
            > RING_ENTRIES - needed - 1) {
        if (time_after(jiffies, deadline))
            return -ETIMEDOUT;
        cpu_relax();
    }
    return 0;
}
```

---

**Q50. Design a complete GPU driver bring-up sequence — PCIe probe to first command submission.**

```c
static int gpu_pci_probe(struct pci_dev *pdev,
                          const struct pci_device_id *id)
{
    struct gpu_device *gdev;
    int ret;

    /* 1. Allocate device structure */
    gdev = devm_kzalloc(&pdev->dev, sizeof(*gdev), GFP_KERNEL);
    if (!gdev)
        return -ENOMEM;

    /* 2. Enable PCI device */
    ret = pcim_enable_device(pdev);
    if (ret) return ret;

    /* 3. Request and map BARs */
    ret = pcim_iomap_regions(pdev, BIT(0) | BIT(2), "gpu");
    if (ret) return ret;
    gdev->mmio   = pcim_iomap_table(pdev)[0];
    gdev->fb_bar = pcim_iomap_table(pdev)[2];

    /* 4. Set DMA mask (48-bit addressing) */
    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(48));
    if (ret) return ret;

    /* 5. Enable bus mastering for DMA */
    pci_set_master(pdev);

    /* 6. Read GPU identity registers */
    gdev->gpu_id  = readl(gdev->mmio + GPU_ID_REG);
    gdev->vram_mb = readl(gdev->mmio + VRAM_SIZE_REG);
    pr_info("GPU 0x%x detected, VRAM: %u MB\n",
            gdev->gpu_id, gdev->vram_mb);

    /* 7. Initialize GMMU (GPU virtual memory) */
    ret = gpu_vmm_init(gdev);
    if (ret) return ret;

    /* 8. Setup command ring */
    ret = gpu_ring_init(gdev);
    if (ret) goto err_vmm;

    /* 9. Setup DMA engine */
    ret = gpu_dma_engine_init(gdev);
    if (ret) goto err_ring;

    /* 10. Register interrupt handler */
    ret = pci_alloc_irq_vectors(pdev, 1, 4, PCI_IRQ_MSI);
    if (ret < 0) goto err_dma;
    ret = devm_request_irq(&pdev->dev, pci_irq_vector(pdev, 0),
                            gpu_irq_top, 0, "gpu", gdev);
    if (ret) goto err_irq;

    /* 11. Register character device */
    ret = gpu_chrdev_register(gdev);
    if (ret) goto err_irq;

    /* 12. Submit first command — NOP to verify ring works */
    ret = ring_submit(&gdev->ring, GPU_NOP_CMD);
    if (ret) goto err_chrdev;

    pci_set_drvdata(pdev, gdev);
    pr_info("GPU driver ready\n");
    return 0;

err_chrdev:  gpu_chrdev_unregister(gdev);
err_irq:     pci_free_irq_vectors(pdev);
err_dma:     gpu_dma_engine_fini(gdev);
err_ring:    gpu_ring_fini(gdev);
err_vmm:     gpu_vmm_fini(gdev);
    return ret;
}

static void gpu_pci_remove(struct pci_dev *pdev)
{
    struct gpu_device *gdev = pci_get_drvdata(pdev);
    gpu_chrdev_unregister(gdev);
    pci_free_irq_vectors(pdev);
    gpu_dma_engine_fini(gdev);
    gpu_ring_fini(gdev);
    gpu_vmm_fini(gdev);
}

static const struct pci_device_id gpu_pci_ids[] = {
    { PCI_DEVICE(0x10DE, 0x2330) }, /* H100 SXM */
    { PCI_DEVICE(0x10DE, 0x2204) }, /* A100 */
    { 0 }
};

static struct pci_driver gpu_pci_driver = {
    .name     = "nvidia_gpu",
    .id_table = gpu_pci_ids,
    .probe    = gpu_pci_probe,
    .remove   = gpu_pci_remove,
};
module_pci_driver(gpu_pci_driver);
```

---

## Quick Reference Summary

| Topic | Key APIs / Concepts |
|-------|-------------------|
| Locking | `spinlock`, `mutex`, `rwlock`, `seqlock`, `RCU` |
| Memory | `kmalloc`, `vmalloc`, `kmem_cache`, `dma_alloc_coherent` |
| Interrupts | `request_irq`, `tasklet`, `workqueue`, `hrtimer` |
| Sync | `wait_queue`, `completion`, `atomic_cmpxchg` |
| Debugging | `KASAN`, `lockdep`, `ftrace`, `addr2line`, `crash` |
| GPU specific | `ioremap`, `remap_pfn_range`, `sg_table`, IOMMU, SR-IOV |
| Data Structures | `list_head`, `rb_root`, `hashtable`, `gen_pool` |
| Algorithms | LIS O(n log n), Dijkstra, Segment Tree, Trie |
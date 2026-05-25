Linux Kernel Memory Management
& Custom mmap Driver
Part 1
|  |


A comprehensive guide to kernel memory allocation strategies,
custom mmap character driver development, and Linux device driver architecture
| Topic | Coverage |
| Chapter 1 | Contiguous Memory Buffer Allocation (2 MB) |
| Chapter 2 | Custom mmap Kernel Character Driver |
| Chapter 3 | Block Driver vs. Character Driver |
| Chapter 4 | Simple RAM Disk Block Driver |


# Chapter 1: Contiguous Memory Buffer Allocation (2 MB)
One of the foundational tasks in embedded Linux and kernel driver development is obtaining a contiguous block of memory. This chapter covers all major approaches for allocating a 2 MB contiguous buffer — both in kernel space and user space — with annotated code examples and guidance on when to use each method.
## 1.1  Kernel-Space Allocation Methods
In kernel space, several APIs exist to allocate memory. The appropriate choice depends on the required physical contiguity, size, and performance constraints.
### 1.1.1  Using kmalloc()
kmalloc() allocates physically and virtually contiguous memory. It is the first-choice allocator for small-to-medium buffers. For a 2 MB allocation the call typically succeeds on modern kernels, but availability is architecture-dependent.
| #include <linux/slab.h> |
|  |
| void *buffer = kmalloc(2 * 1024 * 1024, GFP_KERNEL); |
| if (!buffer) { |
| pr_err("Failed to allocate memory\n"); |
| return -ENOMEM; |
| } |
| // ... use the buffer ... |
| kfree(buffer); |


⚠  Note: kmalloc() is generally limited to allocations up to 4 MB (varies by architecture). It guarantees physical contiguity, making it suitable for DMA-adjacent use cases.

### 1.1.2  Using vmalloc()
vmalloc() allocates virtually contiguous but potentially physically fragmented memory. It is suitable for large allocations where physical contiguity is not required (e.g., software buffers with no DMA involvement).
| #include <linux/vmalloc.h> |
|  |
| void *buffer = vmalloc(2 * 1024 * 1024); |
| if (!buffer) { |
| pr_err("Failed to allocate memory\n"); |
| return -ENOMEM; |
| } |
| // ... use the buffer ... |
| vfree(buffer); |


ℹ  Note: vmalloc() provides virtually contiguous memory but may NOT be physically contiguous. Do not pass vmalloc'd buffers to DMA-capable hardware expecting physical contiguity.

### 1.1.3  Physically Contiguous Memory for DMA
When DMA-capable hardware requires physically contiguous buffers, the GFP_DMA flag or the page allocator must be used. The get_order() + alloc_pages() pattern is preferred for large physically contiguous allocations.
| #include <linux/slab.h> |
| #include <linux/gfp.h> |
|  |
| /* Method A: kmalloc with GFP_DMA (small buffers) */ |
| void *buffer = kmalloc(2 * 1024 * 1024, GFP_KERNEL | GFP_DMA); |
|  |
| /* Method B: alloc_pages for larger allocations */ |
| int order = get_order(2 * 1024 * 1024); |
| struct page *pages = alloc_pages(GFP_KERNEL, order); |
| if (pages) { |
| void *buffer = page_address(pages); |
| /* ... use buffer ... */ |
| __free_pages(pages, order); |
| } |


### 1.1.4  Using CMA — dma_alloc_coherent()
The Contiguous Memory Allocator (CMA) is the recommended approach for DMA-capable drivers on ARM SoCs. dma_alloc_coherent() returns a kernel virtual address plus a DMA handle, ensuring cache coherency between CPU and device.
| #include <linux/dma-mapping.h> |
|  |
| struct device *dev; /* your platform/PCI device */ |
| dma_addr_t dma_handle; |
|  |
| void *buffer = dma_alloc_coherent(dev, |
| 2 * 1024 * 1024, |
| &dma_handle, |
| GFP_KERNEL); |
| if (!buffer) { |
| pr_err("Failed to allocate DMA memory\n"); |
| return -ENOMEM; |
| } |
| /* dma_handle is the bus/DMA address to pass to hardware */ |
| /* buffer is the CPU-side virtual address */ |
|  |
| dma_free_coherent(dev, 2 * 1024 * 1024, buffer, dma_handle); |


## 1.2  User-Space Allocation Methods
User-space applications do not have direct access to physical memory. However, the following standard POSIX interfaces provide contiguous virtual address ranges of any size.
### 1.2.1  Using malloc()
The standard C library's malloc() allocates a virtual address range. While the virtual address space is contiguous, the underlying physical pages may not be.
| #include <stdlib.h> |
|  |
| void *buffer = malloc(2 * 1024 * 1024); |
| if (!buffer) { |
| perror("malloc failed"); |
| return -1; |
| } |
| /* ... use the buffer ... */ |
| free(buffer); |


### 1.2.2  Using mmap() — Anonymous Mapping
mmap() with MAP_ANONYMOUS creates a virtual memory region backed by the kernel's page allocator. This guarantees a contiguous virtual address range and allows fine-grained protection flags.
| #include <sys/mman.h> |
|  |
| void *buffer = mmap(NULL, |
| 2 * 1024 * 1024, |
| PROT_READ | PROT_WRITE, |
| MAP_PRIVATE | MAP_ANONYMOUS, |
| -1, 0); |
| if (buffer == MAP_FAILED) { |
| perror("mmap failed"); |
| return -1; |
| } |
| /* ... use the buffer ... */ |
| munmap(buffer, 2 * 1024 * 1024); |


## 1.3  Allocation Method Comparison
| API | Context | Contiguity | Use Case |
| kmalloc() | Kernel | Physical + Virtual | General-purpose kernel buffers |
| vmalloc() | Kernel | Virtual only | Large SW-only buffers, no DMA |
| alloc_pages() | Kernel | Physical + Virtual | Large physically contiguous DMA buffers |
| dma_alloc_coherent() | Kernel | Physical + Virtual | DMA-coherent device driver buffers |
| malloc() | User | Virtual only | Application heap allocations |
| mmap() | User | Virtual contiguous | Large buffers, shared memory, device mapping |


# Chapter 2: Custom mmap Kernel Character Driver
This chapter presents a fully functional Linux kernel character driver that exposes a 2 MB memory buffer to user space via the mmap() system call. The implementation covers page allocation, VM operations, demand-paging via a fault handler, a user-space test application, and a complete build system.
## 2.1  Driver Architecture Overview
The driver follows the standard Linux character device model. It registers a /dev/custom_mmap character device backed by a pool of individually allocated pages. User-space processes open the device and call mmap() to map those pages directly into their virtual address space.
| Component | Role |
| custom_mmap_init() | Module entry: allocates pages, registers char device, creates /dev node |
| device_mmap() | Implements the file_operations.mmap hook; installs VM operations |
| custom_vma_fault() | Demand-paging fault handler: maps individual pages on first access |
| custom_vma_open/close() | VMA lifecycle callbacks for logging and reference counting |
| custom_mmap_exit() | Module exit: tears down device, frees all allocated pages |


## 2.2  Kernel Module: custom_mmap.c
The listing below is the complete kernel module. It is divided into logical sections: header includes, global variables, VM operations, file operations, and module lifecycle functions.
### 2.2.1  Headers, Macros, and Global State
| #include <linux/module.h> |
| #include <linux/kernel.h> |
| #include <linux/fs.h> |
| #include <linux/mm.h> |
| #include <linux/slab.h> |
| #include <linux/device.h> |
| #include <linux/cdev.h> |
| #include <asm/io.h> |
|  |
| #define DEVICE_NAME "custom_mmap" |
| #define BUFFER_SIZE (2 * 1024 * 1024) /* 2 MB */ |
|  |
| static int major_number; |
| static struct class *custom_class = NULL; |
| static struct device *custom_device = NULL; |
| static void *kernel_buffer = NULL; |
| static struct page **pages = NULL; |
| static int num_pages; |


### 2.2.2  VM Operations — Demand-Paging Fault Handler
The vm_operations_struct hooks into the kernel's page-fault mechanism. When user space accesses a page for the first time, custom_vma_fault() is called to supply the backing page.
| /* Called on every first access to a page in the VMA */ |
| static vm_fault_t custom_vma_fault(struct vm_fault *vmf) |
| { |
| struct vm_area_struct *vma = vmf->vma; |
| unsigned long offset = vmf->pgoff << PAGE_SHIFT; |
| struct page *page; |
|  |
| if (offset >= BUFFER_SIZE) |
| return VM_FAULT_SIGBUS; |
|  |
| /* Retrieve our pre-allocated page for this offset */ |
| page = pages[vmf->pgoff]; |
|  |
| get_page(page); /* bump refcount */ |
| vmf->page = page; /* hand page to kernel fault handler */ |
|  |
| pr_info("custom_mmap: Page fault handled at offset %lu\n", offset); |
| return 0; |
| } |
|  |
| static void custom_vma_open(struct vm_area_struct *vma) |
| { |
| pr_info("custom_mmap: VMA open, virt %lx, phys %lx\n", |
| vma->vm_start, vma->vm_pgoff << PAGE_SHIFT); |
| } |
|  |
| static void custom_vma_close(struct vm_area_struct *vma) |
| { |
| pr_info("custom_mmap: VMA close\n"); |
| } |
|  |
| static struct vm_operations_struct custom_vm_ops = { |
| .open = custom_vma_open, |
| .close = custom_vma_close, |
| .fault = custom_vma_fault, |
| }; |


### 2.2.3  File Operations — open, release, and mmap
The core of the driver is the device_mmap() function, which validates the requested size, sets appropriate VM flags, and installs the custom_vm_ops operations pointer.
| static int device_open(struct inode *inode, struct file *file) |
| { |
| pr_info("custom_mmap: Device opened\n"); |
| return 0; |
| } |
|  |
| static int device_release(struct inode *inode, struct file *file) |
| { |
| pr_info("custom_mmap: Device closed\n"); |
| return 0; |
| } |
|  |
| /* Custom mmap implementation */ |
| static int device_mmap(struct file *filp, struct vm_area_struct *vma) |
| { |
| unsigned long size = vma->vm_end - vma->vm_start; |
|  |
| pr_info("custom_mmap: mmap called, size=%lu, vm_start=%lx, vm_end=%lx\n", |
| size, vma->vm_start, vma->vm_end); |
|  |
| if (size > BUFFER_SIZE) { |
| pr_err("custom_mmap: Requested size %lu exceeds buffer %d\n", |
| size, BUFFER_SIZE); |
| return -EINVAL; |
| } |
|  |
| /* Mark VMA flags */ |
| vma->vm_flags |= VM_IO; /* treat as I/O memory */ |
| vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP; /* safety flags */ |
|  |
| /* Cache policy — choose based on use case */ |
| vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); |
| /* For write-combining: pgprot_writecombine(vma->vm_page_prot) */ |
| /* For cached access: leave vm_page_prot unmodified */ |
|  |
| /* Install demand-paging VM operations */ |
| vma->vm_ops = &custom_vm_ops; |
| vma->vm_private_data = filp->private_data; |
|  |
| custom_vma_open(vma); |
|  |
| pr_info("custom_mmap: Successfully set up VMA for %lu bytes\n", size); |
| return 0; |
| } |
|  |
| static struct file_operations fops = { |
| .owner = THIS_MODULE, |
| .open = device_open, |
| .release = device_release, |
| .mmap = device_mmap, |
| }; |


### 2.2.4  Alternative: Eager Mapping with remap_pfn_range()
Instead of demand paging, all pages can be mapped eagerly during the mmap() call using remap_pfn_range(). This eliminates future page faults at the cost of upfront mapping work.
| /* Eager mapping — map all pages immediately in device_mmap() */ |
| int i; |
| unsigned long pfn; |
|  |
| for (i = 0; i < num_pages && (i * PAGE_SIZE) < size; i++) { |
| pfn = page_to_pfn(pages[i]); |
| if (remap_pfn_range(vma, |
| vma->vm_start + (i * PAGE_SIZE), |
| pfn, |
| PAGE_SIZE, |
| vma->vm_page_prot)) { |
| pr_err("custom_mmap: remap_pfn_range failed at page %d\n", i); |
| return -EAGAIN; |
| } |
| } |


### 2.2.5  Module Initialization and Cleanup
The init function allocates a page pointer array, allocates individual pages with alloc_page() (clearing each for security), registers the character device, and creates the device class and node.
| static int __init custom_mmap_init(void) |
| { |
| int i; |
|  |
| pr_info("custom_mmap: Initializing module\n"); |
|  |
| num_pages = (BUFFER_SIZE + PAGE_SIZE - 1) / PAGE_SIZE; |
|  |
| /* Allocate page-pointer array */ |
| pages = kmalloc(num_pages * sizeof(struct page *), GFP_KERNEL); |
| if (!pages) { |
| pr_err("custom_mmap: Failed to allocate pages array\n"); |
| return -ENOMEM; |
| } |
|  |
| /* Allocate and zero each page individually */ |
| for (i = 0; i < num_pages; i++) { |
| pages[i] = alloc_page(GFP_KERNEL); |
| if (!pages[i]) { |
| pr_err("custom_mmap: Failed to allocate page %d\n", i); |
| goto fail_alloc_pages; |
| } |
| clear_page(page_address(pages[i])); |
| } |
|  |
| kernel_buffer = page_address(pages[0]); |
| pr_info("custom_mmap: Allocated %d pages (%d bytes) at kernel VA %p\n", |
| num_pages, BUFFER_SIZE, kernel_buffer); |
|  |
| /* Register character device */ |
| major_number = register_chrdev(0, DEVICE_NAME, &fops); |
| if (major_number < 0) { |
| pr_err("custom_mmap: Failed to register character device\n"); |
| goto fail_alloc_pages; |
| } |
|  |
| /* Create /sys class and /dev node */ |
| custom_class = class_create(THIS_MODULE, DEVICE_NAME); |
| if (IS_ERR(custom_class)) { |
| unregister_chrdev(major_number, DEVICE_NAME); |
| goto fail_alloc_pages; |
| } |
|  |
| custom_device = device_create(custom_class, NULL, |
| MKDEV(major_number, 0), |
| NULL, DEVICE_NAME); |
| if (IS_ERR(custom_device)) { |
| class_destroy(custom_class); |
| unregister_chrdev(major_number, DEVICE_NAME); |
| goto fail_alloc_pages; |
| } |
|  |
| pr_info("custom_mmap: Device created (major=%d)\n", major_number); |
| return 0; |
|  |
| fail_alloc_pages: |
| for (i = 0; i < num_pages; i++) |
| if (pages[i]) __free_page(pages[i]); |
| kfree(pages); |
| return -ENOMEM; |
| } |
|  |
| static void __exit custom_mmap_exit(void) |
| { |
| int i; |
|  |
| pr_info("custom_mmap: Cleaning up module\n"); |
|  |
| if (custom_device) |
| device_destroy(custom_class, MKDEV(major_number, 0)); |
| if (custom_class) |
| class_destroy(custom_class); |
|  |
| unregister_chrdev(major_number, DEVICE_NAME); |
|  |
| if (pages) { |
| for (i = 0; i < num_pages; i++) |
| if (pages[i]) __free_page(pages[i]); |
| kfree(pages); |
| } |
|  |
| pr_info("custom_mmap: Module removed\n"); |
| } |
|  |
| module_init(custom_mmap_init); |
| module_exit(custom_mmap_exit); |
|  |
| MODULE_LICENSE("GPL"); |
| MODULE_AUTHOR("Your Name"); |
| MODULE_DESCRIPTION("Custom mmap implementation for 2MB buffer"); |
| MODULE_VERSION("1.0"); |


## 2.3  User-Space Test Application: test_mmap.c
The following test application opens the character device, memory-maps the 2 MB buffer, and runs four verification tests covering pattern writing, read-back verification, string I/O, and multi-page access.
| #include <stdio.h> |
| #include <stdlib.h> |
| #include <fcntl.h> |
| #include <sys/mman.h> |
| #include <unistd.h> |
| #include <string.h> |
| #include <errno.h> |
|  |
| #define DEVICE_PATH "/dev/custom_mmap" |
| #define BUFFER_SIZE (2 * 1024 * 1024) |
|  |
| int main(int argc, char *argv[]) |
| { |
| int fd; |
| void *mapped_addr; |
| char *buffer; |
| int i; |
|  |
| printf("Opening device %s...\n", DEVICE_PATH); |
| fd = open(DEVICE_PATH, O_RDWR); |
| if (fd < 0) { |
| perror("Failed to open device"); |
| return 1; |
| } |
|  |
| printf("Mapping 2MB buffer...\n"); |
| mapped_addr = mmap(NULL, BUFFER_SIZE, |
| PROT_READ | PROT_WRITE, |
| MAP_SHARED, fd, 0); |
| if (mapped_addr == MAP_FAILED) { |
| perror("mmap failed"); |
| close(fd); |
| return 1; |
| } |
| printf("Successfully mapped at address: %p\n", mapped_addr); |
| buffer = (char *)mapped_addr; |
|  |
| /* Test 1: Write byte pattern */ |
| printf("\nTest 1: Writing pattern to buffer...\n"); |
| for (i = 0; i < 1024; i++) |
| buffer[i] = (char)(i % 256); |
| printf("Written 1KB of pattern data\n"); |
|  |
| /* Test 2: Read back and verify */ |
| printf("\nTest 2: Verifying data...\n"); |
| int errors = 0; |
| for (i = 0; i < 1024; i++) |
| if (buffer[i] != (char)(i % 256)) errors++; |
| printf("Verification complete: %d errors\n", errors); |
|  |
| /* Test 3: String write */ |
| printf("\nTest 3: Writing string...\n"); |
| const char *test_str = "Hello from user space! Custom mmap test."; |
| strcpy(buffer, test_str); |
| printf("Read back: %s\n", buffer); |
|  |
| /* Test 4: Multi-page access (triggers multiple faults) */ |
| printf("\nTest 4: Accessing different pages...\n"); |
| buffer[0] = 'A'; |
| buffer[4096] = 'B'; /* second page */ |
| buffer[8192] = 'C'; /* third page */ |
| buffer[1024*1024] = 'D'; /* 1 MB offset */ |
| printf("Accessed multiple pages successfully\n"); |
|  |
| /* Cleanup */ |
| printf("\nUnmapping buffer...\n"); |
| if (munmap(mapped_addr, BUFFER_SIZE) < 0) |
| perror("munmap failed"); |
|  |
| close(fd); |
| printf("Test completed successfully!\n"); |
| return 0; |
| } |


## 2.4  Build System: Makefile
The Makefile builds both the kernel module and the user-space test application. It provides convenience targets for loading, unloading, and running tests.
| # Kernel module Makefile |
| obj-m += custom_mmap.o |
|  |
| KDIR := /lib/modules/$(shell uname -r)/build |
| PWD := $(shell pwd) |
|  |
| all: module userspace |
|  |
| module: |
| $(MAKE) -C $(KDIR) M=$(PWD) modules |
|  |
| userspace: |
| gcc -o test_mmap test_mmap.c -Wall |
|  |
| clean: |
| $(MAKE) -C $(KDIR) M=$(PWD) clean |
| rm -f test_mmap |
|  |
| install: |
| sudo insmod custom_mmap.ko |
|  |
| uninstall: |
| sudo rmmod custom_mmap |
|  |
| test: install |
| sudo ./test_mmap |
| sudo rmmod custom_mmap |
| dmesg | tail -50 |


## 2.5  Build & Usage Instructions
| # Step 1: Build the kernel module and test app |
| make |
|  |
| # Step 2: Load the kernel module |
| sudo insmod custom_mmap.ko |
|  |
| # Step 3: Verify device node creation |
| ls -l /dev/custom_mmap |
|  |
| # Step 4: Run the user-space test |
| sudo ./test_mmap |
|  |
| # Step 5: Inspect kernel log output |
| dmesg | tail -50 |
|  |
| # Step 6: Unload the module |
| sudo rmmod custom_mmap |


## 2.6  Key Implementation Details
The following summarises the essential design decisions made in this driver:
- Memory allocation strategy: alloc_page() is used for page-by-page allocation rather than kmalloc(), giving finer control over individual pages and enabling the demand-paging approach.
- Two mapping approaches: Eager mapping via remap_pfn_range() maps all pages at mmap() time. Demand paging via vm_operations fault handler maps pages lazily on first access (default, more efficient for sparse access patterns).
- Cache control: pgprot_noncached() for uncached I/O access, pgprot_writecombine() for sequential write performance, or unmodified vm_page_prot for normal cached access.
- VM flags: VM_IO marks the region as I/O memory; VM_DONTEXPAND prevents mremap(); VM_DONTDUMP excludes the region from core dumps.
- Security: Each page is zeroed with clear_page() during init to prevent information leakage from recycled kernel memory.

# Chapter 3: Block Driver vs. Character Driver
Understanding the distinction between block and character drivers is foundational for Linux kernel development. This chapter provides a comprehensive comparison — from data model and buffering to registration mechanics and typical use cases.
## 3.1  Core Conceptual Differences
### 3.1.1  Character Drivers
- Handle data as a stream of bytes (character by character).
- Access is sequential — data flows in order.
- No kernel buffering or caching; direct unbuffered I/O operations.
- Examples: serial ports (UART), keyboards, mice, sensors, custom devices.

### 3.1.2  Block Drivers
- Handle data in fixed-size blocks (typically 512 B, 1 KB, or 4 KB).
- Support random access — any block can be read or written independently.
- Kernel provides a buffer cache (page cache) for performance.
- Examples: hard drives, SSDs, SD cards, eMMC, USB storage.

## 3.2  Technical Comparison
| Aspect | Character Driver | Block Driver |
| Data Unit | Byte stream | Fixed-size blocks (512 B sectors) |
| Access Pattern | Sequential | Random access |
| Buffering | None — direct I/O | Kernel page cache |
| File Operations | read(), write(), ioctl(), open(), close() | Request queue + bio structures |
| Device Files | /dev/ttyS0, /dev/input/event0 | /dev/sda, /dev/mmcblk0 |
| Complexity | Simpler to implement | More complex (request queues, bio) |
| Filesystem Support | No | Yes — can host ext4, FAT, etc. |
| Seeking | Limited or not supported | Full seek support |


## 3.3  Implementation Differences
### 3.3.1  Character Driver Registration
| static struct file_operations char_fops = { |
| .owner = THIS_MODULE, |
| .open = char_open, |
| .release = char_release, |
| .read = char_read, |
| .write = char_write, |
| .unlocked_ioctl = char_ioctl, |
| .llseek = char_llseek, |
| }; |
|  |
| /* Register character device */ |
| register_chrdev(major, "mychar", &char_fops); |


### 3.3.2  Block Driver Registration
| static struct block_device_operations block_fops = { |
| .owner = THIS_MODULE, |
| .open = block_open, |
| .release = block_release, |
| .ioctl = block_ioctl, |
| .getgeo = block_getgeo, |
| }; |
|  |
| /* Uses request queue (legacy) OR blk-mq (modern) */ |
| struct request_queue *queue; |
| queue = blk_init_queue(block_request, &lock); |
|  |
| /* Allocate and configure gendisk */ |
| struct gendisk *disk = alloc_disk(minors); |
| disk->major = major; |
| disk->fops = &block_fops; |
| disk->queue = queue; |
| set_capacity(disk, device_size_sectors); |
| add_disk(disk); |


## 3.4  Data Flow Architecture
### 3.4.1  Character Driver Data Flow
| User Space |
| | |
| v (read / write / ioctl system call) |
| VFS Layer |
| | |
| v (invokes file_operations handlers directly) |
| Character Driver |
| | |
| v (direct register/memory access) |
| Hardware |
|  |
| => Direct path, minimal kernel overhead, lowest latency |


### 3.4.2  Block Driver Data Flow
| User Space |
| | |
| v (read / write system call) |
| VFS Layer |
| | |
| v (checks page cache) |
| Page Cache (buffer cache) |
| | |
| v (cache miss: issues bio request) |
| Block Layer |
| | |
| v (I/O scheduler merges & orders requests) |
| I/O Scheduler (CFQ / deadline / mq-deadline / none) |
| | |
| v (dispatches to driver request queue) |
| Block Driver (queue_rq handler) |
| | |
| v (DMA transfer or direct memory copy) |
| Hardware / Storage Medium |
|  |
| => Multiple layers — higher throughput via caching and scheduling |


## 3.5  When to Use Each Driver Type
### 3.5.1  Choose a Character Driver When:
- Data is naturally sequential (e.g., serial communication, sensor streams).
- No filesystem support is needed.
- Direct hardware control is required.
- Low-latency, unbuffered access is critical.
- Custom protocols (I2C, SPI devices) or user-space memory-mapped buffers (as in Chapter 2).

### 3.5.2  Choose a Block Driver When:
- Building a storage device driver (persistent data, mountable volume).
- Filesystem support is needed (ext4, FAT32, etc.).
- Random-access I/O patterns dominate the workload.
- Large data transfers benefit from kernel-level caching and I/O scheduling.
- Standard storage interface (eMMC, SD card, RAM disk) is required.

| 📌 ARM / Qualcomm IoT SoC ContextCharacter drivers you likely encounter: • UART drivers (debug console, AT commands) • I²C sensor drivers (accelerometer, temperature) • SPI flash drivers (when not using MTD) • GPIO drivers, custom hardware interfacesBlock drivers you encounter: • eMMC / UFS storage drivers • SD card drivers (mmc subsystem) • NAND flash with MTD / UBI layer • RAM disk drivers (see Chapter 4) |


# Chapter 4: Simple RAM Disk Block Driver
This chapter presents a complete RAM disk block driver (simple_ramdisk.c) that creates a 16 MB mountable block device backed by RAM. It demonstrates the modern blk-mq interface, bio processing, gendisk management, and a full test script.
## 4.1  Driver Architecture
| Component | Purpose |
| ramdisk_device | Master device struct: vmalloc'd data buffer, spinlock, queue, gendisk |
| ramdisk_transfer() | Copies data between user request and the vmalloc'd RAM buffer |
| ramdisk_handle_bio() | Iterates bio segments, calling ramdisk_transfer() per segment |
| ramdisk_queue_rq() | blk-mq entry point; starts request, processes each bio, signals completion |
| ramdisk_getgeo() | Provides fake disk geometry for compatibility with partitioning tools |
| ramdisk_setup_device() | Allocates data buffer, tag set, request queue, and gendisk; calls add_disk() |


## 4.2  Kernel Module: simple_ramdisk.c
### 4.2.1  Headers, Macros, and Device Structure
| #include <linux/module.h> |
| #include <linux/kernel.h> |
| #include <linux/fs.h> |
| #include <linux/blkdev.h> |
| #include <linux/bio.h> |
| #include <linux/genhd.h> |
| #include <linux/vmalloc.h> |
| #include <linux/string.h> |
|  |
| #define RAMDISK_NAME "sramdisk" |
| #define RAMDISK_SIZE (16 * 1024 * 1024) /* 16 MB */ |
| #define SECTOR_SIZE 512 |
| #define RAMDISK_MINORS 1 |
|  |
| struct ramdisk_device { |
| unsigned long size; /* device size in bytes */ |
| u8 *data; /* backing data buffer */ |
| spinlock_t lock; |
| struct request_queue *queue; |
| struct gendisk *gd; |
| }; |
|  |
| static struct ramdisk_device *ramdisk_dev = NULL; |
| static int ramdisk_major = 0; |


### 4.2.2  Data Transfer and bio Processing
| /* Low-level transfer: memcpy between RAM buffer and bio segment */ |
| static void ramdisk_transfer(struct ramdisk_device *dev, |
| sector_t sector, |
| unsigned long nsect, |
| char *buffer, |
| int write) |
| { |
| unsigned long offset = sector * SECTOR_SIZE; |
| unsigned long nbytes = nsect * SECTOR_SIZE; |
|  |
| if ((offset + nbytes) > dev->size) { |
| pr_err("ramdisk: Beyond-end access (%ld + %ld > %lu)\n", |
| offset, nbytes, dev->size); |
| return; |
| } |
|  |
| if (write) |
| memcpy(dev->data + offset, buffer, nbytes); |
| else |
| memcpy(buffer, dev->data + offset, nbytes); |
| } |
|  |
| /* Process a single bio (set of segments in one I/O request) */ |
| static int ramdisk_handle_bio(struct ramdisk_device *dev, struct bio *bio) |
| { |
| struct bio_vec bvec; |
| struct bvec_iter iter; |
| sector_t sector = bio->bi_iter.bi_sector; |
| int write = bio_data_dir(bio); |
|  |
| bio_for_each_segment(bvec, bio, iter) { |
| char *buffer = kmap_atomic(bvec.bv_page); |
| unsigned long offset = bvec.bv_offset; |
| unsigned int len = bvec.bv_len; |
|  |
| ramdisk_transfer(dev, sector, len / SECTOR_SIZE, |
| buffer + offset, write); |
|  |
| kunmap_atomic(buffer); |
| sector += len / SECTOR_SIZE; |
| } |
| return 0; |
| } |


### 4.2.3  blk-mq Request Handler
The queue_rq callback is the entry point for all I/O. It must call blk_mq_start_request() before processing and blk_mq_end_request() upon completion.
| static blk_status_t ramdisk_queue_rq(struct blk_mq_hw_ctx *hctx, |
| const struct blk_mq_queue_data *bd) |
| { |
| struct request *req = bd->rq; |
| struct ramdisk_device *dev = req->q->queuedata; |
| struct bio *bio; |
|  |
| blk_mq_start_request(req); |
|  |
| __rq_for_each_bio(bio, req) |
| ramdisk_handle_bio(dev, bio); |
|  |
| blk_mq_end_request(req, BLK_STS_OK); |
| return BLK_STS_OK; |
| } |
|  |
| static struct blk_mq_ops ramdisk_mq_ops = { |
| .queue_rq = ramdisk_queue_rq, |
| }; |


### 4.2.4  Block Device Operations and Geometry
| static int ramdisk_open(struct block_device *bdev, fmode_t mode) |
| { |
| pr_info("ramdisk: Device opened\n"); |
| return 0; |
| } |
|  |
| static void ramdisk_release(struct gendisk *gd, fmode_t mode) |
| { |
| pr_info("ramdisk: Device closed\n"); |
| } |
|  |
| /* Provide fake geometry for tools like fdisk */ |
| static int ramdisk_getgeo(struct block_device *bdev, struct hd_geometry *geo) |
| { |
| geo->heads = 4; |
| geo->sectors = 16; |
| geo->cylinders = (RAMDISK_SIZE / SECTOR_SIZE) / |
| (geo->heads * geo->sectors); |
| geo->start = 0; |
| return 0; |
| } |
|  |
| static const struct block_device_operations ramdisk_fops = { |
| .owner = THIS_MODULE, |
| .open = ramdisk_open, |
| .release = ramdisk_release, |
| .getgeo = ramdisk_getgeo, |
| }; |


### 4.2.5  Device Setup, Module Init, and Exit
| static int ramdisk_setup_device(struct ramdisk_device *dev) |
| { |
| struct blk_mq_tag_set *tag_set; |
|  |
| dev->size = RAMDISK_SIZE; |
| dev->data = vmalloc(dev->size); |
| if (!dev->data) return -ENOMEM; |
| memset(dev->data, 0, dev->size); |
|  |
| spin_lock_init(&dev->lock); |
|  |
| tag_set = kzalloc(sizeof(*tag_set), GFP_KERNEL); |
| if (!tag_set) { vfree(dev->data); return -ENOMEM; } |
|  |
| tag_set->ops = &ramdisk_mq_ops; |
| tag_set->nr_hw_queues = 1; |
| tag_set->queue_depth = 128; |
| tag_set->numa_node = NUMA_NO_NODE; |
| tag_set->flags = BLK_MQ_F_SHOULD_MERGE; |
| tag_set->driver_data = dev; |
|  |
| if (blk_mq_alloc_tag_set(tag_set)) { |
| kfree(tag_set); vfree(dev->data); return -ENOMEM; |
| } |
|  |
| dev->queue = blk_mq_init_queue(tag_set); |
| if (IS_ERR(dev->queue)) { |
| blk_mq_free_tag_set(tag_set); |
| kfree(tag_set); vfree(dev->data); |
| return PTR_ERR(dev->queue); |
| } |
| dev->queue->queuedata = dev; |
|  |
| dev->gd = alloc_disk(RAMDISK_MINORS); |
| if (!dev->gd) { |
| blk_cleanup_queue(dev->queue); |
| blk_mq_free_tag_set(tag_set); |
| kfree(tag_set); vfree(dev->data); return -ENOMEM; |
| } |
|  |
| dev->gd->major = ramdisk_major; |
| dev->gd->first_minor = 0; |
| dev->gd->fops = &ramdisk_fops; |
| dev->gd->queue = dev->queue; |
| dev->gd->private_data = dev; |
| snprintf(dev->gd->disk_name, 32, RAMDISK_NAME); |
| set_capacity(dev->gd, RAMDISK_SIZE / SECTOR_SIZE); |
|  |
| add_disk(dev->gd); |
| pr_info("ramdisk: Device registered (%d MB)\n", |
| RAMDISK_SIZE / (1024 * 1024)); |
| return 0; |
| } |
|  |
| static int __init ramdisk_init(void) |
| { |
| int ret; |
| pr_info("ramdisk: Initializing\n"); |
|  |
| ramdisk_major = register_blkdev(0, RAMDISK_NAME); |
| if (ramdisk_major < 0) return ramdisk_major; |
|  |
| ramdisk_dev = kzalloc(sizeof(struct ramdisk_device), GFP_KERNEL); |
| if (!ramdisk_dev) { |
| unregister_blkdev(ramdisk_major, RAMDISK_NAME); |
| return -ENOMEM; |
| } |
|  |
| ret = ramdisk_setup_device(ramdisk_dev); |
| if (ret) { |
| kfree(ramdisk_dev); |
| unregister_blkdev(ramdisk_major, RAMDISK_NAME); |
| return ret; |
| } |
|  |
| pr_info("ramdisk: Loaded successfully\n"); |
| return 0; |
| } |
|  |
| static void __exit ramdisk_exit(void) |
| { |
| struct ramdisk_device *dev = ramdisk_dev; |
|  |
| if (dev->gd) { del_gendisk(dev->gd); put_disk(dev->gd); } |
| if (dev->queue) blk_cleanup_queue(dev->queue); |
| if (dev->data) vfree(dev->data); |
|  |
| kfree(dev); |
| unregister_blkdev(ramdisk_major, RAMDISK_NAME); |
| pr_info("ramdisk: Unloaded\n"); |
| } |
|  |
| module_init(ramdisk_init); |
| module_exit(ramdisk_exit); |
|  |
| MODULE_LICENSE("GPL"); |
| MODULE_AUTHOR("Sandeep"); |
| MODULE_DESCRIPTION("Simple RAM Disk Block Driver"); |
| MODULE_VERSION("1.0"); |


## 4.3  Test Script: test_ramdisk.sh
The following Bash script automates all verification steps: device presence check, ext4 formatting, mount, read/write tests, and dd performance benchmarks.
| #!/bin/bash |
| set -e |
|  |
| DEVICE="/dev/sramdisk" |
| MOUNT_POINT="/mnt/ramdisk" |
|  |
| echo "=== RAM Disk Block Driver Test ===" |
|  |
| # Verify module and device |
| lsmod | grep -q simple_ramdisk || { echo "Module not loaded"; exit 1; } |
| [ -b "$DEVICE" ] || { echo "Device $DEVICE not found"; exit 1; } |
| echo "Device $DEVICE found" |
|  |
| # Display device info |
| lsblk $DEVICE |
|  |
| # Test 1: Create filesystem |
| echo "Test 1: Creating ext4 filesystem..." |
| sudo mkfs.ext4 -F $DEVICE && echo "Filesystem created" |
|  |
| # Test 2: Mount |
| echo "Test 2: Mounting..." |
| sudo mkdir -p $MOUNT_POINT |
| sudo mount $DEVICE $MOUNT_POINT && echo "Mounted at $MOUNT_POINT" |
|  |
| # Test 3: Write |
| echo "Test 3: Writing test files..." |
| sudo sh -c "echo 'Hello from RAM disk!' > $MOUNT_POINT/test.txt" |
| sudo sh -c "dd if=/dev/zero of=$MOUNT_POINT/largefile bs=1M count=5 2>/dev/null" |
|  |
| # Test 4: Read back |
| echo "Test 4: Reading test files..." |
| sudo cat $MOUNT_POINT/test.txt |
| sudo ls -lh $MOUNT_POINT/ |
|  |
| # Test 5: Filesystem usage |
| echo "Test 5: Checking filesystem usage..." |
| df -h $MOUNT_POINT |
|  |
| # Test 6: Write performance |
| echo "Test 6: Write performance test..." |
| sudo dd if=/dev/zero of=$MOUNT_POINT/perftest bs=1M count=10 conv=fsync 2>&1 \ |
| | grep -E "copied|MB/s" |
|  |
| # Test 7: Read performance |
| echo "Test 7: Read performance test..." |
| sudo dd if=$MOUNT_POINT/perftest of=/dev/null bs=1M 2>&1 \ |
| | grep -E "copied|MB/s" |
|  |
| # Cleanup |
| sudo umount $MOUNT_POINT && echo "Device unmounted" |
| echo "=== All tests completed successfully! ===" |
| echo "View kernel logs: dmesg | tail -30" |
| echo "Remove module: sudo rmmod simple_ramdisk" |


## 4.4  Build System: Makefile
| obj-m += simple_ramdisk.o |
|  |
| KDIR := /lib/modules/$(shell uname -r)/build |
| PWD := $(shell pwd) |
|  |
| all: |
| $(MAKE) -C $(KDIR) M=$(PWD) modules |
|  |
| clean: |
| $(MAKE) -C $(KDIR) M=$(PWD) clean |
| rm -f *.order *.symvers |
|  |
| install: |
| sudo insmod simple_ramdisk.ko |
| @echo "Module loaded. Device: /dev/sramdisk" |
|  |
| uninstall: |
| @mount | grep -q sramdisk && sudo umount /dev/sramdisk || true |
| sudo rmmod simple_ramdisk && echo "Module unloaded" |
|  |
| test: install |
| chmod +x test_ramdisk.sh && ./test_ramdisk.sh |
|  |
| logs: |
| dmesg | grep ramdisk | tail -20 |
|  |
| info: |
| @lsmod | grep -q simple_ramdisk && lsblk /dev/sramdisk || echo "Not loaded" |
|  |
| help: |
| @echo "Targets: all | clean | install | uninstall | test | logs | info" |


## 4.5  Key Block Driver Concepts Demonstrated
| Concept | Implementation Detail |
| blk-mq architecture | Uses modern multi-queue blk-mq instead of legacy single-queue interface. Configured via blk_mq_tag_set and blk_mq_init_queue(). |
| bio processing | bio_for_each_segment() iterates all scatter-gather segments in a bio. kmap_atomic()/kunmap_atomic() safely access page data in atomic context. |
| Memory management | vmalloc() provides virtually contiguous 16 MB buffer. memset(0) ensures clean slate. No DMA in this simple example. |
| gendisk structure | Represents the entire disk. set_capacity() configures size in 512-byte sectors. add_disk() makes it visible to the VFS and udev. |
| Request lifecycle | blk_mq_start_request() signals processing has begun. blk_mq_end_request(BLK_STS_OK) signals successful completion to the block layer. |


## 4.6  Side-by-Side Comparison: This Chapter vs. Chapter 2
| Aspect | Character Driver (Chapter 2) | Block Driver (Chapter 4) |
| Data Access | Direct byte stream via mmap() | Block-based sectors (512 B) |
| File Ops | open(), release(), mmap() | queue_rq blk-mq handler |
| Registration | register_chrdev() + class_create() | register_blkdev() + add_disk() |
| Main Structure | file_operations + vm_operations | block_device_operations + blk_mq_ops |
| Buffering | None — direct page mapping | Kernel page cache |
| Use Case | Custom memory-mapped device | Mountable storage volume |


## 4.7  Advanced Extensions
The RAM disk driver can be extended with the following enhancements for production-quality or ARM SoC-specific use:
| /* 1. DMA support (for real hardware) */ |
| dev->data = dma_alloc_coherent(device, size, &dma_handle, GFP_KERNEL); |
|  |
| /* 2. Partition support (up to 16 partitions) */ |
| #define RAMDISK_MINORS 16 |
|  |
| /* 3. Multiple device instances */ |
| static struct ramdisk_device *devices[NUM_DEVICES]; |
|  |
| /* 4. TRIM/DISCARD support (for flash-backed devices) */ |
| static struct blk_mq_ops ramdisk_mq_ops = { |
| .queue_rq = ramdisk_queue_rq, |
| .commit_rqs = ramdisk_commit_rqs, |
| }; |
|  |
| /* 5. Persistent storage (save contents on module exit) */ |
| struct file *f = filp_open("/var/ramdisk.img", O_WRONLY|O_CREAT, 0644); |
| kernel_write(f, dev->data, dev->size, &pos); |
| filp_close(f, NULL); |


## 4.8  Quick Reference: Build & Usage
| # Build |
| make |
|  |
| # Load module |
| sudo insmod simple_ramdisk.ko |
|  |
| # Verify device |
| ls -l /dev/sramdisk |
| lsblk /dev/sramdisk |
|  |
| # Create and mount filesystem |
| sudo mkfs.ext4 /dev/sramdisk |
| sudo mkdir -p /mnt/ramdisk |
| sudo mount /dev/sramdisk /mnt/ramdisk |
|  |
| # Use it |
| echo "Hello World" | sudo tee /mnt/ramdisk/test.txt |
| ls -l /mnt/ramdisk/ |
|  |
| # Performance check |
| sudo dd if=/dev/zero of=/mnt/ramdisk/test bs=1M count=10 |
|  |
| # Unmount and unload |
| sudo umount /mnt/ramdisk |
| sudo rmmod simple_ramdisk |



| Linux Kernel Memory Management & Custom mmap Driver — Part 1End of Document | Chapters: 1. Memory Allocation | 2. Custom mmap Driver | 3. Block vs Char Driver | 4. RAM Disk Driver |

